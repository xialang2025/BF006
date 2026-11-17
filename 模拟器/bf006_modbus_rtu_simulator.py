"""BF006 控制器模拟器，提供 4G UART4 V1.3 和 RS485 Modbus RTU。

离线协议处理只使用标准库，连接串口时使用 pyserial。
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from decimal import Decimal, InvalidOperation
import json
import os
import shlex
import sys
from struct import pack
import threading
import time
from typing import Any

APP_TITLE = "BF006 控制器模拟器 · 4G V1.3 / RS485"
PYSERIAL_INSTALL_MESSAGE = (
    "缺少 pyserial 依赖。\n"
    "请在命令行执行：py -m pip install pyserial\n"
    "安装完成后重新启动模拟器。"
)

DEFAULT_SLAVE_ADDRESS = 1
DEFAULT_BAUD_RATE = 9600
SERIAL_READ_TIMEOUT_SECONDS = 0.03
SERIAL_WRITE_TIMEOUT_SECONDS = 0.5
SERIAL_FRAME_GAP_SECONDS = 0.05
UART4_FRAME_GAP_SECONDS = 0.010  #20261117-2 change：按连续静默分帧，帧长以实际串口接收为准。
UART4_READ_TIMEOUT_SECONDS = 0.002
UART4_BAUD_RATE = 9600  #20261117-2 change：实体模块与控制器端固定使用相同字符格式。
UART4_MAX_FRAME = 97
MAX_WORDS_PER_REQUEST = 8

_CLI_OUTPUT_LOCK = threading.Lock()

MODBUS_READ_HOLDING = 0x03
MODBUS_WRITE_SINGLE = 0x06
MODBUS_WRITE_MULTIPLE = 0x10
BF006_PRIVATE_SET_ADDRESS = 0x41
BF006_PRIVATE_CLEAR_TIME = 0x42
BF006_PRIVATE_BROADCAST_ADDRESS = 0xFF
BF006_PRIVATE_REGISTER = 0xC351
BF006_PRIVATE_CONFIRM_BYTE = 0xA5

ERR_ILLEGAL_FUNCTION = 0x01
ERR_ILLEGAL_REGISTER = 0x02
ERR_ILLEGAL_DATA = 0x03

ERROR_CODE_TEXT = {
    ERR_ILLEGAL_FUNCTION: "非法功能",
    ERR_ILLEGAL_REGISTER: "非法寄存器",
    ERR_ILLEGAL_DATA: "非法数据",
    6: "设备正在提交配置，请稍后重试",
}

RUN_STATUS_MANUAL = 0x0001
RUN_STATUS_AUTO_OR_TIMER = 0x0002
RUN_STATUS_AUTO = 0x0004
RUN_STATUS_TIMER = 0x0008
RUN_STATUS_PUMP_A = 0x0010
RUN_STATUS_PUMP_B = 0x0020
RUN_STATUS_A_MAIN = 0x0040
RUN_STATUS_B_MAIN = 0x0080
RUN_STATUS_ALTERNATING = 0x0100
RUN_STATUS_UPPER_LOW = 0x0200
RUN_STATUS_UPPER_MIDDLE = 0x0400
RUN_STATUS_UPPER_HIGH = 0x0800
RUN_STATUS_BOTTOM_LOW = 0x1000
RUN_STATUS_BOTTOM_MIDDLE = 0x2000
RUN_STATUS_BOTTOM_HIGH = 0x4000
RUN_STATUS_ADD_PUMP_OR_OVERFLOW = 0x8000

FAULT_STATUS_UNDER_VOLTAGE = 0x0001
FAULT_STATUS_OVER_VOLTAGE = 0x0002
FAULT_STATUS_OVERLOAD = 0x0004
FAULT_STATUS_DRY_RUN = 0x0008
FAULT_STATUS_PHASE_LOSS = 0x0020
FAULT_STATUS_UPPER_FULL = 0x0040
FAULT_STATUS_BOTTOM_SHORTAGE = 0x0100

CONTROL_SCHEME_ONE_TO_ONE = "一控一"
CONTROL_SCHEME_ONE_TO_TWO = "一控二"
CONTROL_SCHEME_OPTIONS = (CONTROL_SCHEME_ONE_TO_ONE, CONTROL_SCHEME_ONE_TO_TWO)
CONTROL_SCHEME_DEVICE_MODEL_CODES = {
    CONTROL_SCHEME_ONE_TO_ONE: 0xA2,
    CONTROL_SCHEME_ONE_TO_TWO: 0xA3,
}

SENSOR_TYPE_OPTIONS = (
    "0 - 液位电极/浮球",
    "1 - 电接点压力表",
    "2 - 4-20mA液位传感器",
    "3 - 4-20mA压力传感器",
    "4 - 0-5V压力传感器",
)
SENSOR_TYPE_CODES = {label: index for index, label in enumerate(SENSOR_TYPE_OPTIONS)}
SENSOR_TARGET_OPTIONS = ("上水池", "下水池")
SENSOR_LEVEL_OPTIONS = ("低", "中", "高")
SENSOR_STATUS_OPTIONS = (
    ("传感器断线或输入异常 bit0", 0x0001),
    ("超过 P-20 溢出阈值 bit1", 0x0002),
    ("系统处于溢流状态 bit2", 0x0004),
    ("上水池显示闪烁 bit3", 0x0008),
    ("下水池显示闪烁 bit4", 0x0010),
)
SENSOR_STATUS_THRESHOLD_EXCEEDED = 0x0002
SENSOR_STATUS_OVERFLOW_ACTIVE = 0x0004
SENSOR_STATUS_AUTOMATIC_OVERFLOW_MASK = SENSOR_STATUS_THRESHOLD_EXCEEDED | SENSOR_STATUS_OVERFLOW_ACTIVE


@dataclass(frozen=True)
class ParameterDefinition:
    address: int
    number: str
    firmware_name: str
    display_name: str
    minimum: int
    maximum: int
    default: int
    unit: str
    range_text: str
    writable: bool = True
    sensor_range_limited: bool = False


@dataclass(frozen=True)
class MonitorDefinition:
    address: int
    firmware_name: str
    display_name: str
    unit: str = ""


@dataclass(frozen=True)
class FaultDefinition:
    enum_code: int
    firmware_name: str
    display_name: str
    summary_bit: int
    pump: str | None = None

    @property
    def current_code(self) -> int:
        """2100H uses zero for no fault and firmware enum + 1 for a fault."""
        return self.enum_code + 1


@dataclass
class HistoryRecord:
    fault_type: int = 17
    voltage: int = 0
    current: int = 0


@dataclass
class ModbusProtocolResult:
    request: bytes
    response: bytes | None
    message: str
    crc_ok: bool
    ignored: bool = False
    exception_code: int | None = None


PARAMETERS = (
    ParameterDefinition(0x0000, "P-00", "OVP_Value", "过压保护阈值", 85, 480, 260, "V", "85-480（模拟机型）"),
    ParameterDefinition(0x0001, "P-01", "UVP_Value", "欠压保护阈值", 85, 480, 150, "V", "85-480（模拟机型）"),
    #20261109-7 change: Match the BF006 single-pump 80.0 A setting range without changing defaults or the 0.1 A wire scale.
    ParameterDefinition(0x0002, "P-02", "Set_STANDBY_Current1", "A泵空载/干转保护电流", 0, 800, 0, "0.1A", "0-80.0A（模拟机型）"),
    ParameterDefinition(0x0003, "P-03", "Set_OVERLOAD_Current1", "A泵过流保护电流", 0, 800, 100, "0.1A", "0-80.0A（模拟机型）"),
    ParameterDefinition(0x0004, "P-04", "Set_STANDBY_Current2", "B泵空载/干转保护电流", 0, 130, 0, "0.1A", "0-13.0A（模拟机型）"),
    ParameterDefinition(0x0005, "P-05", "Set_OVERLOAD_Current2", "B泵过流保护电流", 0, 130, 100, "0.1A", "0-13.0A（模拟机型）"),
    ParameterDefinition(0x0006, "P-06", "AUTO_Timing_Run", "循环定时运行时间", 0, 9999, 30, "min", "0-9999"),
    ParameterDefinition(0x0007, "P-07", "AUTO_Timing_Stop", "循环定时停机时间", 0, 9999, 60, "min", "0-9999"),
    ParameterDefinition(0x0008, "P-08", "Timer_Standby_Restart", "缺水保护自动重启时间", 0, 9999, 30, "s", "0-9999"),
    ParameterDefinition(0x0009, "P-09", "Timer_Unload_Restart", "过载重启时间", 0, 9999, 30, "s", "0-9999"),
    ParameterDefinition(0x000A, "P-10", "AUTO_FANGXIU", "防锈自动启动间隔", 0, 999, 99, "h", "0-999"),
    ParameterDefinition(0x000B, "P-11", "Dev_Address", "485 通讯地址", 1, 254, 1, "", "1-254"),
    ParameterDefinition(0x000C, "P-12", "Pump_AB_Mode", "双泵主备模式", 0, 2, 0, "", "0=A主，1=B主，2=轮换"),
    ParameterDefinition(0x000D, "P-13", "Sensor_Mode", "供水/排污模式", 0, 1, 0, "", "0=供水，1=排污"),
    ParameterDefinition(0x000E, "P-14", "Sensor_Rang", "传感器量程", 0, 9999, 10, "工程量", "0-9999"),
    ParameterDefinition(0x000F, "P-15", "Sensor_STOP", "传感器停止阈值", 0, 9999, 9, "工程量", "0-传感器量程", sensor_range_limited=True),
    ParameterDefinition(0x0010, "P-16", "Sensor_ON1", "传感器启动阈值", 0, 9999, 5, "工程量", "0-传感器量程", sensor_range_limited=True),
    ParameterDefinition(0x0011, "P-17", "Sensor_ON2", "双泵启动阈值", 0, 9999, 1, "工程量", "0-传感器量程", sensor_range_limited=True),
    ParameterDefinition(0x0012, "P-18", "Auto_Mode", "传感器类型", 0, 4, 0, "", "0-4"),
    ParameterDefinition(0x0013, "P-19", "fault_mask", "故障屏蔽选项", 0, 4, 0, "", "0-4"),
    ParameterDefinition(0x0014, "P-20", "Sensor_overFlow", "传感器溢出报警值", 0, 9999, 0, "工程量", "0-传感器量程", sensor_range_limited=True),
    ParameterDefinition(0x0015, "P-21", "Total_Run_Hour", "累计运行时间", 0, 9999, 0, "h", "只读", writable=False),
    ParameterDefinition(0x0016, "P-22", "Total_Power_Day", "累计通电时间", 0, 9999, 0, "day", "只读", writable=False),
)

PARAMETER_BY_ADDRESS = {item.address: item for item in PARAMETERS}
PARAMETER_BY_NUMBER = {item.number.upper(): item for item in PARAMETERS}

MONITORS = (
    MonitorDefinition(0x2100, "Fault_Code", "当前故障代码"),
    MonitorDefinition(0x2101, "Run_Status", "运行状态位"),
    MonitorDefinition(0x2102, "Fault_Status_Bits", "故障汇总位"),
    MonitorDefinition(0x2103, "LineVoltage", "线路电压", "V"),
    MonitorDefinition(0x2104, "PumpA_Current", "A泵电流", "显示标定"),
    MonitorDefinition(0x2105, "PumpB_Current", "B泵电流", "显示标定"),
    MonitorDefinition(0x2106, "Power", "功率", "W"),
    MonitorDefinition(0x2107, "Sensor_ON2", "双泵启动阈值", "工程量"),
    MonitorDefinition(0x2108, "Sensor_STOP", "停止阈值", "工程量"),
    MonitorDefinition(0x2109, "SensorOut", "当前传感器值", "工程量"),
    MonitorDefinition(0x210A, "UpWater_Level", "上水池水位"),
    MonitorDefinition(0x210B, "BottomWater_Level", "下水池水位"),
    MonitorDefinition(0x210C, "Pump_State", "泵状态位"),
    MonitorDefinition(0x210D, "DeviceModel", "设备型号/规格"),
    MonitorDefinition(0x210E, "Version", "协议版本"),
    MonitorDefinition(0x210F, "Sensor_Type", "当前传感器类型"),
    MonitorDefinition(0x2110, "Sensor_Value", "连续传感器值", "0.01工程量"),
    MonitorDefinition(0x2111, "Sensor_Unit", "连续值单位"),
    MonitorDefinition(0x2112, "Sensor_Target", "传感器作用水池"),
    MonitorDefinition(0x2113, "Sensor_Level", "模拟量水池档位"),
    MonitorDefinition(0x2114, "UpWater_Segments", "上水池六段位图"),
    MonitorDefinition(0x2115, "BottomWater_Segments", "下水池六段位图"),
    MonitorDefinition(0x2116, "Sensor_Status", "传感器/水池状态位"),
    MonitorDefinition(0x2117, "Fault_Bits_L", "完整故障位图低字"),
    MonitorDefinition(0x2118, "Fault_Bits_H", "完整故障位图高字"),
)
MONITOR_BY_ADDRESS = {item.address: item for item in MONITORS}

FAULTS = (
    FaultDefinition(0, "ERR_UVP", "欠压", FAULT_STATUS_UNDER_VOLTAGE),
    FaultDefinition(1, "ERR_OVP", "过压", FAULT_STATUS_OVER_VOLTAGE),
    FaultDefinition(2, "ERR_OVERLOAD1", "A泵过载", FAULT_STATUS_OVERLOAD, "A"),
    FaultDefinition(3, "ERR_STANDBY1", "A泵空载/欠载", FAULT_STATUS_DRY_RUN, "A"),
    FaultDefinition(4, "ERR_LOSE_A1", "A泵A相缺相", FAULT_STATUS_PHASE_LOSS, "A"),
    FaultDefinition(5, "ERR_LOSE_B1", "A泵B相缺相", FAULT_STATUS_PHASE_LOSS, "A"),
    FaultDefinition(6, "ERR_LOSE_C1", "A泵C相缺相", FAULT_STATUS_PHASE_LOSS, "A"),
    FaultDefinition(7, "ERR_OVERLOAD2", "B泵过载", FAULT_STATUS_OVERLOAD, "B"),
    FaultDefinition(8, "ERR_STANDBY2", "B泵空载/欠载", FAULT_STATUS_DRY_RUN, "B"),
    FaultDefinition(9, "ERR_LOSE_A2", "B泵A相缺相", FAULT_STATUS_PHASE_LOSS, "B"),
    FaultDefinition(10, "ERR_LOSE_B2", "B泵B相缺相", FAULT_STATUS_PHASE_LOSS, "B"),
    FaultDefinition(11, "ERR_LOSE_C2", "B泵C相缺相", FAULT_STATUS_PHASE_LOSS, "B"),
    FaultDefinition(12, "ERR_SENSOR", "传感器异常", 0),
    FaultDefinition(13, "ERR_NOLOAD_A", "A泵无负载/空转", FAULT_STATUS_DRY_RUN, "A"),
    FaultDefinition(14, "ERR_NOLOAD_B", "B泵无负载/空转", FAULT_STATUS_DRY_RUN, "B"),
    FaultDefinition(15, "ERR_LOCK_A", "A泵锁定", 0, "A"),
    FaultDefinition(16, "ERR_LOCK_B", "B泵锁定", 0, "B"),
)

FAULT_BY_ENUM_CODE = {item.enum_code: item for item in FAULTS}

FAULT_BIT_OPTIONS = (
    ("欠压 bit0", FAULT_STATUS_UNDER_VOLTAGE),
    ("过压 bit1", FAULT_STATUS_OVER_VOLTAGE),
    ("过载 bit2", FAULT_STATUS_OVERLOAD),
    ("空载/欠载 bit3", FAULT_STATUS_DRY_RUN),
    ("缺相 bit5", FAULT_STATUS_PHASE_LOSS),
    ("上水池满 bit6", FAULT_STATUS_UPPER_FULL),
    ("下水池缺水 bit8", FAULT_STATUS_BOTTOM_SHORTAGE),
)


def modbus_crc16(payload: bytes) -> int:
    """Calculate the standard Modbus RTU CRC16 value."""
    crc = 0xFFFF
    for value in payload:
        crc ^= value
        for _bit in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def append_modbus_crc(payload: bytes) -> bytes:
    """Append CRC in Modbus wire order: low byte followed by high byte."""
    crc = modbus_crc16(payload)
    return payload + bytes((crc & 0xFF, (crc >> 8) & 0xFF))


def frame_to_hex(frame: bytes | None) -> str:
    if frame is None:
        return "无响应"
    return " ".join(f"{value:02X}" for value in frame)


def read_u16(frame: bytes, offset: int) -> int:
    return (frame[offset] << 8) | frame[offset + 1]


def u16_bytes(value: int) -> bytes:
    value &= 0xFFFF
    return bytes(((value >> 8) & 0xFF, value & 0xFF))


def parse_int(text: str) -> int:
    """Parse a decimal value or a value prefixed with 0x."""
    stripped = text.strip()
    if not stripped:
        raise ValueError("数值不能为空")
    return int(stripped, 0) if stripped.lower().startswith(("0x", "+0x", "-0x")) else int(stripped, 10)


def parse_scaled_u16(text: str, scale: int, field_name: str, maximum: int = 0xFFFF) -> int:
    stripped = text.strip()
    if not stripped:
        raise ValueError(f"{field_name}不能为空")
    try:
        value = Decimal(stripped)
    except InvalidOperation as exc:
        raise ValueError(f"{field_name}不是有效数值") from exc

    scaled = value * scale
    if not scaled.is_finite():
        raise ValueError(f"{field_name}不是有限数值")
    if scaled != scaled.to_integral_value():
        decimal_places = len(str(scale)) - 1
        raise ValueError(f"{field_name}最多支持{decimal_places}位小数")
    if not 0 <= scaled <= maximum:
        raise ValueError(f"{field_name}超出协议范围")
    return int(scaled)


def parse_monitor_editor_values(
    *,
    voltage: str,
    pump_a_current: str,
    pump_b_current: str,
    power: str,
    sensor_out: str,
    device_specification: str,
    version: str,
    total_run: str,
    total_power: str,
) -> tuple[int, ...]:
    values = (
        parse_int(voltage),
        parse_scaled_u16(pump_a_current, 10, "A泵电流"),
        parse_scaled_u16(pump_b_current, 10, "B泵电流"),
        parse_int(power),
        parse_scaled_u16(sensor_out, 100, "传感器值"),
        parse_int(device_specification),
        parse_int(version),
        parse_int(total_run),
        parse_int(total_power),
    )
    if any(value < 0 or value > 0xFFFF for value in values):
        raise ValueError("监视量必须在 0-65535 范围内")
    if values[-2] > 9999 or values[-1] > 9999:
        raise ValueError("累计运行时间和累计通电时间必须在 0-9999 范围内")
    return values


def format_scaled_value(raw: int, scale: int, decimal_places: int) -> str:
    text = f"{Decimal(raw) / scale:.{decimal_places}f}"
    return text.rstrip("0").rstrip(".")


def device_model_for_control_scheme(control_scheme: str, specification: int) -> int:
    """Compose 210DH: the selected pump layout is the high byte and the specification is the low byte."""
    try:
        model_code = CONTROL_SCHEME_DEVICE_MODEL_CODES[control_scheme]
    except KeyError as exc:
        raise ValueError("控制方案必须选择一控一或一控二") from exc
    if specification < 0 or specification > 0xFF:
        raise ValueError("规格档位必须在 0-255 范围内")
    return (model_code << 8) | specification


def control_scheme_from_device_model(device_model: int) -> str:
    """Decode 210DH using the physical-controller model codes plus legacy simulator aliases."""
    model_code = (device_model >> 8) & 0xFF
    if model_code in (0x00, 0x01, 0xA2, 0xB2):
        return CONTROL_SCHEME_ONE_TO_ONE
    return CONTROL_SCHEME_ONE_TO_TWO


def expected_request_length(buffer: bytes | bytearray) -> int | None:
    """Return the expected length of the first RTU request in a serial buffer."""
    if len(buffer) < 2:
        return None
    if buffer[1] == MODBUS_WRITE_MULTIPLE:
        if len(buffer) < 7:
            return None
        return 9 + buffer[6]
    return 8


class Uart4Receiver:  #20261117-2 change
    """按静默间隔提取整帧，超长时丢弃到下一次完整静默。"""

    def __init__(self) -> None:
        self.buffer = bytearray()
        self.discard = False
        self.last_rx: float | None = None

    def poll(self, data: bytes, now: float) -> bytes | None:
        """根据本次收到的字节与单调时钟，返回已确认边界的请求或 None。"""
        if data:
            # 驱动可能分批交付同一帧，只有空读才能确认驱动当前无待接收字节。
            self.last_rx = now
            if len(self.buffer) + len(data) > UART4_MAX_FRAME:
                self.discard = True
                self.buffer.clear()
            elif not self.discard:
                self.buffer.extend(data)
        elif self.last_rx is not None and now - self.last_rx >= UART4_FRAME_GAP_SECONDS:
            frame = bytes(self.buffer) if not self.discard and len(self.buffer) >= 11 else None
            self.buffer.clear()
            self.discard = False
            self.last_rx = None
            return frame
        return None

class Bf006State:
    """Mutable BF006 state shared by the protocol core and the GUI."""

    def __init__(self) -> None:
        self.reset_defaults()

    def reset_defaults(self) -> None:
        self.parameter_values = {item.address: item.default for item in PARAMETERS}
        self.run_mode = 0
        self.pump_ab_mode = 0
        self.pump_a_running = False
        self.pump_b_running = False
        self.pump_control_command = 0
        self.upper_water_level = 2
        self.bottom_water_level = 2
        self.add_pump_or_overflow = False
        self.active_fault_enum: int | None = None
        self.fault_status_bits = 0
        self.pump_a_fault = False
        self.pump_b_fault = False
        self.line_voltage = 220
        self.pump_a_current = 0
        self.pump_b_current = 0
        self.power = 0
        self.sensor_out = 0
        self.sensor_value = 0
        #20261117-2 change：保留既有监视默认值，独立记录V1.3采样是否已发布。
        self.phases = 1
        self.rated_voltage = 220
        self.phase_currents: dict[int, int] = {}
        self.pending_tanks: set[int] = set()
        self.sensor_alarm_pending = False
        self.running_sensor_type = 0
        self.adc_sample_valid = False
        self.sensor_sample_type: int | None = None
        self.sensor_level = 2
        self.upper_water_segments = 0x003F
        self.bottom_water_segments = 0x003F
        self._manual_sensor_status = 0
        self.fault_bits_32 = 0
        self.device_model = 0xA393
        self.version = 201
        self.parameter_saved = False
        self.history = [HistoryRecord() for _ in range(10)]

    def set_control_scheme(self, control_scheme: str) -> None:
        self.device_model = device_model_for_control_scheme(control_scheme, self.device_model & 0xFF)

    @property
    def manual_sensor_status(self) -> int:
        return self._manual_sensor_status

    @property
    def sensor_status(self) -> int:
        sensor_type = self.parameter_values[0x0012]
        overflow_threshold_raw = self.parameter_values[0x0014]
        automatic_status = 0
        # P-20 uses 0.1 engineering units while 2110H uses 0.01 engineering units.
        # Zero keeps automatic overflow disabled so protocol states can still be injected manually.
        if (
            sensor_type in (2, 3, 4)
            and overflow_threshold_raw > 0
            and self.sensor_value > overflow_threshold_raw * 10
        ):
            automatic_status = SENSOR_STATUS_AUTOMATIC_OVERFLOW_MASK
        return (self._manual_sensor_status | automatic_status) & 0x1F

    @sensor_status.setter
    def sensor_status(self, value: int) -> None:
        self._manual_sensor_status = value & 0x1F

    def set_sensor_state(
        self,
        *,
        sensor_type: int,
        sensor_value: int,
        sensor_target: int,
        sensor_level: int,
        upper_segments: int,
        bottom_segments: int,
        sensor_status: int,
    ) -> None:
        if sensor_type not in range(5):
            raise ValueError("传感器类型必须在 0-4 范围内")
        if sensor_value < 0 or sensor_value > 0xFFFFFFFF:
            raise ValueError("连续传感器值超出协议 U32 范围")
        if sensor_target not in (0, 1):
            raise ValueError("传感器作用水池必须是上水池或下水池")
        if sensor_level not in (0, 1, 2):
            raise ValueError("模拟量水池档位必须是低、中或高")
        if upper_segments < 0 or upper_segments > 0x3F or bottom_segments < 0 or bottom_segments > 0x3F:
            raise ValueError("上、下水池六段位图必须在 0x00-0x3F 范围内")
        if sensor_status < 0 or sensor_status > 0x1F:
            raise ValueError("传感器状态只允许使用 bit0-bit4")

        self.parameter_values[0x0012] = sensor_type
        self.parameter_values[0x000D] = sensor_target
        self.sensor_value = sensor_value
        #20261117-2 change：显式注入完成传感器类型、工程值和两侧水池的采样发布。
        self.running_sensor_type = sensor_type
        self.sensor_sample_type = sensor_type
        self.pending_tanks.clear()
        self.sensor_alarm_pending = False
        self.sensor_level = sensor_level
        self.upper_water_segments = upper_segments
        self.bottom_water_segments = bottom_segments
        self._manual_sensor_status = sensor_status

    def sensor_unit(self) -> int:
        sensor_type = self.parameter_values[0x0012]
        if sensor_type == 2:
            return 1
        if sensor_type in (3, 4):
            return 2
        return 0

    def set_parameter_value(self, address: int, value: int) -> None:  #20261117-2 add
        """发布已由调用方校验的参数值并标记失效的传感器来源。"""
        if value == self.parameter_values[address]:
            return
        self.parameter_values[address] = value
        self.parameter_saved = False
        if address == 12:
            self.pump_ab_mode = value
        if address in (13, 18):
            self.pending_tanks.update((0, 1))
        if address in (14, 18):
            self.sensor_sample_type = None
        if address in (14, 15, 16, 17) and self.parameter_values[18] >= 2:
            self.pending_tanks.add(self.parameter_values[13])
        if address == 20:
            self.sensor_alarm_pending = (
                self.sensor_sample_type != self.parameter_values[18]
                or bool(self.manual_sensor_status & 1)
            )

    def _parameter_error(
        self,
        address: int,
        value: int,
        values: dict[int, int] | None = None,
    ) -> int:
        definition = PARAMETER_BY_ADDRESS.get(address)
        if definition is None or not definition.writable:
            return ERR_ILLEGAL_REGISTER
        if value < 0 or value > 0xFFFF:
            return ERR_ILLEGAL_DATA

        maximum = definition.maximum
        source = self.parameter_values if values is None else values
        if definition.sensor_range_limited:
            maximum = min(maximum, source.get(0x000E, maximum))
        if value < definition.minimum or value > maximum:
            return ERR_ILLEGAL_DATA
        return 0

    def write_parameter(self, address: int, value: int) -> int:
        error = self._parameter_error(address, value)
        if error:
            return error
        self.set_parameter_value(address, value)
        self.parameter_saved = False
        return 0

    def write_parameters_atomically(self, start_address: int, values: list[int]) -> int:
        preview = dict(self.parameter_values)
        for offset, value in enumerate(values):
            address = start_address + offset
            error = self._parameter_error(address, value, preview)
            if error:
                return error
            preview[address] = value & 0xFFFF

        for offset, value in enumerate(values):
            self.set_parameter_value(start_address + offset, value)
        self.parameter_saved = False
        return 0

    def write_control(self, address: int, value: int) -> int:
        if address == 0x2000:
            action = value & 0x0003
            target = (value >> 4) & 0x0003
            if action == 0 and target == 0:
                self.pump_control_command = 0
                return 0
            if action not in (1, 2) or target not in (1, 2, 3) or value & ~0x0033:
                return ERR_ILLEGAL_DATA
            if action == 2:
                if self.run_mode != 0:
                    return ERR_ILLEGAL_DATA
                if target in (1, 3) and self.pump_a_fault:
                    return ERR_ILLEGAL_DATA
                if target in (2, 3) and self.pump_b_fault:
                    return ERR_ILLEGAL_DATA
            running = action == 2
            if target in (1, 3):
                self.pump_a_running = running
            if target in (2, 3):
                self.pump_b_running = running
            self.pump_control_command = value
            return 0

        if address == 0x2003:
            if value not in (0, 1, 2):
                return ERR_ILLEGAL_DATA
            self.run_mode = value
            return 0

        if address == 0x2004:
            if value not in (0, 1, 2):
                return ERR_ILLEGAL_DATA
            self.pump_ab_mode = value
            self.parameter_values[0x000C] = value
            return 0

        if address == 0x2005:
            if value != 0x0001:
                return ERR_ILLEGAL_DATA
            self.clear_faults()
            return 0

        if address == 0x2006:
            if value != 0x0000:
                return ERR_ILLEGAL_DATA
            self.clear_history()
            return 0

        if address == 0x2007:
            if value != 0x5AA5:
                return ERR_ILLEGAL_DATA
            self.parameter_saved = True
            return 0

        return ERR_ILLEGAL_REGISTER

    def press_local_stop(self) -> None:
        """Simulate the controller panel Stop key while a remote command may still be held."""
        self.pump_a_running = False
        self.pump_b_running = False
        action = self.pump_control_command & 0x0003
        target = (self.pump_control_command >> 4) & 0x0003
        if action != 2:
            return
        if target in (1, 3) and not self.pump_a_fault:
            self.pump_a_running = True
        if target in (2, 3) and not self.pump_b_fault:
            self.pump_b_running = True

    def apply_fault(self, enum_code: int, add_history: bool = True) -> int:
        fault = FAULT_BY_ENUM_CODE.get(enum_code)
        if fault is None:
            return ERR_ILLEGAL_DATA

        self.active_fault_enum = enum_code
        self.fault_status_bits |= fault.summary_bit
        self.fault_bits_32 |= 1 << enum_code
        if fault.pump == "A":
            self.pump_a_fault = True
        elif fault.pump == "B":
            self.pump_b_fault = True

        if add_history:
            current = self.pump_a_current if fault.pump == "A" else self.pump_b_current
            if fault.pump is None:
                current = max(self.pump_a_current, self.pump_b_current)
            self.history.insert(0, HistoryRecord(enum_code, self.line_voltage, current))
            del self.history[10:]
        return 0

    def clear_faults(self) -> None:
        self.active_fault_enum = None
        self.fault_status_bits = 0
        self.fault_bits_32 = 0
        self.pump_a_fault = False
        self.pump_b_fault = False

    def set_manual_fault_bits(self, bits: int, pump_a_fault: bool, pump_b_fault: bool) -> int:
        if bits < 0 or bits > 0xFFFF:
            return ERR_ILLEGAL_DATA
        self.fault_status_bits = bits
        self.pump_a_fault = pump_a_fault
        self.pump_b_fault = pump_b_fault
        return 0

    def clear_history(self) -> None:
        self.history = [HistoryRecord() for _ in range(10)]

    def set_history_record(self, index: int, fault_type: int, voltage: int, current: int) -> int:
        if index < 0 or index >= 10:
            return ERR_ILLEGAL_DATA
        if fault_type < 0 or fault_type > 17:
            return ERR_ILLEGAL_DATA
        if any(value < 0 or value > 0xFFFF for value in (voltage, current)):
            return ERR_ILLEGAL_DATA
        self.history[index] = HistoryRecord(fault_type, voltage, current)
        return 0

    def current_fault_code(self) -> int:
        if self.active_fault_enum is None:
            return 0
        return self.active_fault_enum + 1

    def run_status(self) -> int:
        value = 0
        if self.run_mode == 0:
            value |= RUN_STATUS_MANUAL
        elif self.run_mode == 1:
            value |= RUN_STATUS_AUTO_OR_TIMER | RUN_STATUS_AUTO
        else:
            value |= RUN_STATUS_AUTO_OR_TIMER | RUN_STATUS_TIMER

        if self.pump_a_running:
            value |= RUN_STATUS_PUMP_A
        if self.pump_b_running:
            value |= RUN_STATUS_PUMP_B

        value |= (RUN_STATUS_A_MAIN, RUN_STATUS_B_MAIN, RUN_STATUS_ALTERNATING)[self.pump_ab_mode]
        value |= (RUN_STATUS_UPPER_LOW, RUN_STATUS_UPPER_MIDDLE, RUN_STATUS_UPPER_HIGH)[self.upper_water_level]
        value |= (RUN_STATUS_BOTTOM_LOW, RUN_STATUS_BOTTOM_MIDDLE, RUN_STATUS_BOTTOM_HIGH)[self.bottom_water_level]
        if self.add_pump_or_overflow:
            value |= RUN_STATUS_ADD_PUMP_OR_OVERFLOW
        return value & 0xFFFF

    def pump_state(self) -> int:
        value = 0
        if self.pump_a_running:
            value |= 0x0001
        if self.pump_b_running:
            value |= 0x0002
        if self.pump_a_fault:
            value |= 0x0100
        if self.pump_b_fault:
            value |= 0x0200
        return value

    def read_register(self, address: int) -> tuple[int, int]:
        if address in PARAMETER_BY_ADDRESS:
            return 0, self.parameter_values[address]
        monitor_values = {
            0x2100: self.current_fault_code(),
            0x2101: self.run_status(),
            0x2102: self.fault_status_bits,
            0x2103: self.line_voltage,
            0x2104: self.pump_a_current,
            0x2105: self.pump_b_current,
            0x2106: self.power,
            0x2107: self.parameter_values[0x0011],
            0x2108: self.parameter_values[0x000F],
            0x2109: min(self.sensor_out, 0xFFFF) if self.sensor_value <= 0xFFFF else 0xFFFF,  #20261113-0 change：旧接口超宽时返回无效码。
            0x210A: self.upper_water_level,
            0x210B: self.bottom_water_level,
            0x210C: self.pump_state(),
            0x210D: self.device_model,
            0x210E: self.version,
            0x210F: self.parameter_values[0x0012],
            0x2110: min(self.sensor_value, 0xFFFF) if self.parameter_values[0x0012] >= 2 else 0xFFFF,
            0x2111: self.sensor_unit(),
            0x2112: self.parameter_values[0x000D],
            0x2113: self.sensor_level if self.parameter_values[0x0012] >= 2 else 0xFFFF,
            0x2114: self.upper_water_segments,
            0x2115: self.bottom_water_segments,
            0x2116: self.sensor_status,
            0x2117: self.fault_bits_32 & 0xFFFF,
            0x2118: (self.fault_bits_32 >> 16) & 0xFFFF,
        }
        if address in monitor_values:
            return 0, monitor_values[address] & 0xFFFF
        if 0x2200 <= address <= 0x221D:
            offset = address - 0x2200
            record = self.history[offset // 3]
            return 0, (record.fault_type, record.voltage, record.current)[offset % 3]
        return ERR_ILLEGAL_REGISTER, 0


#20261117-2 add：综合帧按顺序紧凑编码；来源号最高位区分参数与运行字段。
UART4_MAIN_FIELDS = (
    (1, 2), (2, 2), (3, 2), (4, 2), (8, 2), (10, 4),
    (12, 1), (13, 1), (14, 1), (15, 1), (16, 4), (17, 1), (18, 1), (19, 1),
    (0x8E, 2), (0x90, 2), (0x8F, 2), (0x94, 2), (0x80, 2),
    (0x81, 2), (0x83, 2), (0x82, 2), (0x86, 2), (0x87, 2),
)
UART4_EXTRA_FIELDS = (
    (5, 2), (6, 2), (7, 2), (0x84, 2), (0x85, 2), (0x8C, 1), (0x91, 2),
    (0x16, 2), (0x17, 2), (0x0B, 2), (0x15, 1),
)
UART4_REQUEST_LENGTHS = {1: 0, 2: 2, 3: 2, 4: 2, 5: 0, 6: 0, 0x10: 3, 0x11: 2}
UART4_SINGLE_PUMP_PARAMETERS = (4, 5, 12, 17)
UART4_PHASE_FAULTS = 0x0E70
UART4_B_PUMP_FAULTS = 0x14F80
UART4_CLEARABLE_FAULTS = 0x06FFF  # 欠过压、泵的过载、缺相、空载和干转。


class Bf006Uart4Protocol:  #20261117-2 add
    """实现实体4G模块所连接的 BF006 UART4 请求应答。"""

    def __init__(self, state: Bf006State) -> None:
        self.state = state

    @property
    def dual_pump(self) -> bool:
        """获取现有模拟机型选择中的泵数。"""
        return control_scheme_from_device_model(self.state.device_model) == CONTROL_SCHEME_ONE_TO_TWO

    def handle(self, request: bytes, occupied_addresses: tuple[int, ...] = ()) -> bytes | None:
        """校验完整请求并生成匹配的应答；非法帧静默丢弃。"""
        if (
            not 11 <= len(request) <= UART4_MAX_FRAME
            or request[0] != self.state.parameter_values[11]
            or request[0] in (0, 255)
            or request[1:4] != bytes((0x43, 0xBF, 0x06))
            or request[5] & 0x80
            or modbus_crc16(request) != 0
        ):
            return None

        fmt, command, rid_hi, rid_lo, declared_length = request[4:9]
        data = request[9:-2]
        object_number = data[0] if data and command in (2, 3, 4, 0x10, 0x11) else 0xFF
        if fmt != 3:
            # 与实机一致，只诊断仍能按固定公共头确认边界的异格式帧。
            if declared_length != len(data):
                return None
            result = self.error(2)
        elif declared_length != len(data) or not (rid_hi or rid_lo):
            result = self.error(3, object_number if declared_length else 0xFF)
        elif command not in UART4_REQUEST_LENGTHS:
            result = self.error(1)
        elif len(data) != UART4_REQUEST_LENGTHS[command]:
            result = self.error(3, object_number)
        elif command == 1:
            result = self.device_info()
        elif command in (2, 3, 4):
            result = self.read_range(command, *data)
        elif command in (5, 6):
            result = self.summary(command == 6)
        elif command == 0x10:
            result = self.write_parameter(data[0], read_u16(data, 1), occupied_addresses)
        else:
            result = self.control(*data)

        # 改址仍回显本次请求地址；CRC覆盖回填后的全部头和数据。
        header = request[:4] + bytes((3, command | 0x80, rid_hi, rid_lo, len(result)))
        return append_modbus_crc(header + result)

    @staticmethod
    def error(status: int, object_number: int = 0xFF, minimum: int = 0, maximum: int = 0) -> bytes:
        """生成读取与控制共用的固定长度失败数据。"""
        return pack(">BBII", status, object_number, minimum, maximum)

    def device_info(self) -> bytes:
        """编码当前模拟机型的身份、能力和各分段上限。"""
        state = self.state
        return pack(
            ">BBHHBBHHBBBB", 0, 0, state.device_model, state.version, state.phases,
            2 if self.dual_pump else 1, state.rated_voltage, 0x001F, UART4_MAX_FRAME, 16, 9, 8,
        )

    def parameter_bounds(self, number: int) -> tuple[int, int]:
        """从现有定义复用同口径范围，对协议版本不同的参数采用 V1.3 规则。"""
        definition = PARAMETER_BY_ADDRESS[number]
        if number in (0, 1):
            return 85, 480 if self.state.rated_voltage == 380 else 280
        if 2 <= number <= 5:
            return 0, 900
        if definition.sensor_range_limited:
            return 0, self.state.parameter_values[14]
        return definition.minimum, definition.maximum

    def parameter(self, number: int) -> tuple[int, int]:
        """读取参数质量及原值；不适用或异常仍保留已配置的原值。"""
        value = self.state.parameter_values[number]
        if not self.dual_pump and number in UART4_SINGLE_PUMP_PARAMETERS:
            return 1, value
        minimum, maximum = self.parameter_bounds(number)
        return (0 if minimum <= value <= maximum else 3), value

    def write_parameter(self, number: int, value: int, occupied: tuple[int, ...]) -> bytes:
        """按 V1.3 校验单参数，并将合法写入发布给两路串口。"""
        if number not in PARAMETER_BY_ADDRESS:
            return self.error(4, number)
        if not PARAMETER_BY_ADDRESS[number].writable:
            return self.error(5, number)
        if not self.dual_pump and number in UART4_SINGLE_PUMP_PARAMETERS:
            return self.error(6, number)
        minimum, maximum = self.parameter_bounds(number)
        if not minimum <= value <= maximum:
            return self.error(7, number, minimum, maximum)
        if number == 11 and value != self.state.parameter_values[11] and value in occupied:
            return self.error(0x0E, number)
        self.state.set_parameter_value(number, value)
        return pack(">BBH", 0, number, value)

    def read_range(self, command: int, start: int, count: int) -> bytes:
        """校验起点与区间上限并组装参数、运行字段或历史记录段。"""
        first, end, maximum = {2: (0, 23, 16), 3: (1, 25, 9), 4: (0, 10, 8)}[command]
        if not first <= start < end:
            return self.error(4, start)
        if not 1 <= count <= maximum or start + count > end:
            return self.error(3, start)

        result = bytearray((0, start, count))
        for number in range(start, start + count):
            if command == 2:
                result.extend(pack(">BH", *self.parameter(number)))
            elif command == 3:
                result.extend(pack(">BI", *self.runtime(number)))
            else:
                record = self.state.history[number]
                if record.fault_type == 17:
                    result.extend(pack(">BBHH", 4, 17, 0, 0))
                else:
                    quality = 0 if record.fault_type < 17 else 3
                    result.extend(pack(">BBHH", quality, record.fault_type, record.voltage, record.current))
        return bytes(result)

    def run_status(self) -> tuple[int, int]:
        """构造不含保留位的运行状态；控制应答可使用其中已知位。"""
        state = self.state
        quality = 0
        if state.run_mode in (0, 1, 2):
            value = (1, 6, 10)[state.run_mode]
        else:
            quality, value = 3, 0
        if state.pump_a_running:
            value |= RUN_STATUS_PUMP_A
        if self.dual_pump:
            if state.pump_b_running:
                value |= RUN_STATUS_PUMP_B
            priority = state.parameter_values[12]
            if priority in (0, 1, 2):
                value |= RUN_STATUS_A_MAIN << priority
            else:
                quality = 3
        levels = state.upper_water_level, state.bottom_water_level
        if state.pending_tanks:
            quality = 2
        elif any(level not in (0, 1, 2) for level in levels):
            quality = 3
        else:
            value |= (RUN_STATUS_UPPER_LOW << levels[0]) | (RUN_STATUS_BOTTOM_LOW << levels[1])
        return quality, value

    def runtime(self, number: int) -> tuple[int, int]:
        """用共享实时状态生成带质量的运行字段，按机型区分相与泵。"""
        state = self.state
        if number <= 7:
            if (number in (3, 4, 6, 7) and state.phases == 1) or (number >= 5 and not self.dual_pump):
                return 1, 0
            if number in (1, 2, 5):
                if not state.adc_sample_valid:
                    return 2, 0
                return 0, {1: state.line_voltage, 2: state.pump_a_current, 5: state.pump_b_current}[number]
            return (0, state.phase_currents[number]) if number in state.phase_currents else (2, 0)
        if number == 8:
            quality, value = self.run_status()
            return quality, 0 if quality else value
        if number in (9, 10, 11):
            faults = state.fault_bits_32
            invalid = 0xFFFE0000
            if state.phases == 1:
                invalid |= UART4_PHASE_FAULTS
            if not self.dual_pump:
                invalid |= UART4_B_PUMP_FAULTS
            if number == 11 and state.pending_tanks:
                return 2, 0
            if faults & invalid:
                return 3, faults if number == 10 else 0
            if number == 9:
                return 0, (faults & -faults).bit_length()
            if number == 10:
                return 0, faults
            alarm = faults & 3
            alarm |= 4 if faults & 0x0084 else 0
            alarm |= 8 if faults & 0x6108 else 0
            alarm |= 0x20 if faults & UART4_PHASE_FAULTS else 0
            alarm |= 0x40 if state.upper_water_level == 2 else 0
            alarm |= 0x100 if state.bottom_water_level == 0 else 0
            return 0, alarm
        if 12 <= number <= 15:
            if (number & 1) in state.pending_tanks:
                return 2, 0
            value = (
                state.upper_water_segments, state.bottom_water_segments,
                state.upper_water_level, state.bottom_water_level,
            )[number - 12]
            return (3, 0) if number >= 14 and value > 2 else (0, value)
        if number in (16, 17, 18, 20, 21):
            return self.sensor(number)
        if number == 19:
            direction = state.parameter_values[13]
            return (0, direction) if direction in (0, 1) else (3, 0)
        if number == 22:
            return 0, state.device_model
        if number == 23:
            return 0, state.version
        return 1, 0  # 实机功率字段未提供采样。

    def sensor(self, number: int) -> tuple[int, int]:
        """按已发布类型与样本生成工程值、显示标志、单位和档位。"""
        state = self.state
        sensor_type = state.parameter_values[18]
        if sensor_type not in range(5):
            return 3, 0
        if state.running_sensor_type != sensor_type:
            return 2, 0
        if number == 18:
            return 0, sensor_type
        if number == 20:
            return 0, state.sensor_unit()

        continuous = sensor_type >= 2
        sampled = state.sensor_sample_type == sensor_type
        flags = state.manual_sensor_status & 0x1D
        if number == 17:
            if state.pending_tanks or state.sensor_alarm_pending or (continuous and not sampled):
                return 2, 0
            if continuous and not flags & 1 and state.sensor_value > state.parameter_values[20] * 10:
                flags |= 2
            if state.upper_water_level == 2:
                flags |= 8
            return 0, flags
        if not continuous:
            return 1, 0
        if not sampled or (number == 21 and state.pending_tanks):
            return 2, 0
        if flags & 1:
            return 3, 0
        return 0, state.sensor_value if number == 16 else state.sensor_level

    def summary(self, extended: bool) -> bytes:
        """按 05H 或 06H 紧凑布局生成质量位图和全零保留区。"""
        state = self.state
        features = (state.phases == 3) | (self.dual_pump << 1) | ((state.rated_voltage == 380) << 2)
        fields = UART4_MAIN_FIELDS + UART4_EXTRA_FIELDS if extended else UART4_MAIN_FIELDS
        qualities = bytearray((len(fields) + 3) // 4)
        values = bytearray()
        for index, (source, width) in enumerate(fields):
            quality, value = self.parameter(source & 0x7F) if source & 0x80 else self.runtime(source)
            qualities[index // 4] |= quality << (2 * (index % 4))
            values.extend(value.to_bytes(width, "big"))
        return bytes((0, features)) + qualities + values + bytes(10)

    def control(self, action: int, target: int) -> bytes:
        """先校验全部目标，再受理命令并回显当前诊断状态。"""
        state = self.state
        if not 1 <= action <= 6 or (target not in (1, 2, 3) if action <= 2 else target != 0):
            return self.error(4, action)
        if action <= 2:
            if target == 2 and not self.dual_pump:
                return self.error(6, action)
            if state.run_mode != 0:
                return self.error(8, action)
            pumps = target if self.dual_pump else 1
            faults = state.fault_bits_32
            a_blocked = state.pump_a_fault or bool(faults & 0xA07F) or state.add_pump_or_overflow
            b_blocked = state.pump_b_fault or bool(faults & 0x14F83) or bool(state.manual_sensor_status & 4)
            if action == 1 and ((pumps & 1 and a_blocked) or (pumps & 2 and b_blocked)):
                return self.error(9, action)
            # 先记录受理前的运行位，再让下一次模拟读取看到扫描后的状态。
            _, status = self.run_status()
            if pumps & 1:
                state.pump_a_running = action == 1
            if pumps & 2:
                state.pump_b_running = action == 1
        else:
            if action == 6:
                self.clear_active_faults()
            elif state.run_mode != action - 3:
                state.run_mode = action - 3
                state.parameter_saved = False
            _, status = self.run_status()
        return pack(">BBBHI", 0, action, target, status, state.fault_bits_32)

    def clear_active_faults(self) -> None:
        """清除允许复位的当前故障，保留传感器异常、泵锁定与历史。"""
        state = self.state
        clearable = UART4_CLEARABLE_FAULTS
        if not self.dual_pump:
            clearable &= ~UART4_B_PUMP_FAULTS
        state.fault_bits_32 &= ~clearable
        state.active_fault_enum = ((state.fault_bits_32 & -state.fault_bits_32).bit_length() - 1
                                   if state.fault_bits_32 else None)
        state.pump_a_fault = bool(state.fault_bits_32 & 0xA07C)
        state.pump_b_fault = bool(state.fault_bits_32 & 0x14F80)
        state.fault_status_bits &= ~0x002F
        for fault in FAULTS:
            if state.fault_bits_32 & (1 << fault.enum_code):
                state.fault_status_bits |= fault.summary_bit

class Bf006ModbusRtuSimulator:
    """Handle complete BF006 Modbus RTU request frames."""

    def __init__(self, slave_address: int = DEFAULT_SLAVE_ADDRESS, state: Bf006State | None = None) -> None:
        if slave_address < 1 or slave_address > 254:
            raise ValueError("从站地址必须在 1-254 范围内")
        self.slave_address = slave_address
        self.state = state if state is not None else Bf006State()
        self.state.parameter_values[0x000B] = slave_address
        self.last_result: ModbusProtocolResult | None = None
        self.uart4 = Bf006Uart4Protocol(self.state)

    def handle_uart4_frame(self, frame: bytes, occupied_addresses: tuple[int, ...] = ()) -> bytes | None:
        """处理完整 V1.3 请求，同步两路共享地址并记录串口诊断。"""
        response = self.uart4.handle(frame, occupied_addresses)
        self.slave_address = self.state.parameter_values[11]
        status = response[9] if response else None
        message = f"4G V1.3 状态 0x{status:02X}，应答 {len(response)} 字节" if response else "UART4 静默丢弃"
        self.last_result = ModbusProtocolResult(
            frame, response, message, len(frame) >= 11 and modbus_crc16(frame) == 0,
            response is None, status or None,
        )
        return response

    def handle_frame(self, frame: bytes) -> bytes | None:
        if len(frame) < 4:
            self.last_result = ModbusProtocolResult(frame, None, "报文长度不足，忽略。", False, True)
            return None
        if not self._crc_matches(frame):
            self.last_result = ModbusProtocolResult(frame, None, "CRC 校验失败，不响应。", False, True)
            return None

        request_address = frame[0]
        function = frame[1]
        is_private_broadcast = request_address == BF006_PRIVATE_BROADCAST_ADDRESS
        if request_address != self.slave_address and not is_private_broadcast:
            self.last_result = ModbusProtocolResult(
                frame,
                None,
                f"站号 {request_address} 与本机 {self.slave_address} 不符，忽略。",
                True,
                True,
            )
            return None

        if function == MODBUS_READ_HOLDING:
            if is_private_broadcast:
                return self._handle_private_read_address(frame)
            return self._handle_read_registers(frame)
        if function == MODBUS_WRITE_SINGLE and not is_private_broadcast:
            return self._handle_write_single(frame)
        if function == MODBUS_WRITE_MULTIPLE and not is_private_broadcast:
            return self._handle_write_multiple(frame)
        if function == BF006_PRIVATE_SET_ADDRESS and is_private_broadcast:
            return self._handle_private_set_address(frame)
        if function == BF006_PRIVATE_CLEAR_TIME and is_private_broadcast:
            return self._handle_private_clear_time(frame)
        return self._exception(frame, ERR_ILLEGAL_FUNCTION, "不支持的功能码或地址方式。")

    def _crc_matches(self, frame: bytes) -> bool:
        received = frame[-2] | (frame[-1] << 8)
        return received == modbus_crc16(frame[:-2])

    def _handle_read_registers(self, frame: bytes) -> bytes:
        if len(frame) != 8:
            return self._exception(frame, ERR_ILLEGAL_DATA, "03H 请求长度错误。")
        start = read_u16(frame, 2)
        count = read_u16(frame, 4)
        if count < 1 or count > MAX_WORDS_PER_REQUEST:
            return self._exception(frame, ERR_ILLEGAL_DATA, "读取数量必须为 1-8。")

        values: list[int] = []
        for offset in range(count):
            error, value = self.state.read_register(start + offset)
            if error:
                return self._exception(frame, error, f"寄存器 {start + offset:04X}H 不可读。")
            values.append(value)
        payload = bytes((frame[0], MODBUS_READ_HOLDING, count * 2)) + b"".join(u16_bytes(value) for value in values)
        return self._success(frame, payload, f"读取 {start:04X}H 起 {count} 个寄存器。")

    def _handle_private_read_address(self, frame: bytes) -> bytes:
        if len(frame) != 8 or read_u16(frame, 2) != BF006_PRIVATE_REGISTER or read_u16(frame, 4) != 1:
            return self._exception(frame, ERR_ILLEGAL_REGISTER, "私有广播读地址格式错误。")
        payload = bytes((self.slave_address, MODBUS_READ_HOLDING, 0x02, self.slave_address, 0x00))
        return self._success(frame, payload, f"广播读取设备地址：{self.slave_address}。")

    def _handle_write_single(self, frame: bytes) -> bytes:
        if len(frame) != 8:
            return self._exception(frame, ERR_ILLEGAL_DATA, "06H 请求长度错误。")
        address = read_u16(frame, 2)
        value = read_u16(frame, 4)
        if address in PARAMETER_BY_ADDRESS:
            error = self.state.write_parameter(address, value)
        elif 0x2000 <= address <= 0x20FF:
            error = self.state.write_control(address, value)
        else:
            error = ERR_ILLEGAL_REGISTER
        if error:
            return self._exception(frame, error, f"写入 {address:04X}H 被拒绝。")

        response = self._success(frame, frame[:6], f"写入 {address:04X}H = 0x{value:04X}。")
        if address == 0x000B:
            self.slave_address = value
        return response

    def _handle_write_multiple(self, frame: bytes) -> bytes:
        if len(frame) < 11:
            return self._exception(frame, ERR_ILLEGAL_DATA, "10H 请求长度不足。")
        start = read_u16(frame, 2)
        count = read_u16(frame, 4)
        byte_count = frame[6]
        if count < 1 or count > MAX_WORDS_PER_REQUEST or byte_count != count * 2:
            return self._exception(frame, ERR_ILLEGAL_DATA, "批量写入数量或字节数错误。")
        if len(frame) != 9 + byte_count:
            return self._exception(frame, ERR_ILLEGAL_DATA, "10H 请求总长度错误。")

        values = [read_u16(frame, 7 + offset * 2) for offset in range(count)]
        error = self.state.write_parameters_atomically(start, values)
        if error:
            return self._exception(frame, error, "批量写参数被拒绝，未修改任何参数。")
        response = self._success(frame, frame[:6], f"批量写入 {start:04X}H 起 {count} 个参数。")
        if start <= 0x000B < start + count:
            self.slave_address = self.state.parameter_values[0x000B]
        return response

    def _handle_private_set_address(self, frame: bytes) -> bytes:
        if len(frame) != 8 or read_u16(frame, 2) != BF006_PRIVATE_REGISTER:
            return self._exception(frame, ERR_ILLEGAL_REGISTER, "41H 私有地址设置格式错误。")
        new_address = frame[4]
        if new_address < 1 or new_address > 254:
            return self._exception(frame, ERR_ILLEGAL_DATA, "新设备地址必须为 1-254。")
        response = self._success(frame, frame[:6], f"私有命令设置设备地址为 {new_address}。")
        self.state.parameter_values[0x000B] = new_address
        self.slave_address = new_address
        return response

    def _handle_private_clear_time(self, frame: bytes) -> bytes:
        """Support the 42H clear-time extension used by the current BF006 PC app."""
        if len(frame) != 8 or read_u16(frame, 2) != BF006_PRIVATE_REGISTER:
            return self._exception(frame, ERR_ILLEGAL_REGISTER, "42H 私有清零格式错误。")
        command = frame[4]
        confirm = frame[5]
        if confirm != BF006_PRIVATE_CONFIRM_BYTE or command not in (0x21, 0x22):
            return self._exception(frame, ERR_ILLEGAL_DATA, "42H 命令或确认字节错误。")
        if command == 0x21:
            self.state.parameter_values[0x0015] = 0
            label = "累计运行时间"
        else:
            self.state.parameter_values[0x0016] = 0
            label = "累计通电时间"
        return self._success(frame, frame[:6], f"已清零{label}。")

    def _success(self, request: bytes, payload: bytes, message: str) -> bytes:
        response = append_modbus_crc(payload)
        self.last_result = ModbusProtocolResult(request, response, message, True)
        return response

    def _exception(self, request: bytes, code: int, message: str) -> bytes:
        address = request[0] if request else self.slave_address
        function = (request[1] | 0x80) if len(request) > 1 else 0x80
        response = append_modbus_crc(bytes((address, function, code)))
        self.last_result = ModbusProtocolResult(request, response, message, True, False, code)
        return response


class Bf006DeviceBus:
    """Route one shared Modbus RTU stream to independent BF006 slave states."""

    def __init__(self, slave_addresses: list[int] | tuple[int, ...]) -> None:
        addresses = self._validate_addresses(slave_addresses)
        self._devices = {
            address: Bf006ModbusRtuSimulator(address)
            for address in addresses
        }
        self.last_result: ModbusProtocolResult | None = None
        self.last_device_address: int | None = None

    @staticmethod
    def _validate_addresses(slave_addresses: list[int] | tuple[int, ...]) -> list[int]:
        addresses = list(slave_addresses)
        if not addresses:
            raise ValueError("至少需要一个从站地址")
        if len(set(addresses)) != len(addresses):
            raise ValueError("从站地址不能重复")
        if any(address < 1 or address > 254 for address in addresses):
            raise ValueError("从站地址必须在 1-254 范围内")
        return addresses

    @property
    def slave_addresses(self) -> list[int]:
        return sorted(self._devices)

    def get_device(self, slave_address: int) -> Bf006ModbusRtuSimulator:
        try:
            return self._devices[slave_address]
        except KeyError as exc:
            raise ValueError(f"从站地址 {slave_address} 不在当前模拟总线上") from exc

    def reconfigure(self, slave_addresses: list[int] | tuple[int, ...]) -> None:
        """Keep states for retained addresses and create defaults for newly added devices."""
        addresses = self._validate_addresses(slave_addresses)
        retained_devices = self._devices
        self._devices = {}
        for address in addresses:
            self._devices[address] = retained_devices.get(address) or Bf006ModbusRtuSimulator(address)
        self.last_result = None
        self.last_device_address = None

    def change_device_address(self, current_address: int, new_address: int) -> Bf006ModbusRtuSimulator:
        """Move one existing device to an unused address without changing its other state."""
        if new_address < 1 or new_address > 254:
            raise ValueError("从站地址必须在 1-254 范围内")
        device = self.get_device(current_address)
        if new_address != current_address and new_address in self._devices:
            raise ValueError(f"从站地址 {new_address} 已被总线上的其他模拟设备使用")
        if new_address != current_address:
            del self._devices[current_address]
            self._devices[new_address] = device
        device.slave_address = new_address
        device.state.parameter_values[0x000B] = new_address
        return device

    def handle_frame(self, frame: bytes, uart4: bool = False) -> bytes | None:
        if not frame:
            self.last_device_address = None
            self.last_result = ModbusProtocolResult(frame, None, "报文长度不足，忽略。", False, True)
            return None
        route_address = frame[0]
        if not uart4 and route_address == BF006_PRIVATE_BROADCAST_ADDRESS and len(self._devices) == 1:
            route_address = self.slave_addresses[0]
        device = self._devices.get(route_address)
        if device is None:
            self.last_device_address = None
            self.last_result = ModbusProtocolResult(
                frame,
                None,
                f"总线上没有地址 {frame[0]} 的模拟设备，忽略。",
                len(frame) >= 4 and modbus_crc16(frame[:-2]) == (frame[-2] | (frame[-1] << 8)),
                True,
            )
            return None
        requested_address = None if uart4 else self._requested_device_address(frame)
        if requested_address is not None:
            if requested_address != frame[0] and requested_address in self._devices:
                response = device._exception(
                    frame,
                    ERR_ILLEGAL_DATA,
                    f"从站地址 {requested_address} 已被总线上的其他模拟设备使用。",
                )
                self.last_device_address = frame[0]
                self.last_result = device.last_result
                return response
        if uart4:
            response = device.handle_uart4_frame(frame, tuple(self._devices))
        else:
            response = device.handle_frame(frame)
        original_address = route_address
        if device.slave_address != original_address:
            del self._devices[original_address]
            self._devices[device.slave_address] = device
        self.last_device_address = device.slave_address
        self.last_result = device.last_result
        return response

    @staticmethod
    def _requested_device_address(frame: bytes) -> int | None:
        if len(frame) < 4 or modbus_crc16(frame[:-2]) != (frame[-2] | (frame[-1] << 8)):
            return None
        if len(frame) == 8 and frame[1] == MODBUS_WRITE_SINGLE and read_u16(frame, 2) == 0x000B:
            return read_u16(frame, 4)
        if len(frame) >= 11 and frame[1] == MODBUS_WRITE_MULTIPLE:
            start = read_u16(frame, 2)
            count = read_u16(frame, 4)
            byte_count = frame[6]
            if (
                start <= 0x000B < start + count
                and byte_count == count * 2
                and len(frame) == 9 + byte_count
            ):
                value_offset = 7 + (0x000B - start) * 2
                return read_u16(frame, value_offset)
        return None


def _list_windows_registry_serial_ports() -> list[str]:
    """Read virtual COM ports that Windows SetupAPI enumeration can miss."""
    if os.name != "nt":
        return []

    try:
        import winreg

        ports: list[str] = []
        with winreg.OpenKey(
            winreg.HKEY_LOCAL_MACHINE,
            r"HARDWARE\DEVICEMAP\SERIALCOMM",
        ) as key:
            index = 0
            while True:
                try:
                    _name, value, _value_type = winreg.EnumValue(key, index)
                except OSError:
                    break
                if isinstance(value, str) and value.strip():
                    ports.append(value.strip())
                index += 1
        return ports
    except (OSError, ImportError):
        return []


def _serial_port_sort_key(port: str) -> tuple[int, int | str, str]:
    normalized = port.strip().upper()
    if normalized.startswith("COM") and normalized[3:].isdigit():
        return 0, int(normalized[3:]), normalized
    return 1, normalized, normalized


def list_serial_ports(serial_module: Any | None) -> list[str]:
    """Merge pyserial and Windows registry results for physical and virtual ports."""
    ports: list[str] = []
    if serial_module is not None:
        try:
            ports.extend(
                port.device.strip()
                for port in serial_module.tools.list_ports.comports()
                if getattr(port, "device", "").strip()
            )
        except Exception:
            pass
    ports.extend(_list_windows_registry_serial_ports())
    return sorted(set(ports), key=_serial_port_sort_key)


def receive_serial_frames(
    handle: Any,
    uart4: bool,
    stop_event: threading.Event,
    process: Any,
    deadline: float | None = None,
) -> None:  #20261117-2 add
    """读取一条物理链路并按其协议独立组帧，处理回调返回后再继续收帧。"""
    receiver = Uart4Receiver() if uart4 else None
    buffer = bytearray()
    last_rx = 0.0
    while not stop_event.is_set() and (deadline is None or time.monotonic() < deadline):
        data = handle.read(256)
        now = time.monotonic()
        if receiver is not None:
            frame = receiver.poll(data, now)
            if frame is not None:
                process(frame)
        elif data:
            buffer.extend(data)
            last_rx = now
            while buffer:
                length = expected_request_length(buffer)
                if length is None or len(buffer) < length:
                    break
                frame = bytes(buffer[:length])
                del buffer[:length]
                process(frame)
        elif buffer and now - last_rx >= SERIAL_FRAME_GAP_SECONDS:
            frame = bytes(buffer)
            buffer.clear()
            process(frame)


def send_serial_reply(handle: Any, response: bytes | None, uart4: bool, delay: float) -> None:
    """发送本次应答，UART4 丢弃发送期间到达的串口数据。"""
    if response is None:
        return
    if delay:
        time.sleep(delay)
    started = time.monotonic()
    handle.write(response)
    if uart4:
        # 等到字符发送时间及驱动队列排空，避免长帧尾部被下一次请求覆盖。
        wire_end = started + len(response) * 10 / UART4_BAUD_RATE
        deadline = wire_end + SERIAL_WRITE_TIMEOUT_SECONDS
        while time.monotonic() < wire_end or handle.out_waiting:
            if time.monotonic() >= deadline:
                raise OSError("UART4 应答发送超时")
            time.sleep(0.001)
        handle.reset_input_buffer()


class SimulatorGui:
    """Tkinter GUI for local state editing and serial Modbus simulation."""

    def __init__(self, root: Any, serial_module: Any | None, startup_warning: str | None) -> None:
        import tkinter as tk
        from tkinter import ttk

        self.root = root
        self.tk = tk
        self.ttk = ttk
        self.serial_module = serial_module
        self.device_bus = Bf006DeviceBus([DEFAULT_SLAVE_ADDRESS])
        self.simulator = self.device_bus.get_device(DEFAULT_SLAVE_ADDRESS)
        self.state = self.simulator.state
        self.serial_handles: dict[str, Any] = {}
        self.stop_event = threading.Event()
        self.lock = threading.RLock()
        self.response_delay_seconds = 0.0

        self.port_var = tk.StringVar()
        self.baud_var = tk.StringVar(value=str(DEFAULT_BAUD_RATE))
        self.uart4_port_var = tk.StringVar()
        self.parity_var = tk.StringVar(value="无校验(N)")
        self.stopbits_var = tk.StringVar(value="1")
        self.address_var = tk.StringVar(value=str(DEFAULT_SLAVE_ADDRESS))
        self.selected_device_var = tk.StringVar(value=str(DEFAULT_SLAVE_ADDRESS))
        self.response_delay_var = tk.StringVar(value="0")
        self.connection_status_var = tk.StringVar(value="未连接")
        self.device_status_var = tk.StringVar()
        self.parameter_address_var = tk.StringVar(value="P-00")
        self.parameter_value_var = tk.StringVar(value="260")

        self.run_mode_var = tk.StringVar(value="手动")
        self.pump_mode_var = tk.StringVar(value="A主B备")
        self.pump_a_running_var = tk.BooleanVar(value=False)
        self.pump_b_running_var = tk.BooleanVar(value=False)
        self.upper_level_var = tk.StringVar(value="高")
        self.bottom_level_var = tk.StringVar(value="高")
        self.add_pump_var = tk.BooleanVar(value=False)
        self.line_voltage_var = tk.StringVar(value="220")
        self.pump_a_current_var = tk.StringVar(value="0")
        self.pump_b_current_var = tk.StringVar(value="0")
        self.power_var = tk.StringVar(value="0")
        self.sensor_out_var = tk.StringVar(value="0")
        self.control_scheme_var = tk.StringVar(value=CONTROL_SCHEME_ONE_TO_TWO)
        self.device_specification_var = tk.StringVar(value="0x93")
        self.device_model_preview_var = tk.StringVar(value="210DH=0xA393")
        self.version_var = tk.StringVar(value="0x00C9")
        self.total_run_var = tk.StringVar(value="0")
        self.total_power_var = tk.StringVar(value="0")

        self.sensor_type_var = tk.StringVar(value=SENSOR_TYPE_OPTIONS[0])
        self.sensor_value_201_var = tk.StringVar(value="0")
        self.sensor_unit_var = tk.StringVar(value="无连续值（2110H=FFFFH）")
        self.sensor_target_var = tk.StringVar(value=SENSOR_TARGET_OPTIONS[0])
        self.sensor_level_var = tk.StringVar(value=SENSOR_LEVEL_OPTIONS[2])
        self.upper_segment_vars = [tk.BooleanVar(value=True) for _ in range(6)]
        self.bottom_segment_vars = [tk.BooleanVar(value=True) for _ in range(6)]
        self.sensor_status_vars = {mask: tk.BooleanVar(value=False) for _label, mask in SENSOR_STATUS_OPTIONS}
        self.liquid_protocol_var = tk.StringVar()

        self.fault_choice_var = tk.StringVar(value="0 - 无故障")
        self.active_fault_var = tk.StringVar(value="当前无故障")
        self.fault_bit_vars = {mask: tk.BooleanVar(value=False) for _label, mask in FAULT_BIT_OPTIONS}
        self.pump_a_fault_var = tk.BooleanVar(value=False)
        self.pump_b_fault_var = tk.BooleanVar(value=False)

        self.history_index_var = tk.StringVar(value="1")
        self.history_type_var = tk.StringVar(value="17 - 无有效故障")
        self.history_voltage_var = tk.StringVar(value="0")
        self.history_current_var = tk.StringVar(value="0")

        self.parameter_tree = None
        self.monitor_tree = None
        self.history_tree = None

        self.root.title(APP_TITLE)
        self.root.minsize(1180, 780)
        self.root.geometry("1280x860")
        self._configure_style()
        self._build_layout()
        self._refresh_ports(initial=True)
        self._refresh_views(load_editors=True)
        self.root.after_idle(self._center_window)
        if startup_warning:
            self._append_log(startup_warning)

    def _configure_style(self) -> None:
        style = self.ttk.Style(self.root)
        try:
            style.theme_use("vista")
        except self.tk.TclError:
            pass
        style.configure("Treeview", rowheight=25)
        style.configure("Treeview.Heading", padding=(4, 5))

    def _center_window(self) -> None:
        self.root.update_idletasks()
        width = self.root.winfo_width()
        height = self.root.winfo_height()
        left = max(0, (self.root.winfo_screenwidth() - width) // 2)
        top = max(0, (self.root.winfo_screenheight() - height) // 2)
        self.root.geometry(f"{width}x{height}+{left}+{top}")

    def _build_layout(self) -> None:
        main = self.ttk.Frame(self.root, padding=10)
        main.grid(row=0, column=0, sticky="nsew")
        self.root.rowconfigure(0, weight=1)
        self.root.columnconfigure(0, weight=1)
        main.columnconfigure(0, weight=1)
        main.rowconfigure(2, weight=1)
        main.rowconfigure(3, weight=1)

        self._build_connection_area(main)
        self.ttk.Label(main, textvariable=self.device_status_var, anchor="w").grid(
            row=1, column=0, sticky="ew", pady=(0, 8)
        )

        notebook = self.ttk.Notebook(main)
        notebook.grid(row=2, column=0, sticky="nsew")
        parameter_tab = self.ttk.Frame(notebook, padding=8)
        monitor_tab = self.ttk.Frame(notebook, padding=8)
        liquid_tab = self.ttk.Frame(notebook, padding=8)
        fault_tab = self.ttk.Frame(notebook, padding=8)
        history_tab = self.ttk.Frame(notebook, padding=8)
        notebook.add(parameter_tab, text="参数区")
        notebook.add(monitor_tab, text="运行 / 监视")
        notebook.add(liquid_tab, text="液位 / 传感器（协议201）")
        notebook.add(fault_tab, text="故障设置")
        notebook.add(history_tab, text="历史故障")
        self._build_parameter_tab(parameter_tab)
        self._build_monitor_tab(monitor_tab)
        self._build_liquid_tab(liquid_tab)
        self._build_fault_tab(fault_tab)
        self._build_history_tab(history_tab)
        self._build_log_area(main)

    def _build_connection_area(self, parent: Any) -> None:
        frame = self.ttk.LabelFrame(parent, text="串口与设备", padding=8)
        frame.grid(row=0, column=0, sticky="ew", pady=(0, 8))
        frame.columnconfigure(1, weight=1)

        self.ttk.Label(frame, text="RS485 串口").grid(row=0, column=0, sticky="w")
        self.port_combo = self.ttk.Combobox(frame, textvariable=self.port_var, width=15, state="normal")
        self.port_combo.grid(row=0, column=1, sticky="ew", padx=(5, 8))
        self.ttk.Button(frame, text="刷新", command=self._refresh_ports).grid(row=0, column=2, padx=(0, 12))

        self.ttk.Label(frame, text="波特率").grid(row=0, column=3, sticky="w")
        self.ttk.Combobox(
            frame,
            textvariable=self.baud_var,
            values=("1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200"),
            width=9,
            state="readonly",
        ).grid(row=0, column=4, padx=(5, 12))
        self.ttk.Label(frame, text="校验").grid(row=0, column=5, sticky="w")
        self.ttk.Combobox(
            frame,
            textvariable=self.parity_var,
            values=("无校验(N)", "偶校验(E)", "奇校验(O)"),
            width=11,
            state="readonly",
        ).grid(row=0, column=6, padx=(5, 12))
        self.ttk.Label(frame, text="停止位").grid(row=0, column=7, sticky="w")
        self.ttk.Combobox(frame, textvariable=self.stopbits_var, values=("1", "2"), width=4, state="readonly").grid(
            row=0, column=8, padx=(5, 12)
        )

        self.ttk.Label(frame, text="从站地址列表").grid(row=1, column=0, sticky="w", pady=(7, 0))
        self.address_entry = self.ttk.Entry(frame, textvariable=self.address_var, width=17)
        self.address_entry.grid(row=1, column=1, sticky="w", padx=(5, 12), pady=(7, 0))
        self.apply_device_button = self.ttk.Button(frame, text="应用地址", command=self._apply_device_configuration)
        self.apply_device_button.grid(row=1, column=2, padx=(0, 12), pady=(7, 0))

        self.ttk.Label(frame, text="当前编辑设备").grid(row=1, column=3, sticky="w", pady=(7, 0))
        self.device_selector = self.ttk.Combobox(
            frame,
            textvariable=self.selected_device_var,
            values=(str(DEFAULT_SLAVE_ADDRESS),),
            width=8,
            state="readonly",
        )
        self.device_selector.grid(row=1, column=4, padx=(5, 12), pady=(7, 0))
        self.device_selector.bind("<<ComboboxSelected>>", self._on_selected_device_changed)

        self.ttk.Label(frame, text="响应延时(ms)").grid(row=1, column=5, sticky="w", pady=(7, 0))
        self.ttk.Entry(frame, textvariable=self.response_delay_var, width=7).grid(
            row=1, column=6, sticky="w", padx=(5, 12), pady=(7, 0)
        )

        self.connect_button = self.ttk.Button(frame, text="连接", command=self._connect_serial)
        self.connect_button.grid(row=2, column=0, padx=(0, 5), pady=(7, 0), sticky="w")
        self.disconnect_button = self.ttk.Button(frame, text="断开", command=self._disconnect_serial, state="disabled")
        self.disconnect_button.grid(row=2, column=1, padx=(0, 8), pady=(7, 0), sticky="w")
        self.ttk.Button(frame, text="恢复全部默认", command=self._reset_defaults).grid(
            row=2, column=2, pady=(7, 0), sticky="w"
        )

        self.ttk.Label(frame, textvariable=self.connection_status_var).grid(
            row=2, column=3, columnspan=6, sticky="w", pady=(7, 0)
        )
        #20261117-2 change：两个端口可单独填写，也可同时连接同一组设备状态。
        self.ttk.Label(frame, text="4G UART4").grid(row=3, column=0, sticky="w", pady=(7, 0))
        self.uart4_port_combo = self.ttk.Combobox(
            frame, textvariable=self.uart4_port_var, width=15, state="normal"
        )
        self.uart4_port_combo.grid(row=3, column=1, sticky="ew", padx=(5, 8), pady=(7, 0))
        self.ttk.Label(frame, text="V1.3 · 9600 / 8N1 · 不使用的串口留空").grid(
            row=3, column=2, columnspan=6, sticky="w", pady=(7, 0)
        )

    def _build_parameter_tab(self, parent: Any) -> None:
        parent.columnconfigure(0, weight=1)
        parent.rowconfigure(0, weight=1)
        columns = ("address", "number", "name", "firmware", "value", "unit", "range", "access")
        self.parameter_tree = self.ttk.Treeview(parent, columns=columns, show="headings", selectmode="browse")
        for column, heading, width, anchor in (
            ("address", "地址", 70, "center"),
            ("number", "参数号", 70, "center"),
            ("name", "名称", 205, "w"),
            ("firmware", "固件标识", 170, "w"),
            ("value", "当前值", 115, "center"),
            ("unit", "单位", 75, "center"),
            ("range", "范围", 175, "center"),
            ("access", "访问", 65, "center"),
        ):
            self.parameter_tree.heading(column, text=heading)
            self.parameter_tree.column(column, width=width, anchor=anchor, stretch=column in ("name", "range"))
        self.parameter_tree.grid(row=0, column=0, sticky="nsew")
        self.parameter_tree.bind("<<TreeviewSelect>>", self._on_parameter_selected)
        scrollbar = self.ttk.Scrollbar(parent, orient="vertical", command=self.parameter_tree.yview)
        self.parameter_tree.configure(yscrollcommand=scrollbar.set)
        scrollbar.grid(row=0, column=1, sticky="ns")

        editor = self.ttk.Frame(parent)
        editor.grid(row=1, column=0, columnspan=2, sticky="ew", pady=(8, 0))
        editor.columnconfigure(1, weight=1)
        editor.columnconfigure(3, weight=1)
        self.ttk.Label(editor, text="参数号 / 十六进制地址").grid(row=0, column=0, sticky="w")
        self.ttk.Entry(editor, textvariable=self.parameter_address_var).grid(row=0, column=1, sticky="ew", padx=(6, 16))
        self.ttk.Label(editor, text="原始值（十进制或 0x 前缀）").grid(row=0, column=2, sticky="w")
        self.ttk.Entry(editor, textvariable=self.parameter_value_var).grid(row=0, column=3, sticky="ew", padx=(6, 16))
        self.ttk.Button(editor, text="本地写入", command=self._write_parameter_local).grid(row=0, column=4)

    def _build_monitor_tab(self, parent: Any) -> None:
        parent.columnconfigure(0, weight=1)
        parent.columnconfigure(1, weight=1)
        parent.rowconfigure(1, weight=1)

        controls = self.ttk.LabelFrame(parent, text="手动运行状态", padding=8)
        controls.grid(row=0, column=0, sticky="nsew", padx=(0, 5), pady=(0, 8))
        values = (
            ("控制方案", self.control_scheme_var, CONTROL_SCHEME_OPTIONS),
            ("运行模式", self.run_mode_var, ("手动", "自动", "循环定时")),
            ("主备模式", self.pump_mode_var, ("A主B备", "B主A备", "双泵轮换/同启")),
            ("上水池水位", self.upper_level_var, ("低", "中", "高")),
            ("下水池水位", self.bottom_level_var, ("低", "中", "高")),
        )
        for row, (label, variable, options) in enumerate(values):
            self.ttk.Label(controls, text=label).grid(row=row, column=0, sticky="w", pady=2)
            combo = self.ttk.Combobox(controls, textvariable=variable, values=options, state="readonly", width=17)
            combo.grid(row=row, column=1, sticky="ew", padx=(6, 14), pady=2)
            if variable is self.control_scheme_var:
                combo.bind("<<ComboboxSelected>>", self._on_control_scheme_selected)
        self.ttk.Checkbutton(controls, text="A泵运行", variable=self.pump_a_running_var).grid(row=0, column=2, sticky="w")
        self.ttk.Checkbutton(controls, text="B泵运行", variable=self.pump_b_running_var).grid(row=1, column=2, sticky="w")
        self.ttk.Checkbutton(controls, text="加泵/溢出 bit15", variable=self.add_pump_var).grid(row=2, column=2, sticky="w")
        self.ttk.Button(controls, text="控制器本机停止", command=self._press_local_stop).grid(
            row=3, column=2, sticky="w", pady=(4, 0)
        )
        self.ttk.Label(controls, textvariable=self.device_model_preview_var).grid(row=4, column=2, sticky="w")

        measurements = self.ttk.LabelFrame(parent, text="手动监视量", padding=8)
        measurements.grid(row=0, column=1, sticky="nsew", padx=(5, 0), pady=(0, 8))
        monitor_entries = (
            ("线路电压", self.line_voltage_var),
            ("A泵电流(A)", self.pump_a_current_var),
            ("B泵电流(A)", self.pump_b_current_var),
            ("功率", self.power_var),
            ("传感器值(工程值)", self.sensor_out_var),
            ("规格档位(低字节)", self.device_specification_var),
            ("固件版本", self.version_var),
            ("累计运行时间", self.total_run_var),
            ("累计通电时间", self.total_power_var),
        )
        for index, (label, variable) in enumerate(monitor_entries):
            row = index // 3
            column = (index % 3) * 2
            self.ttk.Label(measurements, text=label).grid(row=row, column=column, sticky="w", padx=(0, 4), pady=2)
            entry = self.ttk.Entry(measurements, textvariable=variable, width=11)
            entry.grid(row=row, column=column + 1, sticky="ew", padx=(0, 12), pady=2)
            if variable is self.device_specification_var:
                entry.bind("<KeyRelease>", self._preview_device_model)
        self.ttk.Button(measurements, text="应用运行与监视状态", command=self._apply_runtime_state_local).grid(
            row=3, column=0, columnspan=6, sticky="e", pady=(6, 0)
        )

        columns = ("address", "name", "firmware", "hex", "decimal", "unit")
        self.monitor_tree = self.ttk.Treeview(parent, columns=columns, show="headings", selectmode="browse")
        for column, heading, width, anchor in (
            ("address", "地址", 80, "center"),
            ("name", "名称", 210, "w"),
            ("firmware", "固件标识", 175, "w"),
            ("hex", "十六进制", 105, "center"),
            ("decimal", "十进制", 100, "center"),
            ("unit", "单位", 90, "center"),
        ):
            self.monitor_tree.heading(column, text=heading)
            self.monitor_tree.column(column, width=width, anchor=anchor, stretch=column == "name")
        self.monitor_tree.grid(row=1, column=0, columnspan=2, sticky="nsew")
        scrollbar = self.ttk.Scrollbar(parent, orient="vertical", command=self.monitor_tree.yview)
        self.monitor_tree.configure(yscrollcommand=scrollbar.set)
        scrollbar.grid(row=1, column=2, sticky="ns")

    def _build_liquid_tab(self, parent: Any) -> None:
        parent.columnconfigure(0, weight=1)

        basic = self.ttk.LabelFrame(parent, text="连续传感器与作用水池", padding=10)
        basic.grid(row=0, column=0, sticky="ew", pady=(0, 8))
        basic.columnconfigure(1, weight=1)
        basic.columnconfigure(3, weight=1)

        self.ttk.Label(basic, text="传感器类型（210FH）").grid(row=0, column=0, sticky="w", pady=3)
        sensor_type_combo = self.ttk.Combobox(
            basic,
            textvariable=self.sensor_type_var,
            values=SENSOR_TYPE_OPTIONS,
            state="readonly",
            width=25,
        )
        sensor_type_combo.grid(row=0, column=1, sticky="ew", padx=(6, 20), pady=3)
        sensor_type_combo.bind("<<ComboboxSelected>>", self._update_sensor_unit_preview)

        self.ttk.Label(basic, text="连续工程值（2110H）").grid(row=0, column=2, sticky="w", pady=3)
        self.ttk.Entry(basic, textvariable=self.sensor_value_201_var, width=14).grid(
            row=0, column=3, sticky="ew", padx=(6, 8), pady=3
        )
        self.ttk.Label(basic, textvariable=self.sensor_unit_var).grid(row=0, column=4, sticky="w", pady=3)

        self.ttk.Label(basic, text="作用水池（2112H）").grid(row=1, column=0, sticky="w", pady=3)
        self.ttk.Combobox(
            basic,
            textvariable=self.sensor_target_var,
            values=SENSOR_TARGET_OPTIONS,
            state="readonly",
            width=25,
        ).grid(row=1, column=1, sticky="ew", padx=(6, 20), pady=3)
        self.ttk.Label(basic, text="模拟量水池档位（2113H）").grid(row=1, column=2, sticky="w", pady=3)
        self.ttk.Combobox(
            basic,
            textvariable=self.sensor_level_var,
            values=SENSOR_LEVEL_OPTIONS,
            state="readonly",
            width=14,
        ).grid(row=1, column=3, sticky="ew", padx=(6, 8), pady=3)
        self.ttk.Label(
            basic,
            text="P-18 为 0/1 时，2110H 和 2113H 按协议返回 FFFFH；输入值保留供切换到模拟量类型后测试。",
            wraplength=1080,
        ).grid(row=2, column=0, columnspan=5, sticky="w", pady=(6, 0))

        segments = self.ttk.Frame(parent)
        segments.grid(row=1, column=0, sticky="ew", pady=(0, 8))
        segments.columnconfigure(0, weight=1)
        segments.columnconfigure(1, weight=1)
        for column, (title, variables) in enumerate(
            (("上水池六段位图（2114H）", self.upper_segment_vars), ("下水池六段位图（2115H）", self.bottom_segment_vars))
        ):
            frame = self.ttk.LabelFrame(segments, text=title, padding=10)
            frame.grid(row=0, column=column, sticky="ew", padx=(0, 5) if column == 0 else (5, 0))
            for index, variable in enumerate(variables):
                self.ttk.Checkbutton(frame, text=f"第{index + 1}段 bit{index}", variable=variable).grid(
                    row=0, column=index, sticky="w", padx=(0, 12)
                )

        statuses = self.ttk.LabelFrame(parent, text="手动传感器与水池状态（2116H）", padding=10)
        statuses.grid(row=2, column=0, sticky="ew", pady=(0, 8))
        for index, (label, mask) in enumerate(SENSOR_STATUS_OPTIONS):
            self.ttk.Checkbutton(statuses, text=label, variable=self.sensor_status_vars[mask]).grid(
                row=index // 3, column=index % 3, sticky="w", padx=(0, 24), pady=3
            )

        footer = self.ttk.Frame(parent)
        footer.grid(row=3, column=0, sticky="ew")
        footer.columnconfigure(0, weight=1)
        self.ttk.Label(footer, textvariable=self.liquid_protocol_var, anchor="w", wraplength=1000).grid(
            row=0, column=0, sticky="ew", padx=(0, 12)
        )
        self.ttk.Button(footer, text="应用液位与传感器状态", command=self._apply_liquid_state_local).grid(
            row=0, column=1, sticky="e"
        )

    def _build_fault_tab(self, parent: Any) -> None:
        parent.columnconfigure(0, weight=1)
        selector = self.ttk.LabelFrame(parent, text="当前故障注入", padding=10)
        selector.grid(row=0, column=0, sticky="ew", pady=(0, 8))
        selector.columnconfigure(1, weight=1)
        choices = ["0 - 无故障"] + [
            f"{fault.current_code} - {fault.display_name} ({fault.firmware_name})" for fault in FAULTS
        ]
        self.ttk.Label(selector, text="2100H 当前故障").grid(row=0, column=0, sticky="w")
        self.ttk.Combobox(selector, textvariable=self.fault_choice_var, values=choices, state="readonly").grid(
            row=0, column=1, sticky="ew", padx=(8, 12)
        )
        self.ttk.Button(selector, text="注入所选故障", command=self._inject_fault_local).grid(row=0, column=2, padx=(0, 8))
        self.ttk.Button(selector, text="清除全部故障", command=self._clear_faults_local).grid(row=0, column=3)
        self.ttk.Label(selector, textvariable=self.active_fault_var).grid(
            row=1, column=0, columnspan=4, sticky="w", pady=(8, 0)
        )

        bits = self.ttk.LabelFrame(parent, text="2102H 故障汇总位 / 210CH 泵故障位", padding=10)
        bits.grid(row=1, column=0, sticky="ew")
        for index, (label, mask) in enumerate(FAULT_BIT_OPTIONS):
            self.ttk.Checkbutton(bits, text=label, variable=self.fault_bit_vars[mask]).grid(
                row=index // 4, column=index % 4, sticky="w", padx=(0, 22), pady=4
            )
        self.ttk.Checkbutton(bits, text="A泵故障 bit8", variable=self.pump_a_fault_var).grid(
            row=2, column=0, sticky="w", pady=(8, 2)
        )
        self.ttk.Checkbutton(bits, text="B泵故障 bit9", variable=self.pump_b_fault_var).grid(
            row=2, column=1, sticky="w", pady=(8, 2)
        )
        self.ttk.Button(bits, text="应用手动故障位", command=self._apply_fault_bits_local).grid(
            row=2, column=3, sticky="e", pady=(8, 2)
        )

        note = (
            "说明：注入具体故障会自动更新当前故障代码、故障汇总位、泵故障位，并写入第 1 条历史记录；"
            "“应用手动故障位”用于构造组合故障测试。"
        )
        self.ttk.Label(parent, text=note, wraplength=1050, justify="left").grid(row=2, column=0, sticky="w", pady=(10, 0))

    def _build_history_tab(self, parent: Any) -> None:
        parent.columnconfigure(0, weight=1)
        parent.rowconfigure(0, weight=1)
        columns = ("index", "address", "type", "name", "voltage", "current")
        self.history_tree = self.ttk.Treeview(parent, columns=columns, show="headings", selectmode="browse")
        for column, heading, width, anchor in (
            ("index", "记录", 65, "center"),
            ("address", "起始地址", 90, "center"),
            ("type", "故障类型", 85, "center"),
            ("name", "故障名称", 260, "w"),
            ("voltage", "故障电压", 110, "center"),
            ("current", "故障电流", 110, "center"),
        ):
            self.history_tree.heading(column, text=heading)
            self.history_tree.column(column, width=width, anchor=anchor, stretch=column == "name")
        self.history_tree.grid(row=0, column=0, sticky="nsew")
        self.history_tree.bind("<<TreeviewSelect>>", self._on_history_selected)
        scrollbar = self.ttk.Scrollbar(parent, orient="vertical", command=self.history_tree.yview)
        self.history_tree.configure(yscrollcommand=scrollbar.set)
        scrollbar.grid(row=0, column=1, sticky="ns")

        editor = self.ttk.Frame(parent)
        editor.grid(row=1, column=0, columnspan=2, sticky="ew", pady=(8, 0))
        type_choices = [f"{fault.enum_code} - {fault.display_name}" for fault in FAULTS] + ["17 - 无有效故障"]
        self.ttk.Label(editor, text="记录序号").grid(row=0, column=0, sticky="w")
        self.ttk.Combobox(editor, textvariable=self.history_index_var, values=tuple(str(i) for i in range(1, 11)), width=5, state="readonly").grid(
            row=0, column=1, padx=(5, 14)
        )
        self.ttk.Label(editor, text="故障类型").grid(row=0, column=2, sticky="w")
        self.ttk.Combobox(editor, textvariable=self.history_type_var, values=type_choices, width=26, state="readonly").grid(
            row=0, column=3, padx=(5, 14)
        )
        self.ttk.Label(editor, text="电压").grid(row=0, column=4, sticky="w")
        self.ttk.Entry(editor, textvariable=self.history_voltage_var, width=9).grid(row=0, column=5, padx=(5, 14))
        self.ttk.Label(editor, text="电流").grid(row=0, column=6, sticky="w")
        self.ttk.Entry(editor, textvariable=self.history_current_var, width=9).grid(row=0, column=7, padx=(5, 14))
        self.ttk.Button(editor, text="写入记录", command=self._write_history_local).grid(row=0, column=8, padx=(0, 8))
        self.ttk.Button(editor, text="清空 10 条", command=self._clear_history_local).grid(row=0, column=9)

    def _build_log_area(self, parent: Any) -> None:
        frame = self.ttk.LabelFrame(parent, text="通信日志", padding=8)
        frame.grid(row=3, column=0, sticky="nsew", pady=(8, 0))
        frame.columnconfigure(0, weight=1)
        frame.rowconfigure(0, weight=1)
        self.log_text = self.tk.Text(frame, height=9, wrap="word")
        self.log_text.grid(row=0, column=0, sticky="nsew")
        scrollbar = self.ttk.Scrollbar(frame, orient="vertical", command=self.log_text.yview)
        self.log_text.configure(yscrollcommand=scrollbar.set)
        scrollbar.grid(row=0, column=1, sticky="ns")

    def _refresh_views(self, load_editors: bool = False) -> None:
        with self.lock:
            self._refresh_parameter_tree()
            self._refresh_monitor_tree()
            self._refresh_history_tree()
            self._refresh_fault_controls()
            self._refresh_device_status()
            if load_editors:
                self._load_runtime_editor_vars()
                self._load_liquid_editor_vars()

    def _refresh_parameter_tree(self) -> None:
        selected_address = None
        selected = self.parameter_tree.focus()
        if selected:
            values = self.parameter_tree.item(selected, "values")
            selected_address = values[0] if values else None
        for item in self.parameter_tree.get_children():
            self.parameter_tree.delete(item)
        for definition in PARAMETERS:
            value = self.state.parameter_values[definition.address]
            item_id = self.parameter_tree.insert(
                "",
                "end",
                values=(
                    f"{definition.address:04X}H",
                    definition.number,
                    definition.display_name,
                    definition.firmware_name,
                    self._format_parameter_value(definition, value),
                    definition.unit or "-",
                    definition.range_text,
                    "R/W" if definition.writable else "R",
                ),
            )
            if selected_address == f"{definition.address:04X}H":
                self.parameter_tree.selection_set(item_id)
                self.parameter_tree.focus(item_id)

    def _format_parameter_value(self, definition: ParameterDefinition, value: int) -> str:
        if definition.unit == "0.1A":
            return f"{value / 10:.1f} ({value})"
        if definition.address in (0x000E, 0x000F, 0x0010, 0x0011, 0x0014):
            return f"{value / 10:.1f} ({value})"
        return f"0x{value:04X} ({value})"

    def _refresh_monitor_tree(self) -> None:
        for item in self.monitor_tree.get_children():
            self.monitor_tree.delete(item)
        for definition in MONITORS:
            _error, value = self.state.read_register(definition.address)
            self.monitor_tree.insert(
                "",
                "end",
                values=(
                    f"{definition.address:04X}H",
                    definition.display_name,
                    definition.firmware_name,
                    f"0x{value:04X}",
                    str(value),
                    definition.unit or "-",
                ),
            )

    def _refresh_history_tree(self) -> None:
        for item in self.history_tree.get_children():
            self.history_tree.delete(item)
        for index, record in enumerate(self.state.history):
            fault = FAULT_BY_ENUM_CODE.get(record.fault_type)
            name = fault.display_name if fault else "无有效故障"
            self.history_tree.insert(
                "",
                "end",
                values=(index + 1, f"{0x2200 + index * 3:04X}H", record.fault_type, name, record.voltage, record.current),
            )

    def _refresh_fault_controls(self) -> None:
        for _label, mask in FAULT_BIT_OPTIONS:
            self.fault_bit_vars[mask].set(bool(self.state.fault_status_bits & mask))
        self.pump_a_fault_var.set(self.state.pump_a_fault)
        self.pump_b_fault_var.set(self.state.pump_b_fault)
        if self.state.active_fault_enum is None:
            self.active_fault_var.set(
                f"当前无故障；2102H=0x{self.state.fault_status_bits:04X}，210CH=0x{self.state.pump_state():04X}"
            )
            self.fault_choice_var.set("0 - 无故障")
        else:
            fault = FAULT_BY_ENUM_CODE[self.state.active_fault_enum]
            self.active_fault_var.set(
                f"当前故障：{fault.current_code} {fault.display_name}；"
                f"2102H=0x{self.state.fault_status_bits:04X}，210CH=0x{self.state.pump_state():04X}"
            )
            self.fault_choice_var.set(f"{fault.current_code} - {fault.display_name} ({fault.firmware_name})")

    def _refresh_device_status(self) -> None:
        mode = ("手动", "自动", "循环定时")[self.state.run_mode]
        pumps = []
        if self.state.pump_a_running:
            pumps.append("A泵运行")
        if self.state.pump_b_running:
            pumps.append("B泵运行")
        pump_text = "、".join(pumps) if pumps else "双泵停止"
        fault_text = "无故障" if self.state.active_fault_enum is None else FAULT_BY_ENUM_CODE[self.state.active_fault_enum].display_name
        saved_text = "已保存" if self.state.parameter_saved else "未保存"
        device_count = len(self.device_bus.slave_addresses)
        self.device_status_var.set(
            f"当前设备地址 {self.simulator.slave_address} ｜ 在线模拟 {device_count} 台 ｜ "
            f"{mode} ｜ {pump_text} ｜ {fault_text} ｜ "
            f"RunStatus=0x{self.state.run_status():04X} ｜ 参数{saved_text}"
        )

    def _load_runtime_editor_vars(self) -> None:
        self.control_scheme_var.set(control_scheme_from_device_model(self.state.device_model))
        self.run_mode_var.set(("手动", "自动", "循环定时")[self.state.run_mode])
        self.pump_mode_var.set(("A主B备", "B主A备", "双泵轮换/同启")[self.state.pump_ab_mode])
        self.pump_a_running_var.set(self.state.pump_a_running)
        self.pump_b_running_var.set(self.state.pump_b_running)
        self.upper_level_var.set(("低", "中", "高")[self.state.upper_water_level])
        self.bottom_level_var.set(("低", "中", "高")[self.state.bottom_water_level])
        self.add_pump_var.set(self.state.add_pump_or_overflow)
        self.line_voltage_var.set(str(self.state.line_voltage))
        self.pump_a_current_var.set(format_scaled_value(self.state.pump_a_current, 10, 1))
        self.pump_b_current_var.set(format_scaled_value(self.state.pump_b_current, 10, 1))
        self.power_var.set(str(self.state.power))
        self.sensor_out_var.set(format_scaled_value(self.state.sensor_out, 100, 2))
        self.device_specification_var.set(f"0x{self.state.device_model & 0xFF:02X}")
        self.device_model_preview_var.set(f"210DH=0x{self.state.device_model:04X}")
        self.version_var.set(f"0x{self.state.version:04X}")
        self.total_run_var.set(str(self.state.parameter_values[0x0015]))
        self.total_power_var.set(str(self.state.parameter_values[0x0016]))

    def _load_liquid_editor_vars(self) -> None:
        sensor_type = self.state.parameter_values[0x0012]
        sensor_target = self.state.parameter_values[0x000D]
        self.sensor_type_var.set(SENSOR_TYPE_OPTIONS[sensor_type] if sensor_type in range(5) else SENSOR_TYPE_OPTIONS[0])
        self.sensor_value_201_var.set(format_scaled_value(self.state.sensor_value, 100, 2))
        self.sensor_target_var.set(SENSOR_TARGET_OPTIONS[sensor_target] if sensor_target in (0, 1) else SENSOR_TARGET_OPTIONS[0])
        self.sensor_level_var.set(SENSOR_LEVEL_OPTIONS[self.state.sensor_level])
        for index, variable in enumerate(self.upper_segment_vars):
            variable.set(bool(self.state.upper_water_segments & (1 << index)))
        for index, variable in enumerate(self.bottom_segment_vars):
            variable.set(bool(self.state.bottom_water_segments & (1 << index)))
        for _label, mask in SENSOR_STATUS_OPTIONS:
            self.sensor_status_vars[mask].set(bool(self.state.manual_sensor_status & mask))
        self._update_sensor_unit_preview()
        self._refresh_liquid_protocol_preview()

    def _update_sensor_unit_preview(self, _event: Any = None) -> None:
        sensor_type = SENSOR_TYPE_CODES.get(self.sensor_type_var.get(), 0)
        self.sensor_unit_var.set(
            "m（2111H=1）" if sensor_type == 2 else
            "MPa（2111H=2）" if sensor_type in (3, 4) else
            "无连续值（2110H=FFFFH）"
        )

    def _refresh_liquid_protocol_preview(self) -> None:
        values = [self.state.read_register(address)[1] for address in range(0x210F, 0x2117)]
        self.liquid_protocol_var.set(
            " ｜ ".join(f"{address:04X}H=0x{value:04X}" for address, value in zip(range(0x210F, 0x2117), values))
        )

    def _on_parameter_selected(self, _event: Any = None) -> None:
        selected = self.parameter_tree.focus()
        values = self.parameter_tree.item(selected, "values") if selected else ()
        if not values:
            return
        definition = PARAMETER_BY_NUMBER[str(values[1]).upper()]
        self.parameter_address_var.set(definition.number)
        self.parameter_value_var.set(str(self.state.parameter_values[definition.address]))

    def _parse_parameter_address(self, text: str) -> int:
        normalized = text.strip().upper()
        if normalized in PARAMETER_BY_NUMBER:
            return PARAMETER_BY_NUMBER[normalized].address
        normalized = normalized.removesuffix("H")
        return int(normalized, 16)

    def _write_parameter_local(self) -> None:
        try:
            address = self._parse_parameter_address(self.parameter_address_var.get())
            value = parse_int(self.parameter_value_var.get())
        except ValueError as exc:
            self._append_log(f"本地参数输入错误：{exc}")
            return
        try:
            with self.lock:
                if address == 0x000B:
                    self.simulator = self.device_bus.change_device_address(
                        self.simulator.slave_address,
                        value,
                    )
                    self.state = self.simulator.state
                    error = 0
                else:
                    error = self.state.write_parameter(address, value)
        except ValueError as exc:
            self._append_log(f"本地写参数失败：{exc}")
            return
        if error:
            self._append_log(f"本地写参数失败：{ERROR_CODE_TEXT[error]}")
            return
        if address == 0x000B:
            addresses = self.device_bus.slave_addresses
            self.address_var.set(",".join(str(item) for item in addresses))
            self.device_selector.configure(values=tuple(str(item) for item in addresses))
            self.selected_device_var.set(str(value))
        self._refresh_views(load_editors=False)
        if address in (0x000D, 0x0012, 0x0014):
            self._load_liquid_editor_vars()
        self._append_log(f"本地写参数 {address:04X}H = 0x{value:04X}")

    def _apply_liquid_state_local(self) -> None:
        try:
            sensor_type = SENSOR_TYPE_CODES[self.sensor_type_var.get()]
            sensor_value = parse_scaled_u16(self.sensor_value_201_var.get(), 100, "连续传感器值", 0xFFFFFFFF)
            sensor_target = SENSOR_TARGET_OPTIONS.index(self.sensor_target_var.get())
            sensor_level = SENSOR_LEVEL_OPTIONS.index(self.sensor_level_var.get())
            upper_segments = sum((1 << index) for index, variable in enumerate(self.upper_segment_vars) if variable.get())
            bottom_segments = sum((1 << index) for index, variable in enumerate(self.bottom_segment_vars) if variable.get())
            sensor_status = sum(mask for _label, mask in SENSOR_STATUS_OPTIONS if self.sensor_status_vars[mask].get())
        except (KeyError, ValueError) as exc:
            self._append_log(f"液位/传感器输入错误：{exc}")
            return

        try:
            with self.lock:
                self.state.set_sensor_state(
                    sensor_type=sensor_type,
                    sensor_value=sensor_value,
                    sensor_target=sensor_target,
                    sensor_level=sensor_level,
                    upper_segments=upper_segments,
                    bottom_segments=bottom_segments,
                    sensor_status=sensor_status,
                )
        except ValueError as exc:
            self._append_log(f"液位/传感器输入错误：{exc}")
            return

        self._refresh_views(load_editors=True)
        self._append_log(
            f"已应用协议201液位状态：类型={sensor_type}，连续值原始值={sensor_value}，"
            f"目标水池={sensor_target}，档位={sensor_level}，"
            f"2114H=0x{upper_segments:04X}，2115H=0x{bottom_segments:04X}，"
            f"2116H=0x{self.state.sensor_status:04X}（手动=0x{self.state.manual_sensor_status:04X}）。"
        )

    def _apply_runtime_state_local(self) -> None:
        try:
            numeric_values = parse_monitor_editor_values(
                voltage=self.line_voltage_var.get(),
                pump_a_current=self.pump_a_current_var.get(),
                pump_b_current=self.pump_b_current_var.get(),
                power=self.power_var.get(),
                sensor_out=self.sensor_out_var.get(),
                device_specification=self.device_specification_var.get(),
                version=self.version_var.get(),
                total_run=self.total_run_var.get(),
                total_power=self.total_power_var.get(),
            )
            numeric_values = (
                *numeric_values[:5],
                device_model_for_control_scheme(self.control_scheme_var.get(), numeric_values[5]),
                *numeric_values[6:],
            )
        except ValueError as exc:
            self._append_log(f"运行/监视量输入错误：{exc}")
            return

        with self.lock:
            self.state.run_mode = {"手动": 0, "自动": 1, "循环定时": 2}[self.run_mode_var.get()]
            self.state.pump_ab_mode = {"A主B备": 0, "B主A备": 1, "双泵轮换/同启": 2}[self.pump_mode_var.get()]
            self.state.parameter_values[0x000C] = self.state.pump_ab_mode
            self.state.pump_a_running = self.pump_a_running_var.get()
            self.state.pump_b_running = self.pump_b_running_var.get()
            self.state.upper_water_level = {"低": 0, "中": 1, "高": 2}[self.upper_level_var.get()]
            self.state.bottom_water_level = {"低": 0, "中": 1, "高": 2}[self.bottom_level_var.get()]
            self.state.add_pump_or_overflow = self.add_pump_var.get()
            (
                self.state.line_voltage,
                self.state.pump_a_current,
                self.state.pump_b_current,
                self.state.power,
                self.state.sensor_out,
                self.state.device_model,
                self.state.version,
                self.state.parameter_values[0x0015],
                self.state.parameter_values[0x0016],
            ) = numeric_values
            # Keep the legacy compatibility register and protocol-201 authoritative value aligned
            # when the operator uses the shared runtime measurement editor.
            self.state.sensor_value = self.state.sensor_out
            self.state.running_sensor_type = self.state.parameter_values[18]
            self.state.sensor_sample_type = self.state.running_sensor_type
            self.state.pending_tanks.clear()
            self.state.sensor_alarm_pending = False
            self.state.adc_sample_valid = True  #20261117-2 change：显式提交零电流也算有效采样。
        self._refresh_views(load_editors=True)
        self._append_log(
            f"已应用本地运行状态和监视量；传感器原始值={self.state.sensor_value}，"
            f"2109H/2110H 已同步；{self.control_scheme_var.get()}，"
            f"210DH=0x{self.state.device_model:04X}。"
        )

    def _preview_device_model(self, _event: Any = None) -> None:
        try:
            specification = parse_int(self.device_specification_var.get())
            device_model = device_model_for_control_scheme(self.control_scheme_var.get(), specification)
        except ValueError:
            self.device_model_preview_var.set("210DH=输入无效")
            return
        self.device_model_preview_var.set(f"210DH=0x{device_model:04X}")

    def _on_control_scheme_selected(self, _event: Any = None) -> None:
        try:
            with self.lock:
                self.state.set_control_scheme(self.control_scheme_var.get())
                device_model = self.state.device_model
        except ValueError as exc:
            self.device_model_preview_var.set("210DH=输入无效")
            self._append_log(f"控制方案切换失败：{exc}")
            return

        # The controller model is a live identity register. Commit this selector immediately so
        # the upper computer sees the same one-to-one/one-to-two choice on its next 210DH poll.
        self.device_model_preview_var.set(f"210DH=0x{device_model:04X}")
        self._refresh_monitor_tree()
        self._refresh_device_status()
        self._append_log(
            f"控制方案已即时切换为{self.control_scheme_var.get()}；210DH=0x{device_model:04X}。"
        )

    def _press_local_stop(self) -> None:
        with self.lock:
            self.state.press_local_stop()
        self._refresh_views(load_editors=True)
        if self.state.pump_a_running or self.state.pump_b_running:
            self._append_log("控制器本机停止被尚未释放的远程启动命令保持。")
        else:
            self._append_log("控制器本机停止已生效。")

    def _inject_fault_local(self) -> None:
        try:
            current_code = int(self.fault_choice_var.get().split("-", 1)[0].strip())
        except ValueError:
            self._append_log("故障选择无效。")
            return
        if current_code == 0:
            with self.lock:
                self.state.clear_faults()
            self._append_log("已清除全部当前故障。")
        else:
            with self.lock:
                error = self.state.apply_fault(current_code - 1, add_history=True)
            if error:
                self._append_log(f"注入故障失败：{ERROR_CODE_TEXT[error]}")
                return
            self._append_log(f"已注入故障：{FAULT_BY_ENUM_CODE[current_code - 1].display_name}。")
        self._refresh_views(load_editors=True)

    def _clear_faults_local(self) -> None:
        with self.lock:
            self.state.clear_faults()
        self._refresh_views(load_editors=True)
        self._append_log("已清除当前故障代码、故障汇总位和泵故障位。")

    def _apply_fault_bits_local(self) -> None:
        bits = 0
        for _label, mask in FAULT_BIT_OPTIONS:
            if self.fault_bit_vars[mask].get():
                bits |= mask
        with self.lock:
            self.state.set_manual_fault_bits(bits, self.pump_a_fault_var.get(), self.pump_b_fault_var.get())
        self._refresh_views(load_editors=True)
        self._append_log(f"已手动设置 2102H=0x{bits:04X}、210CH=0x{self.state.pump_state():04X}。")

    def _on_history_selected(self, _event: Any = None) -> None:
        selected = self.history_tree.focus()
        values = self.history_tree.item(selected, "values") if selected else ()
        if not values:
            return
        index = int(values[0])
        record = self.state.history[index - 1]
        fault = FAULT_BY_ENUM_CODE.get(record.fault_type)
        label = fault.display_name if fault else "无有效故障"
        self.history_index_var.set(str(index))
        self.history_type_var.set(f"{record.fault_type} - {label}")
        self.history_voltage_var.set(str(record.voltage))
        self.history_current_var.set(str(record.current))

    def _write_history_local(self) -> None:
        try:
            index = int(self.history_index_var.get()) - 1
            fault_type = int(self.history_type_var.get().split("-", 1)[0].strip())
            voltage = parse_int(self.history_voltage_var.get())
            current = parse_int(self.history_current_var.get())
        except ValueError as exc:
            self._append_log(f"历史故障输入错误：{exc}")
            return
        with self.lock:
            error = self.state.set_history_record(index, fault_type, voltage, current)
        if error:
            self._append_log(f"写历史记录失败：{ERROR_CODE_TEXT[error]}")
            return
        self._refresh_history_tree()
        self._append_log(f"已本地写入历史故障记录 {index + 1}。")

    def _clear_history_local(self) -> None:
        with self.lock:
            self.state.clear_history()
        self._refresh_history_tree()
        self._append_log("已清空 10 条历史故障记录。")

    def _reset_defaults(self) -> None:
        with self.lock:
            for address in self.device_bus.slave_addresses:
                device = self.device_bus.get_device(address)
                device.state.reset_defaults()
                device.state.parameter_values[0x000B] = address
                device.slave_address = address
        self._refresh_views(load_editors=True)
        self._append_log(f"已恢复 {len(self.device_bus.slave_addresses)} 台 BF006 的默认模拟状态。")

    def _refresh_ports(self, initial: bool = False) -> None:
        if self.serial_module is None:
            self.serial_module, warning = import_pyserial()
            if warning:
                self.connection_status_var.set("已识别系统串口，但连接前必须安装 pyserial。")
        ports = list_serial_ports(self.serial_module)
        self.port_combo["values"] = ports
        self.uart4_port_combo["values"] = ports
        if ports and (initial or not self.port_var.get()):
            self.port_var.set(ports[0])
        if not ports:
            self.connection_status_var.set("未检测到可用串口，请检查虚拟串口驱动或设备连接。")

    def _apply_device_configuration(self) -> bool:
        try:
            addresses = parse_slave_addresses(self.address_var.get())
        except ValueError as exc:
            self._append_log(f"从站地址列表错误：{exc}")
            return False

        with self.lock:
            self.device_bus.reconfigure(addresses)
            selected_text = self.selected_device_var.get().strip()
            selected = int(selected_text) if selected_text.isdigit() else addresses[0]
            if selected not in addresses:
                selected = addresses[0]
            self.simulator = self.device_bus.get_device(selected)
            self.state = self.simulator.state

        canonical = ",".join(str(address) for address in self.device_bus.slave_addresses)
        self.address_var.set(canonical)
        self.device_selector.configure(values=tuple(str(address) for address in self.device_bus.slave_addresses))
        self.selected_device_var.set(str(selected))
        self._refresh_views(load_editors=True)
        self._append_log(f"已应用同一串口的从站地址：{canonical}；当前编辑设备 {selected}。")
        return True

    def _on_selected_device_changed(self, _event: Any = None) -> None:
        try:
            selected = int(self.selected_device_var.get(), 10)
            with self.lock:
                self.simulator = self.device_bus.get_device(selected)
                self.state = self.simulator.state
        except ValueError as exc:
            self._append_log(f"切换编辑设备失败：{exc}")
            return
        self._refresh_views(load_editors=True)
        self._append_log(f"当前编辑设备已切换为从站 {selected}。")

    def _connect_serial(self) -> None:
        """打开所选链路；任一端口打开失败时关闭本次已打开的端口。"""
        if self.serial_module is None:
            self._append_log(PYSERIAL_INSTALL_MESSAGE)
            return
        if not self._apply_device_configuration():
            return
        rs485_port = self.port_var.get().strip()
        uart4_port = self.uart4_port_var.get().strip()
        try:
            if not rs485_port and not uart4_port:
                raise ValueError("请输入 RS485 或 4G UART4 串口")
            if rs485_port and rs485_port.casefold() == uart4_port.casefold():
                raise ValueError("RS485 和 UART4 必须使用不同串口")
            delay = parse_int(self.response_delay_var.get())
            if not 0 <= delay <= 60000:
                raise ValueError("响应延时必须为 0-60000 ms")
            if rs485_port:
                self.serial_handles["RS485"] = self._open_serial(rs485_port, False)
            if uart4_port:
                self.serial_handles["UART4"] = self._open_serial(uart4_port, True)
        except Exception as exc:
            self._close_serial_session(f"串口连接失败：{exc}")
            return

        self.response_delay_seconds = delay / 1000.0
        self.stop_event = threading.Event()
        for link, handle in self.serial_handles.items():
            threading.Thread(
                target=self._serial_receive_loop, args=(link, handle, self.stop_event), daemon=True
            ).start()
        self.connect_button.configure(state="disabled")
        self.disconnect_button.configure(state="normal")
        port_summary = "，".join(f"{link} {handle.port}" for link, handle in self.serial_handles.items())
        self.connection_status_var.set(f"已连接 {port_summary}；地址 {self.address_var.get()}")
        self._refresh_device_status()
        self._append_log(f"已连接 {port_summary}。")

    def _open_serial(self, port: str, uart4: bool) -> Any:
        """独立配置 UART4 固定字符格式和现有 RS485 格式。"""
        parity = {"无校验(N)": "N", "偶校验(E)": "E", "奇校验(O)": "O"}
        return self.serial_module.Serial(
            port=port,
            baudrate=UART4_BAUD_RATE if uart4 else int(self.baud_var.get()),
            bytesize=self.serial_module.EIGHTBITS,
            parity="N" if uart4 else parity[self.parity_var.get()],
            stopbits=1 if uart4 else int(self.stopbits_var.get()),
            timeout=UART4_READ_TIMEOUT_SECONDS if uart4 else SERIAL_READ_TIMEOUT_SECONDS,
            write_timeout=SERIAL_WRITE_TIMEOUT_SECONDS,
        )

    def _disconnect_serial(self) -> None:
        self._close_serial_session("已断开串口。")

    def _serial_receive_loop(self, link: str, handle: Any, stop_event: threading.Event) -> None:
        """在独立接收线程中处理一条链路，用设备锁保护共享状态。"""
        uart4 = link == "UART4"

        def process(frame: bytes) -> None:
            with self.lock:
                response = self.device_bus.handle_frame(frame, uart4=uart4)
                result = self.device_bus.last_result
                address = self.device_bus.last_device_address
            send_serial_reply(handle, response, uart4, self.response_delay_seconds)
            if not stop_event.is_set():
                self.root.after(0, self._after_protocol_handled, result, address, uart4)

        try:
            receive_serial_frames(handle, uart4, stop_event, process)
        except Exception as exc:
            if not stop_event.is_set():
                self.root.after(0, self._serial_error_disconnect, f"{link}：{exc}", stop_event)

    def _after_protocol_handled(
        self,
        result: ModbusProtocolResult | None,
        handled_address: int | None = None,
        uart4: bool = False,
    ) -> None:
        if result is None:
            return
        link = "UART4" if uart4 else "RS485"
        self._append_log(f"{link} RX: {frame_to_hex(result.request)}")
        suffix = ""
        if not uart4 and result.exception_code is not None:
            suffix = f"；异常码 {result.exception_code:02X}H {ERROR_CODE_TEXT.get(result.exception_code, '')}"
        self._append_log(f"{link} TX: {frame_to_hex(result.response)}；{result.message}{suffix}")
        if uart4:
            is_write = result.response is not None and result.exception_code is None and result.request[5] in (0x10, 0x11)
        else:
            is_write = len(result.request) > 1 and result.request[1] in (
                MODBUS_WRITE_SINGLE, MODBUS_WRITE_MULTIPLE, BF006_PRIVATE_SET_ADDRESS, BF006_PRIVATE_CLEAR_TIME,
            )
        if is_write:
            addresses = self.device_bus.slave_addresses
            self.address_var.set(",".join(str(address) for address in addresses))
            self.device_selector.configure(values=tuple(str(address) for address in addresses))
            if handled_address == self.simulator.slave_address:
                self.selected_device_var.set(str(handled_address))
                self._refresh_views(load_editors=True)
        else:
            if handled_address == self.simulator.slave_address:
                self._refresh_monitor_tree()
                self._refresh_device_status()

    def _serial_error_disconnect(self, message: str, stop_event: threading.Event) -> None:
        if stop_event is self.stop_event and not stop_event.is_set():
            self._close_serial_session(f"串口错误：{message}")

    def _close_serial_session(self, log_message: str | None = None) -> None:
        self.stop_event.set()
        for handle in self.serial_handles.values():
            try:
                handle.close()
            except Exception:
                pass
        self.serial_handles.clear()
        self.connect_button.configure(state="normal")
        self.disconnect_button.configure(state="disabled")
        self.connection_status_var.set("未连接")
        if log_message:
            self._append_log(log_message)

    def _append_log(self, text: str) -> None:
        timestamp = time.strftime("%H:%M:%S")
        self.log_text.insert("end", f"[{timestamp}] {text}\n")
        self.log_text.see("end")

    def on_close(self) -> None:
        self._close_serial_session()
        self.root.destroy()


def import_pyserial() -> tuple[Any | None, str | None]:
    """Import pyserial lazily so protocol tests can run without it."""
    try:
        import serial
        import serial.tools.list_ports

        return serial, None
    except Exception:
        return None, PYSERIAL_INSTALL_MESSAGE


def run_gui() -> None:
    import tkinter as tk

    serial_module, warning = import_pyserial()
    root = tk.Tk()
    app = SimulatorGui(root, serial_module, warning)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()


def build_status_payload(simulator: Bf006ModbusRtuSimulator) -> dict[str, object]:
    """Return the stable state summary used by every automation command."""
    state = simulator.state
    running_pumps = []
    if state.pump_a_running:
        running_pumps.append("A")
    if state.pump_b_running:
        running_pumps.append("B")
    return {
        "slave_address": simulator.slave_address,
        "protocol_version": state.version,
        "device_model": state.device_model,
        "device_model_hex": f"0x{state.device_model:04X}",
        "control_scheme": control_scheme_from_device_model(state.device_model),
        "run_status": "running" if running_pumps else "stopped",
        "running_pumps": running_pumps,
        "run_mode": state.run_mode,
        "pump_ab_mode": state.pump_ab_mode,
        "pump_control_command": state.pump_control_command,
        "upper_water_level": state.upper_water_level,
        "lower_water_level": state.bottom_water_level,
        "add_pump_or_overflow": state.add_pump_or_overflow,
        "fault_code": state.current_fault_code(),
        "fault_status_bits": state.fault_status_bits,
        "pump_a_fault": state.pump_a_fault,
        "pump_b_fault": state.pump_b_fault,
        "fault_bits_32": state.fault_bits_32,
        "sensor_status": state.sensor_status,
        "parameter_saved": state.parameter_saved,
        "measurements": build_measurement_payload(state),
        "liquid": build_liquid_payload(state),
    }


def emit_cli_payload(payload: dict[str, object]) -> int:
    """Write one UTF-8 JSON result and map its success flag to an exit code."""
    serialized = json.dumps(payload, ensure_ascii=False)
    with _CLI_OUTPUT_LOCK:
        print(serialized, flush=True)
    return 0 if payload.get("ok") else 1


def parse_parameter_address(text: str) -> int:
    """Parse a BF006 P-number or a hexadecimal register address."""
    normalized = text.strip().upper()
    if normalized in PARAMETER_BY_NUMBER:
        return PARAMETER_BY_NUMBER[normalized].address
    if normalized.startswith("0X"):
        return int(normalized, 16)
    return int(normalized, 16)


def parse_slave_addresses(text: str) -> list[int]:
    """Parse comma-separated addresses and inclusive ranges such as ``1,3-5``."""
    addresses: list[int] = []
    for item in text.split(","):
        item = item.strip()
        if not item:
            raise ValueError("从站地址列表包含空项")
        if "-" in item:
            start_text, end_text = item.split("-", 1)
            start = int(start_text.strip(), 10)
            end = int(end_text.strip(), 10)
            if start > end:
                raise ValueError(f"从站地址范围必须从小到大：{item}")
            if start < 1 or end > 254:
                raise ValueError(f"从站地址范围必须在 1-254 内：{item}")
            addresses.extend(range(start, end + 1))
        else:
            addresses.append(int(item, 10))
    if not addresses:
        raise ValueError("至少需要一个从站地址")
    if len(set(addresses)) != len(addresses):
        raise ValueError("从站地址不能重复")
    if any(address < 1 or address > 254 for address in addresses):
        raise ValueError("从站地址必须在 1-254 范围内")
    return addresses


def build_parameter_payload(simulator: Bf006ModbusRtuSimulator, address: int) -> dict[str, object]:
    """Return one parameter definition together with its current raw value."""
    definition = PARAMETER_BY_ADDRESS[address]
    return {
        "address": address,
        "address_hex": f"{address:04X}",
        "number": definition.number,
        "firmware_name": definition.firmware_name,
        "name": definition.display_name,
        "value": simulator.state.parameter_values[address],
        "minimum": definition.minimum,
        "maximum": definition.maximum,
        "default": definition.default,
        "unit": definition.unit,
        "range": definition.range_text,
        "writable": definition.writable,
    }


def build_monitor_payload(simulator: Bf006ModbusRtuSimulator, definition: MonitorDefinition) -> dict[str, object]:
    """Return one monitor definition and the exact value visible through Modbus."""
    error, value = simulator.state.read_register(definition.address)
    if error:
        raise ValueError(ERROR_CODE_TEXT[error])
    return {
        "address": definition.address,
        "address_hex": f"{definition.address:04X}",
        "firmware_name": definition.firmware_name,
        "name": definition.display_name,
        "value": value,
        "value_hex": f"0x{value:04X}",
        "unit": definition.unit,
    }


def build_measurement_payload(state: Bf006State) -> dict[str, object]:
    return {
        "line_voltage_raw": state.line_voltage,
        "pump_a_current_raw": state.pump_a_current,
        "pump_b_current_raw": state.pump_b_current,
        "power_raw": state.power,
        "sensor_out_raw": state.sensor_out,
        "device_model_raw": state.device_model,
        "version_raw": state.version,
        "total_run_raw": state.parameter_values[0x0015],
        "total_power_raw": state.parameter_values[0x0016],
    }


def build_liquid_payload(state: Bf006State) -> dict[str, object]:
    registers = {f"{address:04X}": state.read_register(address)[1] for address in range(0x210F, 0x2117)}
    return {
        "sensor_type": state.parameter_values[0x0012],
        "sensor_value_raw": state.sensor_value,
        "sensor_target": state.parameter_values[0x000D],
        "sensor_level": state.sensor_level,
        "upper_segments": state.upper_water_segments,
        "lower_segments": state.bottom_water_segments,
        "manual_sensor_status": state.manual_sensor_status,
        "effective_sensor_status": state.sensor_status,
        "registers": registers,
    }


def build_history_record_payload(index: int, record: HistoryRecord) -> dict[str, object]:
    fault = FAULT_BY_ENUM_CODE.get(record.fault_type)
    return {
        "index": index + 1,
        "fault_type": record.fault_type,
        "fault_name": fault.display_name if fault is not None else "无有效故障",
        "voltage": record.voltage,
        "current": record.current,
    }


def build_fleet_payload(device_bus: Bf006DeviceBus) -> dict[str, object]:
    addresses = device_bus.slave_addresses
    return {
        "device_count": len(addresses),
        "slave_addresses": addresses,
        "devices": {
            str(address): build_status_payload(device_bus.get_device(address))
            for address in addresses
        },
    }


def build_cli_context_payload(
    simulator: Bf006ModbusRtuSimulator,
    device_bus: Bf006DeviceBus | None = None,
) -> dict[str, object]:
    if device_bus is None:
        return {"state": build_status_payload(simulator)}
    return {"fleet": build_fleet_payload(device_bus)}


def build_frame_route_payload(
    request: bytes,
    response: bytes | None,
    result: ModbusProtocolResult | None,
    device_address: int | None,
) -> dict[str, object]:
    """Keep wire addresses distinct from a device's address after a P-11 write."""
    handled = result is not None and not result.ignored
    request_address = request[0] if request else None
    response_address = response[0] if response else None
    current_device_address = device_address if handled else None
    return {
        # ``slave_address`` remains the concise wire-level response identity used
        # by existing automation. The explicit fields remove P-11 ambiguity.
        "slave_address": response_address,
        "request_address": request_address,
        "response_address": response_address,
        "device_address": current_device_address,
        "handled": handled,
        "responded": response is not None,
    }


def execute_bus_cli_tokens(device_bus: Bf006DeviceBus, tokens: list[str]) -> dict[str, object]:
    """Execute one CLI command against an explicitly addressed device on a shared bus."""
    if not tokens or not tokens[0].startswith("@"):
        if len(device_bus.slave_addresses) != 1:
            return {
                "ok": False,
                "command": " ".join(tokens),
                "error": "多设备命令必须使用 @从站地址 前缀，例如 @2 status。",
                "fleet": build_fleet_payload(device_bus),
            }
        target_address = device_bus.slave_addresses[0]
        command_tokens = tokens
    else:
        try:
            target_address = int(tokens[0][1:], 10)
            device_bus.get_device(target_address)
        except ValueError as exc:
            return {
                "ok": False,
                "command": " ".join(tokens),
                "error": str(exc),
                "fleet": build_fleet_payload(device_bus),
            }
        command_tokens = tokens[1:]
        if not command_tokens:
            return {
                "ok": False,
                "command": " ".join(tokens),
                "error": "@从站地址 后必须提供命令。",
                "fleet": build_fleet_payload(device_bus),
            }

    simulator = device_bus.get_device(target_address)
    # A one-device bus deliberately allows commands without an @address prefix.
    # Bus-sensitive operations must still preserve or update the mapping exactly
    # as their explicitly targeted counterparts do.
    if command_tokens == ["reset-defaults"]:
        simulator.state.reset_defaults()
        simulator.slave_address = target_address
        simulator.state.parameter_values[0x000B] = target_address
        return {
            "ok": True,
            "command": "reset-defaults",
            "target_address": target_address,
            "state": build_status_payload(simulator),
        }
    if len(command_tokens) == 4 and command_tokens[:2] == ["parameter", "set"]:
        try:
            if parse_parameter_address(command_tokens[2]) == 0x000B:
                simulator = device_bus.change_device_address(
                    target_address,
                    parse_int(command_tokens[3]),
                )
                target_address = simulator.slave_address
        except ValueError as exc:
            return {
                "ok": False,
                "command": " ".join(command_tokens),
                "target_address": target_address,
                "error": str(exc),
                "fleet": build_fleet_payload(device_bus),
            }
    result = execute_cli_tokens(simulator, command_tokens, device_bus)
    return {**result, "target_address": target_address}


def parse_cli_named_options(tokens: list[str]) -> dict[str, str]:
    """Parse order-independent ``--name value`` pairs for stateful inner commands."""
    if len(tokens) % 2:
        raise ValueError("Each CLI option must have a value.")
    options: dict[str, str] = {}
    for index in range(0, len(tokens), 2):
        name = tokens[index]
        if not name.startswith("--"):
            raise ValueError(f"Expected an option name, got {name!r}.")
        if name in options:
            raise ValueError(f"Duplicate option: {name}")
        options[name] = tokens[index + 1]
    return options


def parse_on_off(value: str) -> bool:
    normalized = value.lower()
    if normalized not in ("on", "off"):
        raise ValueError("Boolean values must be 'on' or 'off'.")
    return normalized == "on"


def execute_cli_tokens(
    simulator: Bf006ModbusRtuSimulator, tokens: list[str], device_bus: Bf006DeviceBus | None = None,
) -> dict[str, object]:
    """Execute one stateful command shared by one-shot, batch, session, and serve modes."""
    operation = " ".join(tokens)
    control_writes = {
        "pump-a-start": (0x2000, 0x0012),
        "pump-a-stop": (0x2000, 0x0011),
        "pump-b-start": (0x2000, 0x0022),
        "pump-b-stop": (0x2000, 0x0021),
        "all-start": (0x2000, 0x0032),
        "all-stop": (0x2000, 0x0031),
        "release": (0x2000, 0x0000),
        "manual": (0x2003, 0x0000),
        "auto": (0x2003, 0x0001),
        "timer": (0x2003, 0x0002),
        "a-main": (0x2004, 0x0000),
        "b-main": (0x2004, 0x0001),
        "alternating": (0x2004, 0x0002),
        "clear-faults": (0x2005, 0x0001),
        "clear-history": (0x2006, 0x0000),
        "save-parameters": (0x2007, 0x5AA5),
    }
    try:
        if tokens == ["status"]:
            return {"ok": True, "command": operation, "state": build_status_payload(simulator)}
        if tokens == ["reset-defaults"]:
            simulator.state.reset_defaults()
            simulator.slave_address = DEFAULT_SLAVE_ADDRESS
            return {"ok": True, "command": operation, "state": build_status_payload(simulator)}
        if len(tokens) == 3 and tokens[:2] == ["parameter", "get"]:
            address = parse_parameter_address(tokens[2])
            if address not in PARAMETER_BY_ADDRESS:
                return {
                    "ok": False,
                    "command": operation,
                    "error_code": ERR_ILLEGAL_REGISTER,
                    "error": ERROR_CODE_TEXT[ERR_ILLEGAL_REGISTER],
                    "state": build_status_payload(simulator),
                }
            return {
                "ok": True,
                "command": operation,
                "parameter": build_parameter_payload(simulator, address),
                "state": build_status_payload(simulator),
            }
        if len(tokens) == 4 and tokens[:2] == ["parameter", "set"]:
            address = parse_parameter_address(tokens[2])
            value = parse_int(tokens[3])
            error = simulator.state.write_parameter(address, value)
            if error:
                return {
                    "ok": False,
                    "command": operation,
                    "error_code": error,
                    "error": ERROR_CODE_TEXT[error],
                    "state": build_status_payload(simulator),
                }
            if address == 0x000B:
                simulator.slave_address = value
            return {
                "ok": True,
                "command": operation,
                "parameter": build_parameter_payload(simulator, address),
                "state": build_status_payload(simulator),
            }
        if tokens in (["parameter", "list"], ["parameter", "list", "--writable-only"]):
            writable_only = len(tokens) == 3
            parameters = [
                build_parameter_payload(simulator, definition.address)
                for definition in PARAMETERS
                if not writable_only or definition.writable
            ]
            return {
                "ok": True,
                "command": operation,
                "count": len(parameters),
                "parameters": parameters,
                "state": build_status_payload(simulator),
            }
        if len(tokens) == 3 and tokens[:2] == ["monitor", "get"]:
            text = tokens[2].strip()
            address = int(text, 16)
            definition = MONITOR_BY_ADDRESS.get(address)
            if definition is None:
                return {
                    "ok": False,
                    "command": operation,
                    "error_code": ERR_ILLEGAL_REGISTER,
                    "error": ERROR_CODE_TEXT[ERR_ILLEGAL_REGISTER],
                    "state": build_status_payload(simulator),
                }
            return {
                "ok": True,
                "command": operation,
                "monitor": build_monitor_payload(simulator, definition),
                "state": build_status_payload(simulator),
            }
        if tokens == ["monitor", "list"]:
            monitors = [build_monitor_payload(simulator, definition) for definition in MONITORS]
            return {
                "ok": True,
                "command": operation,
                "count": len(monitors),
                "monitors": monitors,
                "state": build_status_payload(simulator),
            }
        if len(tokens) >= 3 and tokens[:2] == ["runtime", "set"]:
            options = parse_cli_named_options(tokens[2:])
            required = {
                "--mode",
                "--pump-a",
                "--pump-b",
                "--pump-mode",
                "--upper-level",
                "--lower-level",
                "--add-pump",
            }
            if set(options) != required:
                missing = sorted(required - set(options))
                unknown = sorted(set(options) - required)
                raise ValueError(f"runtime set options mismatch; missing={missing}, unknown={unknown}")
            run_mode = {"manual": 0, "auto": 1, "timer": 2}[options["--mode"].lower()]
            pump_mode = {"a-main": 0, "b-main": 1, "alternating": 2}[options["--pump-mode"].lower()]
            levels = {"low": 0, "middle": 1, "high": 2}
            pump_a_running = parse_on_off(options["--pump-a"])
            pump_b_running = parse_on_off(options["--pump-b"])
            upper_level = levels[options["--upper-level"].lower()]
            lower_level = levels[options["--lower-level"].lower()]
            add_pump = parse_on_off(options["--add-pump"])
            state = simulator.state
            state.run_mode = run_mode
            state.pump_a_running = pump_a_running
            state.pump_b_running = pump_b_running
            state.pump_ab_mode = pump_mode
            state.parameter_values[0x000C] = pump_mode
            state.upper_water_level = upper_level
            state.bottom_water_level = lower_level
            state.pending_tanks.clear()
            state.add_pump_or_overflow = add_pump
            return {"ok": True, "command": operation, "state": build_status_payload(simulator)}
        if len(tokens) >= 3 and tokens[:2] == ["measurement", "set"]:
            options = parse_cli_named_options(tokens[2:])
            required = {
                "--voltage",
                "--current-a",
                "--current-b",
                "--power",
                "--legacy-sensor",
                "--device-model",
                "--version",
                "--total-run",
                "--total-power",
            }
            phase_fields = {3: "--current-ab", 4: "--current-ac", 6: "--current-bb", 7: "--current-bc"}
            allowed = required | set(phase_fields.values())
            if not required <= options.keys() or not options.keys() <= allowed:
                raise ValueError(
                    f"measurement set options mismatch; missing={sorted(required - set(options))}, "
                    f"unknown={sorted(set(options) - allowed)}"
                )
            phase_currents = {
                number: parse_scaled_u16(options[key], 10, key)
                for number, key in phase_fields.items() if key in options
            }
            values = parse_monitor_editor_values(
                voltage=options["--voltage"],
                pump_a_current=options["--current-a"],
                pump_b_current=options["--current-b"],
                power=options["--power"],
                sensor_out=options["--legacy-sensor"],
                device_specification=options["--device-model"],
                version=options["--version"],
                total_run=options["--total-run"],
                total_power=options["--total-power"],
            )
            state = simulator.state
            (
                state.line_voltage,
                state.pump_a_current,
                state.pump_b_current,
                state.power,
                state.sensor_out,
                state.device_model,
                state.version,
                state.parameter_values[0x0015],
                state.parameter_values[0x0016],
            ) = values
            state.sensor_value = state.sensor_out
            state.running_sensor_type = state.parameter_values[18]
            state.sensor_sample_type = state.running_sensor_type
            state.sensor_alarm_pending = False
            state.phase_currents = phase_currents
            state.adc_sample_valid = True  #20261117-2 change：每相独立采样，省略的三相分量保持未就绪。
            return {
                "ok": True,
                "command": operation,
                "measurements": build_measurement_payload(state),
                "state": build_status_payload(simulator),
            }
        if len(tokens) == 4 and tokens[:2] == ["device", "set-electrical"]:
            phases, rated_voltage = map(parse_int, tokens[2:])
            if (phases, rated_voltage) not in ((1, 220), (3, 220), (3, 380)):
                raise ValueError("机型电气规格须为单相220V、三相220V或三相380V")
            simulator.state.phases = phases
            simulator.state.rated_voltage = rated_voltage
            return {"ok": True, "command": operation, "state": build_status_payload(simulator)}
        if len(tokens) == 3 and tokens[:2] == ["device", "set-model"]:
            value = parse_int(tokens[2])
            if value < 0 or value > 0xFFFF:
                raise ValueError("Device model must be in the U16 range.")
            simulator.state.device_model = value
            return {"ok": True, "command": operation, "state": build_status_payload(simulator)}
        if len(tokens) == 3 and tokens[:2] == ["device", "set-version"]:
            value = parse_int(tokens[2])
            if value < 0 or value > 0xFFFF:
                raise ValueError("Protocol version must be in the U16 range.")
            simulator.state.version = value
            return {"ok": True, "command": operation, "state": build_status_payload(simulator)}
        if len(tokens) >= 3 and tokens[:2] == ["liquid", "set"]:
            options = parse_cli_named_options(tokens[2:])
            required = {
                "--sensor-type",
                "--value",
                "--target",
                "--level",
                "--upper-segments",
                "--lower-segments",
                "--status",
            }
            if set(options) != required:
                raise ValueError(
                    f"liquid set options mismatch; missing={sorted(required - set(options))}, "
                    f"unknown={sorted(set(options) - required)}"
                )
            state = simulator.state
            state.set_sensor_state(
                sensor_type=parse_int(options["--sensor-type"]),
                sensor_value=parse_scaled_u16(options["--value"], 100, "连续传感器值", 0xFFFFFFFF),
                sensor_target={"upper": 0, "lower": 1}[options["--target"].lower()],
                sensor_level={"low": 0, "middle": 1, "high": 2}[options["--level"].lower()],
                upper_segments=parse_int(options["--upper-segments"]),
                bottom_segments=parse_int(options["--lower-segments"]),
                sensor_status=parse_int(options["--status"]),
            )
            return {
                "ok": True,
                "command": operation,
                "liquid": build_liquid_payload(state),
                "state": build_status_payload(simulator),
            }
        if len(tokens) == 2 and tokens[0] == "control":
            action = tokens[1]
            if action == "local-stop":
                simulator.state.press_local_stop()
            elif action in control_writes:
                address, value = control_writes[action]
                error = simulator.state.write_control(address, value)
                if error:
                    return {
                        "ok": False,
                        "command": operation,
                        "error_code": error,
                        "error": ERROR_CODE_TEXT[error],
                        "state": build_status_payload(simulator),
                    }
            else:
                raise ValueError(f"Unknown control action: {action}")
            return {"ok": True, "command": operation, "state": build_status_payload(simulator)}
        if len(tokens) in (3, 4) and tokens[:2] == ["fault", "set"]:
            if len(tokens) == 4 and tokens[3] != "--no-history":
                raise ValueError(f"Unknown fault set option: {tokens[3]}")
            enum_code = parse_int(tokens[2])
            add_history = len(tokens) == 3
            error = simulator.state.apply_fault(enum_code, add_history=add_history)
            if error:
                return {
                    "ok": False,
                    "command": operation,
                    "error_code": error,
                    "error": ERROR_CODE_TEXT[error],
                    "state": build_status_payload(simulator),
                }
            fault = FAULT_BY_ENUM_CODE[enum_code]
            return {
                "ok": True,
                "command": operation,
                "fault": {
                    "enum_code": enum_code,
                    "current_code": fault.current_code,
                    "firmware_name": fault.firmware_name,
                    "name": fault.display_name,
                },
                "history_added": add_history,
                "history_record": (
                    build_history_record_payload(0, simulator.state.history[0]) if add_history else None
                ),
                "state": build_status_payload(simulator),
            }
        if tokens == ["fault", "list"]:
            faults = [
                {
                    "enum_code": fault.enum_code,
                    "current_code": fault.current_code,
                    "firmware_name": fault.firmware_name,
                    "name": fault.display_name,
                    "summary_bit": fault.summary_bit,
                    "pump": fault.pump,
                }
                for fault in FAULTS
            ]
            return {
                "ok": True,
                "command": operation,
                "count": len(faults),
                "faults": faults,
                "state": build_status_payload(simulator),
            }
        if len(tokens) >= 3 and tokens[:2] == ["fault", "bits"]:
            options = parse_cli_named_options(tokens[2:])
            required = {"--summary", "--pump-a", "--pump-b"}
            if set(options) != required:
                raise ValueError(
                    f"fault bits options mismatch; missing={sorted(required - set(options))}, "
                    f"unknown={sorted(set(options) - required)}"
                )
            error = simulator.state.set_manual_fault_bits(
                parse_int(options["--summary"]),
                parse_on_off(options["--pump-a"]),
                parse_on_off(options["--pump-b"]),
            )
            if error:
                return {
                    "ok": False,
                    "command": operation,
                    "error_code": error,
                    "error": ERROR_CODE_TEXT[error],
                    "state": build_status_payload(simulator),
                }
            return {"ok": True, "command": operation, "state": build_status_payload(simulator)}
        if tokens == ["fault", "clear"]:
            simulator.state.clear_faults()
            return {"ok": True, "command": operation, "state": build_status_payload(simulator)}
        if len(tokens) == 3 and tokens[:2] == ["history", "get"]:
            index = parse_int(tokens[2]) - 1
            if index < 0 or index >= len(simulator.state.history):
                raise ValueError("History index must be in the range 1..10.")
            return {
                "ok": True,
                "command": operation,
                "history_record": build_history_record_payload(index, simulator.state.history[index]),
                "state": build_status_payload(simulator),
            }
        if tokens == ["history", "list"]:
            history = [
                build_history_record_payload(index, record)
                for index, record in enumerate(simulator.state.history)
            ]
            return {
                "ok": True,
                "command": operation,
                "count": len(history),
                "history": history,
                "state": build_status_payload(simulator),
            }
        if tokens == ["history", "clear"]:
            simulator.state.clear_history()
            return {"ok": True, "command": operation, "state": build_status_payload(simulator)}
        if len(tokens) >= 4 and tokens[:2] == ["history", "set"]:
            index = parse_int(tokens[2]) - 1
            options = parse_cli_named_options(tokens[3:])
            required = {"--fault", "--voltage", "--current"}
            if set(options) != required:
                raise ValueError(
                    f"history set options mismatch; missing={sorted(required - set(options))}, "
                    f"unknown={sorted(set(options) - required)}"
                )
            error = simulator.state.set_history_record(
                index,
                parse_int(options["--fault"]),
                parse_int(options["--voltage"]),
                parse_int(options["--current"]),
            )
            if error:
                return {
                    "ok": False,
                    "command": operation,
                    "error_code": error,
                    "error": ERROR_CODE_TEXT[error],
                    "state": build_status_payload(simulator),
                }
            return {
                "ok": True,
                "command": operation,
                "history_record": build_history_record_payload(index, simulator.state.history[index]),
                "state": build_status_payload(simulator),
            }
        if len(tokens) >= 2 and tokens[0] == "frame":
            uart4 = tokens[1] == "--uart4"
            request_frame = bytes.fromhex(" ".join(tokens[2:] if uart4 else tokens[1:]))
            request_address = request_frame[0] if request_frame else simulator.slave_address
            matches_target = request_address == simulator.slave_address or (
                not uart4 and request_address == BF006_PRIVATE_BROADCAST_ADDRESS
            )
            if device_bus is not None and matches_target:  #20261117-3 change
                # 两种协议通过总线校验地址占用，并在改址后同步设备索引。
                response = device_bus.handle_frame(request_frame, uart4=uart4)
                result = device_bus.last_result
            else:
                if uart4 and device_bus is not None:
                    raise ValueError("报文地址与 @ 指定设备不一致")
                response = simulator.handle_uart4_frame(request_frame) if uart4 else simulator.handle_frame(request_frame)
                result = simulator.last_result
            return {
                "ok": result is not None and result.crc_ok,
                "command": "frame",
                **build_frame_route_payload(
                    request_frame,
                    response,
                    result,
                    simulator.slave_address,
                ),
                "request": frame_to_hex(request_frame),
                "response": frame_to_hex(response),
                "protocol": {
                    "message": result.message if result is not None else "",
                    "crc_ok": result.crc_ok if result is not None else False,
                    "ignored": result.ignored if result is not None else True,
                    "exception_code": result.exception_code if result is not None else None,
                },
                "state": build_status_payload(simulator),
            }
    except (KeyError, TypeError, ValueError, OSError) as exc:
        return {
            "ok": False,
            "command": operation,
            "error": str(exc),
            "state": build_status_payload(simulator),
        }
    return {
        "ok": False,
        "command": operation,
        "error": "Unknown or malformed CLI command.",
        "state": build_status_payload(simulator),
    }


def run_cli_session(
    simulator: Bf006ModbusRtuSimulator,
    device_bus: Bf006DeviceBus | None = None,
) -> int:
    """Read one stateful command per stdin line and emit JSON Lines events."""
    started_payload = {"ok": True, "event": "session_started"}
    if device_bus is None:
        started_payload["state"] = build_status_payload(simulator)
    else:
        started_payload["fleet"] = build_fleet_payload(device_bus)
    emit_cli_payload(started_payload)
    for command_text in sys.stdin:
        command_text = command_text.strip()
        if not command_text:
            continue
        if command_text.lower() in ("exit", "quit"):
            break
        try:
            tokens = shlex.split(command_text)
        except ValueError as exc:
            emit_cli_payload(
                {
                    "ok": False,
                    "command": command_text,
                    "error": str(exc),
                    **(
                        {"state": build_status_payload(simulator)}
                        if device_bus is None
                        else {"fleet": build_fleet_payload(device_bus)}
                    ),
                }
            )
            continue
        emit_cli_payload(
            execute_bus_cli_tokens(device_bus, tokens)
            if device_bus is not None
            else execute_cli_tokens(simulator, tokens)
        )
    closed_payload = {"ok": True, "event": "session_closed"}
    if device_bus is None:
        closed_payload["state"] = build_status_payload(simulator)
    else:
        closed_payload["fleet"] = build_fleet_payload(device_bus)
    emit_cli_payload(closed_payload)
    return 0


def run_serve_control_input(
    simulator: Bf006ModbusRtuSimulator,
    state_lock: threading.RLock,
    stop_event: threading.Event,
    device_bus: Bf006DeviceBus | None = None,
) -> None:
    """Apply live state commands from stdin while the serial service remains active."""
    sequence = 0
    close_reason = "stdin_eof"
    try:
        for command_text in sys.stdin:
            command_text = command_text.strip()
            if not command_text:
                continue
            if command_text.lower() in ("exit", "quit"):
                close_reason = "requested"
                break
            sequence += 1
            try:
                tokens = shlex.split(command_text)
            except ValueError as exc:
                with state_lock:
                    result = {
                        "ok": False,
                        "command": command_text,
                        "error": str(exc),
                    }
                    if device_bus is None:
                        result["state"] = build_status_payload(simulator)
                    else:
                        result["fleet"] = build_fleet_payload(device_bus)
            else:
                with state_lock:
                    result = (
                        execute_bus_cli_tokens(device_bus, tokens)
                        if device_bus is not None
                        else execute_cli_tokens(simulator, tokens)
                    )
            emit_cli_payload(
                {
                    "ok": result["ok"],
                    "event": "command_result",
                    "sequence": sequence,
                    "input": command_text,
                    "result": result,
                }
            )
    except Exception as exc:
        close_reason = "stdin_error"
        emit_cli_payload(
            {
                "ok": False,
                "event": "control_error",
                "transport": "stdin",
                "error": str(exc),
            }
        )
    finally:
        emit_cli_payload(
            {
                "ok": close_reason != "stdin_error",
                "event": "control_closed",
                "transport": "stdin",
                "reason": close_reason,
                "commands_processed": sequence,
            }
        )
        stop_event.set()


def run_serial_server(
    options: argparse.Namespace,
    simulator: Bf006ModbusRtuSimulator,
    device_bus: Bf006DeviceBus | None = None,
) -> int:
    """以无界面方式在一个或两个端口提供持续的 BF006 控制器状态。"""
    serial_module, warning = import_pyserial()
    if serial_module is None:
        return emit_cli_payload(
            {
                "ok": False,
                "event": "serial_error",
                "port": options.port,
                "error": warning,
                **build_cli_context_payload(simulator, device_bus),
            }
        )

    address = options.address if options.address is not None else simulator.slave_address
    if device_bus is None and (address < 1 or address > 254):
        return emit_cli_payload(
            {
                "ok": False,
                "event": "serial_error",
                "port": options.port,
                "error": "Slave address must be in the range 1..254.",
                **build_cli_context_payload(simulator, device_bus),
            }
        )
    if options.duration is not None and options.duration < 0:
        return emit_cli_payload(
            {
                "ok": False,
                "event": "serial_error",
                "port": options.port,
                "error": "Duration must not be negative.",
                **build_cli_context_payload(simulator, device_bus),
            }
        )
    if options.response_delay_ms < 0 or options.response_delay_ms > 60000:
        return emit_cli_payload(
            {
                "ok": False,
                "event": "serial_error",
                "port": options.port,
                "error": "Response delay must be in the range 0..60000 ms.",
                **build_cli_context_payload(simulator, device_bus),
            }
        )

    baudrate = options.baudrate if options.baudrate is not None else DEFAULT_BAUD_RATE
    ports = [(options.port, options.uart4)]
    handles = []
    try:
        if options.uart4 and (baudrate != UART4_BAUD_RATE or options.parity != "N" or options.stopbits != 1):
            raise ValueError("4G UART4 V1.3 固定使用 9600 / 8N1")
        if options.uart4_port:
            if options.uart4 or options.port.casefold() == options.uart4_port.casefold():
                raise ValueError("双串口须指定不同的 RS485 与 UART4 端口")
            ports.append((options.uart4_port, True))
        for port, uart4 in ports:
            handle = serial_module.Serial(
                port=port,
                baudrate=UART4_BAUD_RATE if uart4 else baudrate,
                bytesize=serial_module.EIGHTBITS,
                parity="N" if uart4 else options.parity,
                stopbits=1 if uart4 else options.stopbits,
                timeout=UART4_READ_TIMEOUT_SECONDS if uart4 else SERIAL_READ_TIMEOUT_SECONDS,
                write_timeout=SERIAL_WRITE_TIMEOUT_SECONDS,
            )
            handles.append((handle, uart4))
    except Exception as exc:
        for handle, _ in handles:
            try:
                handle.close()
            except Exception:
                pass
        return emit_cli_payload(
            {
                "ok": False,
                "event": "serial_error",
                "port": options.port,
                "error": str(exc),
                **build_cli_context_payload(simulator, device_bus),
            }
        )

    if device_bus is None:
        simulator.slave_address = address
        simulator.state.parameter_values[0x000B] = address
        slave_addresses = [address]
    else:
        slave_addresses = device_bus.slave_addresses
    state_lock = threading.RLock()
    stop_event = threading.Event()
    emit_cli_payload(
        {
            "ok": True,
            "event": "serial_opened",
            "port": options.port,
            "baudrate": baudrate,
            "parity": options.parity,
            "stopbits": options.stopbits,
            "slave_address": slave_addresses[0] if len(slave_addresses) == 1 else None,
            "slave_addresses": slave_addresses,
            "device_count": len(slave_addresses),
            "response_delay_ms": options.response_delay_ms,
            "control_stdin": options.control_stdin,
            "uart4_port": options.port if options.uart4 else options.uart4_port,
            "rs485_port": None if options.uart4 else options.port,
            "initialization_results": options.initialization_results,
        }
    )

    if options.control_stdin:
        emit_cli_payload({"ok": True, "event": "control_opened", "transport": "stdin"})
        threading.Thread(
            target=run_serve_control_input,
            args=(simulator, state_lock, stop_event, device_bus),
            name="bf006-cli-control",
            daemon=True,
        ).start()

    deadline = time.monotonic() + options.duration if options.duration is not None else None
    failed = threading.Event()

    def process_request(request_frame: bytes, handle: Any, uart4: bool) -> None:
        with state_lock:
            protocol_target = device_bus if device_bus is not None else simulator
            if device_bus is not None:
                response = device_bus.handle_frame(request_frame, uart4=uart4)
            else:
                response = simulator.handle_uart4_frame(request_frame) if uart4 else simulator.handle_frame(request_frame)
            result = protocol_target.last_result
            handled_device_address = (
                device_bus.last_device_address
                if device_bus is not None
                else simulator.slave_address
            )
            route_payload = build_frame_route_payload(
                request_frame,
                response,
                result,
                handled_device_address,
            )
            payload = {
                "ok": result is not None and result.crc_ok,
                "event": "frame",
                "port": handle.port,
                "link": "uart4" if uart4 else "rs485",
                **route_payload,
                "request": frame_to_hex(request_frame),
                "response": frame_to_hex(response),
                "message": result.message if result is not None else "",
                "crc_ok": result.crc_ok if result is not None else False,
                "ignored": result.ignored if result is not None else True,
                "exception_code": result.exception_code if result is not None else None,
            }
            if device_bus is None:
                payload["state"] = build_status_payload(simulator)
            elif route_payload["device_address"] in device_bus.slave_addresses:
                payload["state"] = build_status_payload(
                    device_bus.get_device(route_payload["device_address"])
                )
            else:
                payload["fleet"] = build_fleet_payload(device_bus)
        send_serial_reply(handle, response, uart4, options.response_delay_ms / 1000.0)
        emit_cli_payload(payload)

    def run_link(handle: Any, uart4: bool) -> None:
        try:
            receive_serial_frames(
                handle, uart4, stop_event, lambda frame: process_request(frame, handle, uart4), deadline,
            )
        except Exception as exc:
            if not stop_event.is_set():
                failed.set()
                stop_event.set()
                with state_lock:
                    context = build_cli_context_payload(simulator, device_bus)
                emit_cli_payload(
                    {"ok": False, "event": "serial_error", "port": handle.port, "error": str(exc), **context}
                )

    workers = [threading.Thread(target=run_link, args=link) for link in handles[1:]]
    for worker in workers:
        worker.start()
    try:
        run_link(*handles[0])
    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        for worker in workers:
            worker.join()
        for handle, _ in handles:
            try:
                handle.close()
            except Exception:
                pass
    exit_code = int(failed.is_set())

    closed_payload = {
        "ok": exit_code == 0,
        "event": "serial_closed",
        "port": options.port,
    }
    with state_lock:
        if device_bus is None:
            closed_payload["state"] = build_status_payload(simulator)
        else:
            closed_payload["fleet"] = build_fleet_payload(device_bus)
    emit_cli_payload(closed_payload)
    return exit_code


def build_cli_parser() -> argparse.ArgumentParser:
    """Build the public BF006 automation command grammar."""
    parser = argparse.ArgumentParser(description="BF006 控制器模拟器，支持 4G V1.3 和 RS485")
    subparsers = parser.add_subparsers(dest="command", required=True)
    status_parser = subparsers.add_parser("status", help="Show the current simulated state")
    status_parser.add_argument(
        "--addresses",
        help="Show a newly created multi-device bus, for example 1,2,3 or 1-3",
    )
    subparsers.add_parser("reset-defaults", help="Restore the complete GUI default state")
    parameter_parser = subparsers.add_parser("parameter", help="Inspect BF006 P parameters")
    parameter_subparsers = parameter_parser.add_subparsers(dest="parameter_command", required=True)
    parameter_get_parser = parameter_subparsers.add_parser("get", help="Read one parameter")
    parameter_get_parser.add_argument("address", help="P-18, 0x0012, or 0012")
    parameter_set_parser = parameter_subparsers.add_parser("set", help="Write one parameter")
    parameter_set_parser.add_argument("address", help="P-18, 0x0012, or 0012")
    parameter_set_parser.add_argument("value", help="Decimal or 0x-prefixed raw register value")
    parameter_list_parser = parameter_subparsers.add_parser("list", help="List BF006 parameters")
    parameter_list_parser.add_argument(
        "--writable-only",
        action="store_true",
        help="Only include writable parameters",
    )
    monitor_parser = subparsers.add_parser("monitor", help="Inspect BF006 monitor registers")
    monitor_subparsers = monitor_parser.add_subparsers(dest="monitor_command", required=True)
    monitor_subparsers.add_parser("list", help="List all monitor register values")
    monitor_get_parser = monitor_subparsers.add_parser("get", help="Read one monitor register")
    monitor_get_parser.add_argument("address", help="Hexadecimal address such as 2116 or 0x2116")
    runtime_parser = subparsers.add_parser("runtime", help="Set the complete BF006 operating state")
    runtime_subparsers = runtime_parser.add_subparsers(dest="runtime_command", required=True)
    runtime_set_parser = runtime_subparsers.add_parser("set", help="Apply one atomic runtime state")
    runtime_set_parser.add_argument("--mode", required=True, choices=("manual", "auto", "timer"))
    runtime_set_parser.add_argument("--pump-a", required=True, choices=("on", "off"))
    runtime_set_parser.add_argument("--pump-b", required=True, choices=("on", "off"))
    runtime_set_parser.add_argument("--pump-mode", required=True, choices=("a-main", "b-main", "alternating"))
    runtime_set_parser.add_argument("--upper-level", required=True, choices=("low", "middle", "high"))
    runtime_set_parser.add_argument("--lower-level", required=True, choices=("low", "middle", "high"))
    runtime_set_parser.add_argument("--add-pump", required=True, choices=("on", "off"))
    measurement_parser = subparsers.add_parser("measurement", help="Set measurement registers atomically")
    measurement_subparsers = measurement_parser.add_subparsers(dest="measurement_command", required=True)
    measurement_set_parser = measurement_subparsers.add_parser("set", help="Apply engineering measurement values")
    measurement_set_parser.add_argument("--voltage", required=True)
    measurement_set_parser.add_argument("--current-a", required=True)
    measurement_set_parser.add_argument("--current-b", required=True)
    for option in ("--current-ab", "--current-ac", "--current-bb", "--current-bc"):
        measurement_set_parser.add_argument(option, help="独立三相电流采样，单位A")
    measurement_set_parser.add_argument("--power", required=True)
    measurement_set_parser.add_argument("--legacy-sensor", required=True)
    measurement_set_parser.add_argument("--device-model", required=True)
    measurement_set_parser.add_argument("--version", required=True)
    measurement_set_parser.add_argument("--total-run", required=True)
    measurement_set_parser.add_argument("--total-power", required=True)
    device_parser = subparsers.add_parser("device", help="Set live BF006 identity registers")
    device_subparsers = device_parser.add_subparsers(dest="device_command", required=True)
    device_electrical_parser = device_subparsers.add_parser("set-electrical", help="设定 V1.3 相数和额定电压")
    device_electrical_parser.add_argument("phases")
    device_electrical_parser.add_argument("rated_voltage")
    device_model_parser = device_subparsers.add_parser("set-model", help="Set raw 210DH DeviceModel")
    device_model_parser.add_argument("value", help="Decimal or 0x-prefixed U16 model value")
    device_version_parser = device_subparsers.add_parser("set-version", help="Set raw 210EH Version")
    device_version_parser.add_argument("value", help="Decimal or 0x-prefixed U16 protocol version")
    liquid_parser = subparsers.add_parser("liquid", help="Set protocol-201 liquid and sensor state")
    liquid_subparsers = liquid_parser.add_subparsers(dest="liquid_command", required=True)
    liquid_set_parser = liquid_subparsers.add_parser("set", help="Apply one atomic liquid state")
    liquid_set_parser.add_argument("--sensor-type", required=True, choices=("0", "1", "2", "3", "4"))
    liquid_set_parser.add_argument("--value", required=True, help="Engineering value with up to two decimals")
    liquid_set_parser.add_argument("--target", required=True, choices=("upper", "lower"))
    liquid_set_parser.add_argument("--level", required=True, choices=("low", "middle", "high"))
    liquid_set_parser.add_argument("--upper-segments", required=True, help="Six-bit mask, decimal or 0x-prefixed")
    liquid_set_parser.add_argument("--lower-segments", required=True, help="Six-bit mask, decimal or 0x-prefixed")
    liquid_set_parser.add_argument("--status", required=True, help="2116H manual bit mask, bit0-bit4")
    control_parser = subparsers.add_parser("control", help="Execute a BF006 2000H-2007H action")
    control_parser.add_argument(
        "action",
        choices=(
            "pump-a-start",
            "pump-a-stop",
            "pump-b-start",
            "pump-b-stop",
            "all-start",
            "all-stop",
            "release",
            "local-stop",
            "manual",
            "auto",
            "timer",
            "a-main",
            "b-main",
            "alternating",
            "clear-faults",
            "clear-history",
            "save-parameters",
        ),
    )
    fault_parser = subparsers.add_parser("fault", help="Inject BF006 firmware faults")
    fault_subparsers = fault_parser.add_subparsers(dest="fault_command", required=True)
    fault_set_parser = fault_subparsers.add_parser("set", help="Inject a zero-based firmware fault enum")
    fault_set_parser.add_argument("enum_code", help="Decimal or 0x-prefixed enum code 0..16")
    fault_set_parser.add_argument(
        "--no-history",
        action="store_true",
        help="Do not add the injected fault to the controller's ten history records",
    )
    fault_subparsers.add_parser("clear", help="Clear current faults and all live fault bits")
    fault_subparsers.add_parser("list", help="List all injectable zero-based firmware fault enums")
    fault_bits_parser = fault_subparsers.add_parser("bits", help="Set manual 2102H and 210CH pump fault bits")
    fault_bits_parser.add_argument("--summary", required=True)
    fault_bits_parser.add_argument("--pump-a", required=True, choices=("on", "off"))
    fault_bits_parser.add_argument("--pump-b", required=True, choices=("on", "off"))
    history_parser = subparsers.add_parser("history", help="Inspect or edit the ten BF006 history records")
    history_subparsers = history_parser.add_subparsers(dest="history_command", required=True)
    history_get_parser = history_subparsers.add_parser("get", help="Read one history record")
    history_get_parser.add_argument("index", type=int, choices=range(1, 11))
    history_set_parser = history_subparsers.add_parser("set", help="Write one history record")
    history_set_parser.add_argument("index", type=int, choices=range(1, 11))
    history_set_parser.add_argument("--fault", required=True)
    history_set_parser.add_argument("--voltage", required=True)
    history_set_parser.add_argument("--current", required=True)
    history_subparsers.add_parser("list", help="List all ten history records")
    history_subparsers.add_parser("clear", help="Clear all ten history records")
    frame_parser = subparsers.add_parser("frame", help="Process one complete request offline")
    frame_parser.add_argument("--uart4", action="store_true", help="Decode a 4G UART4 V1.3 frame")
    frame_parser.add_argument(
        "--addresses",
        help="Route the raw frame through a multi-device bus, for example 1-3",
    )
    frame_parser.add_argument("hex_frame", nargs="+", help="Hexadecimal bytes including CRC")
    session_parser = subparsers.add_parser("session", help="Read persistent commands from standard input")
    session_parser.add_argument(
        "--addresses",
        help="Create multiple persistent slaves, for example 1,2,3 or 1-3",
    )
    subparsers.add_parser("ports", help="List serial ports visible to pyserial or Windows")
    serve_parser = subparsers.add_parser("serve", help="Expose the simulator on a serial port without GUI")
    serve_parser.add_argument("--port", required=True, help="Serial port such as COM4")
    serve_parser.add_argument("--uart4", action="store_true", help="Use --port as 4G UART4 V1.3 at 9600 8N1")
    serve_parser.add_argument("--uart4-port", help="Add an independent 4G UART4 port alongside RS485")
    serve_parser.add_argument(
        "--baudrate",
        type=int,
        default=None,
        choices=(1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200),
    )
    serve_parser.add_argument("--parity", default="N", choices=("N", "E", "O"))
    serve_parser.add_argument("--stopbits", type=int, default=1, choices=(1, 2))
    serve_address_group = serve_parser.add_mutually_exclusive_group()
    serve_address_group.add_argument("--address", type=int, help="Override the initialized slave address")
    serve_address_group.add_argument(
        "--addresses",
        help="Create multiple slaves on the same serial port, for example 1,2,3 or 1-3",
    )
    serve_parser.add_argument("--response-delay-ms", type=int, default=0)
    serve_parser.add_argument("--duration", type=float, help="Optional automatic shutdown time in seconds")
    serve_parser.add_argument(
        "--control-stdin",
        action="store_true",
        help="Accept live state commands from stdin and stop on exit, quit, or EOF",
    )
    serve_parser.add_argument(
        "--init-command",
        dest="init_commands",
        action="append",
        default=[],
        help="Repeat to initialize persistent state before opening the serial port",
    )
    batch_parser = subparsers.add_parser("batch", help="Run commands against one persistent simulated state")
    batch_parser.add_argument(
        "--addresses",
        help="Create multiple slaves on one bus, for example 1,2,3 or 1-3",
    )
    batch_parser.add_argument(
        "--command",
        dest="batch_commands",
        action="append",
        required=True,
        help="Repeat for each stateful command",
    )
    return parser


def run_cli(arguments: list[str] | None = None) -> int:
    """Execute one BF006 automation command and print machine-readable JSON."""
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8")

    options = build_cli_parser().parse_args(arguments)
    simulator = Bf006ModbusRtuSimulator(DEFAULT_SLAVE_ADDRESS)
    if options.command == "status":
        if options.addresses:
            try:
                device_bus = Bf006DeviceBus(parse_slave_addresses(options.addresses))
            except ValueError as exc:
                return emit_cli_payload({"ok": False, "command": "status", "error": str(exc)})
            return emit_cli_payload(
                {"ok": True, "command": "status", "fleet": build_fleet_payload(device_bus)}
            )
        return emit_cli_payload(execute_cli_tokens(simulator, ["status"]))
    if options.command == "reset-defaults":
        return emit_cli_payload(execute_cli_tokens(simulator, ["reset-defaults"]))
    if options.command == "parameter" and options.parameter_command in ("get", "set"):
        tokens = ["parameter", options.parameter_command, options.address]
        if options.parameter_command == "set":
            tokens.append(options.value)
        return emit_cli_payload(execute_cli_tokens(simulator, tokens))
    if options.command == "parameter" and options.parameter_command == "list":
        return emit_cli_payload(
            execute_cli_tokens(
                simulator,
                ["parameter", "list", *(["--writable-only"] if options.writable_only else [])],
            )
        )
    if options.command == "monitor" and options.monitor_command == "list":
        return emit_cli_payload(execute_cli_tokens(simulator, ["monitor", "list"]))
    if options.command == "monitor" and options.monitor_command == "get":
        return emit_cli_payload(execute_cli_tokens(simulator, ["monitor", "get", options.address]))
    if options.command == "runtime" and options.runtime_command == "set":
        return emit_cli_payload(
            execute_cli_tokens(
                simulator,
                [
                    "runtime",
                    "set",
                    "--mode",
                    options.mode,
                    "--pump-a",
                    options.pump_a,
                    "--pump-b",
                    options.pump_b,
                    "--pump-mode",
                    options.pump_mode,
                    "--upper-level",
                    options.upper_level,
                    "--lower-level",
                    options.lower_level,
                    "--add-pump",
                    options.add_pump,
                ],
            )
        )
    if options.command == "measurement" and options.measurement_command == "set":
        extra = []
        for name in ("current_ab", "current_ac", "current_bb", "current_bc"):
            value = getattr(options, name)
            if value is not None:
                extra.extend(("--" + name.replace("_", "-"), value))
        return emit_cli_payload(
            execute_cli_tokens(
                simulator,
                [
                    "measurement",
                    "set",
                    "--voltage",
                    options.voltage,
                    "--current-a",
                    options.current_a,
                    "--current-b",
                    options.current_b,
                    "--power",
                    options.power,
                    "--legacy-sensor",
                    options.legacy_sensor,
                    "--device-model",
                    options.device_model,
                    "--version",
                    options.version,
                    "--total-run",
                    options.total_run,
                    "--total-power",
                    options.total_power,
                    *extra,
                ],
            )
        )
    if options.command == "device" and options.device_command == "set-electrical":
        return emit_cli_payload(
            execute_cli_tokens(simulator, ["device", "set-electrical", options.phases, options.rated_voltage])
        )
    if options.command == "device" and options.device_command == "set-model":
        return emit_cli_payload(execute_cli_tokens(simulator, ["device", "set-model", options.value]))
    if options.command == "device" and options.device_command == "set-version":
        return emit_cli_payload(execute_cli_tokens(simulator, ["device", "set-version", options.value]))
    if options.command == "liquid" and options.liquid_command == "set":
        return emit_cli_payload(
            execute_cli_tokens(
                simulator,
                [
                    "liquid",
                    "set",
                    "--sensor-type",
                    options.sensor_type,
                    "--value",
                    options.value,
                    "--target",
                    options.target,
                    "--level",
                    options.level,
                    "--upper-segments",
                    options.upper_segments,
                    "--lower-segments",
                    options.lower_segments,
                    "--status",
                    options.status,
                ],
            )
        )
    if options.command == "control":
        return emit_cli_payload(execute_cli_tokens(simulator, ["control", options.action]))
    if options.command == "fault" and options.fault_command == "set":
        tokens = ["fault", "set", options.enum_code]
        if options.no_history:
            tokens.append("--no-history")
        return emit_cli_payload(execute_cli_tokens(simulator, tokens))
    if options.command == "fault" and options.fault_command == "clear":
        return emit_cli_payload(execute_cli_tokens(simulator, ["fault", "clear"]))
    if options.command == "fault" and options.fault_command == "list":
        return emit_cli_payload(execute_cli_tokens(simulator, ["fault", "list"]))
    if options.command == "fault" and options.fault_command == "bits":
        return emit_cli_payload(
            execute_cli_tokens(
                simulator,
                [
                    "fault",
                    "bits",
                    "--summary",
                    options.summary,
                    "--pump-a",
                    options.pump_a,
                    "--pump-b",
                    options.pump_b,
                ],
            )
        )
    if options.command == "history" and options.history_command == "get":
        return emit_cli_payload(execute_cli_tokens(simulator, ["history", "get", str(options.index)]))
    if options.command == "history" and options.history_command == "set":
        return emit_cli_payload(
            execute_cli_tokens(
                simulator,
                [
                    "history",
                    "set",
                    str(options.index),
                    "--fault",
                    options.fault,
                    "--voltage",
                    options.voltage,
                    "--current",
                    options.current,
                ],
            )
        )
    if options.command == "history" and options.history_command == "list":
        return emit_cli_payload(execute_cli_tokens(simulator, ["history", "list"]))
    if options.command == "history" and options.history_command == "clear":
        return emit_cli_payload(execute_cli_tokens(simulator, ["history", "clear"]))
    if options.command == "frame":
        if options.addresses:
            try:
                device_bus = Bf006DeviceBus(parse_slave_addresses(options.addresses))
                request_frame = bytes.fromhex(" ".join(options.hex_frame))
                response = device_bus.handle_frame(request_frame, uart4=options.uart4)
                result = device_bus.last_result
            except ValueError as exc:
                return emit_cli_payload({"ok": False, "command": "frame", "error": str(exc)})
            return emit_cli_payload(
                {
                    "ok": result is not None and result.crc_ok,
                    "command": "frame",
                    **build_frame_route_payload(
                        request_frame,
                        response,
                        result,
                        device_bus.last_device_address,
                    ),
                    "request": frame_to_hex(request_frame),
                    "response": frame_to_hex(response),
                    "protocol": {
                        "message": result.message if result is not None else "",
                        "crc_ok": result.crc_ok if result is not None else False,
                        "ignored": result.ignored if result is not None else True,
                        "exception_code": result.exception_code if result is not None else None,
                    },
                    "fleet": build_fleet_payload(device_bus),
                }
            )
        prefix = ["frame", "--uart4"] if options.uart4 else ["frame"]
        return emit_cli_payload(execute_cli_tokens(simulator, [*prefix, *options.hex_frame]))
    if options.command == "session":
        if not options.addresses:
            return run_cli_session(simulator)
        try:
            device_bus = Bf006DeviceBus(parse_slave_addresses(options.addresses))
        except ValueError as exc:
            return emit_cli_payload({"ok": False, "event": "session_error", "error": str(exc)})
        return run_cli_session(simulator, device_bus)
    if options.command == "ports":
        serial_module, warning = import_pyserial()
        return emit_cli_payload(
            {
                "ok": True,
                "command": "ports",
                "serial_available": serial_module is not None,
                "ports": list_serial_ports(serial_module),
                "warning": warning,
            }
        )
    if options.command == "serve":
        device_bus = None
        if options.addresses:
            try:
                device_bus = Bf006DeviceBus(parse_slave_addresses(options.addresses))
            except ValueError as exc:
                return emit_cli_payload(
                    {"ok": False, "event": "initialization_error", "error": str(exc)}
                )
        initialization_results = []
        for command_text in options.init_commands:
            try:
                tokens = shlex.split(command_text)
            except ValueError as exc:
                result = {
                    "ok": False,
                    "command": command_text,
                    "error": str(exc),
                }
                if device_bus is None:
                    result["state"] = build_status_payload(simulator)
                else:
                    result["fleet"] = build_fleet_payload(device_bus)
            else:
                result = (
                    execute_bus_cli_tokens(device_bus, tokens)
                    if device_bus is not None
                    else execute_cli_tokens(simulator, tokens)
                )
            initialization_results.append(result)
            if not result["ok"]:
                return emit_cli_payload(
                    {
                        "ok": False,
                        "event": "initialization_error",
                        "result": result,
                        **(
                            {"state": build_status_payload(simulator)}
                            if device_bus is None
                            else {"fleet": build_fleet_payload(device_bus)}
                        ),
                    }
                )
        options.initialization_results = initialization_results
        return run_serial_server(options, simulator, device_bus)
    if options.command == "batch":
        device_bus = None
        if options.addresses:
            try:
                device_bus = Bf006DeviceBus(parse_slave_addresses(options.addresses))
            except ValueError as exc:
                return emit_cli_payload(
                    {"ok": False, "command": "batch", "error": str(exc)}
                )
        results = []
        for command_text in options.batch_commands:
            try:
                tokens = shlex.split(command_text)
            except ValueError as exc:
                results.append(
                    {
                        "ok": False,
                        "command": command_text,
                        "error": str(exc),
                        **build_cli_context_payload(simulator, device_bus),
                    }
                )
                break
            result = (
                execute_bus_cli_tokens(device_bus, tokens)
                if device_bus is not None
                else execute_cli_tokens(simulator, tokens)
            )
            results.append(result)
            if not result["ok"]:
                break
        payload = {
            "ok": all(result["ok"] for result in results),
            "command": "batch",
            "results": results,
        }
        if device_bus is None:
            payload["state"] = build_status_payload(simulator)
        else:
            payload["fleet"] = build_fleet_payload(device_bus)
        return emit_cli_payload(payload)
    return 2


def main() -> int:
    """Preserve GUI startup unless the explicit automation switch is present."""
    if len(sys.argv) > 1 and sys.argv[1] == "--cli":
        return run_cli(sys.argv[2:])
    run_gui()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

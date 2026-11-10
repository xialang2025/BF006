# -*- coding: utf-8 -*-
"""
BF006 新版 Modbus RTU 报文解析工具。

运行后粘贴十六进制报文，点击“解析报文”即可查看 CRC、功能码、
地址、数据和 BF006 新版协议含义。
"""

import re
import tkinter as tk
from tkinter import messagebox
from tkinter.scrolledtext import ScrolledText


FUNC_NAMES = {
    0x03: "03H 读取保持寄存器",
    0x06: "06H 写单个寄存器",
    0x10: "10H 写多个寄存器",
    0x41: "41H BF006 私有地址设置",
}

EXCEPTION_NAMES = {
    0x01: "非法功能或地址",
    0x02: "非法寄存器",
    0x03: "非法数据",
}

REG_UNITS = {
    0x0000: "V",
    0x0001: "V",
    0x0002: "0.1A",
    0x0003: "0.1A",
    0x0004: "0.1A",
    0x0005: "0.1A",
    0x0006: "min",
    0x0007: "min",
    0x0008: "s",
    0x0009: "s",
    0x000A: "h",
    0x000E: "工程量",
    0x000F: "工程量",
    0x0010: "工程量",
    0x0011: "工程量",
    0x0014: "工程量",
    0x2103: "V",
    0x2104: "0.1A",
    0x2105: "0.1A",
    0x2106: "W",
    0x2107: "工程量",
    0x2108: "工程量",
    0x2109: "工程量",
}

REG_RANGES = {
    0x0000: "按电压规格",
    0x0001: "按电压规格",
    0x0002: "0-设备规格上限",
    0x0003: "0-设备规格上限",
    0x0004: "0-设备规格上限",
    0x0005: "0-设备规格上限",
    0x0006: "0-9999",
    0x0007: "0-9999",
    0x0008: "0-9999",
    0x0009: "0-9999",
    0x000A: "0-999",
    0x000B: "1-254",
    0x000C: "0/1/2",
    0x000D: "0/1",
    0x000E: "0-9999",
    0x000F: "0-传感器量程",
    0x0010: "0-传感器量程",
    0x0011: "0-传感器量程",
    0x0012: "0-4",
    0x0013: "0-4",
    0x0014: "0-传感器量程",
    0x2000: "bit0-bit1 动作，bit4-bit5 对象",
    0x2003: "0/1/2",
    0x2004: "0/1/2",
    0x2005: "0001H",
    0x2006: "0000H",
    0x2007: "5AA5H",
}

PARAM_REGS = {
    0x0000: ("P-00 / OVP_Value", "高电压保护"),
    0x0001: ("P-01 / UVP_Value", "低电压保护"),
    0x0002: ("P-02 / Set_STANDBY_Current1", "A泵缺水保护"),
    0x0003: ("P-03 / Set_OVERLOAD_Current1", "A泵过流保护"),
    0x0004: ("P-04 / Set_STANDBY_Current2", "B泵缺水保护；一控二 B 泵设定"),
    0x0005: ("P-05 / Set_OVERLOAD_Current2", "B泵过流保护；一控二 B 泵设定"),
    0x0006: ("P-06 / AUTO_Timing_Run", "循环定时运行时间"),
    0x0007: ("P-07 / AUTO_Timing_Stop", "循环定时停机时间"),
    0x0008: ("P-08 / Timer_Standby_Restart", "缺水保护自动启动时间，缺水后按时间自动恢复"),
    0x0009: ("P-09 / Timer_Unload_Restart", "过载重启时间"),
    0x000A: ("P-10 / AUTO_FANGXIU", "防锈设置自动启动时间，默认 99 小时启动一次"),
    0x000B: ("P-11 / Dev_Address", "485 通讯地址"),
    0x000C: ("P-12 / Pump_AB_Mode", "一控二双泵模式切换：0 A主，1 B主，2 轮换"),
    0x000D: ("P-13 / Sensor_Mode", "传感器选择模式：0 供水模式，1 排污模式"),
    0x000E: ("P-14 / Sensor_Rang", "传感器量程，根据传感器大小设定"),
    0x000F: ("P-15 / Sensor_STOP", "传感器停止水位"),
    0x0010: ("P-16 / Sensor_ON1", "传感器启动水位"),
    0x0011: ("P-17 / Sensor_ON2", "传感器双泵同启水位"),
    0x0012: ("P-18 / Auto_Mode", "传感器选择：0 液位探头浮球，1 电接点压力表，2 液位传感器，3 压力传感器"),
    0x0013: ("P-19 / fault_mask", "高速深井泵变频器配套选择"),
    0x0014: ("P-20 / Sensor_overFlow", "传感器溢出报警，修改液位数值报警"),
}

CTRL_REGS = {
    0x2000: ("Pump_Control", "A/B泵启停控制"),
    0x2003: ("Run_Mode_Command", "运行模式命令：0 手动，1 自动，2 循环定时"),
    0x2004: ("Pump_AB_Mode_Command", "主备模式命令：0 A主B备，1 B主A备，2 双泵轮换"),
    0x2005: ("Fault_Reset", "0001H 清除当前可恢复故障"),
    0x2006: ("History_Clear", "0000H 清除 10 条历史故障记录"),
    0x2007: ("Save_Parameter", "5AA5H 保存参数到非易失存储"),
}

MON_REGS = {
    0x2100: ("Fault_Code", "当前最高优先级故障代码，0 表示无故障"),
    0x2101: ("Run_Status", "运行状态位"),
    0x2102: ("Fault_Status_Bits", "故障汇总位"),
    0x2103: ("LineVoltage", "线路电压，单位 V"),
    0x2104: ("PumpA_Current", "A泵电流"),
    0x2105: ("PumpB_Current", "B泵电流"),
    0x2106: ("Power", "功率，当前固件返回 0"),
    0x2107: ("Sensor_ON2", "双泵启动阈值"),
    0x2108: ("Sensor_STOP", "停止阈值"),
    0x2109: ("SensorOut", "当前液位/压力/传感器工程量"),
    0x210A: ("UpWater_Level", "上水池水位：0低/空，1中，2高/满"),
    0x210B: ("BottomWater_Level", "下水池水位：0低/缺水，1中，2高/满"),
    0x210C: ("Pump_State", "bit0 A运行，bit1 B运行，bit8 A故障，bit9 B故障"),
    0x210D: ("DeviceModel", "高字节设备型号，低字节规格档位"),
    0x210E: ("Version", "固件版本号"),
}

FAULT_CODES = {
    0: "无故障",
    1: "ERR_UVP 欠压",
    2: "ERR_OVP 过压",
    3: "ERR_OVERLOAD1 A泵过载",
    4: "ERR_STANDBY1 A泵空载/欠载",
    5: "ERR_LOSE_A1 A泵A相缺相",
    6: "ERR_LOSE_B1 A泵B相缺相",
    7: "ERR_LOSE_C1 A泵C相缺相",
    8: "ERR_OVERLOAD2 B泵过载",
    9: "ERR_STANDBY2 B泵空载/欠载",
    10: "ERR_LOSE_A2 B泵A相缺相",
    11: "ERR_LOSE_B2 B泵B相缺相",
    12: "ERR_LOSE_C2 B泵C相缺相",
    13: "ERR_SENSOR 传感器异常",
    14: "ERR_NOLOAD_A A泵无负载/空转",
    15: "ERR_NOLOAD_B B泵无负载/空转",
    16: "ERR_LOCK_A A泵锁定",
    17: "ERR_LOCK_B B泵锁定",
}


def crc16_modbus(data):
    """计算标准 Modbus RTU CRC16。"""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def to_hex(data):
    """将字节列表格式化为十六进制字符串。"""
    return " ".join(f"{byte:02X}" for byte in data)


def get_u16(data, index):
    """按高字节在前读取 U16。"""
    return (data[index] << 8) | data[index + 1]


def parse_hex_text(text):
    """从输入文本中提取十六进制字节。"""
    normalized = text.replace("`", " ").replace(",", " ").replace("，", " ")
    normalized = normalized.replace("0x", " ").replace("0X", " ")
    tokens = re.findall(r"(?<![0-9A-Fa-f])([0-9A-Fa-f]{1,2})(?![0-9A-Fa-fHh])", normalized)
    if not tokens:
        compact = re.sub(r"[^0-9A-Fa-f]", "", text)
        if len(compact) % 2 != 0:
            raise ValueError("连续十六进制字符数量不是偶数，无法按字节解析。")
        tokens = [compact[i:i + 2] for i in range(0, len(compact), 2)]
    return [int(token, 16) for token in tokens]


def reg_info(addr):
    """返回寄存器分区和名称说明。"""
    if addr in PARAM_REGS:
        name, desc = PARAM_REGS[addr]
        return "参数区", name, desc
    if addr in CTRL_REGS:
        name, desc = CTRL_REGS[addr]
        return "控制命令区", name, desc
    if addr in MON_REGS:
        name, desc = MON_REGS[addr]
        return "监视状态区", name, desc
    if 0x2200 <= addr <= 0x221D:
        index = (addr - 0x2200) // 3 + 1
        field = (addr - 0x2200) % 3
        fields = ["故障类型", "故障电压", "故障电流"]
        return "历史故障区", f"Record{index}_{fields[field]}", f"历史故障第 {index} 条：{fields[field]}"
    return "未知区", "未知寄存器", "新版协议未定义该寄存器"


def reg_unit(addr):
    """返回寄存器单位。"""
    if 0x2200 <= addr <= 0x221D:
        field = (addr - 0x2200) % 3
        if field == 1:
            return "V"
        if field == 2:
            return "0.1A"
    return REG_UNITS.get(addr, "无")


def reg_range(addr):
    """返回寄存器允许范围。"""
    if 0x2200 <= addr <= 0x221D:
        field = (addr - 0x2200) % 3
        if field == 0:
            return "故障代码"
        return "运行记录值"
    return REG_RANGES.get(addr, "按协议/固件实时值")


def format_value(addr, value):
    """按寄存器单位格式化数值。"""
    unit = reg_unit(addr)
    if unit == "0.1A":
        return f"{value:04X}H / {value} / {value / 10:.1f} A"
    if unit == "V":
        return f"{value:04X}H / {value} V"
    if unit in ("min", "s", "W"):
        return f"{value:04X}H / {value} {unit}"
    if unit == "h":
        return f"{value:04X}H / {value} h"
    if unit == "工程量":
        return f"{value:04X}H / {value}（传感器工程量，具体单位由传感器类型决定）"
    return f"{value:04X}H / {value}"


def append_reg_detail(lines, addr, value=None, prefix="  "):
    """输出寄存器名称、单位、范围和可选数据值。"""
    area, name, desc = reg_info(addr)
    lines.append(f"{prefix}地址：{addr:04X}H")
    lines.append(f"{prefix}分区：{area}")
    lines.append(f"{prefix}名称：{name}")
    lines.append(f"{prefix}说明：{desc}")
    lines.append(f"{prefix}单位：{reg_unit(addr)}")
    lines.append(f"{prefix}范围：{reg_range(addr)}")
    if value is not None:
        lines.append(f"{prefix}数据：{format_value(addr, value)}")
        meaning = decode_value(addr, value)
        if meaning:
            lines.append(f"{prefix}含义：{meaning}")


def append_blank_between_items(lines, index):
    """在多个寄存器块之间增加空行。"""
    if index > 0:
        lines.append("")


def decode_pump_control(value):
    """解析 2000H 泵控制命令。"""
    action = value & 0x0003
    target = (value >> 4) & 0x0003
    action_text = {
        0: "无动作",
        1: "停止",
        2: "启动",
    }.get(action, "非法动作")
    target_text = {
        1: "A泵",
        2: "B泵",
        3: "A/B双泵",
    }.get(target, "非法对象")
    if action == 2:
        condition = "启动命令要求设备处于手动模式且目标泵无故障。"
    elif action == 1:
        condition = "停止命令允许直接执行。"
    else:
        condition = "不改变当前泵命令。"
    return f"{action_text} {target_text}；{condition}"


def decode_run_status(value):
    """解析 2101H 运行状态位。"""
    bits = [
        (0, "手动模式"),
        (1, "自动或定时模式"),
        (2, "自动模式"),
        (3, "定时模式"),
        (4, "A泵运行"),
        (5, "B泵运行"),
        (6, "A主B备"),
        (7, "B主A备"),
        (8, "双泵模式或轮换模式"),
        (9, "上水位低"),
        (10, "上水位中"),
        (11, "上水位高"),
        (12, "下水位低"),
        (13, "下水位中"),
        (14, "下水位高"),
        (15, "下水位加泵或溢出相关标志"),
    ]
    enabled = [desc for bit, desc in bits if value & (1 << bit)]
    return "、".join(enabled) if enabled else "无状态位为 1"


def decode_fault_status(value):
    """解析 2102H 故障汇总位。"""
    bits = [
        (0, "欠压"),
        (1, "过压"),
        (2, "过载"),
        (3, "空载/欠载"),
        (5, "缺相"),
        (6, "上水池满水"),
        (8, "下水池缺水"),
    ]
    enabled = [desc for bit, desc in bits if value & (1 << bit)]
    return "、".join(enabled) if enabled else "无故障汇总位为 1"


def decode_value(addr, value):
    """结合寄存器地址解析数据值含义。"""
    if addr == 0x2000:
        return decode_pump_control(value)
    if addr == 0x2003:
        return {0: "手动模式", 1: "自动模式", 2: "循环定时模式"}.get(value, "非法运行模式")
    if addr == 0x2004 or addr == 0x000C:
        return {0: "A主B备", 1: "B主A备", 2: "双泵轮换/同启策略"}.get(value, "非法主备模式")
    if addr == 0x000D:
        return {0: "供水模式", 1: "排污模式"}.get(value, "非法控制模式")
    if addr == 0x0012:
        return {
            0: "液位探头/浮球",
            1: "电接点压力表",
            2: "液位传感器",
            3: "压力传感器",
            4: "0-5V 远传压力",
        }.get(value, "非法传感器类型")
    if addr == 0x0013:
        return {
            0: "默认配套方式",
            1: "配套方式 1",
            2: "配套方式 2",
            3: "配套方式 3",
            4: "配套方式 4",
        }.get(value, "非法配套选择")
    if addr == 0x2005:
        return "清除当前可恢复故障" if value == 1 else "非法清故障命令"
    if addr == 0x2006:
        return "清除 10 条历史故障记录" if value == 0 else "非法清历史命令"
    if addr == 0x2007:
        return "保存参数到非易失存储" if value == 0x5AA5 else "非法保存命令"
    if addr == 0x2100:
        return FAULT_CODES.get(value, "未知故障代码")
    if addr == 0x2101:
        return decode_run_status(value)
    if addr == 0x2102:
        return decode_fault_status(value)
    if addr == 0x210C:
        parts = []
        if value & 0x0001:
            parts.append("A泵运行")
        if value & 0x0002:
            parts.append("B泵运行")
        if value & 0x0100:
            parts.append("A泵有故障")
        if value & 0x0200:
            parts.append("B泵有故障")
        return "、".join(parts) if parts else "A/B泵停止且无泵故障标志"
    if 0x2200 <= addr <= 0x221D and (addr - 0x2200) % 3 == 0:
        return FAULT_CODES.get(value, "未知历史故障类型")
    return ""


def analyze_crc(data, lines):
    """输出 CRC 校验结果。"""
    recv_crc = data[-2] | (data[-1] << 8)
    calc_crc = crc16_modbus(data[:-2])
    lines.append("【CRC 校验】")
    lines.append(f"接收 CRC：{recv_crc:04X}H，字节顺序：{data[-2]:02X} {data[-1]:02X}")
    lines.append(f"计算 CRC：{calc_crc:04X}H，字节顺序：{calc_crc & 0xFF:02X} {calc_crc >> 8:02X}")
    lines.append("结论：CRC 正确" if recv_crc == calc_crc else "结论：CRC 错误，固件会丢弃该帧")
    lines.append("")


def analyze_exception(data, lines):
    """解析异常响应帧。"""
    func = data[1] & 0x7F
    err = data[2] if len(data) >= 5 else None
    lines.append("【异常响应】")
    lines.append(f"原功能码：{func:02X}H，异常功能码：{data[1]:02X}H")
    if err is not None:
        lines.append(f"异常码：{err:02X}H，含义：{EXCEPTION_NAMES.get(err, '未知异常码')}")
    return lines


def analyze_read_request(data, lines, response_start=None):
    """解析 03H 请求或响应。"""
    if len(data) == 8:
        start = get_u16(data, 2)
        qty = get_u16(data, 4)
        lines.append("【03H 读取请求】")
        lines.append(f"起始地址：{start:04X}H")
        lines.append(f"读取数量：{qty} word")
        if data[0] == 0xFF and start == 0xC351 and qty == 1:
            lines.append("含义：BF006 私有广播读设备地址。")
        elif qty == 0:
            lines.append("检查：数量为 0，固件应返回异常 03H。")
        elif qty > 8:
            lines.append("检查：数量超过 8，固件应返回异常 03H。")
        else:
            for offset in range(qty):
                append_blank_between_items(lines, offset)
                addr = start + offset
                lines.append(f"Word{offset + 1} 对应寄存器：")
                append_reg_detail(lines, addr, prefix="  ")
        return

    if len(data) >= 5 and len(data) == data[2] + 5:
        byte_count = data[2]
        lines.append("【03H 读取响应】")
        lines.append(f"返回字节数：{byte_count}")
        if byte_count % 2 != 0:
            lines.append("检查：字节数不是偶数，响应数据不是完整 word。")
            return
        for index in range(byte_count // 2):
            append_blank_between_items(lines, index)
            value = get_u16(data, 3 + index * 2)
            if response_start is None:
                lines.append(f"  Word{index + 1}：{value:04X}H / {value}")
            else:
                addr = response_start + index
                lines.append(f"Word{index + 1} 对应寄存器：")
                append_reg_detail(lines, addr, value, prefix="  ")
        if byte_count == 2 and data[3] == data[0] and data[4] == 0:
            lines.append("可能含义：广播读设备地址响应。")
        if response_start is None:
            lines.append("提示：03H 响应帧本身不包含起始地址；如需显示寄存器名称和单位，请在窗口中填写“03H响应起始地址”。")
        return

    lines.append("【03H】帧长度不符合读取请求或读取响应格式。")


def analyze_write_one(data, lines):
    """解析 06H 写单寄存器请求或回显。"""
    if len(data) != 8:
        lines.append("【06H】帧长度应为 8 字节。")
        return
    addr = get_u16(data, 2)
    value = get_u16(data, 4)
    area, name, desc = reg_info(addr)
    lines.append("【06H 写单寄存器】")
    append_reg_detail(lines, addr)
    lines.append(f"写入/回显值：{format_value(addr, value)}")
    meaning = decode_value(addr, value)
    if meaning:
        lines.append(f"数据含义：{meaning}")
    if 0x2100 <= addr <= 0x221D:
        lines.append("检查：该区域只读，写入应返回异常 02H。")
    elif 0x2000 <= addr <= 0x2007 and addr not in CTRL_REGS:
        lines.append("检查：控制区未定义地址，当前固件返回异常 03H。")


def analyze_write_more(data, lines):
    """解析 10H 批量写请求或响应。"""
    if len(data) == 8:
        start = get_u16(data, 2)
        qty = get_u16(data, 4)
        lines.append("【10H 批量写响应】")
        lines.append(f"起始地址：{start:04X}H")
        lines.append(f"写入数量：{qty} word")
        return
    if len(data) < 11:
        lines.append("【10H】帧长度不足，无法构成批量写请求。")
        return
    start = get_u16(data, 2)
    qty = get_u16(data, 4)
    byte_count = data[6]
    lines.append("【10H 批量写请求】")
    lines.append(f"起始地址：{start:04X}H")
    lines.append(f"写入数量：{qty} word")
    lines.append(f"数据字节数：{byte_count}")
    if len(data) != byte_count + 9:
        lines.append(f"检查：帧长度不匹配，期望 {byte_count + 9} 字节，实际 {len(data)} 字节。")
        return
    if qty == 0 or qty > 8:
        lines.append("检查：写入数量非法，当前固件返回异常 03H。")
    if byte_count != qty * 2:
        lines.append("检查：字节数不等于写入数量 x 2，当前固件返回异常 03H。")
    for offset in range(qty):
        append_blank_between_items(lines, offset)
        addr = start + offset
        value = get_u16(data, 7 + offset * 2)
        lines.append(f"Word{offset + 1} 写入寄存器：")
        append_reg_detail(lines, addr, value, prefix="  ")
    if not (0x0000 <= start and start + qty - 1 <= 0x0014):
        lines.append("检查：10H 当前仅允许写参数区，跨出参数区会返回异常 02H。")


def analyze_private_41(data, lines):
    """解析 BF006 私有 41H 地址设置命令。"""
    lines.append("【41H 私有地址设置】")
    if len(data) != 8:
        lines.append("帧长度应为 8 字节。")
        return
    fixed = get_u16(data, 2)
    new_addr = data[4]
    baud = data[5]
    lines.append(f"固定寄存器：{fixed:04X}H")
    lines.append(f"新设备地址：{new_addr:02X}H / {new_addr}")
    lines.append(f"通信参数字段：{baud:02X}H，当前固件接收但不改变波特率")
    if data[0] != 0xFF:
        lines.append("检查：该命令按协议应使用 FFH 私有广播地址。")
    if fixed != 0xC351:
        lines.append("检查：固定寄存器错误，应返回异常 02H。")
    if not (1 <= new_addr <= 254):
        lines.append("检查：新地址非法，应返回异常 03H。")


def analyze_frame(data, response_start=None):
    """解析完整 Modbus RTU 帧。"""
    lines = []
    if len(data) < 4:
        return "报文长度不足，至少需要 地址+功能码+CRC 两字节。"

    lines.append("【原始报文】")
    lines.append(to_hex(data))
    lines.append(f"总长度：{len(data)} 字节")
    lines.append("")

    analyze_crc(data, lines)

    adr = data[0]
    func = data[1]
    lines.append("【基础字段】")
    lines.append(f"地址 ADR：{adr:02X}H" + ("（BF006 私有广播地址）" if adr == 0xFF else ""))
    lines.append(f"功能码 FUNC：{func:02X}H")
    lines.append(f"功能说明：{FUNC_NAMES.get(func & 0x7F if func & 0x80 else func, '未知功能码')}")
    lines.append("")

    if func & 0x80:
        analyze_exception(data, lines)
    elif func == 0x03:
        analyze_read_request(data, lines, response_start=response_start)
    elif func == 0x06:
        analyze_write_one(data, lines)
    elif func == 0x10:
        analyze_write_more(data, lines)
    elif func == 0x41:
        analyze_private_41(data, lines)
    else:
        lines.append("【功能码检查】")
        lines.append("新版协议不支持该功能码，固件应返回异常 01H。")

    return "\n".join(lines)


class ModbusParserApp:
    """BF006 Modbus 报文解析 GUI。"""

    def __init__(self, root):
        self.root = root
        self.root.title("BF006 新版 Modbus 报文解析工具")
        self.root.geometry("980x720")

        self.input_text = ScrolledText(root, height=8, font=("Consolas", 12))
        self.input_text.pack(fill=tk.X, padx=10, pady=(10, 5))
        self.input_text.insert(tk.END, "01 03 21 00 00 08 4E 30")

        option_frame = tk.Frame(root)
        option_frame.pack(fill=tk.X, padx=10, pady=(0, 5))
        tk.Label(option_frame, text="03H响应起始地址：").pack(side=tk.LEFT)
        self.response_start_entry = tk.Entry(option_frame, width=12)
        self.response_start_entry.pack(side=tk.LEFT, padx=(4, 10))
        tk.Label(option_frame, text="只解析03H响应帧时填写，例如 2100；解析请求帧可留空。").pack(side=tk.LEFT)

        button_frame = tk.Frame(root)
        button_frame.pack(fill=tk.X, padx=10, pady=5)

        parse_button = tk.Button(button_frame, text="解析报文", width=14, command=self.parse)
        parse_button.pack(side=tk.LEFT, padx=(0, 8))

        clear_button = tk.Button(button_frame, text="清空", width=10, command=self.clear)
        clear_button.pack(side=tk.LEFT, padx=(0, 8))

        sample_button = tk.Button(button_frame, text="填入示例", width=10, command=self.fill_sample)
        sample_button.pack(side=tk.LEFT)

        self.output_text = ScrolledText(root, font=("Microsoft YaHei UI", 11))
        self.output_text.pack(fill=tk.BOTH, expand=True, padx=10, pady=(5, 10))

    def parse(self):
        """解析输入框中的报文。"""
        text = self.input_text.get("1.0", tk.END).strip()
        if not text:
            messagebox.showwarning("提示", "请先粘贴需要解析的十六进制报文。")
            return
        try:
            data = parse_hex_text(text)
            response_start = self.get_response_start()
            result = analyze_frame(data, response_start=response_start)
        except Exception as exc:
            result = f"解析失败：{exc}"
        self.output_text.delete("1.0", tk.END)
        self.output_text.insert(tk.END, result)

    def clear(self):
        """清空输入和输出。"""
        self.input_text.delete("1.0", tk.END)
        self.response_start_entry.delete(0, tk.END)
        self.output_text.delete("1.0", tk.END)

    def fill_sample(self):
        """填入一个常用读取示例。"""
        self.input_text.delete("1.0", tk.END)
        self.input_text.insert(tk.END, "01 03 21 00 00 08 4E 30")
        self.response_start_entry.delete(0, tk.END)
        self.parse()

    def get_response_start(self):
        """读取 03H 响应帧辅助起始地址。"""
        text = self.response_start_entry.get().strip()
        if not text:
            return None
        text = text.replace("H", "").replace("h", "")
        if text.lower().startswith("0x"):
            text = text[2:]
        return int(text, 16)


def main():
    """启动 GUI。"""
    root = tk.Tk()
    app = ModbusParserApp(root)
    app.parse()
    root.mainloop()


if __name__ == "__main__":
    main()

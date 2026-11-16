"""BF4G-014 串口验收。显式指定写入或控制时会操作测试板，运行期间写入本地记录。"""

# 20261116-15 add 串口验收与逐项恢复。
import argparse
from datetime import datetime
import json
import os
from pathlib import Path
import random
import re
import subprocess
import sys
import tempfile
import time

HEADER = bytes.fromhex("43BF0603")
QUIET = 0.012
MAX_FRAME = 97
THRESHOLDS = {15, 16, 17, 20}


class ProtocolError(RuntimeError):
    """应答结构、回显或业务验证失败。"""


class Rejected(ProtocolError):
    """设备返回合法的错误应答。"""

    def __init__(self, data):
        """记录错误码、目标及上下限。"""
        self.code, self.target = data[:2]
        minimum = int.from_bytes(data[2:6], "big")
        maximum = int.from_bytes(data[6:10], "big")
        super().__init__(f"设备拒绝，状态 {self.code:02X}H，目标 {self.target:02X}H，范围 {minimum}～{maximum}")


def crc16(data):
    """按协议计算循环冗余校验，返回整数。"""
    crc = 0xffff
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xa001 if crc & 1 else 0)
    return crc


def require(condition, message):
    """验证一项可观察的协议条件。"""
    if not condition:
        raise ProtocolError(message)


def store_json(path, data):
    """先落盘临时文件，再替换记录，保留中断前最后一份完整状态。"""
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=path.parent, delete=False) as output:
        temporary = Path(output.name)
        try:
            json.dump(data, output, ensure_ascii=False, indent=2)
            output.write("\n")
            output.flush()
            os.fsync(output.fileno())
        except BaseException:
            output.close()
            temporary.unlink(missing_ok=True)
            raise
    try:
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


class Client:
    """按请求关联号匹配一次应答，并记录原始收发字节。"""

    def __init__(self, port, address, traffic, timeout):
        """设置连接、当前地址和接收截止时间。"""
        self.port = port
        self.address = address
        self.traffic = traffic
        self.timeout = timeout
        self.rid = random.randrange(1, 65536)

    def log(self, direction, frame, note=None):
        """按收发事件即时追加十六进制报文。"""
        record = {
            "time": datetime.now().astimezone().isoformat(timespec="milliseconds"),
            "direction": direction,
            "address": self.address,
            "rid": self.rid,
            "hex": frame.hex(" ").upper(),
        }
        if note:
            record["note"] = note
        self.traffic.write(json.dumps(record, ensure_ascii=False) + "\n")
        self.traffic.flush()

    def exchange(self, command, data=b""):
        """发送一次二进制请求，丢开无关应答，超时交由调用方确认结果。"""
        self.rid = self.rid % 65535 + 1
        token = self.rid.to_bytes(2, "big")
        request = bytes((self.address,)) + HEADER + bytes((command,)) + token + bytes((len(data),)) + data
        request += crc16(request).to_bytes(2, "little")
        time.sleep(QUIET)
        self.log("TX", request)
        require(self.port.write(request) == len(request), "串口写入不完整，结果未确认")
        self.port.flush()

        received = bytearray()
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            chunk = self.port.read(MAX_FRAME)
            if chunk:
                received.extend(chunk)
                continue
            if not received:
                continue
            frame = bytes(received)
            received.clear()
            matches = (
                11 <= len(frame) <= MAX_FRAME
                and len(frame) == frame[8] + 11
                and frame[:8] == request[:5] + bytes((command | 0x80,)) + token
                and crc16(frame) == 0
            )
            self.log("RX", frame, None if matches else "长度、帧头、关联号或校验不符")
            if not matches:
                continue
            data = frame[9:-2]
            require(data, "应答缺少结果码")
            if data[0]:
                require(len(data) == 10, "错误应答长度不符")
                raise Rejected(data)
            return data

        if received:
            self.log("RX", received, "截止时仍有未完成报文")
        self.log("TIMEOUT", b"", "结果未确认")
        raise TimeoutError(f"{command:02X}H 请求在地址 {self.address} 超时，结果未确认")

    def info(self):
        """读取并验证修订、能力、机型和分段上限。"""
        data = self.exchange(1)
        require(len(data) == 16 and data[1] == 0 and data[10:] == bytes.fromhex("001F61100908"),
                "设备信息修订、能力或尺寸与验收目标不符")
        phases, pumps = data[6:8]
        volts = int.from_bytes(data[8:10], "big")
        require((phases, volts) in ((1, 220), (3, 220), (3, 380)) and pumps in (1, 2), "设备机型无效")
        return {"phases": phases, "pumps": pumps, "volts": volts, "revision": data[1],
                "legacy_version": int.from_bytes(data[4:6], "big")}

    def block(self, command, start, count):
        """核对分段读取的长度和回显并解析质量及原值。"""
        widths = {2: 3, 3: 5, 4: 6}
        width = widths[command]
        data = self.exchange(command, bytes((start, count)))
        require(len(data) == 3 + count * width and data[1:3] == bytes((start, count)),
                f"{command:02X}H 分段长度或回显不符")
        items = []
        for offset in range(3, len(data), width):
            quality = data[offset]
            require(quality in ((0, 2, 3, 4) if command == 4 else (0, 1, 2, 3)), "逐项质量码无效")
            raw = data[offset + 1:offset + width]
            value = ({"fault": raw[0], "volts": int.from_bytes(raw[1:3], "big"),
                      "current_tenths": int.from_bytes(raw[3:5], "big")}
                     if command == 4 else int.from_bytes(raw, "big"))
            items.append({"q": quality, "value": value})
        return items

    def param(self, number):
        """读回一个参数的原值和质量。"""
        return self.block(2, number, 1)[0]

    def set_param(self, number, value):
        """写入单项参数并核对内存实值回显，改址应答关联旧地址。"""
        data = bytes((number,)) + value.to_bytes(2, "big")
        require(self.exchange(0x10, data) == b"\0" + data, "单写参数应答回显不符")
        if number == 11:
            self.address = value

    def control(self, action, target=0):
        """按动作和目标验证控制应答并返回当时诊断。"""
        args = bytes((action, target))
        data = self.exchange(0x11, args)
        require(len(data) == 9 and data[1:3] == args, "控制应答回显不符")
        return {"state": int.from_bytes(data[3:5], "big"), "faults": int.from_bytes(data[5:9], "big")}

    def run_state(self, *, ready=False):
        """读取独立编码的模式位，演练前另核对运行状态质量。"""
        item = self.block(3, 8, 1)[0]
        if ready:
            require(item["q"] == 0, "运行状态未就绪或异常，暂不能开始控制演练")
        state = item["value"]
        mode = {1: 0, 6: 1, 10: 2}.get(state & 0x0f)
        require(mode is not None, "运行模式位无效")
        return mode, state


def check_error(client, command, data, status, target):
    """验证指定的错误码和对象。"""
    try:
        client.exchange(command, data)
    except Rejected as error:
        require((error.code, error.target) == (status, target), f"错误应答不符，{error}")
    else:
        raise ProtocolError(f"{command:02X}H 边界请求得到成功应答")


def read_acceptance(client, identity):
    """读取全部分段、两种综合应答及各分段的起末边界。"""
    parameters = client.block(2, 0, 16) + client.block(2, 16, 7)
    runtime = client.block(3, 1, 9) + client.block(3, 10, 9) + client.block(3, 19, 6)
    history = client.block(4, 0, 8) + client.block(4, 8, 2)
    feature = (identity["phases"] == 3) | (2 if identity["pumps"] == 2 else 0) | (4 if identity["volts"] == 380 else 0)
    summaries = {}
    for command, size in ((5, 63), (6, 86)):
        summary = client.exchange(command)
        require(len(summary) == size and summary[1] == feature and not any(summary[-10:]),
                f"{command:02X}H 长度、机型特征或保留区无效")
        if command == 6:
            require(summary[10] & 0xc0 == 0, "06H 质量区保留位无效")
        summaries[f"{command:02X}"] = summary.hex().upper()
    for command, first_invalid, last in ((2, 23, 22), (3, 0, 24), (4, 10, 9)):
        check_error(client, command, bytes((first_invalid, 1)), 4, first_invalid)
        check_error(client, command, bytes((last, 2)), 3, last)
    return {"parameters": parameters, "runtime": runtime, "history": history, "summaries": summaries}


class Session:
    """保存原值、在写入前登记待核对项，并按实际读回逐项恢复。"""

    def __init__(self, client, path, backup):
        """关联一个连接及持久记录。"""
        self.client, self.path, self.backup = client, path, backup
        self.pending = backup["pending"]

    def save(self):
        """写入当前备份及恢复进度。"""
        store_json(self.path, self.backup)

    def mark(self, key, original, attempted):
        """在发送前记录可能生效的值，处理超时或中断。"""
        item = self.pending.setdefault(key, {"original": original, "attempted": []})
        if attempted not in item["attempted"]:
            item["attempted"].append(attempted)
        self.save()

    def done(self, key, wrote):
        """只有读回确认或接到停止应答后才解除待恢复状态。"""
        item = self.pending.pop(key)
        checked = self.backup["checked"]
        checked.append({"item": key, "original": item["original"], "restored_by_write": wrote})
        try:
            self.save()
        except BaseException:
            checked.pop()
            self.pending[key] = item
            raise

    def set_param(self, number, value):
        """记录实际计划改动的单项参数，写入后核对读回。"""
        original = self.backup["before"]["parameters"][number]["value"]
        require(self.client.param(number) == {"q": 0, "value": original}, f"P-{number:02} 已被其他入口修改或质量异常")
        if value == original:
            return
        if number == 11:
            address = self.client.address
            try:
                self.client.address = value
                self.client.info()
            except TimeoutError:
                pass
            else:
                raise ProtocolError(f"计划使用的地址 {value} 已有响应")
            finally:
                self.client.address = address
        self.mark(str(number), original, value)
        self.client.set_param(number, value)
        require(self.client.param(number) == {"q": 0, "value": value}, f"P-{number:02} 写入后读回不符")
        self.client.set_param(number, value)  # 在已登记的同一项上验证等值写入。
        require(self.client.param(number) == {"q": 0, "value": value}, f"P-{number:02} 等值写入后读回不符")

    def exercise_control(self):
        """在独立测试板上检查模式门禁，并逐目标配对开停。"""
        client = self.client
        original, state = client.run_state(ready=True)
        require(original == self.backup["mode_before"], "当前模式已被其他入口修改")
        require(not state & 0x30, "控制演练要求两路运行状态均为停止")
        check_error(client, 0x11, b"\x00\x00", 4, 0)
        for mode in (1, 2, 0):
            current, _ = client.run_state()
            if current != mode:
                self.mark("mode", original, mode)
                client.control(mode + 3)
            require(client.run_state()[0] == mode, "切换后的模式读回不符")
            if mode:
                check_error(client, 0x11, b"\x01\x01", 8, 1)
        client.control(2, 3)  # 清掉模式切换期间可能保留的运行意图。
        targets = (1, 2, 3) if self.backup["device"]["pumps"] == 2 else (1, 3)
        if self.backup["device"]["pumps"] == 1:
            check_error(client, 0x11, b"\x01\x02", 6, 1)
        for target in targets:
            self.mark("stop", 0, target)
            try:
                client.control(1, target)
            finally:
                client.control(2, target)
                self.done("stop", True)

    def find_address(self):
        """只读探测记录中的地址，确认唯一匹配的目标。"""
        addresses = [self.backup["address"]]
        addresses.extend(self.pending.get("11", {}).get("attempted", []))
        matches = []
        for address in dict.fromkeys(addresses):
            self.client.address = address
            try:
                if self.client.info() == self.backup["device"]:
                    matches.append(address)
            except TimeoutError:
                continue
        require(len(matches) == 1, f"恢复需要唯一匹配的测试板，已验证地址为 {matches}")
        self.client.address = matches[0]

    def restore(self):
        """先撤销开泵意图，再按量程依赖恢复参数和原模式。"""
        if not self.pending:
            return
        self.find_address()
        if "stop" in self.pending:
            require(self.client.run_state()[0] == 0, "停止意图待确认，板子已离开手动模式")
            self.client.control(2, self.pending["stop"]["attempted"][-1])
            self.done("stop", True)
        numbers = [int(key) for key in self.pending if key.isdecimal()]
        # 扩大量程先于恢复阈值，缩小量程晚于恢复阈值。
        numbers.sort(key=lambda n: 2 if n == 11 else 1 if n in THRESHOLDS else 0)
        if 14 in numbers and self.client.param(14)["value"] > self.pending["14"]["original"]:
            numbers.remove(14)
            numbers.insert(numbers.index(11) if 11 in numbers else len(numbers), 14)
        for number in numbers:
            key = str(number)
            item = self.pending[key]
            current = self.client.param(number)
            original = item["original"]
            expected = {"q": 0, "value": original}
            if current == expected:
                self.done(key, False)
                continue
            require(current["value"] in item["attempted"], f"P-{number:02} 出现计划外值 {current}，保留待恢复记录")
            self.client.set_param(number, original)
            require(self.client.param(number) == expected, f"P-{number:02} 恢复后读回不符")
            self.done(key, True)
        if "mode" in self.pending:
            item = self.pending["mode"]
            current = self.client.run_state()[0]
            original = item["original"]
            if current == original:
                self.done("mode", False)
            else:
                require(current in item["attempted"], "模式出现计划外修改，保留待恢复记录")
                self.client.control(original + 3)
                require(self.client.run_state()[0] == original, "恢复原模式后读回不符")
                self.done("mode", True)


class SimulatedPort:
    """用实际固件处理报文，并以分段读取模拟串口接收。"""

    def __init__(self, pumps, drop_address_ack, output):
        """启动现有固件测试工程的交互入口。"""
        runner = Path(__file__).with_name("run_bf4g_tests.py")
        self.process = subprocess.Popen(
            [sys.executable, "-X", "utf8", str(runner), "--bridge", str(pumps)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=output,
            text=True, encoding="ascii",
        )
        self.reply = b""
        self.drop_address_ack = drop_address_ack

    def write(self, request):
        """输入原始请求，按选项模拟改址已经执行但应答丢失。"""
        self.process.stdin.write(request.hex() + "\n")
        self.process.stdin.flush()
        line = self.process.stdout.readline()
        require(line, "固件模拟进程退出，请查看 simulator.log")
        self.reply = bytes.fromhex(line)
        if self.drop_address_ack and request[5] == 0x10 and request[9] == 11 and self.reply:
            self.reply = b""
            self.drop_address_ack = False
        return len(request)

    def flush(self):
        """管道已在发送时刷新。"""

    def read(self, size):
        """每次只交付一段，应答末尾保留静默。"""
        if not self.reply:
            time.sleep(QUIET)
        chunk, self.reply = self.reply[:min(size, 13)], self.reply[min(size, 13):]
        return chunk

    def close(self):
        """关闭输入，让固件进程结束并清理编译临时目录。"""
        try:
            self.process.stdin.close()
            self.process.wait(timeout=5)
        finally:
            if self.process.poll() is None:
                self.process.kill()
                self.process.wait()
            self.process.stdout.close()


def parse_setting(text):
    """解析并验证参数编号和线上原始值的表示范围。"""
    match = re.fullmatch(r"(?:P-)?([0-9]{1,2})=([0-9]{1,5})", text, re.IGNORECASE)
    if match is None:
        raise argparse.ArgumentTypeError("参数格式为 P-03=120，数值为线上原始整数")
    number, value = map(int, match.groups())
    if number > 20 or not 0 <= value <= 65535 or (number == 11 and not 1 <= value <= 254):
        raise argparse.ArgumentTypeError("参数号为 0～20，原始值为 0～65535，地址为 1～254")
    return number, value


def timeout_value(text):
    """验证截止时间覆盖最长串口报文。"""
    timeout = float(text)
    if not 0.2 <= timeout <= 30:
        raise argparse.ArgumentTypeError("应答超时为 0.2～30 秒")
    return timeout


def parser():
    """定义串口选择、软件模拟、现场测试和续恢复入口。"""
    root = argparse.ArgumentParser(description=__doc__)
    commands = root.add_subparsers(dest="command", required=True)
    commands.add_parser("list", help="列出可用串口")
    simulator = commands.add_parser("simulate", help="使用实际固件的电脑端模拟")
    simulator.add_argument("--pumps", type=int, choices=(1, 2), default=2, help="模拟泵路数")
    simulator.add_argument("--drop-address-ack", action="store_true", help="模拟改址应答丢失")
    run = commands.add_parser("run", help="连接测试板并执行验收")
    run.add_argument("--port", required=True, help="实际串口，例如 COM5")
    run.add_argument("--address", type=int, choices=range(1, 255), metavar="1~254", default=1)
    run.add_argument("--board", help="记录板号")
    run.add_argument("--set", dest="settings", type=parse_setting, action="append", default=[],
                     metavar="P-NN=VALUE", help="测试写入后恢复，可重复指定不同参数")
    run.add_argument("--exercise-control", action="store_true", help="在无负载测试板演练模式和启停")
    run.add_argument("--clear-fault", action="store_true", help="执行一次当前故障清除，保留历史记录")
    restore = commands.add_parser("restore", help="用备份文件续恢复同一测试板")
    restore.add_argument("--port", required=True, help="同一测试板现在使用的串口")
    restore.add_argument("--backup", type=Path, required=True, help="之前验收目录中的 backup.json")
    for command in (simulator, run, restore):
        command.add_argument("--output", type=Path, help="新建本次记录目录，默认在系统临时目录")
        command.add_argument("--timeout", type=timeout_value, default=0.7, metavar="SECONDS")
    return root


def open_serial(port, timeout):
    """打开指定串口，按项目协议设置传输参数。"""
    import serial

    return serial.Serial(port, baudrate=9600, bytesize=8, parity="N", stopbits=1,
                         timeout=QUIET, write_timeout=timeout)


def load_backup(path):
    """检验备份格式及待恢复项，再用于同一块实板。"""
    with path.open(encoding="utf-8") as source:
        backup = json.load(source)
    try:
        require(backup["format"] == 1 and backup["transport"] == "serial", "需要实板验收生成的备份")
        require(1 <= backup["address"] <= 254, "备份地址无效")
        require(backup["before"]["parameters"][11] == {"q": 0, "value": backup["address"]}, "备份地址记录不一致")
        for key, item in backup["pending"].items():
            values = item["attempted"]
            require(values, "备份缺少拟写入值")
            if key == "mode":
                require(all(value in range(3) for value in [item["original"], *values]), "备份模式值无效")
                require(item["original"] == backup["mode_before"], "备份原模式不一致")
            elif key == "stop":
                require(item["original"] == 0 and all(value in (1, 2, 3) for value in values), "备份停止目标无效")
            else:
                number = int(key)
                require(0 <= number <= 20 and str(number) == key, "备份参数号无效")
                require(all(type(value) is int and 0 <= value <= 65535 for value in values), "备份参数值无效")
                require(backup["before"]["parameters"][number] == {"q": 0, "value": item["original"]},
                        "备份参数原值不一致")
                if number == 11:
                    require(all(1 <= value <= 254 for value in values), "备份改址值无效")
    except (AttributeError, IndexError, KeyError, TypeError, ValueError) as error:
        raise ProtocolError("无法读取备份文件中的设备和参数记录") from error
    return backup


def create_session(client, args, directory):
    """执行读验收并持久保存完整的写前快照。"""
    device = client.info()
    before = read_acceptance(client, device)
    require(before["parameters"][11] == {"q": 0, "value": args.address}, "读回地址与请求地址不符")
    for number, _ in args.settings:
        require(before["parameters"][number]["q"] == 0, f"P-{number:02} 原值质量异常，无法保证恢复")
    mode = client.run_state()[0] if args.exercise_control else None
    backup = {
        "format": 1, "transport": "simulated" if args.command == "simulate" else "serial",
        "port": args.port, "board": args.board, "address": args.address, "device": device,
        "mode_before": mode, "before": before, "pending": {}, "checked": [],
    }
    session = Session(client, directory / "backup.json", backup)
    session.save()
    return session


EXPECTED_FAILURE = (OSError, OverflowError, ProtocolError, ValueError, KeyboardInterrupt)


def run_connected(port, args, directory, traffic, result):
    """执行所选验收，出现失败时仍尝试恢复并保留两阶段结果。"""
    session = None
    try:
        if args.command == "restore":
            backup = load_backup(args.backup)
            client = Client(port, backup["address"], traffic, args.timeout)
            session = Session(client, args.backup, backup)
        else:
            client = Client(port, args.address, traffic, args.timeout)
            session = create_session(client, args, directory)
        result["device"] = session.backup["device"]
        result["backup"] = str(session.path)
        if args.command != "restore":
            result["read_commands"] = ["01", "02", "03", "04", "05", "06"]
            # 按命令行顺序应用量程和阈值，恢复时按当前量程决定顺序。
            if args.settings:
                check_error(client, 0x10, bytes((23,)), 3, 23)
                check_error(client, 0x10, bytes((23, 0, 0)), 4, 23)
                for number, value in args.settings:
                    session.set_param(number, value)
                result["write_checked"] = True
            if args.exercise_control:
                session.exercise_control()
                result["control_checked"] = True
            if args.clear_fault:
                result["clear_fault"] = client.control(6)
    except EXPECTED_FAILURE as error:
        result["error"] = str(error) or "操作被中断"
    finally:
        if session is not None:
            try:
                session.restore()
            except EXPECTED_FAILURE as error:
                result["restore_error"] = str(error) or "恢复被中断"
            result["pending"] = session.pending
            result["checked"] = session.backup["checked"]

    if args.command == "simulate" and session is not None and not session.pending:
        client.address = session.backup["address"]
        after = client.block(2, 0, 16) + client.block(2, 16, 7)
        require(after == session.backup["before"]["parameters"], "模拟恢复后的全部参数与快照不一致")
        require(client.run_state()[0] == session.backup["mode_before"], "模拟恢复后的运行模式不一致")
        result["simulation_restored_snapshot"] = True


def main():
    """打开所选连接，写出收发、备份及独立结果。"""
    cli = parser()
    args = cli.parse_args()
    if args.command == "list":
        try:
            from serial.tools.list_ports import comports
            ports = sorted(comports())
        except (ImportError, OSError) as error:
            cli.exit(1, f"无法列出串口，请检查 pyserial 和串口驱动。{error}\n")
        for port in ports:
            print(f"{port.device}  {port.description}")
        if not ports:
            print("当前没有可枚举的串口。")
        return 0

    if args.command == "simulate":
        args.port, args.address, args.board = None, 1, f"模拟一控{args.pumps}"
        args.settings = [(3, 120), (14, 20), (15, 15), (11, 2)]
        args.exercise_control = args.clear_fault = True
    if args.command == "run" and len(dict(args.settings)) != len(args.settings):
        cli.error("每个参数在一次验收中只能指定一个测试值")

    result = {"command": args.command, "port": args.port, "physical_observation": "未执行"}
    try:
        if args.output is None:
            directory = Path(tempfile.mkdtemp(prefix="bf4g-014-"))
        else:
            directory = args.output.resolve()
            directory.mkdir(parents=True)
    except OSError as error:
        cli.exit(1, f"无法新建验收目录。{error}\n")
    print(f"验收记录 {directory}", flush=True)
    try:
        with (directory / "traffic.jsonl").open("w", encoding="utf-8") as traffic:
            if args.command == "simulate":
                with (directory / "simulator.log").open("w", encoding="utf-8") as simulator_log:
                    port = SimulatedPort(args.pumps, args.drop_address_ack, simulator_log)
                    try:
                        run_connected(port, args, directory, traffic, result)
                    finally:
                        port.close()
            else:
                with open_serial(args.port, args.timeout) as port:
                    run_connected(port, args, directory, traffic, result)
    except ImportError as error:
        result["error"] = f"串口模式需要 pyserial。{error}"
    except (subprocess.SubprocessError, *EXPECTED_FAILURE) as error:
        result.setdefault("error", str(error) or "操作被中断")

    result["passed"] = not any(result.get(key) for key in ("error", "restore_error", "pending"))
    store_json(directory / "result.json", result)
    for key in ("error", "restore_error"):
        if result.get(key):
            print(result[key], file=sys.stderr)
    if result.get("pending"):
        print("仍有待恢复项，详见 backup.json 和 result.json。", file=sys.stderr)
    elif result.get("checked"):
        print("已核对所有本次待恢复项。")
    print("通过" if result["passed"] else "失败")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())

# BF006 控制器模拟器操作指令

## 1. 文档目的

模拟器以 BF006 控制器身份应答实体 4G 模块的 UART4 请求，也能通过独立端口应答 RS485 Modbus RTU。两路可同时工作，共用模拟设备的地址、参数、运行状态和故障记录。

目标程序：

```text
模拟器\bf006_modbus_rtu_simulator.py
```

不带参数打开 GUI。使用串口前需在同一个 Python 环境安装 `pyserial`。GUI 分别选择「RS485 串口」和「4G UART4」，单路使用时清空另一项。命令行操作在程序路径后添加 `--cli`。

## 2. 基础调用方式

以下 PowerShell 示例假设当前目录为项目根目录：

```powershell
$python = "python"
$simulator = Resolve-Path -LiteralPath ".\模拟器\bf006_modbus_rtu_simulator.py"
```

查看完整帮助：

```powershell
& $python $simulator --cli --help
```

查看子命令帮助：

```powershell
& $python $simulator --cli parameter --help
& $python $simulator --cli runtime set --help
& $python $simulator --cli measurement set --help
& $python $simulator --cli liquid set --help
& $python $simulator --cli serve --help
```

## 3. Codex 必须遵守的规则

1. 每次独立执行 CLI 都创建一套新的默认状态，进程结束后状态消失。
2. 多条命令存在先后依赖时，必须使用 `batch`、`session`、`serve --init-command` 或 `serve --control-stdin`。
3. 单次命令成功时，标准输出是一个 UTF-8 JSON 对象；应检查顶层 `ok` 和进程退出码。
4. `session` 与 `serve` 输出 JSON Lines，每一行都是独立 JSON 事件，不能把全部输出一次性传给 `ConvertFrom-Json`。
5. 命令语法错误由 `argparse` 写入标准错误并返回退出码 `2`，不保证输出 JSON。
6. 普通数值不带前缀时按十进制解析，带 `0x` 前缀时按十六进制解析。
7. 参数裸地址按十六进制解析，例如 `0012` 表示 `0012H`；推荐使用 `P-18` 形式。
8. `frame` 输入必须包含完整 CRC；帧字节使用十六进制，不写 `0x` 前缀。
9. `measurement set`、`runtime set` 和 `liquid set` 都是整组原子提交，必须提供该命令的必填参数。任何一项非法时整组不生效。
10. `measurement set` 的 A/B 泵电流使用工程值 A，允许一位小数；传感器工程值允许两位小数。
11. 协议201的连续传感器值使用 `liquid set --value`，单位由 `--sensor-type` 决定：类型2为米，类型3/4为 MPa。
12. `2116H` 的最终值等于手动 `--status` 与 P-20 自动溢出位的按位或结果；自动化断言应读取输出中的 `effective_sensor_status` 或 `monitor get 2116`。
13. RS485 的 `2000H Pump_Control` 是动作命令。以 RS485 模拟上位机关联操作时，启动/停止后应紧接 `control release`。UART4 使用独立的 11H 动作，受理后通过 03H 回读。
14. `fault set` 使用固件零基枚举；`2100H` 当前故障码是枚举值加1。
15. 需要让上位机自己生成本地故障历史时，使用 `fault set <枚举> --no-history`，避免同时写入控制器模拟历史。
16. 上位机正在轮询且场景需要动态变化时，使用 `serve --control-stdin`；每条管理命令与串口帧处理共享同一状态锁，必须逐行检查 `event=command_result` 的 `ok` 和嵌套 `result`。
17. 多设备模式使用 `--addresses 1,2,3` 或 `--addresses 1-3`；地址列表含两台及以上设备时，每条内部命令必须增加 `@从站地址` 前缀，例如 `@2 fault set 3 --no-history`，避免误改其他设备。`--addresses 7` 这种单地址总线允许省略 `@7`，执行 P-11 或恢复默认时仍会正确维护总线地址。
18. 多设备模式下每台设备拥有独立的参数、运行状态、测量值、液位、故障和历史；只能共享串口配置与串口收发线程。

## 4. 命令总览

| 命令 | 作用 | 当前进程内是否修改状态 |
| --- | --- | --- |
| `status` | 查询完整状态摘要 | 否 |
| `reset-defaults` | 恢复 GUI 默认状态 | 是 |
| `parameter list/get/set` | 列出、读取、写入 P 参数 | `set` 是 |
| `monitor list/get` | 查询 `2100H-2118H` | 否 |
| `runtime set` | 设置运行模式、泵状态、主备和兼容液位 | 是 |
| `measurement set` | 设置电压、电流、功率、型号、版本和累计值 | 是 |
| `device set-model/set-version/set-electrical` | 设置型号字、功能版本或 V1.3 电气规格 | 是 |
| `liquid set` | 设置协议201液位、六段位图及 `2116H` | 是 |
| `control` | 执行 `2000H-2007H` 控制动作 | 是 |
| `fault list/set/clear/bits` | 故障枚举、注入、清除和手动汇总位 | 是 |
| `history list/get/set/clear` | 操作十条控制器历史记录 | 是 |
| `frame` | 处理完整 RS485 帧；加 `--uart4` 处理 V1.3 帧 | 视帧内容而定 |
| `batch` | 在同一状态内顺序执行多条命令 | 是 |
| `session` | 从标准输入持续接收命令 | 是 |
| `ports` | 枚举串口 | 否 |
| `serve` | 启动无 GUI 串口服务，支持 UART4、RS485 或双串口，可从 stdin 修改状态 | 服务存活期间保持 |

### 4.1 多设备共用一个 COM 口

查询三台默认设备：

```powershell
& $python $simulator --cli status --addresses 1-3
```

在一个持久状态中分别修改三台设备：

```powershell
& $python $simulator --cli batch `
    --addresses 1-3 `
    --command "@1 runtime set --mode auto --pump-a on --pump-b off --pump-mode a-main --upper-level high --lower-level middle --add-pump off" `
    --command "@2 fault set 3 --no-history" `
    --command "@3 liquid set --sensor-type 2 --value 1.25 --target lower --level middle --upper-segments 0x3F --lower-segments 0x0F --status 0" `
    --command "@1 status" `
    --command "@2 status" `
    --command "@3 status"
```

多设备结果使用 `fleet`：

```json
{
  "device_count": 3,
  "slave_addresses": [1, 2, 3],
  "devices": {
    "1": {},
    "2": {},
    "3": {}
  }
}
```

地址列表支持逗号和闭区间，可组合为 `1,3,5-7`，地址必须唯一并位于1-254；范围端点会在展开前校验，异常的大范围不会占用大量内存。含两台及以上设备时，内部命令缺少 `@地址` 将返回 `ok=false`，不会默认修改第一台或全部设备。

原始报文也可离线走多设备总线路由：

```powershell
& $python $simulator --cli frame --addresses 1-3 02 03 21 0E 00 01 EF C6
```

目标设备由报文首字节决定，不再额外添加 `@地址`。

## 5. JSON 和退出码约定

| 退出码 | 含义 |
| ---: | --- |
| `0` | 命令成功、会话正常结束或串口服务正常关闭 |
| `1` | 参数校验、状态操作、批处理、初始化或串口操作失败 |
| `2` | CLI 语法错误或缺少必填参数 |

单条命令结果至少包含：

```json
{
  "ok": true,
  "command": "status",
  "state": {}
}
```

不要只根据中文提示判断结果。优先检查：

- `$LASTEXITCODE`；
- 顶层 `ok`；
- 失败时的 `error_code` 和 `error`；
- 原始帧的 `protocol.crc_ok`、`protocol.ignored` 和 `protocol.exception_code`。

## 6. 查询完整状态

```powershell
& $python $simulator --cli status
```

`state` 主要字段：

| 字段 | 含义 |
| --- | --- |
| `slave_address` | 当前从站地址 |
| `protocol_version` | `210EH` |
| `device_model` / `device_model_hex` | `210DH` 十进制/十六进制值 |
| `control_scheme` | 根据已确认型号高字节解析的一控一/一控二 |
| `run_status` | `running` 或 `stopped` |
| `running_pumps` | 当前运行泵数组，例如 `["A"]` |
| `run_mode` | 0手动、1自动、2循环定时 |
| `pump_ab_mode` | 0 A主、1 B主、2轮换/同启 |
| `pump_control_command` | 当前保持的 `2000H` 动作值 |
| `fault_code` | `2100H` 当前详细故障码 |
| `fault_status_bits` | `2102H` 汇总位 |
| `fault_bits_32` | `2117H/2118H` 合成的完整故障位图 |
| `sensor_status` | `2116H` 手动位与自动溢出位合成值 |
| `measurements` | 电压、电流、功率、型号、版本、累计值原始状态 |
| `liquid` | 协议201液位状态及 `210FH-2116H` 寄存器字典 |

## 7. P 参数

### 7.1 列出参数

```powershell
& $python $simulator --cli parameter list
& $python $simulator --cli parameter list --writable-only
```

### 7.2 读取参数

```powershell
& $python $simulator --cli parameter get P-18
& $python $simulator --cli parameter get 0x0012
& $python $simulator --cli parameter get 0012
```

### 7.3 写入参数

```powershell
& $python $simulator --cli parameter set P-18 2
& $python $simulator --cli parameter set P-20 5
& $python $simulator --cli parameter set P-11 5
```

写入遵守模拟器现有范围、只读和传感器量程约束。写 `P-11` 会同步当前 CLI 从站地址。

## 8. 监视寄存器

列出全部 `2100H-2118H`：

```powershell
& $python $simulator --cli monitor list
```

读取单个十六进制地址：

```powershell
& $python $simulator --cli monitor get 210D
& $python $simulator --cli monitor get 2116
```

返回的 `value` 是十进制原始值，`value_hex` 是 `0x` 前缀的四位十六进制值。

## 9. 运行状态整组设置

```powershell
& $python $simulator --cli runtime set `
    --mode auto `
    --pump-a on `
    --pump-b off `
    --pump-mode alternating `
    --upper-level middle `
    --lower-level low `
    --add-pump on
```

选项映射：

| 选项 | 可选值 |
| --- | --- |
| `--mode` | `manual`、`auto`、`timer` |
| `--pump-a` / `--pump-b` | `on`、`off` |
| `--pump-mode` | `a-main`、`b-main`、`alternating` |
| `--upper-level` / `--lower-level` | `low`、`middle`、`high` |
| `--add-pump` | `on`、`off` |

这里的上下水池液位对应旧协议 `210AH/210BH` 和 `2101H` 兼容位。协议201模拟量液位仍应使用 `liquid set`。

## 10. 工程测量值整组设置

```powershell
& $python $simulator --cli measurement set `
    --voltage 231 `
    --current-a 5.8 `
    --current-b 7.2 `
    --power 36 `
    --legacy-sensor 1.25 `
    --device-model 0xA293 `
    --version 201 `
    --total-run 12 `
    --total-power 34
```

量纲：

- 电压：整数 V；
- A/B 泵电流：工程值 A，转换为 0.1A 原始值；
- 功率：整数 W；
- `--legacy-sensor`：工程值，转换为 `2109H` 的 0.01 原始值，并同步当前 `2110H` 内部连续值；
- `--device-model`：完整 `210DH` U16 原始值；
- `--version`：`210EH`；
- `--total-run`、`--total-power`：`P-21/P-22` 原始值，范围0-9999。

## 11. 设备身份快捷设置

只修改型号或版本，不重新输入整组测量值：

```powershell
& $python $simulator --cli device set-model 0xA293
& $python $simulator --cli device set-version 201
```

已确认型号高字节：

| 高字节 | 型号 |
| --- | --- |
| `A2H` | 单相一控一 |
| `A3H` | 单相一控二 |
| `B2H` | 三相一控一 |
| `B3H` | 三相一控二 |

低字节是规格档位。旧模拟器 `00H/01H/02H` 仅作为兼容别名。

## 12. 协议201液位与传感器

```powershell
& $python $simulator --cli liquid set `
    --sensor-type 2 `
    --value 1.25 `
    --target lower `
    --level high `
    --upper-segments 0x15 `
    --lower-segments 0x2A `
    --status 0x08
```

选项：

| 选项 | 含义 |
| --- | --- |
| `--sensor-type` | P-18/210FH：0电极浮球、1电接点压力、2液位、3 4-20mA压力、4 0-5V压力 |
| `--value` | 连续工程值，最多两位小数 |
| `--target` | `upper` 或 `lower`，写入2112H |
| `--level` | `low/middle/high`，写入2113H |
| `--upper-segments` | 2114H bit0-bit5 |
| `--lower-segments` | 2115H bit0-bit5 |
| `--status` | 手动2116H bit0-bit4 |

`2116H` 位定义：

| 位 | 含义 |
| ---: | --- |
| bit0 | 传感器断线或输入异常 |
| bit1 | 超过P-20溢出阈值 |
| bit2 | 系统处于溢流状态 |
| bit3 | 上水池显示闪烁 |
| bit4 | 下水池显示闪烁 |

类型0/1时，Modbus读取 `2110H/2113H` 返回 `FFFFH`；CLI仍保留输入值，切换回模拟量类型时可继续使用。

P-20使用0.1工程单位，2110H使用0.01工程单位。模拟器在类型2/3/4且连续值严格大于P-20时自动叠加bit1/bit2。例如0.6大于P-20的0.5：

```powershell
& $python $simulator --cli batch `
    --command "parameter set P-20 5" `
    --command "liquid set --sensor-type 2 --value 0.60 --target lower --level high --upper-segments 0x00 --lower-segments 0x3F --status 0" `
    --command "monitor get 2116"
```

预期 `2116H=0006H`。

## 13. 控制动作

语法：

```powershell
& $python $simulator --cli control <动作>
```

| 动作 | 寄存器和值 |
| --- | --- |
| `pump-a-start` / `pump-a-stop` | `2000H=0012H/0011H` |
| `pump-b-start` / `pump-b-stop` | `2000H=0022H/0021H` |
| `all-start` / `all-stop` | `2000H=0032H/0031H` |
| `release` | `2000H=0000H` |
| `local-stop` | 模拟控制器面板停止键 |
| `manual` / `auto` / `timer` | `2003H=0/1/2` |
| `a-main` / `b-main` / `alternating` | `2004H=0/1/2` |
| `clear-faults` | `2005H=0001H` |
| `clear-history` | `2006H=0000H` |
| `save-parameters` | `2007H=5AA5H` |

推荐动作命令与释放命令放在同一个批处理中：

```powershell
& $python $simulator --cli batch `
    --command "control pump-a-start" `
    --command "control release" `
    --command "status"
```

## 14. 故障

列出固件零基故障枚举：

```powershell
& $python $simulator --cli fault list
```

注入故障并写控制器历史：

```powershell
& $python $simulator --cli fault set 2
```

只设置实时故障，不写控制器历史：

```powershell
& $python $simulator --cli fault set 2 --no-history
```

清除实时故障：

```powershell
& $python $simulator --cli fault clear
```

手动设置 `2102H` 汇总位和 `210CH` 泵身份位，不构造 `2100H` 详细码：

```powershell
& $python $simulator --cli fault bits `
    --summary 0x0104 `
    --pump-a on `
    --pump-b off
```

## 15. 控制器历史记录

```powershell
& $python $simulator --cli history list
& $python $simulator --cli history get 1
& $python $simulator --cli history set 1 --fault 16 --voltage 230 --current 75
& $python $simulator --cli history clear
```

- 索引范围1-10；
- `--fault` 使用控制器历史零基枚举，允许0-17；
- 17代表空记录；
- 电压和电流是协议原始U16值。

## 16. 原始 Modbus RTU 帧

```powershell
& $python $simulator --cli frame 01 03 21 0E 00 01 00 00
```

上例末尾CRC只是占位，实际调用必须换成正确CRC。也可把所有字节写成一个引号字符串：

```powershell
& $python $simulator --cli frame "01 03 21 0E 00 01 00 00"
```

关键输出：

```json
{
  "ok": true,
  "request": "01 03 21 0E 00 01 ...",
  "response": "01 03 02 00 C9 ...",
  "protocol": {
    "crc_ok": true,
    "ignored": false,
    "exception_code": null
  }
}
```

无响应时 `response` 为字符串 `"无响应"`，应结合 `ignored` 和 `crc_ok` 判断原因。

## 17. 恢复默认状态

```powershell
& $python $simulator --cli reset-defaults
```

独立执行后进程马上结束，通常应在批处理或会话中使用：

```powershell
& $python $simulator --cli batch `
    --command "parameter set P-11 9" `
    --command "reset-defaults" `
    --command "parameter get P-11"
```

## 18. 批处理

`batch` 中所有命令共享同一个模拟状态，遇到第一条失败命令立即停止。

```powershell
& $python $simulator --cli batch `
    --command "device set-model 0xA293" `
    --command "runtime set --mode auto --pump-a on --pump-b off --pump-mode a-main --upper-level high --lower-level middle --add-pump off" `
    --command "liquid set --sensor-type 2 --value 1.25 --target lower --level middle --upper-segments 0x3F --lower-segments 0x0F --status 0" `
    --command "monitor get 2110" `
    --command "status"
```

顶层字段：

- `results`：按顺序保存每一步结果；
- `state`：结束时最终状态；
- 任意一步失败时顶层 `ok=false`，退出码为1。

可嵌套命令：

- `status`、`reset-defaults`；
- `parameter list/get/set`；
- `monitor list/get`；
- `runtime set`、`measurement set`、`device set-model/set-version`、`liquid set`；
- `control`；
- `fault list/set/clear/bits`；
- `history list/get/set/clear`；
- `frame`。

`ports`、`batch`、`session` 和 `serve` 不作为内部命令。

## 19. 持久会话

```powershell
@'
device set-model 0xA293
parameter set P-20 5
liquid set --sensor-type 2 --value 0.60 --target lower --level high --upper-segments 0 --lower-segments 0x3F --status 0
monitor get 2116
status
exit
'@ | & $python $simulator --cli session
```

事件顺序：

1. `session_started`；
2. 每条命令一个JSON结果；
3. 输入 `exit`、`quit` 或关闭标准输入后输出 `session_closed`。

单条命令失败不会结束会话；自动化程序必须逐行检查 `ok`。会话正常结束的最终退出码为0，不能用最终退出码替代每行检查。

## 20. 串口枚举

```powershell
& $python $simulator --cli ports
```

输出字段：

- `serial_available`：当前Python能否导入pyserial；
- `ports`：pyserial和Windows注册表合并后的串口列表；
- `warning`：依赖缺失提示。

协议核心、状态命令、`frame`、`batch` 和 `session` 不依赖pyserial；只有 `serve` 需要。

## 21. 无GUI串口服务

基本启动：

```powershell
& $python $simulator --cli serve --port COM4
```

完整配置：

```powershell
& $python $simulator --cli serve `
    --port COM4 `
    --baudrate 9600 `
    --parity N `
    --stopbits 1 `
    --address 1 `
    --response-delay-ms 0 `
    --duration 120
```

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `--port` | 无 | RS485 串口号；加 `--uart4` 时指定 4G 模块侧串口 |
| `--uart4` | 关闭 | 将 `--port` 用作单独的 4G V1.3 串口，固定 9600、8N1 |
| `--uart4-port` | 无 | 在 RS485 之外打开第二个物理串口，固定 9600、8N1 |
| `--baudrate` | 9600 | 1200到115200的常用档位 |
| `--parity` | N | N/E/O |
| `--stopbits` | 1 | 1或2 |
| `--address` | 省略 | 指定时覆盖初始化后的P-11；省略时使用当前P-11 |
| `--addresses` | 省略 | 多设备地址列表，例如 `1,2,3` 或 `1-3`；与 `--address` 互斥 |
| `--response-delay-ms` | 0 | 0-60000ms |
| `--duration` | 持续运行 | 自动关闭秒数 |
| `--init-command` | 无 | 可重复，打开串口前顺序执行 |
| `--control-stdin` | 关闭 | 打开stdin运行期控制；`exit`、`quit`或EOF会关闭服务 |

液位场景初始化：

```powershell
& $python $simulator --cli serve `
    --port COM4 `
    --init-command "device set-model 0xA293" `
    --init-command "parameter set P-20 5" `
    --init-command "liquid set --sensor-type 2 --value 0.60 --target lower --level high --upper-segments 0 --lower-segments 0x3F --status 0" `
    --duration 120
```

初始化规则：

1. 按 `--init-command` 出现顺序执行并共享状态；
2. 任意一步失败时不打开串口；
3. 失败输出 `event=initialization_error` 并返回退出码1；
4. 成功打开事件包含全部 `initialization_results`；
5. 省略 `--address` 时，初始化命令写入的P-11直接成为服务地址。
6. 使用 `--addresses` 时，每条初始化命令必须用 `@地址` 指定目标设备；打开事件返回 `device_count` 和 `slave_addresses`。

多设备同COM启动：

```powershell
& $python $simulator --cli serve `
    --port COM4 `
    --addresses 1-3 `
    --init-command "@1 device set-model 0xA293" `
    --init-command "@2 device set-model 0xA393" `
    --init-command "@3 parameter set P-20 5" `
    --control-stdin
```

服务收到RTU请求后按报文首字节路由，只由地址匹配的模拟设备应答；未知地址不响应。三台设备可连续出现在同一次串口读取中，协议核心会拆帧后分别处理。

`frame` 事件明确区分线上地址和设备迁移后的地址：

- `request_address`：收到的请求帧首字节；
- `response_address`：实际响应帧首字节，无响应时为 `null`；
- `slave_address`：兼容字段，等同 `response_address`；
- `device_address`：成功处理后该模拟设备当前所在地址；P-11 改址时它是新地址；
- `handled`：帧是否被某台模拟设备处理；
- `responded`：串口是否实际写出了响应。

例如2号设备通过 P-11 改为5号时，写入回显仍按 Modbus 规则使用地址2，因此该事件为 `request_address=2`、`response_address=2`、`device_address=5`。紧接着访问旧地址2会得到 `response_address=null`、`handled=false`、`responded=false`，访问新地址5才会应答。

P-11 改地址会把目标设备及其完整状态迁移到新地址；如果新地址已被占用，单寄存器和批量写均返回 `03H` 且不修改任何值。单设备总线继续支持 `FFH` 私有广播；多设备总线不模拟多台设备同时响应私有广播，以免生成不真实的可解析响应。

### 21.1 运行中动态修改状态

`--init-command` 只负责打开串口前的初始状态。需要让上位机持续轮询，并在轮询期间按时间顺序注入液位、故障或控制变化时，使用 `--control-stdin`：

```powershell
& {
    "@2 device set-model 0xA393"
    "@2 liquid set --sensor-type 2 --value 0.40 --target lower --level low --upper-segments 0x3F --lower-segments 0x03 --status 0"
    Start-Sleep -Seconds 2
    "@2 liquid set --sensor-type 2 --value 1.25 --target lower --level middle --upper-segments 0x3F --lower-segments 0x0F --status 0"
    Start-Sleep -Seconds 2
    "@2 fault set 8 --no-history"
    Start-Sleep -Seconds 2
    "@2 fault clear"
    "exit"
} | & $python $simulator --cli serve --port COM4 --addresses 1-3 --control-stdin
```

Codex 也可以启动一个长期运行的命令会话，在看到 `control_opened` 后逐次向该进程写入上述命令。每条非空输入都使用与 `batch/session` 相同的命令语法，输入失败不会关闭服务；`exit`、`quit` 或标准输入结束会有序关闭控制通道和串口服务。

事件顺序和字段：

1. `serial_opened`：串口可用，其中 `control_stdin=true`；
2. `control_opened`：可以开始写入管理命令；
3. `command_result`：包含从1递增的 `sequence`、原始 `input` 和完整 `result`；顶层 `ok` 与 `result.ok` 一致；
4. 任意数量的 `frame`：对应链路的实际收发，`link` 为 `rs485` 或 `uart4`；
5. `control_closed`：包含 `reason=requested/stdin_eof/stdin_error` 和已处理命令数；
6. `serial_closed`：包含最终状态。

控制命令和RTU帧通过同一把状态锁串行修改模拟状态，单条JSON输出也有独立输出锁，因此并发发生时不会得到半更新状态或互相拼接的JSON行。

JSON Lines事件：

| `event` | 含义 |
| --- | --- |
| `serial_opened` | 串口已打开 |
| `control_opened` | stdin运行期控制通道已就绪 |
| `command_result` | 一条运行期管理命令的完整结果 |
| `control_error` | stdin读取异常；随后关闭服务 |
| `control_closed` | stdin控制因请求、EOF或异常关闭 |
| `frame` | 收到并处理请求，`port` 和 `link` 标识对应串口 |
| `serial_error` | 依赖、打开或收发失败 |
| `serial_closed` | 串口已关闭 |
| `initialization_error` | 初始化命令失败，未打开串口 |

服务可在单次串口读取中拆分连续到达的多帧；不完整帧在RTU静默间隔后作为一帧交给协议核心，并通过 `crc_ok/ignored/handled/responded` 报告结果。`ok=true` 只能说明该事件本身被正常解析，不等于设备一定响应，自动化脚本判断在线应优先检查 `responded=true`。

## 22. PowerShell解析示例

单个JSON对象：

```powershell
$result = & $python $simulator --cli status | ConvertFrom-Json
if ($LASTEXITCODE -ne 0 -or -not $result.ok) {
    throw "BF006 CLI status failed"
}
$result.state
```

批处理：

```powershell
$result = & $python $simulator --cli batch `
    --command "fault set 2 --no-history" `
    --command "status" |
    ConvertFrom-Json

if ($LASTEXITCODE -ne 0 -or -not $result.ok) {
    $result.results | ConvertTo-Json -Depth 20
    throw "BF006 batch failed"
}
```

串口JSON Lines：

```powershell
& $python $simulator --cli serve --port COM4 --duration 10 |
    ForEach-Object {
        $event = $_ | ConvertFrom-Json
        if (-not $event.ok) {
            $event | ConvertTo-Json -Depth 20
        }
        $event
    }
```

## 23. 推荐自动化场景

### 23.1 协议201液位传感器，1.25m，下水池中位

```powershell
& $python $simulator --cli batch `
    --command "device set-version 201" `
    --command "liquid set --sensor-type 2 --value 1.25 --target lower --level middle --upper-segments 0x3F --lower-segments 0x0F --status 0" `
    --command "monitor get 2110" `
    --command "monitor get 2111" `
    --command "monitor get 2112" `
    --command "monitor get 2113"
```

预期：2110H=125、2111H=1、2112H=1、2113H=1。

### 23.2 离散传感器没有连续值

```powershell
& $python $simulator --cli batch `
    --command "liquid set --sensor-type 0 --value 0 --target upper --level high --upper-segments 0x3F --lower-segments 0x03 --status 0" `
    --command "monitor get 2110" `
    --command "monitor get 2113"
```

预期2110H和2113H均为65535/FFFFH。

### 23.3 一控一控制能力

```powershell
& $python $simulator --cli serve `
    --port COM4 `
    --init-command "device set-model 0xA293" `
    --duration 120
```

上位机应识别一控一并禁用B泵相关按钮。

### 23.4 双泵持续故障序列

```powershell
@'
fault set 2 --no-history
status
fault set 8 --no-history
status
exit
'@ | & $python $simulator --cli session
```

需要同时让上位机轮询时，使用同一个 `serve --control-stdin` 进程即可按时间顺序注入两台泵的故障，不需要停止串口服务：

```powershell
& {
    "fault set 2 --no-history"
    Start-Sleep -Seconds 2
    "fault set 8 --no-history"
    Start-Sleep -Seconds 3
    "fault clear"
    "exit"
} | & $python $simulator --cli serve --port COM4 --control-stdin
```

### 23.5 验证动作命令释放

```powershell
& $python $simulator --cli batch `
    --command "control all-start" `
    --command "control release" `
    --command "control local-stop" `
    --command "status"
```

预期A/B泵最终都停止且 `pump_control_command=0`。

## 24. Codex执行检查清单

执行前：

- 确认脚本路径存在；
- 确认使用 `--cli`；
- 判断命令之间是否依赖同一状态；
- 有依赖时选择 `batch`、`session`、`serve --init-command` 或 `serve --control-stdin`；
- 上位机轮询期间需要动态变化时确认已经收到 `control_opened`；
- 真实串口操作前执行 `ports`；
- 原始RTU帧确认包含正确CRC；
- 液位场景确认传感器类型、单位、目标水池和六段位图。

执行后：

- 检查退出码；
- 检查JSON顶层 `ok`；
- 批处理逐项检查 `results`；
- 会话/串口逐行解析JSON Lines；
- 运行期控制逐条检查 `command_result.ok` 和 `command_result.result`；
- 读取 `state`、`monitor get` 或原始帧响应确认最终寄存器；
- 控制动作确认已经执行 `control release`；
- 故障自动化确认是否需要 `--no-history`。

## 25. 最短指令速查

```powershell
# 完整状态
& $python $simulator --cli status

# 参数和监视
& $python $simulator --cli parameter get P-18
& $python $simulator --cli parameter set P-18 2
& $python $simulator --cli monitor get 2116

# 型号和液位
& $python $simulator --cli device set-model 0xA293
& $python $simulator --cli liquid set --sensor-type 2 --value 1.25 --target lower --level middle --upper-segments 0x3F --lower-segments 0x0F --status 0

# 控制和故障
& $python $simulator --cli control pump-a-start
& $python $simulator --cli control release
& $python $simulator --cli fault set 2 --no-history
& $python $simulator --cli fault clear

# 持久状态
& $python $simulator --cli batch --command "status"
& $python $simulator --cli session

# 串口
& $python $simulator --cli ports
& $python $simulator --cli serve --port COM4
```

## 26. 实体 4G 模块联调

### 接线及启动

把 USB 转串口适配器接至实体 4G 模块原来连接 BF006 的控制器侧接口。适配器的电平和信号定义按模块接口确认，模块串口配置为 9600 bit/s、8 数据位、无校验、1 停止位。UART4 上发送二进制帧。网络注册、BF_YUN 接入和云端封装由实体模块及平台处理。

PowerShell 示例中 COM7 是接 4G 模块的端口，COM4 是接 RS485 适配器的端口，使用时换成实际端口。

~~~powershell
# 只启用 UART4
& $python $simulator --cli serve --port COM7 --uart4

# 同时启用 RS485 和 UART4
& $python $simulator --cli serve --port COM4 --uart4-port COM7
~~~

默认沿用模拟器的一控二型号字 `A393H`、单相 220 V、地址 1、手动停机和原有参数初值。可通过 `--init-command` 改初始状态，或通过 `--control-stdin` 运行时注入状态。切换机型时，`device set-model 0xA293` 选择一控一，`device set-model 0xA393` 选择一控二；`device set-electrical 3 380` 单独设置相数与额定电压，允许 `1 220`、`3 220`、`3 380`。

### 首轮验证

1. 在 BF_YUN 读取运行状态。V1.3 的 `03H` 运行字段 `08H` 应显示手动、A/B 泵停止。也可先用 `01H` 核对设备信息，再用 `05H` 或 `06H` 读取完整页面数据。
2. 从 BF_YUN 发 `11H`，动作 `01H`、目标 `01H`，启动 A 泵。确认应答 `STATUS=00H`，再用 `03H` 检查字段 `08H` 的 bit4 置位。双串口模式也可从 RS485 读取 `2101H` 核对。
3. 发 `11H`，动作 `02H`、目标 `01H`，停止 A 泵。确认受理后用 `03H` 确认 bit4 清零。B 泵使用目标 02H 和 bit5，两泵使用目标 03H。
4. 切自动或循环后再次发送普通启停，检查返回 `08H`。切回手动后可继续测试。控制应答的状态是受理时的快照，模拟运行位在下一次读取时可见。

离线也可以对同一台模拟器走完 A 泵启停和回读。以下帧包含 CRC，最后一条读取复用了 RID，用于检查请求会重新处理。

~~~powershell
@(
    "frame --uart4 0143BF06030343750208010FEE"
    "frame --uart4 0143BF060311437C0201010950"
    "frame --uart4 0143BF060303437D0208010D8E"
    "frame --uart4 0143BF060311437E0202010818"
    "frame --uart4 0143BF060303437D0208010D8E"
    "exit"
) | & $python $simulator --cli session
~~~

V1.3 失败帧为 21 字节，状态位于应答偏移 9。CLI 的 `protocol.exception_code` 或 `frame.exception_code` 给出非零业务状态。顶层 `ok` 用于检查帧处理及 CRC。P-11 改址在旧址应答，两条链路随后按新址接收；多设备模拟时目标地址已占用返回 `0EH`，保留原址。

### 测量和工况注入

- 用 `measurement set` 注入电压和 A/B 泵 A 相电流。三相机型可另给 `--current-ab`、`--current-ac`、`--current-bb`、`--current-bc`，单位 A，分别是两台泵的 B/C 相电流。未注入的适用测量返回 Q=02H。
- 用 `liquid set --value` 注入连续工程值，可使用超过 RS485 16 位编码范围的工程值，例如 `999.90`。V1.3 的 `03H` 字段 `10H` 和综合应答以 32 位返回。同一值通过 RS485 `2110H` 读取时，超范围返回 `FFFFH`。
- 修改传感器类型、量程、方向或启停阈值后，受影响的 V1.3 字段暂为未就绪。先用 `liquid set` 发布连续值、连续档位和上下池六段位图，再用 `runtime set` 发布上下池运行档位。各字段在收到对应输入后恢复有效。
- 参数、模式和状态保留于本次进程。压力、水位、继电器动作、循环定时和故障恢复时序由联调注入场景驱动。V1.3 清故障保留锁定、传感器异常和历史记录。

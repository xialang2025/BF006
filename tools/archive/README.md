# tools/archive 目录说明

本目录用于存放可复用的归档脚本、清单和模板。

## 已归档工具

### `build_and_write_bf006_hex.ps1`

写入型 BF006 多版本固件编译工具，功能如下：

1. 临时按版本切换 `BF006_STC\src\system.h` 的 `Model_Device` 宏；
2. 调用 Keil C51 执行 `-r` 全量重编，并在 `BF006_STC\pro` 写入编译日志，更新 `Objects` 和 `Listings`；
3. 解析编译错误数和警告数，验证产物确实在本轮更新；
4. 校验 Intel HEX 记录长度、逐行校验和及结束记录；
5. 所有请求版本成功后，将命名后的 HEX 写入根目录 `烧录文件`；
6. 无论成功或异常均按原始字节恢复 `system.h`，并复核 SHA-256。

默认编译 220V/380V × 1C1/1C2 四个已确认命名的版本。具体触发规则、宏映射和运行命令见 `skills\bf006-firmware-build\SKILL.md`。

脚本包含中文注释和提示信息，文件编码固定为 UTF-8 BOM，以兼容 Windows PowerShell 5 的脚本读取规则；维护后必须复核 BOM 仍然存在。

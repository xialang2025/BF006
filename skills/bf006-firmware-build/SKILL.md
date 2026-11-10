---
name: bf006-firmware-build
description: 编译、重编或生成 BF006 的 Keil C51 固件与 Intel HEX 烧录文件。用户提到“编译程序”“编译固件”“重新编译”“生成 HEX”“生成烧录文件”“打包版本”，或要求按 220V/380V、1C1/1C2 宏生成不同版本时，必须使用本 skill。
---

# BF006 固件编译

## 默认任务

用户只说“编译程序”或相近意思而未指定版本时，生成以下四个版本，日期使用执行当天的 `yyMMdd`：

| 版本 | `Model_Device` 宏 | 输出文件 |
|---|---|---|
| 220V 1C1 | `Model_1PH_220_1C1_LCD` | `BF006_220V_1C1_yyMMdd.hex` |
| 220V 1C2 | `Model_1PH_220_1C2_LCD` | `BF006_220V_1C2_yyMMdd.hex` |
| 380V 1C1 | `Model_3PH_380_1C1_LCD` | `BF006_380V_1C1_yyMMdd.hex` |
| 380V 1C2 | `Model_3PH_380_1C2_LCD` | `BF006_380V_1C2_yyMMdd.hex` |

将交付文件写入项目根目录的 `烧录文件` 文件夹。

项目还定义了 `Model_3PH_220_1C1_LCD` 和 `Model_3PH_220_1C2_LCD`，但尚未确认它们的交付文件命名。仅在用户明确要求且给出命名规则后编译这两个版本，禁止与单相 220V 文件同名。

## 执行流程

1. 按 UTF-8 读取 `AGENTS.md`，遵守项目路径、编码和复核规则，不读取参考程序。
2. 运行已归档的写入型工具：

   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File ".\tools\archive\build_and_write_bf006_hex.ps1"
   ```

3. 用户明确指定上述四个版本的子集时，传入 `-Variant`，例如：

   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File ".\tools\archive\build_and_write_bf006_hex.ps1" -Variant 220V_1C1,380V_1C2
   ```

4. 使用 Keil 的全量重编参数 `-r`，不得用旧 HEX、旧时间戳或仅增量编译代替。
5. 以编译日志中的 `0 Error(s)` 判断成功。Keil 在存在警告时可能返回进程退出码 `1`，禁止仅凭退出码误判失败。
6. 确认每轮 HEX 在本轮更新、Intel HEX 每条记录校验和正确、存在结束记录，并确认多版本 SHA-256 互不相同。
7. 确认 `system.h` 在结束或异常后均恢复到执行前的原始字节与 SHA-256；临时宏切换不得改变源码编码、BOM 或换行。
8. 向用户报告文件链接、各版本错误数/警告数、文件大小和校验结论。原有警告需如实报告，不因警告删除交付文件。

## 关键约束

- 优先使用 `D:\Professional\keil5 for C51\Installed\UV4\UV4.exe`；工具可自动尝试已知备用路径。
- Keil 工程固定输出名目前是 `BF006三相380一控一.hex`，它不代表当前宏版本；每次重编后立即按宏映射暂存，禁止根据该固定名称推断版本。
- 仅执行编译且最终源码无变化时，不写 `程序修改记录.md`；修改编译规则、skill 或工具时必须写入修改记录。
- 工具在内存中暂存全部成功产物，所有请求版本均通过后再写入交付目录，降低半成品混入风险。

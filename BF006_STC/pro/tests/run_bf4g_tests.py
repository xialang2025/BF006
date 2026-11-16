"""Compile the firmware protocol with simulated UART4 and Timer 3 registers."""

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

TESTS = Path(__file__).resolve().parent
PROJECT = TESTS.parent
SRC = PROJECT.parent / "src"

REGISTERS = ("S4CON S4BUF ACC IE2 P_SW2 T4T3M TH4 TL4 TH3 TL3 AUXINTIF EA "
             "LowWater_M LowWater_H HighWater_M HighWater_H PUMP_ADD OverFlow_P P34").split()


def run_model(model, model_byte, phases, pumps, volts):
    with tempfile.TemporaryDirectory(prefix="bf006-4g-") as scratch:
        directory = Path(scratch)
        header = (PROJECT / "STC8xxxx.H").read_text(encoding="cp936")
        register_macros = re.findall(
            r"^#define\s+(?:(?:S4_|[RT]I4|CLR_[RT]I4|Timer[34]_|T[34]_Load|T3IF)\w*)[^\n]*",
            header,
            re.MULTILINE,
        )
        config = (PROJECT / "config.h").read_text(encoding="cp936")
        clock = re.search(r"^#define MAIN_Fosc\s+\d+L", config, re.MULTILINE)[0]
        (directory / "config.h").write_text(
            "#ifndef TEST_CONFIG_H\n#define TEST_CONFIG_H\n"
            "typedef unsigned char u8;\n"
            "typedef unsigned short u16;\n"
            "typedef unsigned int u32;\n"
            "typedef u8 bit;\n"
            "#define xdata\n#define code\n"
            + clock + "\nextern u8 " + ", ".join(REGISTERS) + ";\n"
            + "\n".join(register_macros) + "\n#endif\n",
            encoding="cp936",
        )
        system = (SRC / "system.h").read_text(encoding="utf-8")
        definitions = system[system.index("#define ERR_UVP"):system.index("}sys_t;") + 7]
        definitions = re.sub(r"^#define Model_Device \w+", f"#define Model_Device {model}",
                             definitions, flags=re.MULTILINE)
        (directory / "system.h").write_text(
            '#ifndef TEST_SYSTEM_H\n#define TEST_SYSTEM_H\n#include "config.h"\n'
            + definitions + '\nextern sys_t sys;\nextern bit Boot_Flag;\n'
            'extern bit water_status_ready, sensor_sample_ready;\nvoid updatSysdata(void);\n'
            '#include "configPar.h"\n#endif\n', encoding="utf-8"
        )
        for name in ("uart4.h", "configPar.h", "Modbus_New.h", "Modbus.h", "ad.h", "Display.h", "HT1622.h", "user.h"):
            shutil.copyfile(SRC / name, directory / name)
        shutil.copyfile(SRC / "EEPROM.h", directory / "eeprom.h")
        for name, encoding in (("uart4.c", "cp936"), ("updata4G.c", "utf-8"),
                               ("configPar.c", "utf-8")):
            source = (SRC / name).read_text(encoding=encoding)
            source = re.sub(r"\binterrupt\s+\w+", "", source)
            (directory / name).write_text(source, encoding="utf-8")
        system_source = (SRC / "system.c").read_text(encoding="cp936")
        readiness = system_source[system_source.index("static volatile bit water_inputs_ready"):
                                  system_source.index("bit sensor_sample_ready;") + 24]
        sampling = system_source[system_source.index("void Water_Level(void)"):
                                 system_source.index("void RELAY_Auto_Control(void)")]
        scaling = system_source[system_source.index("void updatSysdata(void)"):
                                system_source.index("void Write_Fault_Mess(")]
        (directory / "sampling.c").write_text(
            '#include "system.h"\n#include "ad.h"\nextern void Speak_Con(u8 value);\n'
            + readiness + "\n" + scaling + sampling, encoding="utf-8"
        )
        runner = directory / "bf4g_uart_test.exe"
        subprocess.run(
            [os.environ.get("CC", "gcc"), "-std=c99", "-Wall", "-fpack-struct=1",
             f"-DTEST_MODEL_BYTE={model_byte}", f"-DTEST_PHASES={phases}",
             f"-DTEST_PUMPS={pumps}", f"-DTEST_VOLTS={volts}",
             "-I", str(directory), str(TESTS / "bf4g_uart_test.c"),
             str(directory / "uart4.c"), str(directory / "updata4G.c"),
             str(directory / "sampling.c"), str(directory / "configPar.c"), "-o", str(runner)],
            check=True,
        )
        subprocess.run([str(runner)], check=True)


if __name__ == "__main__":
    run_model("Model_1PH_220_1C1_LCD", 0xa2, 1, 1, 220)
    run_model("Model_1PH_220_1C2_LCD", 0xa3, 1, 2, 220)
    run_model("Model_3PH_380_1C1_LCD", 0xb2, 3, 1, 380)
    run_model("Model_3PH_380_1C2_LCD", 0xb3, 3, 2, 380)

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

REGISTERS = ("S4CON S4BUF ACC IE2 P_SW2 T4T3M TH4 TL4 TH3 TL3 AUXINTIF EA").split()


def main():
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
        model = system[system.index("#define V1_Limit_Current"):system.index("typedef struct")]
        (directory / "system.h").write_text(
            '#include "configPar.h"\n' + model, encoding="utf-8"
        )
        for name in ("uart4.h", "configPar.h", "Modbus_New.h", "Modbus.h"):
            shutil.copyfile(SRC / name, directory / name)
        for name, encoding in (("uart4.c", "cp936"), ("updata4G.c", "utf-8")):
            source = (SRC / name).read_text(encoding=encoding)
            source = re.sub(r"\binterrupt\s+\w+", "", source)
            (directory / name).write_text(source, encoding="utf-8")
        runner = directory / "bf4g_uart_test.exe"
        subprocess.run(
            [os.environ.get("CC", "gcc"), "-std=c99", "-Wall",
             "-I", str(directory), str(TESTS / "bf4g_uart_test.c"),
             str(directory / "uart4.c"), str(directory / "updata4G.c"), "-o", str(runner)],
            check=True,
        )
        subprocess.run([str(runner)], check=True)


if __name__ == "__main__":
    main()

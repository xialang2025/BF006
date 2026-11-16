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
             "LowWater_M LowWater_H HighWater_M HighWater_H PUMP_ADD OverFlow_P P34 RELAY1 RELAY2 RELAY3").split()


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
            'extern bit water_status_ready, sensor_sample_ready, sensor_alarm_pending;\n'
            'extern u8 sensor_pending_tanks;\nvoid updatSysdata(void);\nvoid sensorOverFlowCheck(void);\n'
            '#define SENSOR_PENDING_UPPER 1\n#define SENSOR_PENDING_LOWER 2\n'
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
                                  system_source.index("bit sensor_alarm_pending;") + len("bit sensor_alarm_pending;")]
        sampling = system_source[system_source.index("void Water_Level(void)"):
                                 system_source.index("static bit cycle_pump_ready(void)")]
        scaling = system_source[system_source.index("void updatSysdata(void)"):
                                system_source.index("void Write_Fault_Mess(")]
        cycle = system_source[system_source.index("static bit cycle_pump_ready(void)"):
                              system_source.index("void FanXiu_pro(void)")]
        rust = system_source[system_source.index("void FanXiu_pro(void)"):
                             system_source.index("\n}", system_source.index("void FanXiu_pro(void)")) + 2]
        error_state = system_source[system_source.index("void Fault_Error_Clear("):
                                    system_source.index("void Pump_Current_Check(")]
        error_state = error_state.split("#if (Model_Voltage == Model_3PH)")[0]
        fault_checks = system_source[system_source.index("u8 check_A_Fault(void)"):
                                     system_source.index("void Set_Pump1_OverLoad_Err(")]
        voltage_checks = system_source[system_source.index("u8 Check_UVP(void)"):
                                       system_source.index("void LosePhase_Check(void)")]
        recovery = system_source[system_source.index("void ReStart_Clear_Fault(void)"):
                                 system_source.index("void zhengdingA(void)")]
        recovery_limit = re.search(r"^#define Fault_UVP_Recovery_Time \d+", system_source, re.MULTILINE)[0]
        (directory / "sampling.c").write_text(
            '#include "system.h"\n#include "ad.h"\n'
            'extern void Speak_Con(u8 value);\nextern void Speak_Stop(void);\n'
            + recovery_limit + "\n"
            + readiness + "\n" + scaling + sampling + cycle + rust + "\n"
            + error_state + fault_checks + voltage_checks + recovery, encoding="utf-8"
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

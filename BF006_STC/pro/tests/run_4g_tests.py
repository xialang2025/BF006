from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time


# 用 Keil C51 构建临时测试工程，再用 µCsim 执行其机器码。
# 用法：python -X utf8 run_4g_tests.py 测试名 机型宏 s51.exe [--old-protocol]
def run_test(name, model, simulator, old_protocol=False):
    firmware = Path(__file__).resolve().parents[2]
    keil = next(path for path in (
        Path(r'D:\Professional\keil5 for C51\Installed\UV4\UV4.exe'),
        Path(r'C:\Keil_v5\UV4\UV4.exe'),
        Path(r'C:\Keil\UV4\UV4.exe'),
    ) if path.is_file())

    with tempfile.TemporaryDirectory(prefix='bf006-test-') as temporary:
        root = Path(temporary)
        src = root / 'src'
        pro = root / 'pro'
        pro.mkdir()
        shutil.copytree(firmware / 'src', src, ignore=shutil.ignore_patterns('*-Conflict-*'))
        for file in ('STARTUP.A51', 'config.h', 'STC8xxxx.H'):
            shutil.copyfile(firmware / 'pro' / file, pro / file)
        shutil.copyfile(firmware / 'pro' / 'tests' / f'{name}.c', pro / 'test.c')

        system = src / 'system.h'
        content, count = re.subn(rb'(?m)^#define Model_Device \w+',
                                 b'#define Model_Device ' + model.encode('ascii'),
                                 system.read_bytes())
        assert count == 1
        system.write_bytes(content)
        if old_protocol:
            modbus = src / 'Modbus.h'
            modbus.write_bytes(modbus.read_bytes().replace(b'MODBUS_USE_OLD_PROTOCOL 0',
                                                           b'MODBUS_USE_OLD_PROTOCOL 1'))

        project = (firmware / 'pro' / 'BF006.uvproj').read_text(encoding='utf-8-sig')
        project = re.sub(r'<OutputName>.*?</OutputName>', '<OutputName>BF006Test</OutputName>', project)
        project = project.replace(r'<FilePath>.\main.c</FilePath>', r'<FilePath>.\test.c</FilePath>')
        project = project.replace('<FileName>main.c</FileName>', '<FileName>test.c</FileName>')
        if name == 'config_store_test':
            project = re.sub(r'<File>\s*<FileName>EEPROM\.c</FileName>.*?</File>', '', project, flags=re.S)
            project = project.replace('<Define></Define>',
                                      '<Define>BF006_CONFIG_SLOT_A=512,BF006_CONFIG_SLOT_B=1024</Define>', 1)
        else:
            project = re.sub(r'<File>\s*<FileName>RS485\.c</FileName>.*?</File>', '', project, flags=re.S)
        if name == 'uart4_context_test':
            project = project.replace('<Define></Define>', '<Define>BF006_CONTEXT_SIMULATION</Define>', 1)
        (pro / 'test.uvproj').write_text(project, encoding='utf-8')
        subprocess.run([keil, '-r', 'test.uvproj', '-j0', '-o', 'build.log'], cwd=pro, timeout=45)
        for _ in range(100):
            build_log = (pro / 'build.log')
            build = build_log.read_text(encoding='gbk', errors='replace') if build_log.exists() else ''
            if re.search(r'\d+ Error\(s\)|Target not created\.', build):
                break
            time.sleep(0.1)
        if '0 Error(s)' not in build or 'UNRESOLVED EXTERNAL' in build:
            raise AssertionError(build)
        print(f'{model} {name}: {next(line for line in build.splitlines() if "0 Error(s)" in line)}', flush=True)
        listing = (pro / 'Listings' / 'BF006Test.M51').read_text(encoding='gbk', errors='replace')
        stop = re.search(r'C:([0-9A-F]+)H\s+PUBLIC\s+TEST_DONE\b', listing, re.I).group(1)
        hex_file = pro / 'Objects' / 'BF006Test.hex'
        if not hex_file.is_file():
            raise AssertionError('Keil 未生成测试映像')
        result = subprocess.run(
            [simulator, '-t', 'C52', '-c', '-'],
            input=f'file "Objects/BF006Test.hex"\nrun 0 0x{stop}\ndx 0x0BFE 0x0BFE\nquit\n', text=True,
            capture_output=True, cwd=pro, timeout=30, check=True,
        ).stdout
        value = re.search(r'^0x0*bfe\s+([0-9a-f]{2})\b', result, re.I | re.M)
        if not re.search(rf'Stop at 0x0*{stop.lstrip("0")}:', result, re.I) or not value:
            raise AssertionError(result)
        if int(value.group(1), 16) != 0:
            raise AssertionError(f'BF006_TEST_RESULT={int(value.group(1), 16)}')
        print('BF006_TEST_RESULT=0', flush=True)


if __name__ == '__main__':
    run_test(sys.argv[1], sys.argv[2], sys.argv[3], '--old-protocol' in sys.argv[4:])

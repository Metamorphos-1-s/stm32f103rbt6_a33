"""Read-only preflight, app-only resource install, and exact current-image restore."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools/stage5pa13c'))
from hardware_shadow import SerialTransport, ModbusClient, probe

PROGRAMMER = 'E:/ST/STM32CubeCLT_1.18.0/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe'
APP_BYTES = 126976
CONFIG_BYTES = 4096


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def read_identity():
    with SerialTransport('COM5', 115200, 'N', 1, 350) as port:
        return probe(ModbusClient(port, 1))


def programmer(args, log):
    result = subprocess.run([PROGRAMMER] + args, capture_output=True)
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_bytes(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError('programmer failed: ' + str(log))


def dump_current(out):
    out.mkdir(parents=True, exist_ok=True)
    identity = read_identity()
    save(out / 'preflight.json', identity)
    app = out / 'application_before.bin'
    config = out / 'config_before.bin'
    programmer(['-c', 'port=SWD', 'mode=HotPlug', '-u', '0x08000000', '0x1F000', str(app)], out / 'application_dump.log')
    programmer(['-c', 'port=SWD', 'mode=HotPlug', '-u', '0x0801F000', '0x1000', str(config)], out / 'config_dump.log')
    if app.stat().st_size != APP_BYTES or config.stat().st_size != CONFIG_BYTES:
        raise RuntimeError('unexpected dump size')
    record = dict(identity=identity, application_sha256=sha(app), config_sha256=sha(config),
                  application_bytes=APP_BYTES, config_bytes=CONFIG_BYTES)
    save(out / 'backup_identity.json', record)
    return record


def install_resource(image, backup, out):
    out.mkdir(parents=True, exist_ok=True)
    expected = json.loads((backup / 'backup_identity.json').read_text(encoding='utf-8'))
    config = backup / 'config_before.bin'
    current_config = out / 'config_before_install.bin'
    programmer(['-c', 'port=SWD', 'mode=HotPlug', '-u', '0x0801F000', '0x1000', str(current_config)], out / 'config_before_install.log')
    if sha(current_config) != expected['config_sha256']:
        raise RuntimeError('configuration changed before diagnostic install')
    programmer(['-c', 'port=SWD', 'mode=UR', 'reset=HWrst', '-e', '[0 123]', '-w', str(image),
                '0x08000000', '-v', '-rst'], out / 'program_verify.log')
    app = out / 'application_after_install.bin'
    cfg = out / 'config_after_install.bin'
    programmer(['-c', 'port=SWD', 'mode=HotPlug', '-u', '0x08000000', '0x1F000', str(app)], out / 'application_after_install.log')
    programmer(['-c', 'port=SWD', 'mode=HotPlug', '-u', '0x0801F000', '0x1000', str(cfg)], out / 'config_after_install.log')
    if app.read_bytes()[:image.stat().st_size] != image.read_bytes() or any(v != 0xFF for v in app.read_bytes()[image.stat().st_size:]):
        raise RuntimeError('diagnostic application verify mismatch')
    if sha(cfg) != expected['config_sha256']:
        raise RuntimeError('diagnostic install changed configuration')
    save(out / 'boot.json', read_identity())


def restore_current(backup, out):
    out.mkdir(parents=True, exist_ok=True)
    expected = json.loads((backup / 'backup_identity.json').read_text(encoding='utf-8'))
    app = backup / 'application_before.bin'
    cfg = backup / 'config_before.bin'
    current = out / 'config_before_restore.bin'
    programmer(['-c', 'port=SWD', 'mode=HotPlug', '-u', '0x0801F000', '0x1000', str(current)], out / 'config_before_restore.log')
    if sha(current) != expected['config_sha256']:
        raise RuntimeError('configuration changed; refuse blind restore')
    programmer(['-c', 'port=SWD', 'mode=UR', 'reset=HWrst', '-e', '[0 123]', '-w', str(app),
                '0x08000000', '-v', '-rst'], out / 'restore_verify.log')
    after = out / 'application_after_restore.bin'
    config = out / 'config_after_restore.bin'
    programmer(['-c', 'port=SWD', 'mode=HotPlug', '-u', '0x08000000', '0x1F000', str(after)], out / 'application_after_restore.log')
    programmer(['-c', 'port=SWD', 'mode=HotPlug', '-u', '0x0801F000', '0x1000', str(config)], out / 'config_after_restore.log')
    if after.read_bytes() != app.read_bytes() or config.read_bytes() != cfg.read_bytes():
        raise RuntimeError('full application/config restore mismatch')
    terminal = read_identity()
    save(out / 'terminal.json', terminal)
    return terminal

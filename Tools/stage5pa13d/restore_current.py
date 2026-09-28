"""Restore only this run's exact 0x051D backup; no historical overwrite."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools/stage5pa13c'))
from hardware_shadow import SerialTransport, ModbusClient, probe

PROGRAMMER = 'E:/ST/STM32CubeCLT_1.18.0/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def restore(output):
    output.mkdir(parents=True, exist_ok=True)
    backup = ROOT / 'Results/stage5pa13d/hardware/backup/application.bin'
    config = ROOT / 'Results/stage5pa13d/hardware/backup/config_region.bin'
    identity = json.loads((ROOT / 'Results/stage5pa13d/hardware/backup/identity.json').read_text())
    if backup.stat().st_size != 126976 or config.stat().st_size != 4096:
        raise RuntimeError('backup size mismatch')
    if digest(backup) != identity['application_sha256'] or digest(config) != identity['config_sha256']:
        raise RuntimeError('backup hash mismatch')
    def cli(args, log):
        result = subprocess.run([PROGRAMMER] + args, capture_output=True)
        (output / log).write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError('programmer failed: ' + log)
    before = output / 'config_before_restore.bin'
    cli(['-c', 'port=SWD', 'mode=HotPlug', '-u', '0x0801F000', '0x1000', str(before)], 'config_before_restore.log')
    if before.read_bytes() != config.read_bytes():
        raise RuntimeError('unexpected config change: no blind overwrite of user config')
    cli(['-c', 'port=SWD', 'mode=UR', 'reset=HWrst', '-e', '[0 123]',
         '-w', str(backup), '0x08000000', '-v', '-rst'], 'restore_verify.log')
    after = output / 'application_after_restore.bin'
    cfg_after = output / 'config_after_restore.bin'
    cli(['-c', 'port=SWD', 'mode=HotPlug', '-u', '0x08000000', '0x1F000', str(after)], 'application_after_restore.log')
    cli(['-c', 'port=SWD', 'mode=HotPlug', '-u', '0x0801F000', '0x1000', str(cfg_after)], 'config_after_restore.log')
    if after.read_bytes() != backup.read_bytes() or cfg_after.read_bytes() != config.read_bytes():
        raise RuntimeError('restoration byte mismatch')
    with SerialTransport('COM5', 115200, 'N', 1, 350) as transport:
        terminal = probe(ModbusClient(transport, 1))
    (output / 'terminal.json').write_text(json.dumps(terminal, indent=2) + '\n')
    if terminal['firmware'] != '0x051D' or terminal['map'] != '0x0105' or terminal['r5']['mode'] or terminal['r5']['application'] or terminal['r5']['offset_ug']:
        raise RuntimeError('restored unsafe identity/mode')
    if terminal['fault_mask'] or terminal['overrun_count'] or terminal['dirty']:
        raise RuntimeError('restored unsafe counters')
    return terminal


if __name__ == '__main__':
    print(json.dumps(restore(Path(sys.argv[1]))))

"""Restore this A13D-R run's own complete ordinary application/config proof."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Tools/stage5pa13c'))
from hardware_shadow import SerialTransport,ModbusClient,probe
PROGRAMMER='E:/ST/STM32CubeCLT_1.18.0/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe'

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest().upper()
def restore(output):
    output.mkdir(parents=True,exist_ok=True)
    directory=ROOT/'Results/stage5pa13dr/hardware/backup'
    app=directory/'application.bin'; config=directory/'config_region.bin'
    identity=json.loads((directory/'identity.json').read_text())
    if app.stat().st_size!=126976 or config.stat().st_size!=4096 or sha(app)!=identity['application_sha256'] or sha(config)!=identity['config_sha256']:
        raise RuntimeError('current-run backup invalid')
    def cli(args,log):
        r=subprocess.run([PROGRAMMER]+args,capture_output=True)
        (output/log).write_bytes(r.stdout+r.stderr)
        if r.returncode:raise RuntimeError('restore programmer failed: '+log)
    before=output/'config_before.bin'
    cli(['-c','port=SWD','mode=HotPlug','-u','0x0801F000','0x1000',str(before)],'config_before.log')
    if before.read_bytes()!=config.read_bytes():raise RuntimeError('unexpected user config change; no overwrite')
    cli(['-c','port=SWD','mode=UR','reset=HWrst','-e','[0 123]','-w',str(app),'0x08000000','-v','-rst'],'restore_verify.log')
    after=output/'application_after.bin'; cfg=output/'config_after.bin'
    cli(['-c','port=SWD','mode=HotPlug','-u','0x08000000','0x1F000',str(after)],'application_after.log')
    cli(['-c','port=SWD','mode=HotPlug','-u','0x0801F000','0x1000',str(cfg)],'config_after.log')
    if after.read_bytes()!=app.read_bytes() or cfg.read_bytes()!=config.read_bytes():raise RuntimeError('restore byte mismatch')
    with SerialTransport('COM5',115200,'N',1,350) as transport:
        terminal=probe(ModbusClient(transport,1))
    (output/'terminal.json').write_text(json.dumps(terminal,indent=2)+'\n')
    if terminal['firmware']!='0x051D' or terminal['map']!='0x0105' or terminal['r5']['mode'] or terminal['r5']['application'] or terminal['r5']['offset_ug'] or terminal['dirty'] or terminal['fault_mask'] or terminal['overrun_count']:
        raise RuntimeError('restored unsafe terminal')
    return terminal

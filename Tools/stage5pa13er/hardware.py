"""A13E-R scoped preflight/backup, app-only installation and exact recovery."""
import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Tools/stage5pa13c'))
sys.path.insert(0,str(ROOT/'Tools/stage5b_hw'))
from hardware_shadow import SerialTransport,ModbusClient,probe
from parse_config_slots import parse_dump

PROGRAMMER='E:/ST/STM32CubeCLT_1.18.0/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe'
BASE=ROOT/'Results/stage5pa13er/hardware'
ORDINARY_SHA='DDF57FB131E5EF675D94B08A5A8752EB0FB588BA5230DEA6F34C57D6B2F62DDD'
EXPECTED_CONFIG_SHA='856BD8F5C14760561FC4BFEC4274FE0C5480C19BC4422046617C2450F8439733'

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest().upper()
def save(path,obj):path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(obj,indent=2)+'\n')
def programmer(arguments,log):
    result=subprocess.run([PROGRAMMER]+arguments,capture_output=True)
    log.parent.mkdir(parents=True,exist_ok=True)
    log.write_bytes(result.stdout+result.stderr)
    if result.returncode:raise RuntimeError('programmer failed: '+str(log))
def read_identity():
    with SerialTransport('COM5',115200,'N',1,350) as port:
        return probe(ModbusClient(port,1))
def require_ordinary(identity):
    cfg=identity['configuration'];r5=identity['r5']
    if (identity['firmware'],identity['map'])!=('0x051D','0x0105') or (
        cfg['sample_rate'],cfg['filter_mode'],cfg['filter_strength'],cfg['gain'])!=(0,1,3,3) or (
        cfg['raw_zero'],cfg['raw_span'],cfg['span_mass_ug'])!=(41868,485780,500000000) or (
        identity['revision'],identity['saved_revision'])!=(19,19) or (
        identity['dirty'] or identity['fault_mask'] or identity['overrun_count'] or
        not identity['calibration_valid'] or cfg['persistent_format']!=3 or
        identity['checkweigh']['mode'] or r5['application'] or r5['mode'] or
        r5['offset_ug'] or r5['limited'] or r5['save_request_count_low']):
        raise RuntimeError('unexplained current identity/config/volatile difference; stop before SWD')

def backup():
    pre=read_identity()
    save(BASE/'preflight.json',pre)
    require_ordinary(pre)
    location=BASE/'backup';location.mkdir(parents=True,exist_ok=True)
    app=location/'application.bin';cfg=location/'config_region.bin'
    programmer(['-c','port=SWD','mode=HotPlug','-u','0x08000000','0x1F000',str(app)],location/'application_dump.log')
    programmer(['-c','port=SWD','mode=HotPlug','-u','0x0801F000','0x1000',str(cfg)],location/'config_dump.log')
    reference=ROOT/'Results/stage5pa13er/software/ordinary_051D_rebuilt.bin'
    if (len(app.read_bytes())!=126976 or len(cfg.read_bytes())!=4096 or digest(reference)!=ORDINARY_SHA or
        app.read_bytes()[:reference.stat().st_size]!=reference.read_bytes() or
        any(x!=255 for x in app.read_bytes()[reference.stat().st_size:]) or
        digest(cfg)!=EXPECTED_CONFIG_SHA):
        raise RuntimeError('readback differs from exact 051D ordinary application or known V3; no flash')
    slots=parse_dump(cfg,modbus_active_slot='A' if pre['configuration']['slot']==1 else 'B',
                     modbus_sequence=pre['configuration']['storage_sequence'])
    save(location/'slots.json',slots)
    if not all(s['valid'] for s in slots['slots']) or not all(slots['modbus_comparison'][k]
        for k in ('active_slot_matches','sequence_matches')):raise RuntimeError('V3 slots mismatch')
    identity=dict(application_sha256=digest(app),config_sha256=digest(cfg),
        application_bytes=126976,config_bytes=4096,ordinary_sha256=ORDINARY_SHA,
        firmware=pre['firmware'],map=pre['map'],revision=19,saved_revision=19,
        active_slot=slots['active_slot'])
    save(location/'identity.json',identity)
    return identity

def install(image,firmware,map_,out):
    identity=json.loads((BASE/'backup/identity.json').read_text())
    gate=json.loads((ROOT/'Results/stage5pa13er/software/software_gate.json').read_text())
    if image.stat().st_size>126976 or digest(image)!=gate['images'][image.name]['sha256'] or (
        image.stat().st_size!=gate['images'][image.name]['bytes']):
        raise RuntimeError('image changed')
    out.mkdir(parents=True,exist_ok=True)
    current=out/'config_before.bin'
    programmer(['-c','port=SWD','mode=HotPlug','-u','0x0801F000','0x1000',str(current)],out/'config_before.log')
    if digest(current)!=identity['config_sha256']:raise RuntimeError('configuration changed before install')
    programmer(['-c','port=SWD','mode=UR','reset=HWrst','-e','[0 123]',
        '-w',str(image),'0x08000000','-v','-rst'],out/'program_verify.log')
    app=out/'app_after.bin';cfg=out/'config_after.bin'
    programmer(['-c','port=SWD','mode=HotPlug','-u','0x08000000','0x1F000',str(app)],out/'application_readback.log')
    programmer(['-c','port=SWD','mode=HotPlug','-u','0x0801F000','0x1000',str(cfg)],out/'config_readback.log')
    if app.read_bytes()[:image.stat().st_size]!=image.read_bytes() or any(x!=255 for x in app.read_bytes()[image.stat().st_size:]) or digest(cfg)!=identity['config_sha256']:
        raise RuntimeError('application prefix/FF tail/config verification failed; recover from current backup')
    boot=read_identity();save(out/'boot.json',boot)
    if (boot['firmware'],boot['map'])!=(firmware,map_) or boot['dirty'] or boot['fault_mask'] or boot['overrun_count'] or boot['revision']!=19 or boot['saved_revision']!=19 or boot['r5']['mode'] or boot['r5']['application'] or boot['r5']['offset_ug'] or boot['checkweigh']['mode']:
        raise RuntimeError('unexpected boot identity/default OFF; immediately restore')
    return boot

def restore(out):
    out.mkdir(parents=True,exist_ok=True)
    store=BASE/'backup';identity=json.loads((store/'identity.json').read_text())
    app=store/'application.bin';cfg=store/'config_region.bin'
    if app.stat().st_size!=126976 or cfg.stat().st_size!=4096 or digest(app)!=identity['application_sha256'] or digest(cfg)!=identity['config_sha256']:
        raise RuntimeError('this-run backup invalid; no destructive restoration')
    before=out/'config_before.bin'
    programmer(['-c','port=SWD','mode=HotPlug','-u','0x0801F000','0x1000',str(before)],out/'config_before.log')
    if before.read_bytes()!=cfg.read_bytes():raise RuntimeError('configuration changed; refuse blind overwrite')
    programmer(['-c','port=SWD','mode=UR','reset=HWrst','-e','[0 123]',
        '-w',str(app),'0x08000000','-v','-rst'],out/'restore_verify.log')
    after=out/'application_after.bin';config=out/'config_after.bin'
    programmer(['-c','port=SWD','mode=HotPlug','-u','0x08000000','0x1F000',str(after)],out/'application_after.log')
    programmer(['-c','port=SWD','mode=HotPlug','-u','0x0801F000','0x1000',str(config)],out/'config_after.log')
    if after.read_bytes()!=app.read_bytes() or config.read_bytes()!=cfg.read_bytes():
        raise RuntimeError('full 126976/4096-byte recovery mismatch')
    terminal=read_identity();save(out/'terminal.json',terminal)
    require_ordinary(terminal)
    return terminal

def main():
    p=argparse.ArgumentParser();p.add_argument('--action',choices=('preflight','install-resource','install-active','restore'),required=True)
    args=p.parse_args()
    if args.action=='preflight': print(json.dumps(backup()))
    elif args.action=='restore': print(json.dumps(restore(BASE/'manual_restore')))
    else:
        name='resource_0x0521.bin' if args.action=='install-resource' else 'active_0x0520.bin'
        fw='0x0521' if args.action=='install-resource' else '0x0520'
        mp='0x010A' if args.action=='install-resource' else '0x0109'
        location=BASE/('resource_flash' if args.action=='install-resource' else 'active_flash')
        try:
            print(json.dumps(install(ROOT/'Results/stage5pa13er/software'/name,fw,mp,location)))
        except Exception as exc:
            save(location/'install_failure.json',dict(error=str(exc)))
            try:
                restore(location/'failure_rollback')
            except Exception as rollback_error:
                save(location/'rollback_failure.json',dict(error=str(rollback_error)))
                raise RuntimeError('install failed; rollback not proved: '+str(rollback_error)) from exc
            raise RuntimeError('install failed; exact ordinary restored: '+str(exc)) from exc
if __name__=='__main__':main()

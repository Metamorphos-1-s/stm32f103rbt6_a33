#!/usr/bin/env python3
import argparse, hashlib, json, re, subprocess, time
from pathlib import Path

MAGIC=0x354C4447

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def run(args):
    return subprocess.run(args,check=True,capture_output=True,text=True,
                          encoding='utf-8',errors='replace').stdout
def address(elf):
    text=run(['arm-none-eabi-nm','-n',elf])
    match=re.search(r'^([0-9a-fA-F]+)\s+B\s+g_stage5l_measurement_control$',text,re.M)
    if not match: raise RuntimeError('diagnostic control symbol missing')
    return int(match.group(1),16)
def read(programmer,base):
    text=run([programmer,'-c','port=SWD','mode=HotPlug','-r32',hex(base),'48'])
    words=[]
    for line in text.splitlines():
        if re.match(r'^0x[0-9A-Fa-f]+\s*:',line):
            words.extend(int(x,16) for x in re.findall(r'\b[0-9A-Fa-f]{8}\b',line.split(':',1)[1]))
    if len(words)!=12 or words[0]!=MAGIC or words[1]!=1: raise RuntimeError('invalid diagnostic control block')
    keys=('magic','version','request_sequence','applied_sequence','command','requested_mode','requested_strength','status','override_active','effective_mode','effective_strength','preserved_dirty')
    return dict(zip(keys,words))
def write(programmer,addr,values):
    run([programmer,'-c','port=SWD','mode=HotPlug','-w32',hex(addr)]+[hex(x) for x in values])
def main():
    p=argparse.ArgumentParser();p.add_argument('--elf',required=True);p.add_argument('--programmer',required=True);p.add_argument('--output',required=True);p.add_argument('--restore',action='store_true');p.add_argument('--mode',type=int);p.add_argument('--strength',type=int);a=p.parse_args()
    if not a.restore and (a.mode is None or a.strength is None): p.error('--mode and --strength are required for apply')
    base=address(a.elf);before=read(a.programmer,base);seq=(before['request_sequence']+1)&0xffffffff
    command=2 if a.restore else 1;mode=0 if a.restore else a.mode;strength=0 if a.restore else a.strength
    write(a.programmer,base+16,[command,mode,strength]);write(a.programmer,base+8,[seq]);time.sleep(.3);after=read(a.programmer,base)
    expected_status=2 if a.restore else 1
    if after['applied_sequence']!=seq or after['status']!=expected_status or (not a.restore and (after['override_active']!=1 or after['effective_mode']!=mode or after['effective_strength']!=strength)) or (a.restore and after['override_active']!=0): raise RuntimeError('diagnostic request failed')
    result={'utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'action':'restore' if a.restore else 'apply','elf':str(Path(a.elf)),'elf_length':Path(a.elf).stat().st_size,'elf_sha256':sha(a.elf),'control_address':f'0x{base:08X}','before':before,'after':after}
    Path(a.output).parent.mkdir(parents=True,exist_ok=True);Path(a.output).write_bytes((json.dumps(result,indent=2)+'\n').encode('utf-8'));print(json.dumps(result));return 0
if __name__=='__main__':raise SystemExit(main())

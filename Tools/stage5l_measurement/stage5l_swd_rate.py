#!/usr/bin/env python3
import argparse, hashlib, json, re, subprocess, time
from pathlib import Path

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def run(a): return subprocess.run(a,check=True,capture_output=True,text=True,encoding='utf-8',errors='replace').stdout
def symbol(elf):
    m=re.search(r'^([0-9a-fA-F]+)\s+B\s+g_stage5l_measurement_control$',run(['arm-none-eabi-nm','-n',elf]),re.M)
    if not m: raise RuntimeError('diagnostic control symbol missing')
    return int(m.group(1),16)
def read(programmer,base):
    text=run([programmer,'-c','port=SWD','mode=HotPlug','-r32',hex(base),'60']);words=[]
    for line in text.splitlines():
        if re.match(r'^0x[0-9A-Fa-f]+\s*:',line):words.extend(int(x,16) for x in re.findall(r'\b[0-9A-Fa-f]{8}\b',line.split(':',1)[1]))
    keys=('magic','version','request_sequence','applied_sequence','command','requested_mode','requested_strength','status','override_active','effective_mode','effective_strength','preserved_dirty','requested_rate','effective_rate','rate_override_active')
    if len(words)!=15 or words[0]!=0x354c4447 or words[1]!=2:raise RuntimeError('invalid diagnostics v2 control block')
    return dict(zip(keys,words))
def write(p,a,v):run([p,'-c','port=SWD','mode=HotPlug','-w32',hex(a),hex(v)])
def main():
    p=argparse.ArgumentParser();p.add_argument('--elf',required=True);p.add_argument('--programmer',required=True);p.add_argument('--output',required=True);p.add_argument('--restore',action='store_true');p.add_argument('--rate',type=int,choices=(0,1));a=p.parse_args()
    if not a.restore and a.rate is None:p.error('--rate is required')
    base=symbol(a.elf);before=read(a.programmer,base);seq=(before['request_sequence']+1)&0xffffffff;command=4 if a.restore else 3;requested=0 if a.restore else a.rate
    write(a.programmer,base+16,command);write(a.programmer,base+48,requested);write(a.programmer,base+8,seq)
    deadline=time.monotonic()+5
    while True:
        after=read(a.programmer,base)
        if after['applied_sequence']==seq:break
        if time.monotonic()>=deadline:raise RuntimeError('rate switch timeout')
        time.sleep(.1)
    expected=2 if a.restore else 1
    if after['status']!=expected or after['rate_override_active']!=(0 if a.restore else 1) or (not a.restore and after['effective_rate']!=requested):raise RuntimeError('rate switch failed')
    result={'utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'action':'restore-rate' if a.restore else 'apply-rate','elf':a.elf,'elf_length':Path(a.elf).stat().st_size,'elf_sha256':sha(a.elf),'control_address':f'0x{base:08X}','before':before,'after':after}
    Path(a.output).parent.mkdir(parents=True,exist_ok=True);Path(a.output).write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps(result));return 0
if __name__=='__main__':raise SystemExit(main())

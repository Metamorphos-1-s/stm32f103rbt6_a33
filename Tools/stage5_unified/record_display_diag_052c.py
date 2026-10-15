"""Non-blocking read-only display-chain recorder for 0x052C."""
import argparse, csv, hashlib, json, sys, time
from datetime import datetime, timezone
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools/stage5pa13c"))
from hardware_shadow import SerialTransport, ModbusClient, word32, word64

def utc(): return datetime.now(timezone.utc).isoformat()
def main():
    p=argparse.ArgumentParser(); p.add_argument("--output",type=Path,required=True)
    p.add_argument("--duration",type=float,default=600); p.add_argument("--stop-file",type=Path)
    p.add_argument("--firmware", type=lambda value: int(value, 0), default=0x052C)
    a=p.parse_args(); a.output.mkdir(parents=True)
    fields=("utc","host_monotonic_ns","sequence","uptime_ms","raw_adc","filtered_raw",
      "net_ug","gross_ug","display_state","display_locked","display_mass_ug","display_anchor_ug",
      "display_threshold_ug","candidate_elapsed_ms","release_reason","r5_application","r5_mode",
      "r5_state","r5_offset_ug","stable","fault","overrun","display_count","weight_view")
    rows=errors=0
    with (a.output/"frames.txt").open("w",encoding="ascii") as frames, (a.output/"samples.csv").open("w",encoding="utf-8",newline="") as data, (a.output/"errors.jsonl").open("w",encoding="utf-8") as err:
      def log(line): frames.write(f"{utc()} {time.monotonic_ns()} {line}\n"); frames.flush()
      w=csv.DictWriter(data,fieldnames=fields); w.writeheader(); data.flush(); end=time.monotonic()+a.duration
      with SerialTransport("COM5",115200,"N",1,350,frame_logger=log) as port:
       c=ModbusClient(port,1)
       while time.monotonic()<end and not (a.stop_file and a.stop_file.exists()):
        try:
         realtime,_=c.read(0,32); diag,_=c.read(0x20,28); active,_=c.read(0x340,40); disp,_=c.read(0x1e0,17); r5,_=c.read(0x280,40)
         if (realtime[14],realtime[15])!=(0x010F,a.firmware): raise RuntimeError("identity changed")
         flags=(realtime[4],realtime[5]); w.writerow(dict(utc=utc(),host_monotonic_ns=time.monotonic_ns(),sequence=word32(diag[0:2],"high"),uptime_ms=word32(diag[2:4],"high"),raw_adc=word32(realtime[28:30],"high"),filtered_raw=word32(realtime[30:32],"high"),net_ug=word64(realtime[16:20],"high"),gross_ug=word64(realtime[20:24],"high"),display_state=disp[0],display_locked=disp[1],display_mass_ug=word64(disp[2:6],"high"),display_anchor_ug=word64(disp[6:10],"high"),display_threshold_ug=word64(disp[10:14],"high"),candidate_elapsed_ms=word32(disp[14:16],"high"),release_reason=disp[16],r5_application=r5[1],r5_mode=r5[2],r5_state=r5[3],r5_offset_ug=word64(r5[6:10],"high"),stable=int(bool(flags[0]&0x10)),fault=word32(diag[25:27],"high"),overrun=word32(diag[13:15],"high"),display_count=word32(realtime[0:2],"high"),weight_view=realtime[13])); data.flush(); rows+=1
        except Exception as ex: errors+=1; err.write(json.dumps({"utc":utc(),"error":str(ex)})+"\n"); err.flush()
        time.sleep(.05)
    s={"rows":rows,"read_errors":errors,"ended_utc":utc(),"samples_sha256":hashlib.sha256((a.output/"samples.csv").read_bytes()).hexdigest().upper()}; (a.output/"summary.json").write_text(json.dumps(s,indent=2)+"\n",encoding="utf-8"); print(json.dumps(s))
if __name__=="__main__": main()

import argparse, csv, json, socket, struct, time
from pathlib import Path

def connect(host, port):
    s = socket.create_connection((host, port), timeout=2.0)
    s.settimeout(1.0)
    return s

def exchange(s, tid, address, quantity):
    pdu = struct.pack('>BHH', 3, address, quantity)
    s.sendall(struct.pack('>HHHB', tid, 0, len(pdu)+1, 1) + pdu)
    head = b''
    while len(head) < 7:
        chunk = s.recv(7-len(head))
        if not chunk: raise ConnectionError('closed before MBAP')
        head += chunk
    rx_tid, proto, length, unit = struct.unpack('>HHHB', head)
    body = b''
    while len(body) < length-1:
        chunk = s.recv(length-1-len(body))
        if not chunk: raise ConnectionError('closed in response')
        body += chunk
    if rx_tid != tid: raise RuntimeError('transaction_id')
    if proto != 0 or unit != 1: raise RuntimeError('mbap')
    if not body or body[0] != 3 or len(body) != 2+quantity*2: raise RuntimeError('response')

def main():
    p=argparse.ArgumentParser(); p.add_argument('--host',default='192.168.1.100'); p.add_argument('--port',type=int,default=502); p.add_argument('--duration-s',type=float,required=True); p.add_argument('--interval-ms',type=int,default=100); p.add_argument('--output',required=True); a=p.parse_args()
    out=Path(a.output); out.parent.mkdir(parents=True,exist_ok=True); started=time.time(); tid=0; ok=bad=timeouts=tid_errors=mbap_errors=reconnects=0; lat=[]; s=connect(a.host,a.port)
    try:
        with out.open('w',newline='',encoding='utf-8') as f:
            w=csv.writer(f); w.writerow(['utc','transaction_id','address','quantity','ok','recovered','latency_ms','error'])
            while time.time()-started < a.duration_s:
                cycle=time.time(); tid=(tid+1)&0xffff or 1; address=32 if ok%10==0 else 0; quantity=2 if address else 16; error=''; success=False; recovered=False; t0=time.time()
                for attempt in range(2):
                    try:
                        exchange(s,tid,address,quantity); success=True; ok+=1; break
                    except (ConnectionError,ConnectionResetError,ConnectionAbortedError,OSError,socket.timeout) as e:
                        error=repr(e)
                        try: s.close()
                        except Exception: pass
                        if attempt==0:
                            time.sleep(0.025); s=connect(a.host,a.port); reconnects+=1; recovered=True; continue
                        if isinstance(e,socket.timeout): timeouts+=1
                        else: bad+=1
                    except RuntimeError as e:
                        error=str(e)
                        if error=='transaction_id': tid_errors+=1
                        elif error=='mbap': mbap_errors+=1
                        else: bad+=1
                        break
                ms=(time.time()-t0)*1000; lat.append(ms); w.writerow([time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),tid,address,quantity,int(success),int(recovered),f'{ms:.3f}',error if not success else '']); f.flush()
                if not success: break
                time.sleep(max(0,a.interval_ms/1000-(time.time()-cycle)))
    finally:
        try: s.close()
        except Exception: pass
    summary={'host':a.host,'port':a.port,'duration_s':round(time.time()-started,3),'interval_ms':a.interval_ms,'requests':ok+bad+timeouts+tid_errors+mbap_errors,'successes':ok,'recoverable_reconnects':reconnects,'bad_responses':bad,'timeouts':timeouts,'transaction_id_errors':tid_errors,'mbap_errors':mbap_errors,'tcp_5000':'checked_separately','latency_min_ms':min(lat) if lat else None,'latency_max_ms':max(lat) if lat else None}
    out.with_name('summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8'); print(json.dumps(summary)); return 0 if not (bad or timeouts or tid_errors or mbap_errors) else 1
if __name__=='__main__': raise SystemExit(main())

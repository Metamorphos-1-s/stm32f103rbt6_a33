import { BleWriteQueue } from './ble-write-queue';
import { CommandResponse, BleFrame, decodeCommandResponse, encodeCommandRequest } from '../protocol/ble-codec';

const READ_ONLY_OPERATIONS = new Set([1, 2]);
interface Pending { id:number; operation:number; bytes:Uint8Array; resolve:(r:CommandResponse)=>void; reject:(e:Error)=>void; timer?:ReturnType<typeof setTimeout>; attempt:number }
export interface CommandMetrics { success:number; timeouts:number; mismatches:number }

export class BleCommandClient {
  private nextId=1; private tail=Promise.resolve(); private pending?:Pending;
  readonly metrics:CommandMetrics={success:0,timeouts:0,mismatches:0};
  constructor(private readonly writer:BleWriteQueue, private readonly timeoutMs=2000) {}
  request(operation:number,data=new Uint8Array(0)):Promise<CommandResponse>{
    if(!READ_ONLY_OPERATIONS.has(operation)) return Promise.reject(new Error('operation is not allowed in read-only Stage 1'));
    const run=()=>this.execute(operation,data); const result=this.tail.then(run); this.tail=result.then(()=>undefined,()=>undefined); return result;
  }
  private execute(operation:number,data:Uint8Array){return new Promise<CommandResponse>((resolve,reject)=>{let id=this.nextId++&0xffff;if(id===0)id=this.nextId++&0xffff;const bytes=encodeCommandRequest({transactionId:id,operation,flags:0,data},0,0);this.pending={id,operation,bytes,resolve,reject,attempt:0};this.sendPending();});}
  private async sendPending(){const p=this.pending;if(!p)return;p.attempt++;try{await this.writer.write(p.bytes);p.timer=setTimeout(()=>{if(this.pending!==p)return;if(p.attempt<2){this.metrics.timeouts++;this.sendPending();}else{this.pending=undefined;this.metrics.timeouts++;p.reject(new Error('BLE command timeout'));}},this.timeoutMs);}catch(e){if(this.pending===p)this.pending=undefined;p.reject(e as Error);}}
  onFrame(frame:BleFrame){if(frame.type!==0x81)return;let response:CommandResponse;try{response=decodeCommandResponse(frame.payload);}catch{return;}const p=this.pending;if(!p||response.transactionId!==p.id||response.operation!==p.operation){this.metrics.mismatches++;return;}if(p.timer)clearTimeout(p.timer);this.pending=undefined;if(response.result!==0){p.reject(new Error(`command result ${response.result}, detail ${response.detailCode}`));return;}this.metrics.success++;p.resolve(response);}
  cancel(reason='BLE connection closed'){this.writer.cancel();const p=this.pending;if(!p)return;if(p.timer)clearTimeout(p.timer);this.pending=undefined;p.reject(new Error(reason));}
}

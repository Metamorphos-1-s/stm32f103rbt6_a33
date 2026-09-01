import { BleAdapter } from './ble-adapter';
export class BleWriteQueue { private tail=Promise.resolve();private generation=0;
 constructor(private readonly adapter:BleAdapter,private readonly deviceId:string,private readonly service:string,private readonly characteristic:string,private readonly withResponse=true,private readonly chunkSize=20){}
 write(bytes:Uint8Array){const generation=this.generation;const run=async()=>{for(let offset=0;offset<bytes.length;offset+=this.chunkSize){if(generation!==this.generation)throw new Error('BLE write cancelled');await this.adapter.write(this.deviceId,this.service,this.characteristic,bytes.slice(offset,Math.min(offset+this.chunkSize,bytes.length)),this.withResponse);}};const result=this.tail.then(run);this.tail=result.then(()=>undefined,()=>undefined);return result;}
 cancel(){this.generation++;}
}

import { BleAdapterError, BleAdapterState, BleCharacteristic, BleDevice, BleDisconnectEvent, BleNotification } from './ble-types';
export { BleDevice } from './ble-types';

export interface BleAdapter {
  open(): Promise<void>; close(): Promise<void>;
  startDiscovery(onDevice:(d:BleDevice)=>void): Promise<void>; stopDiscovery(): Promise<void>;
  connect(id:string, timeoutMs?:number): Promise<void>; disconnect(id:string): Promise<void>;
  discoverServices(id:string): Promise<string[]>;
  discoverCharacteristics(id:string, service:string): Promise<BleCharacteristic[]>;
  subscribe(id:string, service:string, characteristic:string, onData:(data:Uint8Array)=>void): Promise<void>;
  write(id:string,service:string,characteristic:string,data:Uint8Array,withResponse?:boolean): Promise<void>;
  onAdapterStateChange(listener:(state:BleAdapterState)=>void):()=>void;
  onConnectionStateChange(listener:(event:BleDisconnectEvent)=>void):()=>void;
  dispose():void;
}

export const UUID={service:'0000ffe0-0000-1000-8000-00805f9b34fb',notify:'0000ffe1-0000-1000-8000-00805f9b34fb',write:'0000ffe2-0000-1000-8000-00805f9b34fb'};
export const normalizeUuid=(value:string)=>value.toLowerCase().replace(/^0000([0-9a-f]{4})-0000-1000-8000-00805f9b34fb$/,'$1');

function callWx<T>(method:string, options:Record<string,unknown>={}):Promise<T>{return new Promise((resolve,reject)=>{const api=(wx as any)[method];if(typeof api!=='function'){reject(new BleAdapterError('UNSUPPORTED',`${method} unavailable`));return;}api({...options,success:resolve,fail:(e:any)=>reject(mapWxError(e))});});}
function mapWxError(error:any):BleAdapterError { const code=Number(error?.errCode);const message=String(error?.errMsg||'BLE operation failed');if(code===10001)return new BleAdapterError('BLUETOOTH_OFF',message);if([10008,10009,10012].includes(code))return new BleAdapterError('UNSUPPORTED',message);if(/auth|permission|authorize|权限/i.test(message))return new BleAdapterError('PERMISSION_DENIED',message);return new BleAdapterError('OTHER',message); }

export class WxBleAdapter implements BleAdapter {
  private deviceFound?: (event:any)=>void; private valueChanged?: (event:any)=>void;
  private listenersRegistered=false;
  private adapterListeners=new Set<(s:BleAdapterState)=>void>(); private connectionListeners=new Set<(e:BleDisconnectEvent)=>void>();
  private readonly adapterChanged=(s:any)=>this.adapterListeners.forEach(x=>x({available:!!s.available,discovering:!!s.discovering}));
  private readonly connectionChanged=(e:any)=>this.connectionListeners.forEach(x=>x({deviceId:String(e.deviceId),connected:!!e.connected}));
  async open(){await callWx('openBluetoothAdapter');if(!this.listenersRegistered){(wx as any).onBluetoothAdapterStateChange?.(this.adapterChanged);(wx as any).onBLEConnectionStateChange?.(this.connectionChanged);this.listenersRegistered=true;}}
  async close(){await this.stopDiscovery().catch(()=>{});await callWx('closeBluetoothAdapter').catch(()=>{});this.dispose();}
  async startDiscovery(onDevice:(d:BleDevice)=>void){if(this.deviceFound)(wx as any).offBluetoothDeviceFound?.(this.deviceFound);this.deviceFound=(event:any)=>{for(const d of event.devices||[])onDevice({...d,deviceId:String(d.deviceId),lastSeenAt:Date.now()});};(wx as any).onBluetoothDeviceFound(this.deviceFound);await callWx('startBluetoothDevicesDiscovery',{allowDuplicatesKey:true,interval:500});}
  async stopDiscovery(){await callWx('stopBluetoothDevicesDiscovery').catch(()=>{});if(this.deviceFound)(wx as any).offBluetoothDeviceFound?.(this.deviceFound);this.deviceFound=undefined;}
  async connect(deviceId:string,timeoutMs=8000){await callWx('createBLEConnection',{deviceId,timeout:timeoutMs});}
  async disconnect(deviceId:string){await callWx('closeBLEConnection',{deviceId}).catch(()=>{});}
  async discoverServices(deviceId:string){const r:any=await callWx('getBLEDeviceServices',{deviceId});return (r.services||[]).map((s:any)=>String(s.uuid));}
  async discoverCharacteristics(deviceId:string,serviceId:string){const r:any=await callWx('getBLEDeviceCharacteristics',{deviceId,serviceId});return (r.characteristics||[]).map((c:any)=>({uuid:String(c.uuid),properties:c.properties||{}}));}
  async subscribe(deviceId:string,serviceId:string,characteristicId:string,onData:(data:Uint8Array)=>void){if(this.valueChanged)(wx as any).offBLECharacteristicValueChange?.(this.valueChanged);this.valueChanged=(e:BleNotification)=>{if(e.deviceId===deviceId&&normalizeUuid(e.characteristicId)===normalizeUuid(characteristicId))onData(new Uint8Array(e.value as any));};(wx as any).onBLECharacteristicValueChange(this.valueChanged);await callWx('notifyBLECharacteristicValueChange',{deviceId,serviceId,characteristicId,state:true});}
  async write(deviceId:string,serviceId:string,characteristicId:string,data:Uint8Array,withResponse=true){const value=data.buffer.slice(data.byteOffset,data.byteOffset+data.byteLength);await callWx('writeBLECharacteristicValue',{deviceId,serviceId,characteristicId,value,writeType:withResponse?'write':'writeNoResponse'});}
  onAdapterStateChange(listener:(s:BleAdapterState)=>void){this.adapterListeners.add(listener);return()=>this.adapterListeners.delete(listener);}
  onConnectionStateChange(listener:(e:BleDisconnectEvent)=>void){this.connectionListeners.add(listener);return()=>this.connectionListeners.delete(listener);}
  dispose(){if(this.deviceFound)(wx as any).offBluetoothDeviceFound?.(this.deviceFound);if(this.valueChanged)(wx as any).offBLECharacteristicValueChange?.(this.valueChanged);if(this.listenersRegistered){(wx as any).offBluetoothAdapterStateChange?.(this.adapterChanged);(wx as any).offBLEConnectionStateChange?.(this.connectionChanged);}this.listenersRegistered=false;this.deviceFound=undefined;this.valueChanged=undefined;this.adapterListeners.clear();this.connectionListeners.clear();}
}

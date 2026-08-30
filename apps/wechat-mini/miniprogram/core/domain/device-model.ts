import { Int64Value } from './int64-value';
export interface DeviceInfo { protocolVersion:number; firmwareVersion:number; schemaVersion:number; registerMapVersion:number; capabilities:number }
export interface WeightSnapshot { displayMassUg:Int64Value; netMassUg:Int64Value; grossMassUg:Int64Value; tareMassUg:Int64Value; stable:boolean; displayLocked:boolean; overload:boolean; unit:number; decimalPlaces:number; division:number; timestampMs:number; dataAgeMs:number }
export interface SlowStatus { rawCount:number; filteredRaw:number; capacityUg:Int64Value; overloadThresholdUg:Int64Value; persistentDirty:boolean; faultMask:number }
export interface CheckweighStatus { state:number; flags:number; weightSource:number; configRevision:number }
export interface ActiveConfig { raw:Uint8Array; startupAutoZeroEnable?:boolean }
export interface CommunicationStatus { transport:'BLE'|'MODBUS_TCP'|'MODBUS_RTU'; connected:boolean; lastError?:string }
export type ConnectionState='CLOSED'|'OPENING_ADAPTER'|'SCANNING'|'CONNECTING'|'DISCOVERING'|'SUBSCRIBING'|'READY'|'RECONNECT_WAIT'|'ERROR'|'INCOMPATIBLE';
export interface ProtocolDiagnostics { sequenceGaps:number; duplicates:number; crcErrors:number; resyncBytes:number; lastDataAt?:number }
export interface CommandResponse { transactionId:number; operation:number; result:number; detailCode:number; data:Uint8Array }

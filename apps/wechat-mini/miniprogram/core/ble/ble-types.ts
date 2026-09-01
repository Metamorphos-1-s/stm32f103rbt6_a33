export type ConnectionState =
  | 'CLOSED' | 'OPENING_ADAPTER' | 'ADAPTER_UNAVAILABLE' | 'SCANNING'
  | 'CONNECTING' | 'DISCOVERING' | 'SUBSCRIBING' | 'VERIFYING' | 'READY'
  | 'DEGRADED' | 'RECONNECT_WAIT' | 'DISCONNECTING' | 'ERROR' | 'INCOMPATIBLE';

export interface BleDevice {
  deviceId: string;
  name?: string;
  localName?: string;
  RSSI?: number;
  advertisServiceUUIDs?: string[];
  lastSeenAt?: number;
}

export interface BleCharacteristic {
  uuid: string;
  properties: { notify?: boolean; indicate?: boolean; write?: boolean; writeNoResponse?: boolean };
}

export interface BleServiceDiscovery {
  serviceUuid: string;
  notify: BleCharacteristic;
  write: BleCharacteristic;
}

export interface BleAdapterState { available: boolean; discovering: boolean }
export interface BleDisconnectEvent { deviceId: string; connected: boolean }
export interface BleNotification { deviceId: string; serviceId: string; characteristicId: string; value: Uint8Array }

export class BleAdapterError extends Error {
  constructor(public readonly kind: 'BLUETOOTH_OFF'|'PERMISSION_DENIED'|'UNSUPPORTED'|'TIMEOUT'|'OTHER', message: string) { super(message); }
}

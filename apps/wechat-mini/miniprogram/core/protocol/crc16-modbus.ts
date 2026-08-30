export function crc16Modbus(bytes: Uint8Array): number {
  let crc = 0xffff;
  for (const value of bytes) {
    crc ^= value;
    for (let bit = 0; bit < 8; bit++) crc = (crc & 1) ? ((crc >>> 1) ^ 0xa001) : (crc >>> 1);
  }
  return crc & 0xffff;
}

export function appendCrc(bytes: Uint8Array): Uint8Array {
  const out = new Uint8Array(bytes.length + 2); out.set(bytes);
  const crc = crc16Modbus(bytes); out[out.length - 2] = crc & 0xff; out[out.length - 1] = crc >>> 8;
  return out;
}

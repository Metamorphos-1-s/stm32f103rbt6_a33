/** Exact signed 64-bit value represented as the protocol-facing words. */
export class Int64Value {
  constructor(public readonly low: number, public readonly high: number) {}
  static fromBigInt(value: bigint): Int64Value {
    const bits = BigInt.asIntN(64, value); return new Int64Value(Number(BigInt.asUintN(32, bits)), Number(BigInt.asIntN(32, bits >> 32n)));
  }
  static fromLittleEndian(bytes: Uint8Array, offset = 0): Int64Value {
    let bits = 0n; for (let i = 0; i < 8; i++) bits |= BigInt(bytes[offset + i]) << BigInt(i * 8); return Int64Value.fromBigInt(BigInt.asIntN(64, bits));
  }
  toBigInt(): bigint { return BigInt.asIntN(64, (BigInt(this.high >>> 0) << 32n) | BigInt(this.low >>> 0)); }
  toDecimalString(): string { return this.toBigInt().toString(10); }
  toSafeNumber(): number { const n = this.toBigInt(); if (n < BigInt(Number.MIN_SAFE_INTEGER) || n > BigInt(Number.MAX_SAFE_INTEGER)) throw new RangeError('int64 exceeds safe integer range'); return Number(n); }
  toLittleEndian(): Uint8Array { let n = BigInt.asUintN(64, this.toBigInt()); const out = new Uint8Array(8); for (let i=0;i<8;i++){out[i]=Number(n&255n);n>>=8n;} return out; }
}

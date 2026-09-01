import test from 'node:test'; import assert from 'node:assert/strict'; import {Int64Value} from '../miniprogram/core/domain/int64-value'; import {formatMass} from '../miniprogram/core/domain/mass-format';
test('signed int64 exact conversion',()=>{for(const n of [-1n,0n,1n,9223372036854775807n,-9223372036854775808n])assert.equal(Int64Value.fromBigInt(n).toDecimalString(),n.toString());assert.equal(formatMass(Int64Value.fromBigInt(-1500000n),'g',3),'-1.500');});
test('pound formatting uses exact conversion ratio',()=>assert.equal(formatMass(453592370n,'lb',3),'1.000'));
test('mass formatting avoids BigInt exponentiation unsupported by WeChat runtime',()=>{assert.equal(formatMass(1234567890n,'kg',4),'1.2345');assert.equal(formatMass(-1234567n,'g',2),'-1.23');});

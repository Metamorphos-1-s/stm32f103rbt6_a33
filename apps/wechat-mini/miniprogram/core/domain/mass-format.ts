import { Int64Value } from './int64-value';
export type MassUnit = 'kg'|'g'|'lb';
const SCALE: Record<MassUnit,bigint> = {kg:1000000000n,g:1000000n,lb:453592370n};
export function formatMass(value: Int64Value|bigint, unit: MassUnit, decimals = 3): string {
  const raw = typeof value === 'bigint' ? value : value.toBigInt(); const scale=SCALE[unit]; const neg=raw<0n; const abs=neg?-raw:raw; const whole=abs/scale; const fraction=abs%scale;
  const digits=Math.max(0, decimals); const fractionDigits=scale.toString().length-1; let text=digits ? `${whole}.${fraction.toString().padStart(fractionDigits,'0').slice(0,digits)}` : whole.toString(); return neg ? `-${text}` : text;
}
export function microgramsFromUnit(value: string, unit: MassUnit): bigint { if(!/^-?\d+(\.\d+)?$/.test(value.trim())) throw new Error('invalid mass'); const [w,f='']=value.trim().split('.'); const scale=SCALE[unit]; const digits=scale.toString().length-1; if(f.length>digits) throw new Error('too many decimals'); const sign=w.startsWith('-')?-1n:1n; const whole=BigInt(w); return whole*scale + sign*BigInt((f+'0'.repeat(digits)).slice(0,digits)||'0'); }

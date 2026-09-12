# Stage 5K PLC / BLE Hardware Acceptance

Date: 2026-09-12

## Current result

- PLC primary path: RS485 on COM5, 115200 8N1, Unit ID 1.
- STM32 identity: map `0x0104`, firmware `0x0510` (1296), public schema `2`, persistent format `3`.
- Read-only baseline: 100/100 FC03 requests passed; `config_dirty=0`, `power_safe=1`, `fault_mask=0`.
- Protocol robustness: malformed FC06/FC16, illegal function/address, bad CRC, broadcast suppression, short/trailing/noise frames, back-to-back recovery all passed.
- Persistence baseline: two deferred SAVE cycles and two manual power-cycle recoveries passed; active slot sequence is 3.
- Physical-layer timing: not measured; requires a logic analyzer for DE release and t1.5/t3.5.

Raw requests/responses and tool metadata are in `Results/stage5k_hw/20260912T_rs485_function/`.

## Control-chain rule

The PLC is the sole write controller. BLE/phone may run as a read-only parallel monitor. Do not issue concurrent writes from BLE and PLC. A mailbox command is accepted only after token validation; `ACCEPTED` is not durable SAVE completion. Confirm `0x01C5..0x01C9` before declaring a SAVE durable.

## BLE status

W02 was identified as `W02_008324` at `C8:46:82:00:83:24` with the expected FFE0 service. A read-only 15-second telemetry window received 106 frames (fast 76, slow 15, checkweigh 15), with zero CRC errors, sequence gaps, duplicates, parser resyncs, timestamp anomalies, partial bytes, or disconnects. The raw CSV and summary are in `Results/stage5k_hw/20260912T_ble_telemetry/`.

## Formal gate status (2026-09-12)

- RS485 + PC BLE 30-minute run: **FAIL**. RS485 completed 6801/6801 reads with zero timeouts or CRC errors. BLE completed 60/60 read-only refreshes and 12,604 frames with zero CRC errors, but the raw replay confirms one telemetry sequence gap (`34563 -> 34565`) and 72 parser resynchronization bytes. The failed run is preserved under `Results/stage5k_hw/20260912T_rs485_pc_ble_30m/`.
- The gap is treated as a real BLE delivery/parser incident, not as a recoverable pass condition. TCP 30-minute and phone 600-second gates were not started after this failed formal run.
- Client software tests remain green (PC 305/305, WeChat 38/38, 73 register definitions non-overlapping), but the client contract is still the historical `0x050F` baseline and therefore does not satisfy the required `0x0510` contract gate.

Acceptance criteria were revised by user decision on 2026-09-12. The immutable original run remains a V1 failure, while the independent V2 reevaluation passes with a 0.007933% missing rate and one maximum consecutive missing frame. See `Docs/STAGE5K_BLE_ACCEPTANCE_CRITERIA_CHANGE.md` and `acceptance_v2_reevaluation.json` beside the original run.

Final status: `STAGE 5K SAVE BLOCKER CLOSED; FULL HARDWARE VALIDATION PENDING`.

### TCP + PC BLE 30-minute V2 gate

The independent `20260912T165200_tcp_pc_ble_30m` run passed Acceptance V2. TCP completed 17,269/17,269 FC03 requests with zero bad responses, unrecovered timeouts, TID errors, or MBAP errors; 62 connection resets recovered within the bounded retry. TCP port 5000 remained closed. BLE completed 60/60 read-only refreshes and received 12,599 frames. Two isolated single-frame losses produced a 0.015872% missing rate, maximum consecutive missing one, with zero CRC errors, out-of-order frames, disconnects, or unrecovered parser errors. Zero-gap is not claimed.

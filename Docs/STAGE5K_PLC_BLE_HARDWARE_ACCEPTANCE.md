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

The W02 was powered with the phone disconnected, but the Windows BLE scan did not expose a uniquely identifiable W02 advertisement or the expected service. No guessed-address connection was attempted. BLE acceptance remains pending until the W02 name/address (or an advertising scan result) is confirmed.


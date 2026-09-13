# A33 PLC Modbus Quick Start

现场部署时从 RS485 Modbus RTU 和 Ethernet Modbus TCP 中选择一种作为 PLC
主控制链路。手机 BLE 可以在 PLC 控制期间并行进行只读监控。两个 PLC
接口同时写控制不属于支持的部署模型。

## Connection

- RS485: A/B/GND through the board's USART2 transceiver; default 115200, 8-N-1,
  Unit ID 1.
- TCP: CH579 gateway at `192.168.1.100:502`, Unit ID 1. TCP port 5000 is not a
  product service.
- PDU addresses are zero based. PLC `40001` notation is `40001 + PDU address`.
- Firmware `0x0510`, register map `0x0104`, public configuration schema `2`.

Current rate boundary: Profile 0 / 10 Hz is the only allowed product path.
Profile 1 / 40 Hz is diagnostic-only pending Stage 5L-R root-cause closure and
must not be selected by a PLC. 640/1280 Hz remain prohibited. This containment
does not change Map `0x0104` or the frozen `0x0510` device behavior.

Multi-register values use the configured word order. Read `0x0103` first:
`0` is high-word-first and `1` is low-word-first. Signed 32/64-bit values are
two's-complement bit patterns.

## Recommended Reads

- `0x0000-0x001F`: conditioned display, status flags, NET/GROSS/TARE mass and
  raw values.
- `0x0020-0x003F`: sample, storage, calibration and fault diagnostics.
- `0x0100-0x013F`: complete active configuration (64 registers).
- `0x01A0-0x01A9`: communication configuration and pending apply.
- `0x01C0-0x01C4`: persistent Format 3, active slot, sequence and state.
- `0x01E0-0x01F0`: conditioned display telemetry.

At 115200 baud, a 100 ms process-data poll is suitable for ordinary PLC use;
state and diagnostics may be polled every 500-1000 ms. Display mass is the
conditioned value at `0x0000-0x0001`; authoritative NET/GROSS/TARE microgram
values are at `0x0010-0x001B`. Decimal places and active unit are `0x0002` and
`0x0003`. Status flags are `0x0004-0x0005`; stable, zero, tare and overload are
bits 4-7 respectively. Checkweigh state is `0x0231`.

## Commands and Configuration

Use the mailbox at `0x0040-0x0057`: write token, command, arguments and flags,
then write execute value `0xA55A` at `0x004B`. Read response token/result at
`0x004C-0x004F`. Tokens must be non-zero; retry a timeout with the same token.
The cached response prevents duplicate execution.

Configuration writes go to staging (`0x0140-0x017F`) and require
`BEGIN -> write -> VALIDATE -> APPLY_RAM`. `CANCEL` restores Active and releases
the owner. SAVE is explicit and writes Flash only after a valid APPLY. Mailbox
`ACCEPTED` means the request was queued, not that Flash is durable. Poll
`0x01C5-0x01C9`: result `0 IDLE`, `1 PENDING`, `2 SUCCESS`, `3 NO_CHANGE`,
`4 FAILED`, `5 POWER_UNSAFE`, `6 BUSY`, `7 INVALID_STATE`, `8 INTERNAL_ERROR`;
`0x01C6` is the request token, `0x01C7` the source, and `0x01C8-0x01C9` the
associated revision in configured word order. Reads are non-consuming.

BLE monitoring does not take the Modbus configuration owner. Do not issue
simultaneous configuration writes from RS485 and TCP PLC paths.

## IEC 61131-3 Structured Text Reference

The following is a vendor-neutral reference and has not been compiled in a
specific Siemens, Mitsubishi, Omron or Delta environment:

```iecst
FUNCTION_BLOCK A33_ReadProcessData
VAR_INPUT Enable : BOOL; END_VAR
VAR_OUTPUT Done, Error : BOOL; NetUg, GrossUg, TareUg : LINT; Status : DWORD; END_VAR
VAR
  Words : ARRAY[0..31] OF WORD;
  Order : WORD;
END_VAR
(* Use FC03 to read 0000-001F. Decode signed LINT values with the word order
   read from 0103; set Done only after a complete CRC-checked response. *)
END_FUNCTION_BLOCK

FUNCTION_BLOCK A33_ExecuteCommand
VAR_INPUT Execute : BOOL; Command : WORD; Token : WORD; END_VAR
VAR_OUTPUT Accepted, Error : BOOL; ResponseToken, Result : WORD; END_VAR
(* FC16 writes 0040-004B, with 0xA55A last. A timeout leaves the operation
   unresolved; do not allocate a new token for the retry. *)
END_FUNCTION_BLOCK

FUNCTION_BLOCK A33_WaitOperationResult
VAR_INPUT Start : BOOL; Token : WORD; END_VAR
VAR_OUTPUT Done, Success, NoChange, Error : BOOL; Result : WORD; END_VAR
(* Poll 01C5-01C9 until terminal. SUCCESS=2 and NO_CHANGE=3 are successful
   terminal results; ACCEPTED from 004D is never treated as Flash success. *)
END_FUNCTION_BLOCK
```

After PLC reconnect or device power-up, read identity, word order, status,
active configuration and persistent diagnostics before issuing any write.
On timeout, stop writes, retry the same request token after the protocol quiet
period, and re-read identity/state before resuming.

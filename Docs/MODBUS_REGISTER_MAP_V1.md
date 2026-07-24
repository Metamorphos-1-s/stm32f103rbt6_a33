# Modbus Holding Register Map V1

Addresses are zero-based PDU addresses. PLC notation is 40001 plus the PDU address. A 16-bit register has normal Modbus byte order. Multi-register values use configured high-word-first or low-word-first order.

| PDU range | Purpose | Access |
|---|---|---|
| `0000-001F` | display values, status, net/gross/tare ug, raw values | RO |
| `0020-003F` | sample, CS1237, storage, key, fault and calibration diagnostics | RO |
| `0040-005F` | command request/response mailbox | mixed |
| `0100-013F` | active metrology/profile configuration | RO |
| `0140-017F` | staged configuration (`active + 0x40`) | RW |
| `0180-019F` | calibration staging and active calibration | mixed |
| `01A0-01BF` | communication configuration model | mixed |
| `01C0-01DF` | persistence state and statistics | RO |

## Realtime block

- `0000-0001` current page display int32; `0002` decimals; `0003` unit.
- `0004-0005` status; `0006-000B` net/gross/tare display int32.
- `000C` division; `000D` page; `000E=0100` map version; `000F` firmware value.
- `0010-001B` net/gross/tare signed int64 ug.
- `001C-001F` raw and filtered raw signed int32.

## Mailbox

`0040` token, `0041` command, `0042-0049` arguments, `004A` flags and `004B` execute. Write `A55A` last. `004C-0057` contain response token/result/state/value/status. Reserved writes are rejected. Token zero is invalid; repeating the last token does not execute twice.

Commands cover zero/reset-zero/tare/clear-tare, view/unit/profile, manual output, config begin/validate/apply/cancel/save, calibration workflow, and factory reset. Asynchronous ACCEPTED is not completion.

## Configuration

Active fields include compliance/unit/mask/word order, Max/e, zero policies, each unit's decimals/division, brightness, load-cell metadata, both complete profiles and validation status. Staging has the same layout at `+0040`; `017E` is validation result and `017F` is dirty.

FC16 validates the whole address range before mutation. EXECUTE must be the final register when present. Active and response registers are read-only. APPLY_RAM never saves flash; SAVE is a separate mailbox command.

## RTU transport

Default settings are 115200 baud, 8 data bits, no parity, one stop bit, slave
address 1, and zero response delay. EVEN/ODD uses STM32F1 9-bit word length so
the application still has eight data bits.

An ADU is `address | PDU | crc_low | crc_high`, maximum 256 bytes. CRC-16/MODBUS
uses initial `FFFF`, reflected polynomial `A001`, and low-byte-first wire order.
Supported functions are 03, 06, and 16. Exceptions are 01 illegal function,
02 illegal/read-only address, 03 illegal value or malformed quantity, 04 device
failure, and 06 busy. Bad CRC and nonmatching addresses are silent.

Address 0 is broadcast. Broadcasts never receive a response and default policy
0 executes no FC06/FC16 writes; broadcast FC03 is also ignored.

Write communication staging registers, then execute mailbox command ID 24
(`COMMUNICATION_APPLY`). The response uses the old address and serial settings;
new settings become active only after TX DMA, USART TC, and RS485 DE release.
APPLY changes RAM only. SAVE remains mailbox command ID 13 and is asynchronous:
its acceptance response completes before flash maintenance starts.

RS232 and RS485 share USART2 bytes, address, CRC, functions, and this register
map. Firmware does not read the external interface selector. Modbus reference
`40001` corresponds to protocol holding-register offset `0000`.

# Stage 5B-H Hardware Validation

## Identity and scope

- Firmware baseline: `6954a90d7de9996fcc4800cf97c181a4dcd367f0` (`6954a90 阶段5B`)
- Register map: `0x0100`; firmware register: `0x050A`; persistent Schema V2: `2`
- Available equipment stated by the task: Windows PC, ST-Link, USB-RS232, USB-RS485, Python 3, STM32CubeProgrammer, manual power switching
- This delivery adds PC tools, tests and documentation only. Firmware files are unchanged.

The Python source compiles and the offline CRC/frame/map/slot tests passed. A clean Release build also passed: FLASH 51,736 bytes, RAM 10,608 bytes, ELF SHA-256 `B114BB895D4C03442587F2D1D6560E3C36EF3FB24ADCA567CBD0088E275CCCA4`. STM32CubeProgrammer 2.19.0 programmed and verified this ELF through ST-Link V2 (`V2J47S7`) at 3.29 V, then reset the MCU successfully. The operation erased application sectors 0 through 50; no Mass Erase was issued and the A/B configuration region was not erased. After the board connection was adjusted, COM5 returned and read-only address-1 probes at 115200 N1 validated CRC, map `0x0100`, firmware `0x050A`, Schema V2, power-safe state and sample data. A post-reset 100-request smoke run passed with zero errors. Two zero-tolerance soak attempts each stopped on one timeout. A third classified run completed 4,946 normal logical requests and recorded three timeouts; each timeout recovered on the first retry of the same register in the same open serial session. The run then stopped on one truncated response: the expected 7-byte FC03 response ended after byte 6, missing the final CRC byte. SWD Hot Plug capture after the run found `framer.transport_error_count=5` (up by three from the prior snapshot), zero UART/DMA hardware errors, `server.tx_error_count=1`, and one fewer TX completion/response than valid request. Framer/server were idle and DMA read/write positions matched. No Modbus write, SAVE, or manual power-cycle test was performed.

## Setup

Install `pyserial>=3.5`, enumerate adapters with `stage5b_hw.py list-ports`, then record each adapter's device, description, hardware ID, VID, PID, serial number and location. Wiring and electrical warnings are in [Tools/stage5b_hw/README.md](Tools/stage5b_hw/README.md).

Build and flash with `build_and_flash.ps1 -AllowFlash`. This records the Git state and ELF SHA-256, writes and verifies the ELF over SWD, resets the target, avoids Mass Erase, and preserves the A/B configuration region by design.

## Test sequence and status

| Area | Command or method | Status |
|---|---|---|
| CRC-16/MODBUS and raw frame codec | from `Tools/stage5b_hw`: `python -m unittest discover tests` | OFFLINE TESTED |
| Register-map consistency | `register_map_check.py` against current C sources | OFFLINE TESTED |
| Release build and ELF identity | `cmake --preset Release`, clean build and SHA-256 | PASS |
| ST-Link program/verify/reset | `flash_firmware.ps1` | PASS |
| FC03 probe and identity | `stage5b_hw.py probe` | PASS after operator reset at 115200 N1 |
| Short read-only stability | 100 FC03 reads, 50 ms interval | PASS; 100/100 responses |
| RS232, 100 reads and errors | `stage5b_hw.py rs232` | NOT RUN |
| RS485, 100 reads and errors | `stage5b_hw.py rs485` | NOT RUN |
| FC06/FC16 guarded writes | `write-single`, `write-multiple`, `config-apply` | NOT RUN |
| Illegal function/address/value and broadcast | `errors --allow-write` | NOT RUN |
| Mailbox token deduplication | `commands --repeat-token` | NOT RUN |
| RAM staging/validate/apply/restore | `config-apply --allow-write` | NOT RUN |
| Communication parameter switch/recovery | manual guarded procedure | NOT RUN |
| SAVE and storage state | `save --allow-write --allow-flash` | NOT RUN |
| A/B CRC, commit and sequence | dump before/after, then `parse_config_slots.py` | NOT RUN |
| Manual power-cycle recovery | `save --manual-power-cycle` | NOT RUN |
| Long polling | two zero-tolerance plus one classified run | FAIL: repeatable transient RX drops and one truncated TX response |
| DMA/UART/server counters | SWD Hot Plug snapshots after stress test | CAPTURED; RX transport errors and one TX error found |

The current register model has no battery-voltage or hardware-revision register. Probe reports both as unavailable instead of inventing values. The complete Stage 5B DMA/UART/server diagnostic snapshot is also not Modbus-mapped, so it must be captured over SWD.

## Communication change recovery

Only proceed after recording the current `0x01A0..0x01A9` block and enabling `--allow-comm-change`. Stage the candidate, validate, apply, wait for the acknowledgement at the old parameters, close the port, then reopen at the new baud/parity/stop/address and probe. Restore with the same flow and do not SAVE. If recovery fails, try only the known old and expected new combinations; then power-cycle so the unsaved Flash configuration returns. Do not scan all 247 addresses indefinitely.

## SAVE and power recovery

Before SAVE, require `POWER_SAFE=1`, storage idle, and capture current/saved revisions plus an A/B dump. After the mailbox acknowledgement, poll storage until idle and dump again. Verify that the inactive slot becomes the new active slot, sequence advances with wrap-safe comparison, CRC and commit marker are valid, and the old slot remains valid. A repeated SAVE of unchanged content must be checked for no additional erase/program activity using storage diagnostics.

Manual power-off timing is operator dependent. It can show recovery to an old complete or new complete record, but it is not exhaustive interruption coverage and must never be described as such.

## Equipment limitations

**NOT VERIFIED WITH CURRENT EQUIPMENT:** exact TIM4 frequency, RTU t1.5/t3.5 waveform timing, PA1 DE setup/release timing, RS485 analogue signal integrity/reflections, and exhaustive Flash interruption timing.

**REQUIRES LOGIC ANALYZER:** TIM4 1 MHz, frame boundaries, DE timing, final-stop-bit-to-DE-release delay, and bus-level correlation.

**REQUIRES ADJUSTABLE POWER SUPPLY:** actual PVD Level 6 threshold, slow VDD ramp-down, brownout under load, and controlled interruption during Flash programming.

## Exit decision

Generated probe reports:

- `Tools/stage5b_hw/reports/20260724_181258_unknown/` (115200 N1 timeout)
- `Tools/stage5b_hw/reports/20260724_181308_unknown/` (9600 N1 timeout)
- `Tools/stage5b_hw/reports/20260724_flash_requested/` (program, verify and reset passed)
- `Tools/stage5b_hw/reports/20260724_182741_rs485/` (COM5 not found; no frame sent)
- `Tools/stage5b_hw/reports/20260724_182742_rs485/` (COM5 not found; no frame sent)
- `Tools/stage5b_hw/reports/20260724_224143_unknown/` (COM6, 115200 N1 timeout)
- `Tools/stage5b_hw/reports/20260724_224144_unknown/` (COM6, 9600 N1 timeout)
- `Tools/stage5b_hw/reports/20260725_010528_unknown/` (COM5, 115200 N1 full probe passed)
- `Tools/stage5b_hw/reports/20260725_010529_unknown/` (COM5, 9600 N1 timeout)
- `Tools/stage5b_hw/reports/20260725_010544_unknown/` (COM5, 115200 N1 retry timeout)
- `Tools/stage5b_hw/reports/20260725_010604_unknown/` (COM5, 115200 N1 delayed retry timeout)
- `Tools/stage5b_hw/reports/20260725_013718_unknown/` (post-reset COM5, 115200 N1 probe passed)
- `Tools/stage5b_hw/reports/20260725_013719_rs485/` (post-reset 100-read smoke passed)
- `Tools/stage5b_hw/reports/20260725_014900_rs485/` (soak attempt 1: 725 success, then 1 timeout)
- `Tools/stage5b_hw/reports/20260725_015100_unknown/` (post-timeout probe passed without reset)
- `Tools/stage5b_hw/reports/20260725_015130_rs485/` (soak attempt 2: 5075 success, then 1 timeout)
- `Tools/stage5b_hw/reports/20260725_020147_unknown/` (second post-timeout probe passed without reset)
- `Tools/stage5b_hw/reports/20260725_021341_rs485/` (classified soak: 3 retry-recovered timeouts, 1 truncated response)
- `Tools/stage5b_hw/reports/20260725_swd_after_classified_soak/` (post-failure Hot Plug RAM dump and parsed counters)

## Confirmed defects from stress diagnostics

1. RX producer sampling is not atomic across DMA wrap. `Uart2DmaTransport_Process()` snapshots `rx_complete_count` and then reads the DMA remaining counter separately. A wrap between those operations can combine the old wrap count with the new low position, making `producer < s_producer_absolute`. The transport then flags a receive error and the framer discards the pending request. The three newly observed framer transport errors match the three classified timeouts, while all UART/DMA hardware error counters remain zero.
2. One FC03 response was truncated after six of seven bytes. The simultaneous server TX error and missing TX completion/response counter confirm a target-side TX completion failure; exact DE/UART/DMA timing still requires a logic analyzer.

Before repeating, identify whether COM5 is the RS232 or RS485 adapter, select the matching board switch, verify common ground, and for RS485 verify A/B polarity. Do not infer the interface type from the CH340 descriptor.

Stage 5B hardware acceptance: **NOT MET**.

Ready to enter Stage 5C: **NO**. First complete the board-level table, retain generated report directories and raw logs, inspect the SWD diagnostic snapshots, and resolve any failures. Logic-analyzer and adjustable-supply tests remain necessary for final timing and brownout claims even if PC functional tests pass.

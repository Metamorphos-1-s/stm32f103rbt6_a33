# Stage 5B-H Hardware Validation

## Identity and scope

- Firmware baseline: `6954a90d7de9996fcc4800cf97c181a4dcd367f0` (`6954a90 阶段5B`)
- Register map: `0x0100`; firmware register: `0x050A`; persistent Schema V2: `2`
- Available equipment stated by the task: Windows PC, ST-Link, USB-RS232, USB-RS485, Python 3, STM32CubeProgrammer, manual power switching
- This delivery adds PC tools, tests, diagnostics, and the firmware fixes described below.

The Python source compiles and the offline CRC/frame/map/slot tests passed. The original Release build exposed repeatable RX and TX failures under stress: stale DMA wrap sampling discarded requests, while dividing DWT cycles before elapsed-time subtraction caused a false RS485 TX timeout near the 16 MHz cycle-counter wrap at about 268 seconds. Both defects were fixed and covered by 1,094 Stage 5B host checks, including the DMA absolute counter's natural 32-bit rollover. The final Release build used 51,944 bytes of FLASH and 10,624 bytes of RAM; ELF SHA-256 is `FDB15806A72C02FC4766B98E7A0F6C6D2DB57905BAB7ED91DDB28655034D0400`. STM32CubeProgrammer 2.19.0 programmed and verified it through normal SWD with software reset because NRST was not connected. The final zero-tolerance 600-second run completed 5,261 logical requests with zero timeout, CRC, exception, or retry. SWD captured 15,787 valid/addressed FC03 requests and exactly 15,787 completed responses, five compensated DMA wrap races, zero framer transport errors, zero TX start/timeout errors, zero UART/DMA errors, idle state machines, and matching DMA read/write positions. After the rollover boundary adjustment, a further probe and 100-read smoke test passed; its SWD snapshot captured 108/108 completed FC03 responses and all error counters at zero. No Modbus write, SAVE, or manual power-cycle test was performed.

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
| Long polling | final 600 s zero-tolerance run after fixes | PASS: 5261/5261 logical requests; zero timeout/CRC/exception |
| DMA/UART/server counters | SWD Hot Plug snapshots after stress test | PASS after fixes; final snapshots have zero error counters |

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
- `Tools/stage5b_hw/reports/20260725_024156_rs485/` (RX-only fix: zero RX timeout, TX cycle-wrap failure reproduced)
- `Tools/stage5b_hw/reports/20260725_swd_after_rx_fix_soak/` (RX fix verification and TX timeout classification)
- `Tools/stage5b_hw/reports/20260725_025028_rs485/` (final RX/TX fixes: 600-second zero-tolerance PASS)
- `Tools/stage5b_hw/reports/20260725_swd_after_rx_tx_fix_pass/` (final Hot Plug RAM dump and parsed zero-error counters)
- `Tools/stage5b_hw/reports/20260725_dma_counter_wrap_flash/` (final rollover-boundary build programmed and verified)
- `Tools/stage5b_hw/reports/20260725_030641_rs485/` (final read-only probe passed)
- `Tools/stage5b_hw/reports/20260725_030650_rs485/` (final 100-read smoke passed)
- `Tools/stage5b_hw/reports/20260725_swd_after_counter_wrap_smoke/` (final smoke Hot Plug snapshot; 108/108 responses and zero errors)

## Resolved defects from stress diagnostics

1. RX producer sampling was not atomic across DMA wrap. `Uart2DmaPosition_Resolve()` now compensates a single stale wrap when the apparent regression is less than one DMA buffer, rejects genuine larger regressions, and treats the absolute byte count modulo 32 bits so long-running operation continues across its natural rollover. The final long-run SWD snapshot recorded five successful compensations and zero transport errors.
2. `BSP_TimeNowUs()` divided `DWT->CYCCNT` before elapsed-time subtraction. At the hardware cycle-counter wrap, the RS485 controller interpreted the discontinuous microsecond value as more than 200 ms elapsed and aborted an active response. The controller now uses raw cycles plus wrap-safe `BSP_TimeCyclesElapsed()` and explicit microsecond-to-cycle conversion. The final 600-second run crossed at least two hardware cycle wraps with zero TX errors.

Before repeating, identify whether COM5 is the RS232 or RS485 adapter, select the matching board switch, verify common ground, and for RS485 verify A/B polarity. Do not infer the interface type from the CH340 descriptor.

Stage 5B hardware acceptance: **NOT MET**.

Ready to enter Stage 5C: **NO**. First complete the board-level table, retain generated report directories and raw logs, inspect the SWD diagnostic snapshots, and resolve any failures. Logic-analyzer and adjustable-supply tests remain necessary for final timing and brownout claims even if PC functional tests pass.

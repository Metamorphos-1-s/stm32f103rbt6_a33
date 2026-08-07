# Stage 5B-H hardware integration tools

This directory drives the existing Stage 5B firmware with raw Modbus RTU frames. It does not change firmware, register addresses, DMA, USART2, TIM4, PA1 direction control, Schema V2, or the A/B Flash protocol.

## Install

Use Python 3.8 or newer on Windows:

```powershell
python -m pip install -r Tools\stage5b_hw\requirements.txt
python Tools\stage5b_hw\stage5b_hw.py list-ports
```

Copy `example_config.json` to a local test configuration and set explicit COM ports. Command-line values override JSON values. `COM10` and higher are supported by pyserial.

## Wiring and safety

ST-Link: connect `SWDIO`, `SWCLK`, `GND`, `NRST`, and `VTref`.

RS485: connect `A`, `B`, and `GND`. If there is no response, check whether the adapter uses reversed A/B naming. Termination depends on the actual bus topology.

RS232: connect `TX`, `RX`, and `GND` through the board's real RS232 interface. USB-TTL is not USB-RS232, and a true RS232 voltage-level adapter must never be wired directly to STM32 PA2/PA3.

Do not back-power the board from an adapter's 5 V output. Board, ST-Link, and adapter must share ground. Do not test RS232 and RS485 simultaneously. Close the serial port before changing the physical 232/485 switch. ST-Link may remain connected, but do not halt the MCU during a soak test.

All FC06/FC16 operations are denied by default. The `rs232` and `rs485` suites remain read-only unless `--include-errors --allow-write` is explicitly supplied. Dangerous operations require an `--allow-*` switch, a source/register-map consistency check, and a typed confirmation. With `--yes`, the exact action must also appear in the JSON `automation_authorizations` list. Factory reset, calibration and deliberate power-loss testing have separate gates.

## Commands

```powershell
python Tools\stage5b_hw\stage5b_hw.py probe --port COM5 --interface rs232 --baud 9600 --parity N --stopbits 1 --slave 1
python Tools\stage5b_hw\stage5b_hw.py read 0x0000 32 --port COM5
python Tools\stage5b_hw\stage5b_hw.py hf2-status --port COM5 --interface rs485
python Tools\stage5b_hw\stage5b_hw.py smoke --config my_board.json --count 100
python Tools\stage5b_hw\stage5b_hw.py rs485 --port COM6 --count 100
python Tools\stage5b_hw\stage5b_hw.py rs485 --port COM6 --count 100 --include-errors --allow-write
python Tools\stage5b_hw\stage5b_hw.py soak --port COM6 --interface rs485 --duration-s 3600 --interval-ms 50 --output-csv soak.csv
python Tools\stage5b_hw\stage5b_hw.py commands --port COM5 --name NOP --allow-write
python Tools\stage5b_hw\stage5b_hw.py commands --port COM5 --name RUNTIME_DRIFT_CONTROL --arg0 1 --allow-write --allow-actions
python Tools\stage5b_hw\stage5b_hw.py config-apply --port COM5 --allow-write
python Tools\stage5b_hw\stage5b_hw.py config-apply --port COM5 --communication --new-baud 19200 --new-slave 2 --allow-write --allow-comm-change
python Tools\stage5b_hw\stage5b_hw.py save --port COM5 --allow-write --allow-flash --manual-power-cycle
```

PDU addresses are zero based. The tool prints the corresponding PLC-style address as `40001 + PDU address`. Every exchange prints raw TX/RX hexadecimal bytes and test runs write reports under `reports/YYYYMMDD_HHMMSS_<interface>/`.

`hf2-status` requires map `0x0102` and reads the complete DisplayConditioner and
Runtime Drift blocks. It reports signed masses in both ug and g and treats a
nonzero reserved register `0x0203` as an error. Runtime drift command 25 is
volatile, is not saved to Flash, and accepts only `--arg0 0` or `--arg0 1`.

`full` runs identity, smoke and soak tests. It adds malformed write-frame and temporary RAM configuration tests only when `--allow-write` is present. It never silently runs SAVE, factory reset, calibration, communication changes, or deliberate power loss.

Communication changes use the old settings for the EXECUTE acknowledgement, close the port, probe at the explicitly supplied new settings, then apply and probe the old settings again. They never SAVE. If the process is interrupted, try only the recorded old and expected new combinations, then power-cycle to restore the unsaved Flash configuration; the detailed recovery procedure is in the root validation document.

## Flash and A/B slots

```powershell
powershell -ExecutionPolicy Bypass -File Tools\stage5b_hw\build_and_flash.ps1 -AllowFlash
powershell -ExecutionPolicy Bypass -File Tools\stage5b_hw\dump_config_flash.ps1 -OutputDirectory .\slot_dump
python Tools\stage5b_hw\parse_config_slots.py .\slot_dump\config_region.bin --json .\slot_dump\slots.json
```

The flash flow builds Release, programs the ELF over SWD, verifies and resets. It does not issue Mass Erase and preserves `0x0801F000..0x0801FFFF`. The dump flow performs a read-only 4096-byte read and splits it into `slot_a.bin` and `slot_b.bin`.

## SWD diagnostics

The current Modbus map does not expose the full `Stage5BModbusDiagnosticSnapshot`. After a soak test, halt once and inspect `Stage5BModbusDiagnosticSnapshot`, `Stage4BStorageDiagnosticSnapshot`, and `MassSnapshot` in CubeIDE/GDB. Record manager/server/framer/TX states, DMA positions, frame length, RX/valid/CRC/address/overflow counts, FC03/06/16 counts, TX response/error, UART error and DMA overrun counts. Do not set breakpoints during normal stress operation; debugger halts can themselves cause DMA overwrite.

For the current Release ELF, the retained UART/framer/server statistics can be captured without NRST after a test has stopped:

```powershell
powershell -ExecutionPolicy Bypass -File Tools\stage5b_hw\dump_swd_diagnostics.ps1 -OutputDirectory .\swd_snapshot
```

The script uses SWD Hot Plug, uploads one RAM range, and writes `swd_diagnostics.json`. Do not attach during normal soak operation. The addresses are Release-link-map specific and must be revalidated after any firmware rebuild that changes layout.

PC latency includes USB-adapter and Windows scheduling delay. It cannot prove TIM4 1 MHz, exact t1.5/t3.5, PA1 DE timing, or release 20 us after the final stop bit.

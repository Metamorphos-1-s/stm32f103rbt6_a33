# Stage 5F Six-Digit Edit Selection and Blink

## Baseline and scope

Stage 5F started from `main` and `stage5e-hw-tested` target
`46027f6ea7b2a00e4250bd10a92d932e7a3fba70` on branch
`stage5f-six-digit-edit`. Its only product changes are six-position numeric
selection and a selected-position blink in the existing menu and calibration
numeric editors.

Long-press acceleration, key-repeat timing, numeric value formats, ConfigEdit,
Schema/Flash layout, metrology, calibration math, checkweigh/alarm behavior,
Modbus, BLE, UART settings, and the `.ioc` file are unchanged.

## Design

`NumericEditCursor` stores `selected_digit` in the range 0..5, where 0 is the
rightmost digit and 5 is the leftmost digit. ZERO advances and wraps through
all six values. The step table is `1, 10, 100, 1000, 10000, 100000`; mass
editing retains the existing `division_digit * step` behavior and all deltas
remain `int64_t`.

The formatter order is left-to-right (`segments[0]` through `segments[5]`), so
the physical index is `5 - selected_digit`. GRID1 through GRID6 retain their
confirmed left-to-right mapping. STAR still decreases and HASH still
increases. KeyService was not changed.

Blinking uses a 500 ms full period: visible for 250 ms and hidden for 250 ms.
It runs from the existing wrap-safe millisecond tick and introduces no delay or
busy wait. Entering edit, moving with ZERO, and changing a value reset the
selected position to the visible phase. Leaving edit, confirming, cancelling,
or reaching an invalid page stops rendering the cursor.

The blink mask affects rendered segments only. The hidden phase clears the
selected digit segments but preserves `BOARD_SEG_DP`. A selected leading blank
temporarily displays zero in the visible phase and returns to blank in the
hidden phase; it is never written back to the numeric value.

## Software verification

Host coverage verifies all six steps and their full wrap, actual 10000 and
100000 edits, the unchanged lower four positions, six physical display
mappings, 0/249/250/499/500 ms phase behavior, tick wrap, decimal-point
preservation, leading-blank visibility, ZERO and STAR/HASH visible resets,
FUNCTION/TARE/invalid behavior, CAP and Stability editing, alarm validation,
and the independent calibration-mass path.

Final pre-hardware gates passed:

- Host CTest: 15/15.
- Stage 5B Python: 28/28.
- Stage 5C Python: 12/12.
- Debug: 85,964 B Flash, 14,832 B RAM; Stage 5E delta +624/+16 B.
- Release: 73,840 B Flash, 14,816 B RAM; Stage 5E delta +576/+16 B.
- BoardDiagnostics: 122,476 B Flash, 14,720 B RAM; delta +992/+16 B.
- Image ends are `0x08014FCC`, `0x08012070`, and `0x0801DE6C`.
- Margins to `0x0801F000` are 41,012 B, 53,136 B, and 4,500 B.

## Hardware verification

Release commit `bf95d295a5261a7aeb3243eca13a1dc29d9b80ff` was programmed
and verified over ST-LINK without mass erase. The ELF SHA256 is
`28A13AA37CBD57219BD441BE66430A79FBCAC8BD8B619B95CC1810BA69E20888`.
The preserved configuration remained Schema V2/344 B, slot B, sequence 16,
Modbus map `0x0103`, unit g, dP 2, division 1, and CAP 3000.00.

Panel tests passed for CAP right-1 through right-6 and full wrap, right-5
100.00 edits, right-6 1000.00 edits, selected-only blink, decimal-point
preservation, the `0500.00`/` 500.00` leading-blank cursor, immediate
visibility after ZERO and value changes, FUNCTION confirm, TARE cancel, and an
`InUALd` page without residual blink. OL, Hi, HyS, Stability, ordinary menu
navigation, and the calibration mass editor were regressed. Calibration right-5
editing and right-6 selection/capacity rejection passed; TARE cancelled without
committing calibration.

## Communication regression

After a software reset restored `config_dirty=0`, a clean concurrent Release
run completed for 60 seconds:

- BLE: FAST 310, SLOW 62, CHECKWEIGH 62, commands 3/3.
- BLE CRC, gaps, duplicates, disconnects, timeouts, retries, parser errors, and
  frame drops: all zero.
- RS485: 222/222 FC03 requests at address 1, 115200 N1; timeout, CRC,
  exception, and retry counts all zero; mean latency 55.326 ms.
- SWD: CS1237 read/overrun, event drop, fault mask, BLE UART/CRC/overflow/drop,
  and Modbus UART/DMA/RTU/CRC/timeout error counters all zero.

Local evidence is under
`Results/stage5f_hw/20260812_2310_stage5f_60s_clean`. The SWD parser address
table and dump length were updated for the Stage 5F Release RAM link map; no
product behavior was changed by that test-tool correction.

## Result

Stage 5F has P0=0 and P1=0. Six-digit selection, cursor blink, higher-position
editing, cancellation, validation, and concurrent transport behavior are
hardware verified. No known Stage 5F limitation remains; UI editing is frozen
and the project may proceed to Stage 6 metrology qualification after merge,
main rebuild, and the `stage5f-ui-tested` tag.

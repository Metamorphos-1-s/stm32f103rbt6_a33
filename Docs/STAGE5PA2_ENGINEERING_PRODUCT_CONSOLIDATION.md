# Stage 5P-A2 Engineering Product Consolidation

## Software Result

**STAGE 5P-A2 SOFTWARE CONSOLIDATION READY; SAVE/POWER-CYCLE HARDWARE
CLOSURE INCOMPLETE; 0x051A REMAINS AN ENGINEERING CANDIDATE; DEVICE RESTORED
TO FROZEN 0x0517.**

No new algorithm was introduced. A2 only formalizes the already implemented
10/40 Hz product path, default contract, and V3 request-state persistence.

## Baseline And Frozen Boundaries

Start: `5848e5473c351e3ed7ad8a9f661ae03b3e762826` on
`stage5pa1b-r5-profile-switch-closure`. The frozen A1/A1B product-path blob
set is recorded in `Results/stage5pa2/software/frozen_blobs.json`; no listed
algorithm, public mapping, or persistent codec source changed in A2.

The branch candidate is Firmware `0x051A`, Map `0x0104`, Schema 2 and
Persistent Format 3. The 281-byte V3 payload remains unchanged. Payload byte
280 contains the existing request bits: R5 mode bits 0-1, R5 application bit 2,
Checkweigh mode bits 4-5; undefined bits are rejected to safe OFF+SHADOW/OFF.
Old V3 byte 280 = 0 decodes safely. CRC, slot size, commit-last and dual-slot
sequence contracts remain unchanged.

## Default And Upgrade Contract

Fresh/default product configuration is 10 Hz + filt1 (moving average) strength3,
R5 OFF+SHADOW and Checkweigh OFF. Existing valid V3 configurations retain
their profile, rate, filter, strength, calibration, capacity, units, division,
communication, alarm and brightness values; firmware does not silently convert
existing filt3 devices to filt1.

## Persistence Semantics Audited

Only user requests persist: R5 application/mode, Checkweigh mode, and existing
profile rate/filter/strength. R5 offset, reference, windows, evaluation,
automatic rebase, rate-normalization bucket, display anchor, holdoff and
Checkweigh classification/output state remain volatile. Startup restores the
request with zero compensation and outputs safe until a valid new sample.

Explicit SAVE remains the only Flash write. APPLY marks dirty and increments
revision; SAVE advances saved revision through the existing asynchronous mailbox
and two-slot store. The Host suite covers old V3, legal/illegal byte280 values,
all request combinations, calibration retention and field equality.

## Software Gates

- Host CTest: 36/36 PASS.
- Stage 5P persistence: all legal request combinations and invalid bits PASS.
- R5 legacy parity: 25,057 total / 22,557 real / 0 mismatch.
- R5 sample admission: 95,200 synthetic / 0 mismatch.
- D1-D: 49,533 / 0 mismatch.
- Stage 5N-A: 45,711 / 0 mismatch.
- Stage 5B/5C/5L Python suites: PASS.
- Debug, Release, strict ARM and 0x051A candidate builds: PASS.
- Product RAM: 19,464 B; conservative stack 1,504 B; collision margin 536 B.
- ASan/UBSan: NOT RUN.

Candidate artifacts:

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| BIN | 107,532 | `32A3184D93A0E1CC92C1F786455623176223DC2FC2430748F924B020AEF428B8` |
| ELF | 2,024,200 | `5937DF5E29E6B65A7595449B8E786A1B68F0BBD331A6EBCB1E4BDEE2DECC74DA` |
| MAP | 1,444,959 | `96A4129C0A37E51032761A91B5DFBBD20D4C26F0AFC554EDF78A1972961918B8` |

## Hardware Stop Point

The current read-only device preflight is Firmware 0x0517, Map 0x0104,
10 Hz/filt3, R5 OFF+SHADOW, offset/reference/evaluation zero, fault/overrun/
dirty zero, revision/saved 8/8, and approximately 500 g loaded.

No SWD configuration read, application erase, 0x051A flash, SAVE, physical
power-cycle or persistent configuration write has been performed in A2.
The intended application range for 0x051A is pages 0-105
(`0x08000000-0x0801A7FF`); the 4 KB V3 region at `0x0801F000` will be backed
up before any authorized test.

Hardware execution is waiting for the exact authorization:

`授权烧录0x051A并执行受控SAVE/断电恢复测试`

Formal Release, metrology, cross-sensor, new 12-hour, external DRDY, physical
sensor fault, D1-C nonzero-offset, ASan and UBSan qualifications remain
separate waivers.

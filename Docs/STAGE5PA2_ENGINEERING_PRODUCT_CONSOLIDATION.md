# Stage 5P-A2 Engineering Product Consolidation

## Final Result

**STAGE 5P-A2 SOFTWARE CONSOLIDATION READY; SAVE/POWER-CYCLE PLC HARDWARE
CLOSURE PASS; LOCAL MENU CLOSURE NOT RUN; 0x051A REMAINS AN ENGINEERING
CANDIDATE; DEVICE RESTORED TO FROZEN 0x0517.**

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

## Hardware Evidence

The current read-only device preflight is Firmware 0x0517, Map 0x0104,
10 Hz/filt3, R5 OFF+SHADOW, offset/reference/evaluation zero, fault/overrun/
dirty zero, revision/saved 8/8, and approximately 500 g loaded.

The authorized 0x051A application-only flash and Verify succeeded. The V3
configuration region was backed up before testing; A/B were valid, active B,
sequence 8, SHA-256
`A615A475C2D7B5D64126EAEB3AAE9E3D094348FA4C4AEC7BDB8EF0010D4A3BB4`.

H2 unsaved APPLY and physical power-cycle restored the prior 10 Hz/filt3,
OFF+SHADOW/OFF request and left the Flash SHA unchanged. H3 explicitly saved:

- 40 Hz + filt1: revision/saved 9/9, active slot A, sequence 9;
- R5 SHADOW+STATIC: revision/saved 10/10, slot B, sequence 10;
- R5 ACTIVE+STATIC: revision/saved 11/11, slot A, sequence 11;
- R5 ACTIVE+DOSING: revision/saved 12/12, slot B, sequence 12;
- Checkweigh STATIC: revision/saved 13/13, slot A, sequence 13;
- Checkweigh DYNAMIC: revision/saved 14/14, slot B, sequence 14.

Each SAVE returned SUCCESS with a Modbus token/source/revision; CRC and
commit markers were valid. Each requested state was confirmed after a real
physical power cycle. Startup outputs stayed off and volatile R5 state began
from zero. H6 then passed all 10/40 Hz × filt0-filt3 30-second combinations
under normal five-block polling; 10 Hz measured 9.98397-9.98425 Hz and 40 Hz
39.93177-39.93845 Hz, with zero read errors, polling gaps, overrun, fault or
LIMITED. R5 ACTIVE+DOSING remained frozen during H6.

H4 local keypad/menu transaction coverage was not run in this session. The
Modbus/PLC owner, SAVE and physical power-cycle closure is therefore complete;
local menu closure remains deferred.

Per the prompt's default choice C, the exact pretest configuration backup was
restored after the final evidence. Application-only 0x0517 rollback Verify
passed and final configuration readback SHA is again
`A615A475C2D7B5D64126EAEB3AAE9E3D094348FA4C4AEC7BDB8EF0010D4A3BB4`.
Final device is 0x0517, 10 Hz/filt3/strength3, R5 OFF+SHADOW, Checkweigh OFF,
offset/reference/evaluation zero, fault/overrun/dirty/SAVE zero and revision/
saved 8/8.

Formal Release, metrology, cross-sensor, new 12-hour, external DRDY, physical
sensor fault, D1-C nonzero-offset, ASan and UBSan qualifications remain
separate waivers.

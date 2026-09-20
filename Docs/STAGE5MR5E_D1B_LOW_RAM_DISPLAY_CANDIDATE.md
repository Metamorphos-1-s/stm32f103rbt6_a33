# Stage 5M-R5E-D1-B Low-RAM Display Candidate

## Software result

**STAGE 5M-R5E-D1-B LOW-RAM DISPLAY CANDIDATE SOFTWARE READY; FIRMWARE 0x0513
ARTIFACT BUILT; NONZERO-OFFSET HARDWARE QUALIFICATION PENDING; R5 ALGORITHM
UNCHANGED; STAGE 5N ENTRY DEFERRED.**

The 0x0513 artifact has not been flashed. Flashing resets the MCU and clears
the current volatile R5 application/mode/offset/reference state; explicit user
authorization is required.

## Candidate

The candidate is enabled only for the Beta panel while R5 application is
ACTIVE. OFF and SHADOW use the existing DisplayConditioner count unchanged.
Authoritative WeightEngine gross/net, PLC/Modbus/BLE weight, alarms, stability
and R5 input are untouched.

It works in current display-count units, so g/kg/lb, decimals and division are
handled by the existing UnitConverter. A desired count must persist for 1,000
ms. The panel then moves at most one display division per unique 10 Hz sample
until caught up. A deviation greater than 8 divisions releases immediately.
State is exactly 16 bytes and contains no array or dynamic allocation.

Eight candidates were compared: 1d/2d hysteresis crossed with 1/2/3/5 second
confirmation. Exactly one passed: 1d + 1 second. On synthetic +/-0.22 g slow
drift it had maximum lag 1d, maximum update 1d and no >2d/5s stale interval.
Boundary-noise A-B-A count was zero. 500 g load/unload added zero samples of
delay and zero count loss. OFF/SHADOW replay uses baseline counts unchanged.

The first C parity attempt failed 48,082/48,120 because the Python model did
not yet encode baseline initialization and current-event reason semantics.
Subsequent contract corrections occurred before new hardware holdout. Final
fixed-point comparison is 48,120 samples with zero mismatch. R5 parity remains
25,057 samples with zero mismatch.

## Resources and identity

R5E used 19,432 linker RAM bytes. D1-B uses 19,448: `.data` 12 B and `.bss`
18,408 B. Static end to `_estack` is 2,056 B. The conservative stack bound
remains 1,488 B, leaving 568 B collision margin, above the 512 B gate. Main
resolved chain remains 1,128 B. The process function is loop-free and 420 bytes
of Thumb code; a conservative static instruction estimate is below 25 us at
72 MHz versus the 100 ms sample period.

Standard Release remains byte-identical at
`82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486`.
The 0x0513 Beta BIN is 101,480 B, SHA-256
`25921F905BBD03F1EE3F21309479DF311005379A440A7164F882C0245F600E6F`.
Map 0x0104, Schema 2 and Persistent Format 3 are unchanged.

R5 algorithm blobs and R5D/R5E evidence remain frozen. Hardware qualification
must demonstrate a naturally learned nonzero offset of at least 0.020 g and at
least two display divisions of authoritative movement, plus load/unload,
OFF/SHADOW and reset coverage. Until then this is not a complete Beta result.

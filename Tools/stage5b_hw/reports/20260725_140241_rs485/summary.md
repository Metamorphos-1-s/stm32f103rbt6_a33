# Stage 5B-H Summary

- **PASS** bad CRC: no response
- **PASS** other address: no response
- **PASS** broadcast FC03: no response
- **PASS** broadcast FC06: no response
- **PASS** broadcast FC16: no response
- **PASS** illegal function: exception 01
- **PASS** FC03 quantity zero: exception 03
- **PASS** FC03 quantity 126: exception 03
- **PASS** FC03 illegal address: exception 02
- **PASS** FC06 read-only: exception 02
- **PASS** FC06 illegal enum: exception 03
- **PASS** FC16 quantity zero: exception 03
- **PASS** FC16 quantity 124: exception 03
- **PASS** FC16 byte count: exception 03
- **PASS** FC16 truncated: no response
- **PASS** FC16 trailing byte: exception 03
- **PASS** three-byte short frame: no response
- **PASS** noise over 256: no response
- **PASS** fixed-seed random: no response
- **PASS** back-to-back frames: valid FC03 remained responsive afterward
- **PASS** post-error recovery: valid FC03 succeeded

Microsecond timing: NOT VERIFIED WITH CURRENT EQUIPMENT
PVD threshold: REQUIRES ADJUSTABLE POWER SUPPLY

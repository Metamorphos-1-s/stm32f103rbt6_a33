# Stage 5P-A Controlled Product Consolidation

## Software Result

**SOFTWARE CANDIDATE READY; TARGET OR HARDWARE CLOSURE PENDING**

The controlled product candidate is Firmware 0x0518, Map 0x0104, Schema 2,
Persistent Format 3. It preserves R5, D1-D and guarded Checkweigh and supports
10/40 Hz plus filt0-filt3 with rate-aware filter initialization. New/default
configuration is 10 Hz + filt1 strength3; existing valid configurations are
not silently overwritten.

The Stage 5P-A EARLY_STATIC_TRACKING candidate is rejected by offline opened
data and excluded from the binary. This does not change or weaken the existing
R5 long-term algorithm.

## Gates

Host CTest is 35/35 under MSVC `/W4 /WX`; ARM `-Wall -Wextra -Werror` is clean.
R5 parity is 25,057/0, Stage 5N-A parity 45,711/0 and D1-D parity 49,533/0.
Persistence request-byte roundtrip, old V3 safe fallback, 10/40 timing and
rate-aware filter tests pass. ASan/UBSan are NOT RUN.

0x0518 BIN is 107,556 B. RAM is 19,456 B; static analysis reports 2,048 B
static-to-estack, 1,504 B conservative stack and 544 B collision margin.
Release SHA remains frozen at
`82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486`.
The 0x0517 rollback BIN remains
`9D8C5881CD880D2C5D6A7D0C499A4EACF9495A550B08B237A81D4BB2F71641C5`.

## Hardware Boundary

No 0x0518 hardware flash has been performed in this stage. Before any hardware
work, owner authorization must be explicit. The allowed operation will back up
the V3 slots and application, erase application pages only, Verify, and then
test 10/40 Hz, filt0-filt3, persistent request modes and the final safe state.
External 40 Hz physical timing, cross-sensor, metrology, physical sensor-fault,
new 12 h and ASan/UBSan qualifications remain waived/deferred.


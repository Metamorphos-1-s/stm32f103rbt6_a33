# Stage 5P-A Controlled Product Consolidation

## Final Result

**STAGE 5P-A NOT READY; 40 HZ HARDWARE RATE GATE FAILED; 0x0518
WITHDRAWN AND DEVICE ROLLED BACK TO FROZEN 0x0517.**

`OWNER-ACCEPTED QUALIFICATION WAIVERS; NOT METROLOGICALLY OR CROSS-SENSOR
QUALIFIED` does not waive the target-side 40 Hz rate gate. The observed rate
deviation is therefore a hard stop, not a deferred qualification item.

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

## Hardware Result And Hard Stop

The authorized application-only 0x0518 flash and Verify succeeded. The V3
configuration region SHA-256 before testing was
`A615A475C2D7B5D64126EAEB3AAE9E3D094348FA4C4AEC7BDB8EF0010D4A3BB4`.
All rate/filter changes below were applied in RAM; no SAVE was issued.

| Configured path | Target sequence rate | Error | Overrun delta | Result |
|---|---:|---:|---:|---|
| 10 Hz filt0 | 9.9834 Hz | -0.17% | 0 | PASS |
| 10 Hz filt1 | 9.9949 Hz | -0.05% | 0 | PASS |
| 10 Hz filt2 | 9.9834 Hz | -0.17% | 0 | PASS |
| 10 Hz filt3 | 9.9896 Hz | -0.10% | 0 | PASS |
| 40 Hz filt0 | 29.1940 Hz | -27.02% | 0 | FAIL |
| 40 Hz filt1 | 28.9673 Hz | -27.58% | 0 | FAIL |
| 40 Hz filt2 | 26.9211 Hz | -32.70% | 0 | FAIL |
| 40 Hz filt3 | 26.7006 Hz | -33.25% | 0 | FAIL |

No sequence rollback or repeated target sequence was observed within a run,
and fault/overrun remained zero. The captures used approximately 5 Hz Modbus
polling. The interaction between that load and the shortfall is not yet
explained, so the evidence cannot be replaced by the earlier approximately
39.8966 Hz diagnostic result. The prompt's hard-stop rule was applied before
R5 multi-mode, physical load-step, persistent-request power-cycle or default
filt1 detailed hardware tests. These gates are `NOT RUN AFTER HARD STOP`.

The frozen 0x0517 ELF was then programmed application-only and verified. The
post-rollback device is Firmware 0x0517, Map 0x0104, Persistent Format 3,
revision/saved 8/8, OFF+SHADOW, offset/reference/evaluation zero, and
fault/overrun/dirty zero. Configuration SHA-256 after rollback remains exactly
`A615A475C2D7B5D64126EAEB3AAE9E3D094348FA4C4AEC7BDB8EF0010D4A3BB4`.

External 40 Hz physical timing, cross-sensor, metrology, physical sensor-fault,
new 12 h and ASan/UBSan qualifications remain waived/deferred. EARLY is
`REJECTED OFFLINE` and was never enabled on hardware.

## Artifact And Repository State

The withdrawn 0x0518 software artifact is retained only as failure evidence:
BIN 107,556 B,
`7CD3DA2A8EC3A52E1A3B4EA07902668A4687183EF2A9739DD1F81D59F79C0501`;
ELF 2,016,500 B,
`EB1AC785DD2DAF29F5B05F3CE0F2457B36D6367408AFF9C5B863B1AD4A512458`;
MAP 1,444,036 B,
`C54CC6C2CB7DB07D9690E2627D810CBB8287F5BF2E243872A753A66D5F7DFE4D`.
It is not a controlled product release candidate after the hardware failure.

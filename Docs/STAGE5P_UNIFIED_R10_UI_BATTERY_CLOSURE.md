# Stage 5P Unified-R10 UI and battery closure

Baseline: `88a86f0b7c1a40c3017b5aaca00da21df98b3128`

## Preflight

Fresh readback found:

- Firmware `0x0524` / Map `0x010C`;
- active profile 10 Hz, filt1/strength3, calibration valid;
- saved revision 20/20, dirty=0, fault=0, overrun=0;
- R5 ACTIVE+STATIC, Checkweigh OFF;
- non-zero volatile offset approximately `-40,430 µg`.

Because the device was carrying a non-zero volatile offset, no PLC mode
concurrency operation or reset was performed. The offset was preserved.

## Software evidence

The existing Host Debug/Release regression remains 47/47. R9's guarded
pair/SAVE and power-cycle evidence remains PASS. The R9 BUSY root cause remains
UNCONFIRMED from the original R8 evidence because complete raw mailbox frames
were not archived.

## R10 results

| Item | Result |
| --- | --- |
| Fresh 0x0524 identity and state readback | PASS |
| Menu candidate confirmation / STAR-HASH navigation | NOT RUN: user operation not completed |
| TARE candidate cancellation | NOT RUN |
| PLC/menu concurrency | NOT RUN: non-zero offset preserved |
| ACTIVE/checkweigh mutual exclusion | NOT RUN |
| Battery percentage/diagnostic voltage UI | NOT RUN |
| Idle dimming and wake | NOT RUN |
| Low-voltage physical stimulus | NOT RUN |
| Physical 500 g load/unload | NOT RUN by scope |
| Non-zero-offset DOSING freeze | NOT RUN by scope |
| Single-epoch resource qualification | INCOMPLETE |
| Cross-record nine-path engineering evidence | PASS |
| A13B long-duration efficacy | INCONCLUSIVE |

No device writes, SAVE, ZERO, TARE, burn, or mode changes were made in R10.
The full preflight is retained at
`Results/stage5p_unified/hardware/stage5r10_preflight.json`. The 0x0524 device
remains in its readback ACTIVE+STATIC state with its volatile offset intact.
No formal Release, metrology, cross-sensor or long-duration claim is made.

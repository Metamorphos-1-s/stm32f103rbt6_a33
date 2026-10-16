# Stage 5P 0x0530 focused hardware acceptance

## Conclusion

**0x0530 INSTALLED; FOCUSED STATIC AND LOAD/UNLOAD EVIDENCE PASS IN
OBSERVED SCOPE; DROPLET/PANEL VISUAL ITEMS PENDING OR NOT RUN.**

The software handoff was fixed at source HEAD
`a2174ede65a1927ae491e73289c95bdcaa97deb5` from
`stage5p-display-anchor-follow-v3`. The hardware branch is
`stage5p-0530-hardware-acceptance`. The handoff ZIP and all supplied
software evidence were audited before device writes. The historical
0x052E result is retained as the baseline; no 0x052E result is presented
as 0x0530 evidence.

## Software and identity gates

The supplied 0x0530 BIN is 98,796 B and SHA-256
`8ADB103085629F9A0F645CA25F324C6B53DDB395C714E62D1C0306ED7CBCBA34`.
The vector table has an aligned initial SP at `0x20005000` and a Thumb
reset address inside the application region. The supplied manifest's 2,166
Git blob IDs all match the fixed Git tree; all 23 supplied evidence files
match their manifest hashes. The software report's gates remain software
only: Host Debug/Release 51/51, 78 display scenarios, Python 17/17,
ASan/UBSan pass as reported, static RAM/stack pass with 608 B margin.

The local hardware tools used the ST GNU Tools for STM32 compiler bundle
identity from the handoff and STM32CubeProgrammer 2.19.0. COM5 was
identified as the CH340 USB adapter (VID 1A86, PID 7523). One ST-LINK was
identified by serial `E1007200D0D2139393740544`, target ID `0x410`, Flash
128 KiB, target voltage 3.29 V.

## Preflight and backup

Initial COM5 preflight read **0x052E / Map 0x010F**, 40 Hz/filt1/strength3,
R5 ACTIVE+STATIC, checkweigh STATIC, slot A sequence 23, revision/saved
23/23, dirty/fault/overrun 0. The measured load was approximately 0.355 g;
the historical 500 g state was not assumed. A 100-request FC03 link gate
then returned 100/100 valid responses with zero port failures, timeouts,
CRC errors or exceptions.

After the owner checked the SWD connection, the complete backups were read:

* Application 126,976 B SHA-256:
  `F8EAB8D554E8163F55DD41DFC85EC445DDF6237E4B2242422AAD6823B6168730`
* V3 configuration 4,096 B SHA-256:
  `05CB5BAFB2E722702F8B132F1FAC6C273BA273A7A1A3ABD50F6FDCA72CECFBC1`

Both V3 slots were committed and CRC-valid; A sequence 23 was active and
B sequence 22 was older. The saved calibration, battery, communication,
filter and request fields were preserved.

## Programming and readback

The first attempted erase command was rejected by the programmer and no
candidate was written. The accepted retry used Normal SWD and the explicit
single argument `-e '[0 123]'`, then wrote the fixed 0x0530 BIN at
`0x08000000`, verified it, and performed an explicit reset. Configuration
pages 124-127 were not erased or written.

Full post-program readback showed:

* first 98,796 B exactly equal to the supplied BIN;
* remaining 28,180 application bytes all `0xFF`;
* V3 configuration byte-identical to the preflash backup;
* active slot A sequence 23 with valid CRC/commit.

Final COM5 identity was 0x0530/0x010F, 40 Hz/filt1/strength3,
R5 ACTIVE+STATIC, checkweigh STATIC, revision/saved 23/23,
dirty/fault/overrun 0. Reset cleared the prior volatile offset as
expected; no SAVE, ZERO, TARE or calibration was performed.

## Static display evidence

The original 0x052E 120 s baseline contained 1,150 valid rows, zero read
errors and 38 display-count changes. Its display range was 500.34-500.45 g
and its raw evidence is in `baseline_static`.

The 0x0530 120 s candidate capture contained 1,144 valid rows, zero read
errors and only 3 display-count changes. DisplayConditioner was LOCKED for
all rows. The source CSV SHA-256 is
`A59FB222050626CBBE366A5D205240AD363465883C2483B4200188746F3BC6C8`.
This is MCU-side sparse host evidence; it is not an electrical LED
readback. No user visual feedback was supplied for this interval, so the
panel visual sub-item is `NOT OBSERVED`.

The subsequent 600 s candidate static capture contained 5,660 rows,
zero read errors, zero fault/overrun rows, no sequence regression and 31
display-count changes. The CSV SHA-256 is
`1FD6C53A56B0B5E666CBCDBA3909A84F0FA1D48998C3C7E24FF7176CE12BA82E`.
Display state remained CANDIDATE/LOCKED; no prolonged TRACKING interval
was observed. No idle visual jitter was reported by the owner.

## Three-round 500 g load/unload evidence

The final continuous recording contains 15,125 rows, zero read errors,
zero fault/overrun rows and no device sequence regression. Host polling is
sparse and non-atomic; 15,124 sequence gaps are host omissions, not proof
of MCU loss. The CSV SHA-256 is
`9D35C0235E0FE58D9B4E998C6E3BDB6C6BAE6B8B73D5A0FFACE5DB2B24F002BD`.

The six data-inferred weight edges were:

| Event | Data-inferred edge interval UTC |
| --- | --- |
| Unload 1 | 09:42:37.808-09:42:37.908 |
| Load 1 | 09:45:54.788-09:45:54.896 |
| Unload 2 | 09:49:14.609-09:49:14.710 |
| Load 2 | 09:51:29.565-09:51:29.666 |
| Unload 3 | 09:55:03.668-09:55:03.768 |
| Load 3 | 09:57:49.236-09:57:49.338 |

Owner replies are retained separately as message-time markers and are not
used as exact physical edges. Each event was followed by at least 30-60 s
of the requested static interval. The data showed the display crossing the
250 g load threshold in the same sparse host observation window as the
weight threshold; this is not a complete 40 Hz same-sample or LED timing
claim. The event analysis reports zero fault/overrun rows.

## Droplet test and remaining limits

The owner explicitly skipped the stable-container, one-drop-per-second,
2-second pause and 10-second pause tests. These are **NOT RUN**, not
inconclusive or pass. No panel visual feedback was supplied for 0x0530;
the visual item is **NOT OBSERVED**. No low-voltage, cross-sensor,
long-duration drift or formal metrology qualification was attempted.

The 0x0530 hardware result is therefore limited to installation integrity,
configuration protection, communication, sparse static display telemetry,
and three observed load/unload edge pairs. It does not upgrade A13B
long-duration efficacy, cross-sensor qualification or formal metrology.

## Final state and evidence

Final read-only state at closeout: 0x0530/0x010F, 40 Hz/filt1/strength3,
R5 ACTIVE+STATIC, checkweigh STATIC, slot A sequence 23,
revision/saved 23/23, dirty/fault/overrun 0. The final closeout snapshot at
`2026-10-16T14:40:43.777593+00:00` records an empty platform:
net/gross 385,789 ug (0.385789 g), display count 39 with two decimals
(0.39 g), and volatile R5 offset -219,089 ug. This offset is not persistent
configuration.

The earlier empty-platform snapshot at
`2026-10-16T10:02:19.922533+00:00`, after the owner removed the third
500 g load and before the droplet tests were skipped, records net/gross
367,786 ug (approximately 0.37 g), display count 37, and offset +53,419 ug.
The 0.37 g reading and the final -219,089 ug offset therefore belong to
different snapshots.

The initial 0x052E preflight already recorded net/gross +355,071 ug
(0.355071 g). A positive near-zero reading was present before installing
0x0530; these sparse snapshots do not establish a new display-firmware
zero-return regression, nor do they qualify zero return or drift.
The residual remains a separate metrology observation.

Raw frames, CSV files, event markers, backups, readbacks, programmer logs,
edge analysis and SHA manifests are under
`Results/stage5p_0530_hardware/20261016T083710Z_0530_focus/`.

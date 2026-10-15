# Stage 5P 0x052E display smoothing v2 hardware closure

## Result

0x052E was installed after explicit authorization. The corrected sequence
used Normal SWD connection, application erase `[0 123]`, Verify, and an
explicit MCU reset. COM5 returned normally after reset. The V3 region was
not written.

The candidate is currently retained on the device for observation. This is
a focused display-rate/noise result, not a full product qualification.

## Firmware and configuration

* Firmware: `0x052E`, Map `0x010F`
* BIN: 98,980 B
* BIN SHA-256:
  `AC13730228B15E5C9EA031D64795E28CF589986EEB18F63B6F056396346ED7D9`
* ELF SHA-256:
  `6D620F8A0DEB4FE706248B615E06ED5F86B46CF93E0593464A2B063AFC626155`
* V3 configuration SHA before/after:
  `05CB5BAFB2E722702F8B132F1FAC6C273BA273A7A1A3ABD50F6FDCA72CECFBC1`
* Slot A sequence 23 remained active; revision/saved 23/23.

Final readback: 40 Hz/filt1/strength3, R5 ACTIVE+STATIC/TRACKING,
offset +1,587 ug, checkweigh STATIC, dirty/fault/overrun 0. The existing
500 g load remained on the platform. No SAVE, ZERO, TARE or profile change
was performed.

The 126,976 B application readback matched the 0x052E BIN prefix and had
an all-FF tail. Full application readback SHA was
`F8EAB8D554E8163F55DD41DFC85EC445DDF6237E4B2242422AAD6823B6168730`.

## Display capture

The 120-second 500 g capture contains 979 rows and zero read errors. CSV
SHA-256:
`2D25865DC1566E3795B7B5B96D13FC3A4AE9F77E0012BE35B837DD42866DA14E`.

Compared with the 0x052D v1 capture:

| Metric | 0x052D v1 | 0x052E v2 |
| --- | ---: | ---: |
| Rows | 965 | 979 |
| Display-count changes | 818 | 39 |
| Unique display counts | 15 | 11 |
| Read errors | 0 | 0 |
| Observed change interval | 113-139 ms | 112-135 ms |

The v2 anchor held the display through the small alternating noise band;
the display no longer followed every 500.57-500.63 g fluctuation. Large
step handling and the 100 ms publication gate remain in the code. This is
MCU-side display-count evidence; actual LED segment light output was not
electrically read back. The user's earlier visual report is retained as
separate evidence.

## Qualification boundary

| Item | Result |
| --- | --- |
| 0x052E programming, Verify and COM5 startup | PASS |
| Application-only erase and V3 preservation | PASS |
| 500 g static display-rate/noise capture | PASS in recorded scope |
| Human visual confirmation during v2 capture | NOT OBSERVED |
| Long idle static jitter qualification | NOT RUN in this retry |
| Fresh load/unload edge response | NOT RUN; load was already present |
| 40 Hz metrology/R5/checkweigh safety regression | Existing software gates retained; no new full matrix |
| Long-duration drift efficacy | INCONCLUSIVE, unchanged |
| Cross-sensor/formal metrology qualification | NOT RUN |

The earlier 0x052D failed-attempt evidence remains preserved; the v2
retry shows that the corrected erase/reset procedure starts and communicates
normally. Full visual qualification still needs a user-observed idle and
load-edge check without changing the current saved configuration.

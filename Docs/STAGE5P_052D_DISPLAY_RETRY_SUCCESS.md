# Stage 5P 0x052D display candidate retry

## Result

The first 0x052D attempt remains recorded as a failure in
`STAGE5P_052D_DISPLAY_SMOOTH_DIAGNOSTIC.md`: the programming operation
verified, but the post-program COM5 check was performed before an explicit
MCU reset and timed out. Its rollback and the configuration-page boundary
mistake are preserved.

This retry used the corrected procedure and succeeded:

* Firmware `0x052D`, Map `0x010F`
* BIN 98,788 B, SHA-256
  `D9A5278BBAB8FE2F68F13F1C7A175A686C81F1A5101819ED65AACB816BE8EEBF`
* application erase syntax: `-e '[0 123]'` (application pages only)
* explicit SWD reset after programming
* COM5 readback successful after reset

The configuration SHA before erase, immediately after application-page
erase, after programming, and at the final readback remained
`05CB5BAFB2E722702F8B132F1FAC6C273BA273A7A1A3ABD50F6FDCA72CECFBC1`.
V3 slot A sequence 23 remained active; no configuration write occurred.

## 500 g display-rate capture

With the existing 500 g load still on the platform, the retry recorder
captured 965 rows and zero read errors. CSV SHA-256:
`3D964A2B2DA547E98C0BFF46DBFC5B7666A76E0E1B5F03827A52E12655F4DEA4`.
The raw device sample path remained 40 Hz/filt1/strength3. The MCU-side
display-count changes observed in the 120-second capture had intervals
between approximately 113 and 139 ms, consistent with the candidate's
100 ms numeric publication gate and the host poll cadence. This is not a
physical LED photometric measurement and does not prove a human visual
rate independently of the panel.

The final COM5 snapshot read 0x052D/0x010F, 40 Hz/filt1/strength3,
R5 ACTIVE+STATIC/TRACKING, offset +32,238 ug, checkweigh STATIC,
revision/saved 23/23, dirty/fault/overrun 0. The saved V3 configuration
was unchanged. No SAVE, ZERO, TARE, profile change or alarm configuration
change was performed.

## Qualification boundary

| Item | Result |
| --- | --- |
| Corrected 0x052D programming/Verify and COM5 startup | PASS |
| V3 configuration preservation | PASS |
| MCU-side 10 Hz numeric publication evidence | PASS in recorded scope |
| Human panel visual confirmation | NOT RUN in retry; no new visual message was supplied |
| Long static idle flicker qualification | INCONCLUSIVE; no long idle event in retry |
| 500 g load/unload qualification | NOT RUN; the load was already present and no unload was performed |
| R5/checkweigh/SAVE behavior | Existing 0x052C evidence retained; not requalified here |
| A13B efficacy, full Host suite, cross-sensor, formal metrology | unchanged |

The 0x052D candidate is now running on the device for observation, but it
is not a complete display qualification. The earlier failure evidence is
not reclassified; this retry demonstrates that the failure was partly the
programming/reset sequence rather than a confirmed candidate runtime crash.

Artifacts are under `Results/stage5p_052d_display_candidate/retry_2/`.

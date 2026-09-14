# Stage 5L-R G3 External Timing

Status: `STAGE 5L-R G3 BLOCKED; EXTERNAL INSTRUMENT UNAVAILABLE`.

## Completed preflight

G3 starts from `1328f4019ba7e44613274e1444c60e5aea21f429` on branch `stage5lr-g3-external-timing`. All nine G2 Manifest V2 records validate against committed Git blobs. Client and CH579 remain unchanged at `c4e4906f0a47a427793df6cfcb414756ac7984cc` and `eb888925e4fcc9dcd9bf89e8cc42e5b28679e520`.

COM5 is a present CH340 device at VID/PID 1A86:7523. Strict FC03-only reads confirm product Firmware 0x0510, Map 0x0104, Public Schema 2, Persistent Format 3, Profile 0, gain 128, filter 3/3, CS1237 RUNNING, calibration valid, revision/saved revision 7/7, dirty 0, fault 0, ConfigStore idle, active slot A and storage sequence 7. Active canonical SHA-256 is `91D346E87BD112EFAC3B513A8CAFBBDDE9642069A15280DB7565374378BA43E1`. COM3 was neither opened nor probed.

## Pin and electrical facts

Current firmware maps PB10 to `MCU_AD_SCLK` and PB11 to `MCU_AD_DOUT`. PB10 is open-drain/NOPULL. PB11 is input/NOPULL for ready/data and temporarily open-drain for configuration writes. Both depend on board pull-ups. The repository has no schematic, PCB source or test-point mapping. Prior new-board measurements reported approximately 4.997 V pull-ups, so a 3.3 V-only logic input must not be connected directly without a rated adapter or probe.

The driver requests nominal 1 microsecond SCLK high and low phases. A digital capture must sample at least 10 MHz, preferably 20 MHz or higher. Actual instrument threshold, bandwidth, probe ratio, channel mapping and export format must be recorded before connection.

## Current blocker

No connected logic analyzer or oscilloscope and no sigrok, Saleae Logic, DSView or PicoScope capture tool was detected. The operator then explicitly confirmed that no external instrument is available. No diagnostic firmware was flashed and no 40 Hz command was issued in G3.

SWD timing from G2 is not external evidence. G3 external capture, two-session repeatability and metrology requalification did not start; Profile 1 remains contained. Missing items are external 10/40 Hz DRDY rates, SCLK pulse counts and widths, DOUT phase margin, external raw decode, two independent cold-start sessions, 40 Hz empty/500 g/ten-cycle metrology, and final re-enablement evidence.

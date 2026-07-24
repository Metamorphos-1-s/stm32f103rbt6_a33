# Menu Controller

The non-blocking menu browses calibration, metrology, display, retention, save,
and exit items. Editing uses `CommandService` and `ConfigEdit`; it never writes
`DeviceConfig` directly. STAR/HASH change items or values, ZERO changes edit
step, FUNCTION confirms, and TARE cancels. A 30-second timeout cancels pending
edits. Sample rate and gain are read-only in Stage 4A, and SAVE reports noSAVE.

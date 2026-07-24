# Menu Controller

The non-blocking menu browses calibration, metrology, display, retention, save,
and exit items. Editing uses `CommandService` and `ConfigEdit`; it never writes
`DeviceConfig` directly. STAR/HASH change items or values, ZERO changes edit
step, FUNCTION confirms, and TARE cancels. A 30-second timeout cancels pending
edits. Sample rate and gain remain read-only. Stage 4B enables asynchronous
SAVE and adds a second-confirmation factory reset (TARE cancels and
FUNCTION-long confirms).

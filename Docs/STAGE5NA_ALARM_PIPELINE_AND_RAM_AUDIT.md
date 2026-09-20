# Stage 5N-A Alarm Pipeline and RAM Audit

## Current configuration and units

The live 0x0514 device was read through COM5 without writes. Alarm limit
enable, lower limit, upper limit, hysteresis, internal buzzer enable, external
buzzer enable and qualified beep enable are all zero. Weight source is NET.
Formal alarm state and every lamp/buzzer diagnostic are zero. Configuration is
clean at revision/saved revision 8/8.

Limits are stored and compared as signed integer micrograms. Conversion to the
active display unit occurs only at presentation boundaries. The metrology
quantities remain distinct: legal verification interval `e = 1.000 g`, current
display division `d = 0.01 g`, and stable-enter threshold `0.05 g`.

## Formal alarm path

`App_UpdateAlarmOutputs` runs from the 20 ms task but processes a measurement
only when sample sequence, configuration revision, application state or weight
fault changes. `LimitChecker_Process` selects authoritative NET or GROSS from
the `WeightSnapshot`; in the current Beta that weight includes any ACTIVE R5
correction. It never reads panel display count, display anchor or conditioned
display mass.

Formal classification requires formal stable. While unstable, the current
implementation retains the last stable classification if one exists; otherwise
it reports DISABLED. Calibration or disabled limits clear qualification.
Weight-invalid fault produces FAULT, overload produces OVERLOAD, and invalid
alarm configuration produces FAULT. A classification-affecting revision change
resets the checker through an explicitly unstable snapshot.

The existing equality contract is lower-inclusive and upper-inclusive:
`weight < low` is LOW, `low <= weight <= high` is OK, and `weight > high` is
HIGH. Persisted alarm validation is stricter than the Stage 5N-A shadow
contract: enabled persisted limits require `low < high` and hysteresis no more
than half-span. Stage 5N-A still tests equal limits and `low > high`; the latter
is always INVALID.

R5 mode is not currently an input to the formal LimitChecker. OFF, SHADOW,
STATIC and DOSING therefore affect formal alarm only through their effect on
the authoritative snapshot. Stage 5N-A adds explicit read-only
`process_active = (R5 mode == DOSING)` solely to suppress STATIC SHADOW; it does
not change the formal checker.

ZERO, TARE, clear-tare, profile/filter/unit changes and gross/net view changes
can change the underlying snapshot or revision, but the formal checker has no
dedicated action reason for each. Stage 5N-A supplies explicit reset reasons
to its isolated state machine. Startup, calibration, overload, weight-invalid
fault, invalid input, sequence gap, non-monotonic/long timestamp, revision and
all listed actions reset or suppress SHADOW classification.

## Outputs and external surfaces

The formal output mapping is direct and active-high:

- OK -> green lamp PB6
- LOW -> yellow lamp PB8
- HIGH, OVERLOAD or FAULT -> red lamp PB7
- internal buzzer -> PB5 when enabled
- external RGY buzzer -> PB9 when enabled

`AlarmOutputManager_Apply` writes these pins through `OutputGpio_Set`.
Modbus public Map 0x0104 exposes formal configuration and state at
0x0220-0x023B, including state 0x0231, alarm-active 0x0233, lamps 0x0234-0x0236
and buzzers 0x0237-0x0238. PLC software consuming Modbus sees those formal
registers. No separate independent PLC alarm computation was found. BLE
checkweigh telemetry also snapshots the same formal AlarmOutputDiagnostics.

The Stage 5N-A candidate will not call LimitChecker, AlarmOutputManager or
OutputGpio and will not replace `App_GetAlarmOutputDiagnostics`. Public Modbus,
BLE checkweigh, physical GPIO and panel display therefore remain byte-for-byte
on the formal path. Only an engineering Beta diagnostic range observes Alarm
SHADOW. This proves target integration can proceed without changing official
weight or output semantics.

## Weight paths

STATIC SHADOW uses the authoritative NET/GROSS mass and official stable bit.
DYNAMIC SHADOW uses the fastest safe calibrated value available without
duplicating WeightEngine: the current raw ADC sample passed through the frozen
`CalibrationModel_ConvertMass` with the active calibration and zero offset.
The live calibration is raw zero -44,047, raw span -487,965 and span mass
500,000,000 ug. The raw value is never compared directly with mass limits.

The product has one configured WeightFilter instance. Modes NONE, AVERAGE,
IIR and MEDIAN3+IIR are alternatives, not parallel filt0/filt1/filt2/filt3
outputs. Stage 5M-F did not create a reusable qualified fast path. Dynamic
SHADOW therefore computes one calibrated mass from current raw input and never
feeds it into formal stable, R5, official weights or display.

## State and resource budget

Formal alarm static state consists of `LimitChecker`, `AlarmOutputManager`,
last sample/revision/state/fault flags and a cached AlarmConfig. None is reused
or modified by Stage 5N-A. The selected independent SHADOW design needs four
uint32 fields and eight uint8 fields, exactly 24 bytes before target ABI
verification. It has no arrays, allocation or queue.

D1-C collision margin is 568 bytes and the hard minimum is 512 bytes. Stage
5N-A budgets at most 24 bytes permanent state and 24 bytes additional worst
call-chain stack. The engineering diagnostic will read a snapshot of that
state without duplicating histories. Final map and stack-usage analysis remain
mandatory before 0x0515 can be built or flashed.

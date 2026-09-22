# Stage 5N-B Output Ownership and Safety Audit

## Current Pipeline

The product path is `WeightSnapshot -> LimitChecker -> AlarmOutputManager ->
OutputGpio -> BSP GPIO`. `LimitChecker` currently computes the formal state.
`AlarmOutputManager` owns the formal state, lamp selection and nonblocking
buzzer timing. It is the only application module that writes all five alarm
outputs. GPIO is active-high at the BSP API: green PB6, yellow PB8, red PB7,
internal buzzer PB5 and external output/buzzer PB9.

`COMMAND_REQUEST_MANUAL_OUTPUT` currently returns ACCEPTED but has no GPIO
implementation. Therefore no second runtime writer currently races the alarm
manager. Stage 5N-B does not implement manual output. Any future manual-output
implementation must acquire the same ownership instead of writing GPIO beside
the guarded alarm path.

The old LimitChecker retains its last stable LOW/OK/HIGH classification while
official stable is false. That behavior is retained for standard Release and
0x0515, but is unsuitable for guarded ACTIVE because PENDING must immediately
release outputs. Stage 5N-B therefore replaces only the 0x0516 source selection
inside `App_UpdateAlarmOutputs`; it does not alter LimitChecker.

## Guarded Ownership

The frozen Stage 5N-A candidate continues to calculate SHADOW. A separate
`GuardedCheckweigh` selector owns the volatile mode `OFF/STATIC/DYNAMIC`.
STATIC reads only the frozen STATIC output; DYNAMIC reads only the actual
current DYNAMIC output. It maps valid LOW/OK/HIGH into `CheckweighResult`, then
the existing AlarmOutputManager atomically applies the state and all GPIOs.
No new module calls OutputGpio or BSP output functions.

Mode changes call `AlarmOutputManager_AllOff` immediately. The selector remains
safe for the first new candidate sample and cannot activate until the following
new sample. OFF, candidate PENDING/INVALID, any fault, calibration and a sample
age over 250 ms map to existing formal DISABLED and turn every output off. The
precise safety reason remains visible through engineering diagnostics.

The manager's existing output semantics remain:

- LOW: yellow lamp; no alarm buzzer.
- OK: green lamp; optional one-shot 100 ms qualified beep.
- HIGH: red lamp; optional internal/external 250 ms alternating alarm phases.
- DISABLED: no lamp and no buzzer.

The timers use elapsed timestamps and never block the main loop. Host GPIO
tests verify mutually exclusive lamps, internal/external enables and immediate
all-off for OFF/PENDING/INVALID/fault/stale.

## Runtime And Persistence

The mode state is static RAM initialized to OFF. Reset and power-on always
return to OFF. A change increments only a volatile generation counter; it does
not call SystemContext replacement, persistence or SAVE and cannot change
revision, saved revision or dirty. Existing AlarmConfig remains the sole source
for lower/upper limits, NET/GROSS selection and buzzer enables. A disabled or
invalid AlarmConfig makes ACTIVE safe-disabled.

The local advanced menu item is `ALArn`; choices are `OFF`, `StAtIC` and
`dynAnI`. It reuses the R5E transaction contract: STAR/HASH choose, short
FUNCTION confirms, long FUNCTION applies, TARE cancels, menu timeout cancels,
and a generation mismatch caused by PLC control returns BUSY rather than
overwriting the external update.

PLC control reuses the existing mailbox. Beta command 34 sets mode and command
35 gets status. SET can require the current generation. Invalid modes and BLE
write attempts are rejected. There is no SAVE and no persistent field.

## Protocol Compatibility

Public Map remains 0x0104, Schema 2 and Persistent Format 3. No existing public
address or value changes. The existing public alarm state at 0x0231 still uses
the frozen enum 0 DISABLED, 1 LOW, 2 OK, 3 HIGH, 4 OVERLOAD and 5 FAULT. Since
that enum has no PENDING/INVALID/OFF values, all guarded safety states appear to
old clients as DISABLED(0), never as a valid weight region. Physical outputs
are all off.

The Beta-only 0x02C0-0x02CF diagnostic block distinguishes mode, generation,
reason, formal state and output flags. It follows the repository's existing
engineering-extension convention and is explicitly outside the public 0x0104
contract. BLE V1 framing is unchanged; BLE remains read-only and reports the
same formal AlarmOutputManager diagnostics. PC, W02/CH579 and existing PLC
clients therefore need no Map migration. New PLC software uses the mailbox and
engineering status only when it intentionally targets firmware 0x0516.

## Audit Answers

1. Formal classification is currently calculated by LimitChecker; 0x0516
   guarded mode selects the frozen candidate instead.
2. AlarmOutputManager exclusively owns formal lamp and buzzer output.
3. Manual output cannot currently race because it has no GPIO implementation.
4. OFF publishes DISABLED and forces all alarm outputs off while SHADOW runs.
5. GuardedCheckweigh selects one candidate and passes one CheckweighResult to
   the sole AlarmOutputManager.
6. Safety conditions map synchronously to DISABLED/all-off; no previous region
   is retained.
7. Existing CommandService/mailbox runtime control is extended under Beta
   compilation.
8. PLC mode control does not require a public Map upgrade.
9. DeviceConfig has alarm thresholds/enables but no runtime mode field.
10. Mode resides only in reset-cleared RAM and never enters Format 3.

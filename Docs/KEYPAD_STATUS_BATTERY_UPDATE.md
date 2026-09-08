# Keypad, STATUS and battery divider update

This note records the software contract implemented on the
`fix-usart3-command-source-validation` development branch. It is not hardware
qualification evidence.

The current firmware release value is `0x050B` (5.11), with Register Map
`0x0104` and persistent Schema `2`. Earlier hardware records using `0x050A`
remain historical evidence and are not rewritten.

## UI ownership and persistence

RUN STAR short is reserved and has no command, display-success, configuration,
output, communication, or persistence effect. STAR long activates
`StatusController`. While active, that controller consumes every key event
before RUN dispatch, so STATUS TARE/HASH/ZERO cannot become weighing actions.
The previous display page and weight view are restored on exit.

STATUS keeps the entry communication snapshot, a confirmed candidate, a
separate unconfirmed edit value, and the entry configuration revision. LIST
TARE or timeout discards the whole not-yet-applied candidate; VIEW/EDIT TARE
returns to LIST and EDIT TARE cancels only that field's unconfirmed value. STAR
long cancels only the current unconfirmed edit, validates the confirmed
candidate, rejects a foreign revision, and requests the existing asynchronous
CommunicationManager apply. Only a
successful, candidate-matching apply at the expected next revision can request
PersistenceManager SAVE. The saved revision must match the request revision
before `donE` is shown. There is no retry.

The menu follows the same whole-snapshot safety rule. FUNCTION short confirms
and applies an item to RAM. FUNCTION long cancels the current unconfirmed value,
saves prior confirmed changes, waits for the matching Flash result, and exits
after `donE`/`noCHG`. EDIT TARE returns to the menu list; LIST TARE and timeout
exit without requesting Flash. Explicit `SAUE` can save a dirty snapshot that
predates menu entry, while long FUNCTION only saves changes confirmed in this
menu session. Since ConfigStore persists a complete configuration/runtime
snapshot, any unexpected revision rejects automatic save; no field-level merge
is attempted. Profile switching waits for its real asynchronous result and
reads the resulting revision instead of predicting it.

## STATUS items

LIST shows only six-character driver-supported labels. FUNCTION opens the
selected value in VIEW or EDIT; TARE returns from either parameter level to the
same label. LIST ignores key-repeat events. The STAR used to enter STATUS is
blocked until its matching release event, preventing post-long-press repeats
from moving the selection.

APPLY and SAVE use separate bounded transaction timeouts. Errors and uncertain
results remain visible for the full UI message interval. If communication was
applied to RAM but SAVE failed, another explicit long STAR retries SAVE only;
it does not reconfigure the UART or increment the configuration revision again.

| Item | Access | Display or UI domain |
|---|---|---|
| Firmware | Read only | `FW_RELEASE_VERSION` as major.minor |
| Register Map | Read only | `MODBUS_REGISTER_MAP_VERSION` as major.minor |
| Schema | Read only | `DEVICE_CONFIG_SCHEMA_VERSION` |
| Profile | Read only | active profile index |
| SPd | Read only | active profile actual 10/40/640/1280 Hz value |
| GAIn | Read only | active profile actual 1/2/64/128 gain |
| Battery | Read only | common BatteryAdc result in volts |
| Protocol | Read only | current RTU/custom value; custom is not editable |
| Address | Editable | 1..247, wrapping |
| USART2 baud | Editable | 9600/19200/38400/57600/115200, wrapping |
| Parity | Editable | None/Even/Odd, wrapping |
| Stop bits | Editable | 1/2 |
| Word order | Editable | high-word-first/low-word-first |

The address is shared by USART2 RS232/RS485 and the fixed-parameter
USART3/CH579 Modbus server. Baud, parity, and stop bits reconfigure USART2 only;
USART3 remains 115200 8N1. Apply uses the existing server suspend, old-response
drain, UART restart, server resume, and rollback state machine.

## Menu range audit

| Parameter | Local UI domain | ConfigEdit / validator | Application constraint |
|---|---|---|---|
| Unit | enabled kg/g/lb values | enabled mask; Class III excludes lb | current unit display must represent CAP |
| Profile | qualified presets | valid profile index | production presets remain 10/40 Hz |
| CAP | positive six-digit display value | positive; complete metrology validation | not above known load-cell rating |
| dIU | 1/2/5 | exactly 1/2/5 | current unit/CAP must remain representable |
| dP | 0..5 | 0..5 | current unit/CAP must remain representable |
| FILt | supported enum values | mode-specific strength validation | no unsupported filter value generated |
| StAb | 10..10000 ms, saturating | 10..10000 ms | active profile only |
| ZrnG | non-negative six-digit value | 0..CAP | relationship checked at full validation |
| OL | non-negative six-digit value | 0 or >=CAP; product requires >0 and <= sensor rating | relationship checked at full validation |
| briGHt | 1..7, wrapping | 0..7 for legacy persisted compatibility | local UI never generates 0 |
| P-Zr/trrEt/alarm enables | 0/1 | boolean only | local UI toggles only |
| Lo/Hi | signed six-digit display value | Lo < Hi when enabled | relationship checked at full validation |
| HyS | non-negative six-digit value | <= half of Hi-Lo span | relationship checked at full validation |
| Src | Net/Gross | valid alarm source enum | local UI toggles only |

SPd and GAIn are hidden from the advanced edit sequence and shown read-only in
STATUS. `rEAd` means Read Only, not Exit. Profile remains the safe production
rate selector. Arbitrary gain is not exposed because gain changes weight scaling
and existing calibration records are not gain-bound. A future gain editor must
provide safe reconfiguration, settling, rollback, and either mandatory
recalibration or per-gain calibration records.

## Battery divider migration

All current boards use `VBAT--47k--ADC--10k--GND`, a 5.7 ratio. At 16.8 V the
ADC pin is approximately 2.947 V and a 3.3 V 12-bit ADC reads approximately
3657, below full scale. The same conversion path reports approximately 14.8 V
for a 2.596 V ADC input and feeds both display and alarm decisions.

Startup normalization changes only the exact legacy 30000/10000 pair to
47000/10000. It sets the existing migration-pending-save and dirty state so the
new ratio is used immediately, but does not write Flash during startup. Other
ratios are preserved as possible factory-specific calibration. Longer term, the
resistor ratio should be selected by hardware revision/build configuration and
Flash should contain only voltage gain/offset calibration.

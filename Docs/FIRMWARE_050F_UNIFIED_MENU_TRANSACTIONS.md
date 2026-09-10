# Firmware 0x050F unified local menu transactions

Firmware 0x050F keeps Register Map 0x0104 and persistent Schema 2. Schema 2
records written by 0x050E remain compatible.

Both local configuration surfaces now use an original/candidate/edit/commit
model. Short FUNCTION confirms only into the current session candidate. It does
not update Active RAM, dirty, revision, UART settings, or Flash. TARE at LIST
and session timeout discard the candidate; edit-level TARE cancels only the
unconfirmed field. Long FUNCTION is the only normal commit action. The legacy
SAUE and EHIt enum values are retained for compatibility but navigation skips
them in ordinary and advanced menus.

PersistenceManager writes the candidate to the inactive A/B slot under the
existing CRC and commit-last protocol. During the bounded operation the runtime
uses a transient candidate without publishing a revision. A successful Flash
commit publishes the target revision once and clears dirty. A pre-commit error
re-applies the original configuration and restores the exact runtime/revision
snapshot. A committed lock or runtime-finalization error is REBOOT_REQUIRED;
the controller retains ownership and never retries the SAVE.

STATUS uses the existing asynchronous CommunicationManager suspend, response
drain, USART2 reconfigure, resume, and rollback sequence only after long
FUNCTION. Its visible sequence is APPLY, SAUE, donE. Long STAR never commits.
On a pre-commit SAVE failure, STATUS restores the original USART2 settings after
PersistenceManager restores the configuration snapshot.

Brightness is the only immediate visual preview. It never enters Active config
before commit and is restored on TARE, timeout, validation failure, or SAVE
failure. Profile and all other menu fields remain candidate-only.

Dirty ownership is intentionally narrow:

- A local menu candidate is saved only by that active session.
- Exact battery-divider migration pending state may be saved by the next user
  long FUNCTION; startup never writes Flash automatically.
- A calibration completed from the local menu explicitly transfers its dirty
  revision back to that menu for long-FUNCTION save.
- Modbus and BLE transactions retain their own APPLY/SAVE flows.
- Unknown dirty state or any foreign revision change is rejected with bUSY.

This note describes software behavior. Hardware qualification is recorded
separately after programming and observation.

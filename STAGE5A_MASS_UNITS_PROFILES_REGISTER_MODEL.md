# Stage 5A Handover

## Baseline and scope

- Actual starting commit: `8b4930178e96715bf77863882443c3db8d5b1045`.
- Goal: one physical mass model, selectable kg/g/lb display, reference Class III checks, two acquisition profiles, Schema V2, and a transport-independent Modbus register model.
- CubeMX `.ioc`, `Core`, startup, HAL, A/B addresses, CRC32 and commit-last protocol were not changed.
- This stage does not implement RTU framing, CRC16, UART receive, RS485 direction control, BLE transport, W02 AT commands, alarms, USB or Ethernet.

## Architecture

Key additions are under `Domain/measurement`, `App/weighing_profile_manager.*`, `Protocol/modbus`, and `Tests/host/test_stage5a.c`. Configuration remains in `Config`, persistence remains in `Services/config_store`, and local UI remains in `UI`.

`MassValueUg` is signed `int64_t` micrograms. Calibration, gross/net/tare, stability spread, zero limits and overload decisions use this type. Checked add/subtract/absolute/multiply-divide routines avoid floating point and handle `INT64_MIN`. `MassSnapshot` is authoritative; `WeightSnapshot` remains a source-compatible alias during migration.

kg and g use exact integer factors. lb uses exactly `453592370 ug/lb`. Every display is derived afresh from micrograms, then rounded symmetrically and quantized to division digit 1/2/5. Switching units never converts a previous display value, so repeated kg/g/lb switching cannot accumulate error. Six-digit range validation rejects configurations whose positive maximum exceeds 999999 or negative value exceeds -99999.

Default display settings are kg `3 dp / d=1`, g `0 dp / d=1`, lb `3 dp / d=1`. Defaults are development values and are not verified on scale hardware.

## Metrology requirements

GENERAL mode permits kg/g/lb. CLASS_III_REFERENCE permits kg or g, requires `e=d`, `n=Max/e <= 10000`, initial zero <=20% Max, and semi-automatic plus AZT range <=4% Max. Minimum load is reported as `20e`; display overload is `Max+9e`. Mechanical safe load is separate load-cell metadata and is not used as display overload.

Load-cell rated capacity, sensitivity in uV/V, and safe-load permille each have an independent known flag. Sensor direction is derived from `raw_span-raw_zero`: unknown, positive or negative; it is readable but not directly writable through Modbus.

## Profiles and CS1237

HIGH_PRECISION defaults to 10 Hz/gain 128 and HIGH_SPEED to 40 Hz/gain 128. Filter and stability settings belong to each profile. `WeighingProfileManager` pauses official sample consumption, drains the FIFO, writes the target config, waits for driver verify/settling, resets metrology, and commits `active_profile` only on success. Failure attempts the previous profile; failure of both raises the CS1237 configuration fault. A channel and internal REFOUT remain forced on. No blocking delay is used.

## Keys and menus

Long press is 1500 ms. STAR/HASH repeat starts at 600 ms and repeats every 150 ms; FUNCTION/TARE/ZERO never repeat. Two or more logical keys enter conflict state, suppress all business events, increment a diagnostic count, and require fully debounced release.

The ordinary menu starts with UNIT and PROFILE. At its home page, four short presses `STAR, HASH, STAR, HASH`, with <=1000 ms between presses and <=4000 ms total, enter advanced mode. Wrong input, timeout, leaving home, cancel or menu exit clears progress.

## Persistence

V2 payload size is 344 bytes with explicit little-endian field encoding. It adds physical mass configuration, unit settings, metadata, two profiles, active profile, word order, mass calibration/tare and alarm mass fields. The 2 KiB A/B slots, CRC32 coverage and commit-last marker are unchanged.

V1 is decoded with its fixed 164-byte layout, converted once to physical mass in RAM, and marked `migration_pending_save/config_dirty`. The original flash slot is not modified. Only an explicit SAVE writes V2 to the inactive slot. Power-loss and PVD gating continue through ConfigStore and StoragePowerGuard.

## Modbus model

The model implements holding-register read, single write and atomic multiple write without HAL or UART. One read copies context and `MassSnapshot` before formatting, preventing torn multi-register values. 32/64-bit values support high-word-first and low-word-first order; bytes within each register remain Modbus big-endian transport semantics.

Mailbox execution requires nonzero token and `0xA55A`; a repeated token returns the cached response without repeating an action. Configuration writes go to a 64-register (128-byte) staging area. BEGIN copies active values, VALIDATE uses the production validator, APPLY_RAM routes the candidate through CommandService/ConfigApplication, and SAVE remains separate.

See `Docs/MODBUS_REGISTER_MAP_V1.md` for block addresses. This is a register-model host test, not a real Modbus communication test; USART2 does not yet answer Modbus frames.

## Verification and resources

- MSVC host tests: four test executables, including Stage 5A mass/unit/V1-V2/key/word-order/mailbox coverage.
- ARM GCC builds: Debug, Release and BoardDiagnostics link successfully.
- Stage 4B-R baseline: Debug 58664/5232, Release 32904/5232, BoardDiagnostics 58344/5240 bytes (FLASH/RAM).
- Stage 5A Debug: 71984/7872, delta +13320/+2640 bytes.
- Stage 5A Release: 39612/7880, delta +6708/+2648 bytes.
- Stage 5A BoardDiagnostics: 70104/7864, delta +11760/+2624 bytes.

## Hardware status and Stage 5B

NOT TESTED ON HARDWARE.

Hardware work remains for actual profile noise/response, all CS1237 rates and settling, stability values, zero range, sensor direction, unit readability, repeat speed, menu sequence usability, and real V1 flash migration. Stage 5B should add RTU timing/framing, CRC16, bounded UART buffering, address/broadcast policy, RS485 TX direction and explicit communication apply while calling this register model unchanged.

# Firmware 0x050E keypad cross-session SAVE validation

## Result

```text
A33 FIRMWARE 0x050E KEYPAD CROSS-SESSION SAVE VALIDATION PASS
DP 2→1 LOCAL SAVE CONFIRMED
DP 1→2 BASELINE RESTORE SAVE CONFIRMED
LOCAL KEYPAD SAVE 2/2 — NO RETRIES
FINAL PHYSICAL POWER CYCLE PASS
FINAL ACTIVE CONFIGURATION MATCHES BASELINE
NO MODBUS WRITE OR MAILBOX COMMAND PERFORMED
PC STAGE 2B ARCHIVED EVIDENCE UNCHANGED
MAIN UNCHANGED
```

## Identity and software

- Production commit: `93bd6ad236c1e0bd79beca79b69688926ad61afd`
- Firmware: `0x050E`
- Register Map/Schema: `0x0104` / `2`
- Release ELF SHA-256: `9D916325F667AC833500F74E4C99031F5B95D4046940F61818088AA8A29FD2FE`
- Previous 0x050C/0x050D failure evidence remains preserved.

The 0x050E change separates volatile runtime weight-view state from persistent
configuration revision. Internal tare synchronization no longer consumes a
configuration revision; actual TARE/CLEAR TARE persistence semantics remain
unchanged. Runtime view browsing is no longer persisted as a side effect of a
later configuration SAVE.

## Hardware sequence

The precondition was recovered to dP=2, dirty 0, revision 21/21, slot A,
sequence 21 using the preserved A21/B20 configuration slots. The 0x050E
application was programmed once, verified once and reset once while retaining
the configuration region. Post-flash read-only identity and Active Hashes
passed.

The operator then performed the exact advanced-menu sequence. Both local SAVE
operations displayed `SAUE -> donE`:

1. dP `2 -> 1`, TARE exit, re-entry, one SAVE: dP=1, revision 22/22, slot B,
   sequence 22.
2. dP `1 -> 2`, TARE exit, re-entry, one SAVE: dP=2, revision 23/23, slot A,
   sequence 23.

After one complete physical power cycle, read-only verification returned dP=2,
brightness 3, dirty 0, revision 23/23, slot A/23, IDLE ConfigStore and idle
Mailbox. The final Active JSON and STM32 binary hashes were:

```text
B7D78D5BD4A6DE0BE2C0DA201C167A0178608FCFF49F6297664C87C79018EE73
4BA7DA269DECB38D631ED8076A4FE90B7EF70AA04123CF15D5662B7DBBD4DD98
```

## Counts and scope

- Local keypad SAVE: `2/2`; retries: `0`
- Modbus FC03: `18` read-only requests; FC06/FC16: `0`
- Mailbox commands: `0`
- ST-Link: one application program, one verify, one reset; configuration-slot recovery was separately performed and verified
- Physical power cycles: `1`
- PC Stage 2B Mailbox SAVE history remains `2/2` and was not modified

Evidence files are listed in
`Docs/evidence/FIRMWARE_050E_KEYPAD_SAVE_MANIFEST.json`. Real keypad menu SAVE
is validated here; BLE, RS232/RS485 persistence, PC Client 0x050E re-freeze,
other configuration persistence, factory reset, calibration and Stage 2C are
outside this validation.

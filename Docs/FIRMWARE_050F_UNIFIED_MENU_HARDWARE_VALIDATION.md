# Firmware 0x050F unified menu hardware validation

## Identity

- Starting evidence commit: `17ac93eac7a8a64cae23cbe12f7ab76bc862d6d2`
- Unified transaction commit: `d379c18155d7ccbbde4e005549f02ea524e025c8`
- Menu exit fix: `e3cb00b561e34192a30dcb67bfde2048b667e4fc`
- Final overlay containment commit: `b119703cee70b228aa240e7f3477c7dca9946841`
- Firmware / Map / Schema: `0x050F` / `0x0104` / `2`
- Final Release ELF SHA-256:
  `15C8269A80962E2CA7329A2623AB69F2286C2E3336373D0B658E2755E2B8DE8D`

## Preserved failures and fixes

The first cancellation trial exposed a real ownership bug. Menu LIST TARE
called `ExitMenu` but then fell through to the final `Render`, leaving a menu
label on screen after App state returned to RUN. Later TARE input therefore
executed a real tare. No SAVE occurred; read-only evidence showed revision
29/23, dirty 1, brightness 3, and unchanged Flash A/23. Commit `e3cb00b` returns
immediately after exit and adds a previous-page assertion.

A later non-reproducible STATUS observation again showed a stale `FIr` page
while TARE reached RUN. Commit `b119703` normalizes STATUS return pages and adds
defence in depth: RUN periodically repairs stale MENU/EDIT/STATUS pages and
consumes an event arriving in that inconsistent scheduling window. Both failed
runtime states were discarded by application reprogram/reset from unchanged
Flash A/23; no recovery SAVE was used.

## Final hardware sequence

The final image was programmed and byte-verified through ST-Link at 3.29 V,
preserving both configuration slots. Post-reset read-only Modbus showed 0x050F,
Map 0x0104, Schema 2, revision 23/23, dirty 0, brightness 3, and A/23.

The operator then confirmed:

1. FUNCTION brightness 3 to 4, short FUNCTION confirm, LIST TARE: immediate
   return to the previous weight page, brightness restored to 3, no donE/SAVE.
2. STATUS long STAR: no APPLY, SAUE, or donE; TARE exited without tare action.
3. STATUS TARE exit and 35-second timeout exit both returned to the weight page
   without side effects.
4. FUNCTION brightness 3 to 4 plus long FUNCTION displayed SAUE then donE and
   exited. Read-only verification showed brightness 4, revision 24/24, dirty 0,
   and B/24.
5. FUNCTION brightness 4 to 3 plus long FUNCTION displayed SAUE then donE and
   exited. Read-only verification showed brightness 3, revision 25/25, dirty 0,
   and A/25.
6. One complete physical power cycle with no key action or SAVE restored the
   same 0x050F identity, brightness 3, revision 25/25, dirty 0, and A/25.

The final SWD dump found both records valid and committed. A/25 contains the
brightness-3 Baseline with valid CRC `0xD4863506`; B/24 contains brightness 4
with valid CRC `0x03C0FF2C`.

## Counts and offline gates

- Host CTest executables: 16/16 PASS (`/W4 /WX`)
- Stage 5B Python: 30/30 PASS
- Stage 5C Python: 12/12 PASS
- Debug / Release clean builds: PASS, no errors or warnings
- Debug: 96,732 B Flash / 20,144 B RAM
- Release: 83,040 B Flash / 20,104 B RAM
- Register map and firmware-source check: PASS
- Application program / verify / reset: 3 / 3 / 3
- Configuration-region SWD read-only dumps: 2
- Successful local menu SAVE: 2; retries: 0
- Final physical power cycles: 1
- Read-only FC03 requests: 43
- FC06 / FC16 / Mailbox commands: 0 / 0 / 0
- STATUS communication APPLY/SAVE: 0 / 0

The failed keypad observations and their volatile tare effects are preserved as
failure evidence. They are not counted as PASS. The final validation applies
only to the corrected `b119703` image.

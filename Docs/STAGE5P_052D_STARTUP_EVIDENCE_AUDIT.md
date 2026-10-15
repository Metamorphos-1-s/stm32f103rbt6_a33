# Stage 5P 0x052D startup evidence audit

## Scope and result

This is an offline review of commit `7931a740012a16dcfcd30be71affd16ae40f355d`.
No device, serial port, SWD programmer, configuration, or calibration was
accessed. The earlier `FAIL_ROLLED_BACK` conclusion is preserved. The audit
does **not** qualify 0x052D on target or prove the display fix.

The archived evidence does not distinguish an application startup fault from
an incomplete programming/reset sequence. The 0x052D program log confirms
Verify but does not record `MCU Reset` or a software reset. The preceding
successful 0x052C program log records both. The separate explicit erase
command for the 0x052D attempt reported `Sector erase operation failed`.
The subsequent `-w` operation automatically erased sectors 0–96 and verified
the 98,788 B candidate. Its first 256 B were then read back and match the
candidate, but the **complete application region was not read back before
rollback**. The configuration read immediately after programming matches the
preflash 4 KB image.

These facts make a missing explicit reset a concrete procedural suspect, not
a proven sole root cause. `Reset mode: Software reset` in the connection
header is a setting; it is not the `MCU Reset` action printed in the earlier
0x052C log. COM5 timed out twice, and no CPU PC/fault-register snapshot was
saved at that point. The display-only source diff adds no Modbus/USART
initialization changes, but that does not rule out a runtime fault.

The vector table in the candidate and postwrite 256 B prefix agrees. Its
initial stack pointer is `0x20005000`; its Thumb reset vector is
`0x08002E75`, within the binary. Those are structural checks, not execution
evidence.

During rollback, an erase log reported success, but the first subsequent
Modbus preflight selected V3 slot B sequence 22 instead of the preflash slot
A sequence 23. The archived report restored the entire 4 KB V3 region and
verified it byte-for-byte; the final device was 0x052C, OFF+SHADOW, clean
23/23. The explicit erase operation used in the failed attempt must not be
reused without a verified page-boundary and full readback procedure.

## Executed offline checks

- `audit_052d_boot.py` records the positive image/Verify/config checks and
  the missing reset/full-readback checks separately. Output:
  `Results/stage5p_052d_display_candidate/startup_audit.json`.
- Two audit regression cases pass, including rejection of a 124 KB image
  with a valid prefix but incorrect remaining bytes.
- The original 0x052D display conditioner test compiles with host GCC
  `-Wall -Wextra -Werror` and passes. This does not cover target startup.
- No new firmware image was built, flashed, or qualified in this review.

## Next target attempt

The next hardware run should keep the previous failure record immutable and
start with a fresh application/config backup and explicit current state
readback. Verify the application-only Flash page selection against the
`0x08000000–0x0801EFFF` application range and the V3 region starting at
`0x0801F000`. Do not proceed after a failed erase command. Use an explicit
postwrite reset, retain the programmer command line and log, then read the
whole 126,976 B application and 4,096 B V3 configuration and compare both
to expected bytes before interpreting COM5 behavior.

If COM5 still fails after a documented reset and full readback, preserve the
candidate in place long enough to capture CPU PC, xPSR, SCB CFSR/HFSR and
reset-cause state by SWD before another reset or rollback. That observation
will separate HardFault/startup failure from a communication-only problem.
Only after boot and Modbus are observed should the 40 Hz display and load
visual tests resume. An unsuccessful run should restore the exact original
application and configuration and verify both by full readback.

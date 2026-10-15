# Stage 5P 0x052E display smoothing v2

## Finding from the 0x052D capture

The retry 0x052D capture contained 965 valid rows and zero read errors.
At the 500 g load, display counts repeatedly moved through approximately
500.57-500.63 g. The device-side fields show that official stable was true
for only 41 rows and false for 924 rows; the display conditioner was in
TRACKING for 923 rows. The raw and filtered ADC values moved only by the
small noise band corresponding to the visible count changes. The earlier
100 ms publication gate reduced the rate to roughly 8-9 Hz but did not
prevent the visible small oscillation.

This identifies the remaining path as display-conditioner behavior during
an unstable-but-small-change interval, not TM1628's internal scan. It is
not a change to official stability, R5 admission, formal checkweigh or
alarm output.

## v2 candidate

The new candidate keeps the existing display-only 10 Hz publication gate
and adds a display-specific quiet interval:

* A small display step (at most eight display divisions) is allowed to
  qualify a display anchor even while official stable is false.
* Alternating small direction changes leak/reset motion evidence and keep
  the anchor, preventing the 500.57-500.63 g noise band from being shown
  as a rapid visual oscillation.
* Five consecutive same-direction evidence samples release the display
  to tracking; a large step still releases immediately.
* ADC and all metrology paths remain at 40 Hz. NET/GROSS numeric segment
  publication remains capped at 100 ms except for the first value and
  large steps. TM1628 only flushes changed RAM bytes as before.

The firmware identity is deliberately advanced to `0x052E`, Map `0x010F`.
The v1 0x052D image and its hardware evidence are preserved.

## Software gates

Focused Host Debug/Release display-smoothing tests pass. They cover
three-sample/quiet qualification, alternating unstable noise retention,
five-sample same-direction release and immediate large-step release.
The existing 0x052C focused SAVE, startup, 5NB output and D1-D display
tests pass 4/4 after the change.

Strict ARM Release (GCC 13.3.1, `-Os`, strict warnings):

* Firmware `0x052E`, Map `0x010F`
* BIN 98,980 B, SHA-256
  `AC13730228B15E5C9EA031D64795E28CF589986EEB18F63B6F056396346ED7D9`
* ELF SHA-256
  `6D620F8A0DEB4FE706248B615E06ED5F86B46CF93E0593464A2B063AFC626155`
* RAM 19,432 B / 20,480 B
* Conservative stack collision margin 536 B (threshold 512 B)
* Flash 98,980 B / 126,976 B

With the display flag disabled, a clean rebuild still reproduces the
ordinary 0x052C BIN SHA
`E87C5FCFDCC4FE8800725741AFE39BA8378FDE917A7C45A4453BA4CBE8826DD6`.

## Hardware boundary

The device is currently still running the already-tested 0x052D image;
the 0x052E image has **not** been flashed. The existing 0x052D run remains
valid as a v1 rate observation, but no v2 panel result is claimed here.
The saved V3 configuration is unchanged and the current 500 g load remains
on the platform. A separate explicit authorization is required before
installing 0x052E and checking the v2 visual behavior.

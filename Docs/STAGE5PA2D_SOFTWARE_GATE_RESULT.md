# Stage 5P-A2D Software Gate Result

## Disposition

**FAILED RESOURCE GATE; STOPPED BEFORE BOARD ACCESS.** The exact supplied
two-commit patch was applied to an isolated worktree at baseline
`d241f750b8f6420bfc5b6138531e2e7df614caf5`. The patch file SHA-256 is
`5D0534E805537B6A67E0A875797D81A02F375801CDFC0FE3018F6392D08CF440`.
No patch conflict occurred. Firmware identity is 0x051C, but this build is not
authorized for flashing under the stated resource gate.

## Gate Results

| Gate | Result |
|---|---|
| Host MSVC Debug CTest | 41/41 PASS |
| Host MSVC Release CTest | 41/41 PASS |
| Legacy/candidate calibration and fake-Flash tests | PASS within both CTest runs |
| Legacy/candidate menu tests | PASS within both CTest runs |
| ARM Debug `Stage5PA2DCalibration` | Builds, but resource gate FAIL |
| ARM Release | Builds, 19,416 B linker RAM |
| ARM Debug `-Wextra -Werror`, call graph | Builds, 19,472 B linker RAM |
| Debug RAM ceiling | **FAIL: 19,472 B > 19,464 B by 8 B** |
| Minimum stack / heap | 1,024 B / 0 B, PASS |
| Conservative stack bound | 1,504 B |
| Static collision margin | 528 B, PASS against 512 B floor |
| `git diff --check` | PASS |
| COM5 preflight / SWD backup / 0x051C flash / board tests | NOT RUN, blocked by RAM gate |

The 8-byte increase is attributable to `Protocol/command_service`'s static
`s_session`, which grows from 112 to 120 bytes in the patched build. No RAM
ceiling was relaxed, no unrelated buffer was reduced, and the supplied patch
commits were not rewritten. The static stack analysis is not a physical stack
watermark measurement.

## Built Images

| Build | BIN bytes | SHA-256 |
|---|---:|---|
| 0x051C ARM Debug | 108,880 | `0036A42DB7FB7CF6CD4EE54FACC85111145A8786908B96BEE4F1113A37C99A3D` |
| 0x051C ARM Release | 92,532 | `11CE5DF326FE099C0C64E2184C21C310A3781C7F3FD8635F08527D6F177C6F89` |
| 0x051C ARM strict Debug | 108,880 | Same SHA as ARM Debug |

These are actual local build hashes, not qualification hashes. The last known
board state from Stage 5P-A2C was frozen 0x0517 with its exact pre-test V3
configuration restored. It was not re-probed in this stopped A2D execution, so
current live state is **not independently confirmed here**.

The required next step is a separately reviewed RAM reduction in the 0x051C
candidate followed by fresh software gates. This failed build must not be
flashed or described as having passed calibration/menu hardware validation.

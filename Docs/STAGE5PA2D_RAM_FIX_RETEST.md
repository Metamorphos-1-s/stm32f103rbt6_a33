# Stage 5P-A2D RAM Fix Retest

## Result

The supplied `stage5pa2d_ram_gate_fix.patch` has SHA-256
`04C9F37DD5364B9C520E2CC81E27E2434E8321C40BD982512811EFACC2DDD1FB`.
It applies cleanly after the original A2D two-commit patch on the isolated
worktree. The earlier failed build and evidence remain preserved. This patch
removes the redundant local calibration capacity snapshot while retaining the
CommandService session's capacity lock; a host test now exercises a capacity
change without a revision increment before commit.

## Software Gates

| Gate | Retest |
|---|---|
| Host Debug CTest | 41/41 PASS |
| Host Release CTest | 41/41 PASS |
| ARM Debug | PASS, RAM 19,464 B, Flash 108,760 B |
| ARM Release | PASS, RAM 19,408 B, Flash 92,428 B |
| ARM strict Debug `-Wextra -Werror` | PASS, RAM 19,464 B |
| Minimum stack / heap | 1,024 B / 0 B |
| Conservative stack bound | 1,504 B |
| Static collision margin | 536 B, above 512 B floor |
| Candidate identity | 0x051C |
| Debug/strict BIN | 108,760 B; SHA-256 `6D88021729A47B8317919D3DF0A14D36228E5AB161CFC338BD7D2D381A99ADE3` |
| Release BIN | 92,428 B; SHA-256 `F02458F073766D3A9E2C7F278DFFDF258277067D653D05593853EF4BF966D114` |

The patched `s_session` is again 112 B (0x70), and the Debug RAM equals the
existing 19,464 B ceiling. The strict and Debug images are byte-identical.
These are build results only, not board qualification.

## Board Preflight Stop

COM5 was read twice without control writes. Both snapshots identify firmware
0x0517 and Map 0x0104 but report dirty=1 and revision/saved=10/8. R5 reports
state LIMITED, reason SEQUENCE (10), while requested application/mode are
SHADOW/OFF. Fault and overrun remain zero, and SAVE request count is zero.
This differs from the expected clean pretest 8/8 state. The RAM configuration
contains uncommitted changes that could be lost on an SWD reset or flash.

At this intermediate stop, SWD backup, 0x051C programming, calibration, menu
validation and physical power-cycle tests were **NOT RUN** pending user
disposition of the uncommitted state. The user then explicitly chose to
discard unsaved RAM changes. Complete read-only backups preceded a reset to
the saved clean 8/8 configuration, after which the focused target session
continued. The resulting hardware evidence and final disposition are recorded
in `Docs/STAGE5PA2D_FOCUSED_HARDWARE_VALIDATION.md`.

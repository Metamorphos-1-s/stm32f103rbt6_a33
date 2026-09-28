# A13E-R target resource plan before any device access

No previous A13D/A13D-R target durations or previous 051D state can establish
the new ACTIVE build's live resources. Software gate includes strict 10,774 /
88,765 / 107,491 C/Python replays, six synthetic replays, full 46 Host tests,
ordinary 051D exact SHA and final four ARM maps/callgraph gates >=512B. No COM5
or SWD until these pass. Identifiable new diagnostic 0521/010A vs engineering
0520/0109; original 051D/0105 remains independently verifiable.

Resource diagnostic enables both A13E and unchanged A13D-R cumulative target
statistics. Target owns each Feed peak/call count/sequence, loop/interval peaks,
stack paint watermark and main-loop generation. Five read blocks are used;
any host poll gaps are reported without interpolation. Require all nine paths,
invalid bucket0, each MCU produced=consumed=engine=timed sequence, no restart or
fault/overrun/read error/dirty/SAVE/revision change. Actual HCLK converts inclusive
DWT maxima; <=10ms Feed, <=25ms App_Run/loop and >=512B static AND runtime RAM.
The diagnostic build has source instrumentation, possible layout and IRQ timing
differences; its measurements are NOT exact unmodified 0520 timings. Compare
math object, compiler options, ARM disassembly/call chain and RAM with 0520.
No diagnostic result may qualify A13B drift effectiveness.

First device operation after software PASS: COM5 read-only preflight identity,
10Hz/filt1/strength3, calibration, revision/saved, dirty/fault/overrun,
checkweigh and volatile R5; stop on unexplained changes. SWD backup NEW full
126,976B app and 4,096B V3 config with hashes/dual-slot audit. Burn only pages
0–123 using verified interval syntax, Verify, readback FF tail/config. Mode
OFF+SHADOW boot verified before enabling anything. Live target measurement with
five-block Modbus load: OFF, empty STATIC reference/observation/median, one500g
load/step/fast, brief DOSING, one unload; newly active path tested only under
explicit command and safe preconditions. SWD never halts MCU during timed load.

On ANY failure stop live capture, close COM5 and restore this run's exact 051D
app backup, check config bytes and terminal OFF+SHADOW; do not flash 0520.
On resource PASS restore exact 051D first, then separately install 0520 after
fresh checks. All raw failures remain, and each physical placement/removal is
requested one action at a time. End 0520 OFF+SHADOW/offset0; real power-cycle
requires explicit user action. No ACTIVE left unattended.

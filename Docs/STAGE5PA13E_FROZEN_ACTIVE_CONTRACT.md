# A13E guarded ACTIVE contract (before implementation/device access)

Baseline 7ccfe181d81079e8f1b670a7fedb574d0b79cbfe. Ordinary 051D SHA
DDF57FB131E5EF675D94B08A5A8752EB0FB588BA5230DEA6F34C57D6B2F62DDD.
A13B efficacy INCONCLUSIVE and historical A13D FAIL remain unchanged.
Frozen A13 mathematics/parameters are not edited. Scope current sensor,
10Hz/filt1/strength3; no formal effect/zero/metrology/slow-feed approval.

Independent switch A33_ENABLE_STAGE5PA13E_ACTIVE requires A13C path. Ordinary
engineering identity 0520/Map0109; optional cumulative-resource diagnostic
identity0521/Map010A, separate from historical051F semantics. Default switchesOFF.

1. Boot/restored V3 requests always produce OFF+SHADOW, offset0 without changing
   configuration, dirty, revision or SAVE. No engineering runtime request is
   persisted. BLE writes refused, including atomic pair command.
2. Explicit ACTIVE request only from local keys or wired PLC, with valid current
   calibration/weight, no fault/overload/rail/LIMITED, and actual engine profile
   10Hz/filt1/strength3. ACTIVE with OFF mode is refused. Transition from SHADOW
   with nonzero candidate offset is refused, without modifying candidate or formal
   weight; operator must choose OFF (clears it), then enable explicitly.
3. Atomic pair command validates application/mode, expected runtime generation
   and all enable conditions before mutation. All runtime mode/application changes
   increment a volatile generation. Local edit stores generation; stale/ABA PLC
   changes return BUSY. Mode and output change within synchronous main-loop command,
   no serial/ISR publishes the intermediate state. Legacy single mode/application
   commands also use the same complete pair transaction.
4. OFF unconditionally exits to SHADOW/raw weight, clears offset/windows and records
   signed formerly applied offset as the possible exit jump. It does NOT promise
   zero jump. STATIC/DOSING preserve offset; DOSING frozen by unchanged A13 math.
5. Invalid calibration/input, fault, overload/rail, sequence/time gap, candidate
   LIMITED/numeric invalidity or unsupported/reconfigured profile immediately
   stop applying correction, go OFF+SHADOW and record reason/exit offset. Recovery
   does not restore ACTIVE. ZERO/calibration/profile reset also clear candidate;
   TARE/CLEAR TARE preserve offset and formal net=gross-tare.
6. Each valid sample's final gross=uncompensated gross-applied offset, net=gross-tare;
   display input and Modbus use that final snapshot. Panel hysteresis is not authority.
   Atomic engineering telemetry supplies sequence/time, uncompensated/formal weight,
   applied and candidate offsets, mode/application/state and exit diagnostics.
7. This minimal engineering stage forbids concurrent formal Checkweigh ACTIVE
   (STATIC or DYNAMIC). Enable A13 while checkweigh is active is refused; enable
   checkweigh while A13 ACTIVE is refused with explicit command failure. Therefore
   fast ADC path is NOT falsely described as equal to compensated filtered mass.
   Shadow diagnostics may remain; no candidate correction drives alarm outputs.
8. drIFt long FUNCTION applies volatile pair and shows donE on actual success,
   BUSY/Err on failure, NEVER SAUE or config SAVE. Cancel/timeout discard candidate.
   Ordinary menu SAVE cannot persist engineering pair. Old R5/V3 behavior stays
   unchanged when switchOFF. Config and local/runtime generations both checked.

Software gate: unchanged math blobs; replay A13B88765/A9107491/A13C-R captured
trajectories; real integration Host tests distinguish math from wiring; Debug/Release
and strict ARM; ordinary Release and051D hash comparisons; nested static collision
>=512B with unchanged communication capacities, reservation and256B indirect allowance.
Resource diagnostic must precede ACTIVE candidate installation: cumulative per-path
Feed<=10ms, full loop/interval<=25ms, measured untouched RAM>=512B and conserved MCU
samples at10Hz under five-block polling. Previous diagnostic numbers are not this
build's measurements. Any failure stops ACTIVE qualification and is preserved.

Hardware only after relevant software/static gates: fresh COM5/full app126976B and
V3 config4096B backup, app pages[0 123] only, Verify/prefix/FFtail/config equality.
Normal engineering candidate installation only after new diagnostic resources pass.
One watched STATIC load/unload, nonzero DOSING load/unload, OFF/control conflict,
checkweigh exclusion, real power-cycle defaultOFF. No ZERO/TARE/cal/SAVE during this
focused run. On error restore own exact051D backup, compare bytes, confirm OFF/offset0.
Successful terminal0520 OFF+SHADOW/offset0. No ACTIVE left running unattended.

All raw failures retained; short low-stimulus data is not efficacy PASS. No 12h or
full matrix. Only a separate future phase may expand scope/qualification. Final
reports separate PASS/FAIL/INCONCLUSIVE/NOT RUN and ordinary vs diagnostic timings.

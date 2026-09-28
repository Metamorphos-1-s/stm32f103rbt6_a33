# A13D-R prospective cumulative resource contract (before new data)

Baseline 5dd51a472aabd4a14a2b69701dbcc70baf687b4d. A13D remains FAIL; its
missing timed call1590 is neither recovered nor interpolated by this contract.
A13B efficacy remains INCONCLUSIVE and owner-deferred. No ACTIVE, tuning,
ZERO/TARE/calibration/SAVE. New diagnostic identity 0x051F / Map0x0108.

## Target, not host, owns the complete evidence

After App_Init returns, ConfigStore atomically leases its slot-read scratch to
136 B of statistics. The union preserves its original 281-byte payload capacity.
No DMA/Modbus/BLE capacity, stack reserve or indirect-call allowance changes.
All loads/initialization and SAVE/factory requests are rejected while the lease
is held, before data/counter/Flash mutation. Ordinary build macros keep the
original byte buffer and all original behavior. Dual-slot/power-cut regressions
and lease-conflict tests must pass. Reboot ends the lease and allows normal V3
loading. V3 serialization, calibration and compensation mathematics unchanged.

All MCU call sites of A13C_Feed pass through one wrapper, including OFF startup
readiness and invalid-input sites. IRQs remain enabled; DWT wrapping/read/call
overhead is included, not subtracted. Per-path max cycles, peak sequence and
call count persist from diagnostic initialization until reset:
OFF, HOLDOFF, REFERENCE_FILL, OBSERVATION_FILL, normal TRACKING median,
fast TRACKING evaluation, STEP_PENDING, STEP_SETTLING, DOSING; invalid state
has a separate bucket and invalidates qualification. Fast identity is based
on the actual boost-evaluation counter increment, not a host timestamp guess.
Global max retains its sequence/path, and loop max/interval and lowest touched
stack address persist even when the host misses many polls. Maxima never reset
on mode commands. Counter and metadata overflow latch invalid, never wrap
silently. Per-path 16-bit counts are exact for a maximum 3600-second 10Hz run;
reaching UINT16_MAX invalidates evidence. Sequence uses 28 bits in the global
peak identity; overflow invalidates it. Generation/loop counters are also
guarded at UINT32_MAX.

## Consistency and reset proof

Main-loop writers increment generation odd-before/even-after updates. IRQs
never modify statistics; the synchronous Modbus handler does not re-enter
sampling/loop writers. One FC03 reads the complete 122-register block at
0x0380 with generation at both ends. Reader rejects odd/mismatched generation,
wrong signature/identity, peak/sequence/count incoherence or non-monotonic
statistics. It never combines two requests into one alleged atomic snapshot.
Actual per-path peaks are target-retained even if commands cross samples.

Counters first/last/total enforce every timed call. Once driver RUNNING is
established, produced-consumed=FIFO, engine=consumed-invalid, timed last=engine,
sum(path counts)=last-first+1. This distinguishes a host polling gap from MCU
sample loss. Startup is archived; qualification begins once MCU uptime>=5s,
valid calibration, RUNNING and conserved chains hold. Statistics still cover
all preceding post-init calls and stack painting precedes all App_Run work.

No reset command/statistics-clear API exists. Actual HAL uptime, loop count,
generation, all path counts, last sequence and maxima must be monotonic.
Host heartbeat gap must be <=5s; adjacent MCU uptime delta must agree with
host monotonic delta within 250ms. Beginning comparison only after uptime>=5s
makes an intervening reset impossible to hide within that heartbeat/tolerance
window even if the host skipped one or more polls. Longer silence is INCOMPLETE
and triggers restoration, never proof of continuous operation. Clock wrap or
backwards uptime is not accepted in this bounded one-hour run.

## Fixed thresholds and coverage

- A13C max <=10ms; complete App_Run+scan max and start interval <=25ms, using
  actual read HCLK, normal five-block read-only load. No Host timing substitute.
- Complete nested-IRQ static collision margin and runtime untouched RAM >=512B.
  Keep main + priority5 IRQ + SysTick + both hardware frames/alignment and
  the full 256-byte indirect allowance. IRQ priority audit matches A13D.
- No target sample loss, sustained FIFO>1 for >1s, invalid input/counter growth,
  overrun/read error/fault/dirty or abnormal SAVE/revision. No formal weight path
  or alarm receives candidate correction.
- PASS requires nonzero target counts in all nine valid buckets and maximum/
  sequence evidence for each. Reference/observation/median and step/fast/DOSING
  are not inferred from a few host frames. Missing coverage => INCOMPLETE.
- Host poll gaps remain explicitly reported; they no longer automatically fail
  if target cumulative evidence, reset proof and conservation stay valid. This
  is prospective only, not a relaxation of A13D's historical frozen rule.

Watermark painter follows audited A13D rules: thread/MSP checks, no stack frame,
IRQ mask, aligned static end to currentMSP-64, exact offset/no rounding, no live
frame/static/Flash overwrite. Init through App_Init is not measured watermark.
Empty DWT overhead measured and retained, never subtracted. Shared-scratch
alignment and all wrappers must pass fresh Debug/Release maps/callgraph.

Host tests must simulate command delay over sample2 containing the true peak,
then read sample3/4 and recover the target max/peak identity and all counts.
Inject timeout, stack breach, inconsistent generation, reboot, target loss and
overflow; each must be rejected. Submit plan/checker/tests before new device
data. Any resource/software gate failure stops before device access.

After software gate: fresh COM5 + full current-run app/config backup, application
pages0-123 interval erase/Verify, complete prefix/FF tail/config comparison
outside timed data. Fresh reset starts final five-block acquisition. One empty
STATIC, one500g load/unload, short DOSING covers resources only. Any target or
diagnostic violation stops immediately. PASS/FAIL both restore exact current
051D, compare full app/config bytes and confirm OFF+SHADOW/offset0/safe counters.

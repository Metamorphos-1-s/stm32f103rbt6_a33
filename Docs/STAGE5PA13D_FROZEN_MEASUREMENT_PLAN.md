# A13D pre-acquisition measurement plan and fixed gates

Baseline 4a6f3edfbab74a7fd7ba1ff35c768534c05d2704. No device access until
Host, strict ARM and static RAM/call-chain gates pass. A13B efficacy remains
INCONCLUSIVE. No ACTIVE, algorithm tuning, ZERO/TARE/calibration/SAVE.

## Timing and overhead

Use an isolated diagnostic switch and independent firmware identity. Read
actual HCLK via HAL_RCC_GetHCLKFreq and expose it; unsigned DWT cycle differences
handle wrap. Do not reset the existing microsecond time base. Wrap only the
A13C_Feed invocation in the engineering adapter; log last cycle count, device
sequence. Host computes maxima and peak identity from the complete captured
stream, and deduplicates every
sequence and groups each last duration by actual output state (OFF, reference,
observation, TRACKING median, STEP_PENDING/SETTLING, boost, DOSING). IRQs are
not masked during timed work, so preemption is conservatively included.
Record an empty DWT-read bracket maximum at initialization; do not subtract it.
Wrapper/bookkeeping and diagnostic reading affect the diagnostic image, not
the frozen 0x051D BIN. Compare A13C function machine bytes/disassembly and
caller/callgraph, compiler options, .data/.bss and stack changes explicitly.

Wrap each complete main-loop App_Run in main, not every ISR. Log loop maximum
duration and start-to-start interval; duration includes enabled IRQs and normal
Modbus work. Snapshot reading and stack scan overhead stay in measured loop
intervals. Host polling shall perform five read-only blocks per cycle; preserve
actual errors, duplicates/gaps and device timestamps, never interpolate.

## MSP and watermark safety

Audit finds reset vector/MSP startup, no RTOS, PSP selection or CONTROL writes;
expose actual CONTROL and IPSR checks. Initialize diagnostics after App_Init
returns, then invoke a leaf/no-stack-frame assembly painter immediately before
the first App_Run. Disable maskable IRQs, verify thread mode + MSP, use only
caller-saved registers and no push/calls. Paint aligned [_ebss, current MSP-64)
with a fixed nonzero word. Static RAM stays below _ebss; active main/initializer
stack frames stay at/above MSP; 64 extra bytes are excluded. Restore PRIMASK.
There is no active ISR at this thread-mode point. NMI/fatal handlers and the
startup-to-paint call chains are outside the qualification scope.

Scan from aligned static end to the prior lowest-touched address, monotonically
update the lowest altered word, and check the bottom 512 B sentinel area.
Scanner and diagnostics' own stack usage and enabled ISRs are included after
painting. Do not repurpose stack reserve, shrink DMA/Modbus/BLE, or write any
configuration Flash. Do not halt MCU inside the throughput capture. Any SWD
read/halt is outside and explicitly separated from continuous qualification.

## Frozen budgets (no relaxation after results)

For configured 10 Hz, use nominal 100 ms = HCLK/10 cycles and also report actual
sample periods. A13C timed call maximum <=10 ms (10% nominal period). Complete
main loop maximum execution and start-to-start interval <=25 ms (25% period).
This leaves >=75 ms per 100 ms for additional service opportunities and IRQs,
with the timed-call/loop IRQ contribution already conservatively counted.
No uncaptured device sequence in phase qualification; driver produced minus
bridge consumed equals current FIFO; accepted engine sequence equals bridge
consumed minus invalid count after warmup. No FIFO sustained backlog (>1 sample
for >1 s), overrun, read-error counter growth, fault, dirty, revision/SAVE change,
stack guard change or reset. Static conservative collision margin and actual
untouched region from static end to deepest observed stack >=512 B each.
Required state coverage and missing dynamic fields => INCOMPLETE, not PASS.
Throughput qualification starts at the first positive, weight-valid device
sequence whose resource last-call sequence matches the candidate sequence,
with driver RUNNING and valid calibration. Earlier ADC/filter startup polls
remain raw evidence but are not a claimed steady-stream coverage interval.
Once this boundary is reached, every sequence through stop must be captured.
Stack high-water covers all post-paint App_Run work, including this warmup.

Five fixed blocks per poll: realtime 0x0000/32, general diagnostics 0x0020/28,
storage 0x01C0/10, engineering R5 0x0280/40, and atomic A13C+resource extension
0x0300/108. Reserved gap registers are zero only in the independent diagnostic
Map 0x0106. Last call timing and candidate sequence share the atomic final
block; separate earlier realtime values are never silently treated as that
same sample. Overhead empty-bracket maximum is stored in flags bits 8–31,
while bits 0–7 report paint/DWT/stack validity. Timing is never overhead-subtracted.

Diagnostic state must fit the existing margin; target <=24 B added permanent
state and minimal caller stack. If actual map/callgraph crosses 512 B, stop
before device access. Host processing time is not a target measurement.
NVIC UART/DMA/TIM4 preemption priority is 5; SysTick is 15. Include an
additional SysTick software chain plus its 32-byte exception frame on top of
the legacy main + one IRQ + 256-byte indirect allowance. Do not consume or
shrink that indirect allowance to hide nested IRQ cost. A discarded initial
48-byte prototype is not authorized for flashing. Also reserve 8 bytes for
alignment padding across both nested exception frames. The final 24-byte
variant must pass this complete conservative gate. Its exact 12-bit byte
offset plus four safety flags is not a quantized watermark; if the initial
paintable span reaches 4096 bytes it refuses painting. The empty bracket
maximum is stored exactly in 16 bits and overflow invalidates measurement.

Both PASS and FAIL restore the current-run full 126976 B application backup
and preserve/compare the 4096 B configuration. Application pages 0-123 must be
erased as a bracketed interval, configuration 124-127 untouched. Final exact
0x051D prefix, FF tail, OFF+SHADOW/offset0 and safe counters must be read back.

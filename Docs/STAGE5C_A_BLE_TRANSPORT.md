# Stage 5C-A: W02 BLE transport and connection management

## Baseline and hardware facts

- Stage 5A closeout main baseline: `167f4f43290dcfceccd7e510965b5e0b9445ef9`.
- Working branch: `stage5c-ble-a-h2`.
- The repository contains no W02 vendor part number or module datasheet.
- CubeMX `.ioc` enables USART1 on PA9/PA10 and labels it `MCU_BLE_TX` /
  `MCU_BLE_RX`. It is configured 9600, 8-N-1, asynchronous, no flow control.
- USART2 on PA2/PA3 is the existing RS485/Modbus port. It remains 115200 and
  uses its existing circular RX/TX DMA transport.
- USART3 is absent from the `.ioc`, generated code, NVIC table, and BSP. No
  USART3 pins or DMA channel can therefore be claimed as a fact.
- PA8 is `W02_PWRKEY`, GPIO open-drain, no pull, released high, active-low.
  The existing driver enforces a 50-200 ms pulse and a 250 ms hard limit;
  the startup request uses the existing 80 ms default. There is no external
  power-enable, PowerOff, Wake, Reset, or module-ready input in the repository.

No CubeMX or `.ioc` change was made. Stage 5C-A uses the already-generated
USART1 resource and does not guess a USART3 mapping.

## Implemented layers

- `Drivers/w02/w02_uart.c` provides the USART1 interrupt-backed byte RX and
  bounded interrupt TX driver. HAL callbacks remain centralized in the
  existing UART BSP so USART1 and USART2 callback symbols do not collide.
- `Services/ble_transport` owns static 512-byte RX and 256-byte TX rings.
  RX is ISR-safe enqueue only; parsing is deliberately absent. TX is copied
  into a 128-byte staging buffer and submitted non-blocking. A read is capped
  at 128 bytes per call.
- `Services/ble_connection_manager` reports observable module/UART states:
  `STARTING`, `READY`, `UART_ACTIVE`, and `FAULT`. The BLE link state is
  `BLE_LINK_UNKNOWN`; UART activity is never reported as a phone BLE
  connection.
- `App_Run()` calls the manager once per loop. No `HAL_Delay`, malloc/free,
  RTOS, floating-point, or blocking long UART operation was added.

## Diagnostics

The connection diagnostics expose module/link state, RX/TX bytes, RX overflow,
UART/TX errors, reset count, last RX/TX timestamps, and reserved zero frame,
parse, and CRC counters. `EVENT_BLE_STATE_CHANGED` is published only from the
main-context manager when the observable module state changes.

## Protocol and UUID boundary

`Drivers/w02/README.md` and `Protocol/ble_protocol/README.md` contain no AT
command set, transparent framing definition, service UUID, characteristic UUID,
or notification/write contract. Stage 5C-A therefore transports binary-safe
UART bytes only. Application frames, configuration writes, SAVE, calibration,
and UUID discovery remain deferred; no protocol or UUID was invented.

## Verification

- Host tests: 9/9 passed, including Stage 5C-A initialization, RX ordering,
  ring wrap/overflow, bounded TX, TX busy/complete/reject recovery, reset,
  state events, link-unknown behavior, the 600-second diagnostic scheduler,
  and USART2 DMA restart recovery.
- ARM builds pass for Debug, Release, and BoardDiagnostics. Debug uses
  112,996 B Flash / 12,392 B RAM; Release uses 61,424 B Flash / 12,400 B RAM;
  BoardDiagnostics uses 111,876 B Flash / 12,400 B RAM. The Release load
  remains below `0x0801F000`; Schema V2 remains 344 B with config slots A
  `0x0801F000-0x0801F7FF` and B `0x0801F800-0x0801FFFF`.
- W02 UART, PC/phone BLE, and the RS485+BLE concurrency path have now been
  tested on hardware. Link state remains UNKNOWN because the W02 did not emit
  a reliable UART connect/disconnect indication.

## Deferred scope

Stage 5C-B will define read-only telemetry using the existing mass semantics:
authoritative `MassValueUg` values, explicit compensated/operational/display
fields, stability/display-lock state, unit/format, runtime-drift state, and
explicit persistent-dirty reporting. Stage 5C-C/D remain configuration,
SAVE, and calibration work and are not part of this branch's transport layer.

## Hardware bring-up H2

Observed BLE device: `W02_008324`, LE-only.

- Primary service `0xFFE0` is present.
- `FFE1` supports READ, WRITE, WRITE WITHOUT RESPONSE, NOTIFY, and CCCD
  `0x2902`.
- `FFE2` supports WRITE and WRITE WITHOUT RESPONSE.
- Primary service `0xFF12` was observed but its purpose is unknown and it is
  not used by this stage.

The phone-to-MCU path is hardware verified: writing bytes
`41 42 43 31 32 33` (`ABC123`) to `FFE2` reached USART1 PA10 and
`BleTransport_RxPushFromIsr()` in order. All six bytes were received,
`rx_bytes` increased by six, and `rx_overflow` remained zero.

The H2 firmware adds a BoardDiagnostics-only, one-shot test function:

`Stage5C_BleDiagnosticsRequestHello()`

Call it from a halted/debug-connected GDB session after enabling FFE1 Notify.
It queues exactly `48 65 6C 6C 6F 20 57 30 32` (`Hello W02`) through
`BleTransport_Write()`. It returns ACCEPTED, BUSY, QUEUE_FULL, NOT_READY, or
ERROR and never bypasses the transport ring. Observe
`Stage5C_BleDiagnosticsGetSnapshot()` for the before/after byte counts,
pending depth, UART errors, and TX-complete confirmation. Release firmware
does not transmit this payload automatically.

`BLE_MODULE_UART_ACTIVE` now means RX or TX activity occurred within the last
1000 ms and returns to READY after the window. It remains independent of the
BLE link state. Until repeatable connect/disconnect UART notifications are
captured, `BLE_LINK_UNKNOWN` is retained by design.

Current H2 status:

- Phone -> FFE2 -> W02 -> STM32: PASS.
- STM32 -> W02 -> FFE1 Notify -> phone/PC: PASS.
- Ten-round bidirectional transport: PASS.
- Connect/disconnect UART observation: no reliable indication observed;
  `BLE_LINK_UNKNOWN` is retained.
- RS485 + BLE 600-second concurrency regression: completed; BLE transport
  PASS, USART2 recovery PASS, RS485 link quality has transient errors described
  below.

H2 build snapshot: Debug 112,996 B Flash / 12,392 B RAM; Release 61,424 B
Flash / 12,400 B RAM; BoardDiagnostics 111,876 B Flash / 12,400 B RAM.
The Release image remains below config slot A at `0x0801F000`. Schema V2
remains 344 B and the A/B slot addresses are unchanged.

## PC concurrent hardware test

Formal run: `Results/hardware/stage5c_concurrent_fixed_20260807_235107`.
The PC used W02 address `C8:46:82:00:83:24`, FFE1 Notify, and FFE2 write
without response. In parallel, COM5 polled slave 1 at 115200, 8-N-1 using
read-only FC03 requests for 600 seconds.

BLE completed without loss: 600/600 `ABC123` writes succeeded and 600/600
`Hello W02` notifications were parsed (5400 bytes), with zero mismatched or
partial bytes, zero write errors, and zero disconnects. The MCU snapshot also
reported 600 accepted TX requests, 600 validated RX frames, zero busy results,
zero mismatches, and zero USART1 errors.

The RS485 runner completed 1425 probe cycles: 1391 complete successes and 34
timeouts, with zero PC-side CRC errors or Modbus exceptions. The MCU remained
in `COMM_STATE_RUNNING`; the framer returned to `WAITING`, the server returned
to `IDLE`, and the active fault mask was zero. Cumulative MCU counters for the
run were 7042 valid/addressed FC03 frames and 7042 responses, with zero TX
errors. The UART recorded 19 frame errors, one noise error, and one hardware
overrun; the framer recorded 18 recovered transport errors and four CRC
errors. These are transient RS485 signal-quality errors, so this run is not a
zero-error RS485 qualification despite completing the full duration.

An earlier 600-second run exposed a recovery defect: after a UART error
restarted circular RX DMA, the transport retained the old wrap-count baseline.
That made every later process pass report a receive error and left the framer
discarding indefinitely. `Uart2DmaTransport_Process()` now rebases the DMA
position and event snapshot after restart. The formal rerun ended with zero
software DMA overruns and one wrap-race compensation instead of millions of
repeated transport errors. A host regression reproduces the restart sequence
and verifies that subsequent bytes are received normally.

The persisted configuration region was read before and after flashing the
diagnostic image. Both SHA-256 values were
`4190EF58D7B31D76832E21857FF4D7856E7B9F66A7E5BF57A9CC063F2C8C0871`.

A second automated 600-second rerun is recorded in
`Results/hardware/stage5c_concurrent_rerun_20260808_010427`. BLE again
completed 600/600 writes and 600/600 notifications with zero transport
errors. RS485 completed 1431 cycles, with 1401 complete successes, 30
timeouts, zero PC-side CRC errors, and zero exceptions. The MCU ended in
`COMM_STATE_RUNNING` with framer `WAITING`, server `IDLE`, zero software DMA
overruns, one wrap-race compensation, and an active fault mask of zero. The
repeatability of the timeout/error rate points to transient RS485 signal
quality as the remaining hardware issue, rather than a persistent software
transport lockup.

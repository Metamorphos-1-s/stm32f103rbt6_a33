# Stage 5C-A: W02 BLE transport and connection management

## Baseline and hardware facts

- Stage 5A closeout main baseline: `167f4f43290dcfceccd7e510965b5e0b9445ef9`.
- Working branch: `stage5c-ble-a`.
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

- Host tests: 8/8 passed, including Stage 5C-A initialization, RX ordering,
  ring wrap/overflow, bounded TX, TX busy/complete/reject recovery, reset,
  state events, link-unknown behavior, and concurrency-safe nonblocking paths.
- ARM builds pass for Debug, Release, and BoardDiagnostics. Debug uses
  112,412 B Flash / 12,256 B RAM; Release uses 61,000 B Flash / 12,264 B RAM;
  BoardDiagnostics uses 110,172 B Flash / 12,256 B RAM. Release load remains
  load image ends at `0x0800EE47`, below `0x0801F000`; Schema V2 remains 344 B with config slots A
  `0x0801F000-0x0801F7FF` and B `0x0801F800-0x0801FFFF`.
- Hardware W02 UART, BLE connect/disconnect, and RS485+BLE concurrency have
  not yet been tested on a board.

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
- STM32 -> W02 -> FFE1 Notify -> phone: pending.
- Ten-round bidirectional transport: pending.
- Connect/disconnect UART observation: pending.
- RS485 + BLE 600-second concurrency regression: pending.

H2 build snapshot: Debug 112,612 B Flash / 12,312 B RAM; Release 61,144 B
Flash / 12,320 B RAM; BoardDiagnostics 110,372 B Flash / 12,304 B RAM.
The Release load image ends at `0x0800EED7`, below config slot A at
`0x0801F000`. Schema V2 remains 344 B and the A/B slot addresses are unchanged.

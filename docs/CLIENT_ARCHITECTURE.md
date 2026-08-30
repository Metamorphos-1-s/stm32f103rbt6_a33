# Client Architecture

The WeChat Mini Program discovers a user-selected W02 through the native BLE
API, subscribes to FFE1 notifications and writes framed commands to FFE2. W02
is a transparent bridge to STM32 USART1. BLE is an auxiliary monitoring path;
sequence gaps and stale data remain visible diagnostics and never become
fabricated weight samples.

The Windows client has the same domain model above either Modbus TCP (PC to
CH579 gateway to STM32) or Modbus RTU (PC USART2 RS232/RS485 directly to
STM32). `IModbusTransport` owns bytes and timing, protocol codecs own framing,
and `DeviceRepository` maps registers to domain values. WPF never builds a
Modbus frame or touches sockets/SerialPort directly.

Writes are serialized, bounded by timeout, matched by transaction/request
identity, and retried with identical request bytes where a retry is safe.
Configuration changes are read back; SAVE waits for an explicit completion.

Stage 1 adds production UI and platform transports. Stage 2 adds diagnostics,
configuration and calibration workflows after hardware validation.

The Stage 0-V build and test evidence is maintained in
`docs/STAGE0_TOOLCHAIN_VALIDATION.md`; it intentionally does not claim BLE or
Modbus hardware access.

# PC Stage 1 Hardware Environment

Recorded during software validation on 2026-09-02.

- Client baseline: `89e29ee03bd205860dcec7b8344248ad01be08ad`
- STM32 baseline: `9e242c7649ad79ac4c3bae347b3259847f4bc09e`
- CH579 required baseline: `3cd21ecefe6486d28a664ea356cb4ed6e7b3090a`
- CH579 repository/image was not locally available for commit verification.
- CH579 became reachable at `192.168.1.100:502` through Ethernet; PC source
  address was `192.168.1.10`.
- COM3: USB-SERIAL CH340, `VID_1A86&PID_7523`.
- COM5: USB-SERIAL CH340, `VID_1A86&PID_7523`.
- COM1: built-in communications port.
- COM5 was identified by the user as USB-RS232 and selected for the RTU test.
- An initial COM5 probe was blocked at the first receive with `IOException:
  The I/O operation was aborted because of either a thread exit or an
  application request.` The `SerialDebug` process was running at the time;
  the port was not forcefully closed and no blind retries were sent.
- After SerialDebug was closed, a Python FC03 cross-check returned Map
  `0x0104`. The C# transport was changed from CH340-incompatible BaseStream
  async reads to a cancellable background short-timeout SerialPort read loop.
- The corrected C# path passed a 10.087-second probe, a 600.087-second run and
  10/10 connect/monitor/disconnect cycles on COM5.
- A controlled port-occupancy test returned UnauthorizedAccessException with
  zero transmitted requests and no application crash; COM5 was released after
  the test.
- Physical COM5 removal stopped reception, exposed stale data and exhausted
  exactly three bounded reconnect attempts before FAULTED. Replugging COM5 and
  initiating a new connection restored Map `0x0104` and completed 91/91
  responses over 10.097 seconds with zero protocol or transport error.
- COM3 remains unclassified and is not used.

TCP completed a 600.040-second Core run, 10 connect/disconnect cycles and a
WPF live-monitoring check. Network-interruption recovery and panel comparison
remain open. RS232 unplug/recovery passed; panel comparison remains open.
RS485 remains NOT RUN.

During the controlled TCP interruption probe on 2026-09-04, the test included
a full instrument/CH579 power loss. The client retained the last snapshot,
detected stale data, attempted exactly three reconnects, then entered FAULTED;
the probe recorded 3 timeouts and 1 transport error with no CRC/MBAP/Unit/bad
frames. The target remained unreachable afterwards, so cable-only recovery was
not assessed. After CH579 was powered back on, a new client recovery probe
completed 91/91 responses over 10.021 seconds with Map `0x0104` and zero
Timeout/CRC/MBAP/Unit/bad-frame/transport errors. This is a clean
post-power-cycle reconnect, not same-process automatic recovery.

On 2026-09-05 a cable-only test kept CH579/instrument powered. The client
detected the loss, retained stale data, attempted three reconnects and entered
FAULTED. The cable was reinserted after that bounded window, so same-process
recovery was not observed; TCP was reachable again immediately afterwards.

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
- A 10-second COM5 probe was blocked at the first receive with `IOException:
  The I/O operation was aborted because of either a thread exit or an
  application request.` The `SerialDebug` process was running at the time;
  the port was not forcefully closed and no blind retries were sent.
- COM3 remains unclassified and is not used.

TCP completed a 600.040-second Core run, 10 connect/disconnect cycles and a
WPF live-monitoring check. Network-interruption recovery and panel comparison
remain open. RS232 requires a clean COM5 retest after SerialDebug is closed;
RS485 remains NOT RUN.

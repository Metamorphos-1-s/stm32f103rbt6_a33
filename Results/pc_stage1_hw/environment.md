# PC Stage 1 Hardware Environment

Recorded during software validation on 2026-09-02.

- Client baseline: `89e29ee03bd205860dcec7b8344248ad01be08ad`
- STM32 baseline: `9e242c7649ad79ac4c3bae347b3259847f4bc09e`
- CH579 required baseline: `3cd21ecefe6486d28a664ea356cb4ed6e7b3090a`
- CH579 repository/image was not locally available for commit verification.
- `192.168.1.100:502` did not accept a TCP connection from the active WLAN.
- COM3: USB-SERIAL CH340, `VID_1A86&PID_7523`.
- COM5: USB-SERIAL CH340, `VID_1A86&PID_7523`.
- COM1: built-in communications port.
- The physical RS232/RS485 type and USART2 wiring of COM3/COM5 were not
  confirmed. No serial request was sent to avoid driving an unknown interface.

TCP, RS232 and RS485 hardware outcomes remain NOT RUN. Mock transport results
are software evidence only.

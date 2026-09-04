# PC Stage 2A Hardware Environment

- Client branch: `pc-stage2a-runtime-operations`
- STM32 baseline: `9e242c7649ad79ac4c3bae347b3259847f4bc09e`
- CH579 baseline: `3cd21ecefe6486d28a664ea356cb4ed6e7b3090a`
- TCP: `192.168.1.100:502`, Unit 1, previously Stage 1 hardware validated.
- COM5 was previously validated independently as USB-RS232 and USB-RS485 on
  separate runs; both adapters must never be attached simultaneously.
- Stage 2A writes are not run automatically. Explicit authorization and a
  documented safe test setup are required before the first command.

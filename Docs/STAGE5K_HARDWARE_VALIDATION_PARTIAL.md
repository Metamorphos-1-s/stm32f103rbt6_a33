# Stage 5K hardware validation partial run

Run `20260911T160500Z_stage5k_preflash` used STM32 commit
`6c03809bd70c8e34f4829a8a37fe21f73fb6cb1f` and Release ELF SHA-256
`A82F89F0FE12480078D125BB8BEE581439A1CEFE1A77319EACFB7E4FEA396E2D`.
ST-Link identified device ID `0x410`, 128 KiB Flash, voltage 3.29 V, ST-Link
serial `E1007200D0D2139393740544`.

The device was programmed and verified without full-chip erase. Before flash,
both configuration slots contained valid historical V2 records (A sequence 25,
B sequence 24). The 4096-byte region SHA-256 was
`4BCDE2EB7482D29D2AD5153FC1166BBFA1DC651B95F62314F1B50019587F1867`; the
post-first-boot region had the identical hash, proving no automatic migration or
overwrite. First boot reported Firmware `0x0510`, Map `0x0104`, public Schema
`2`, and Persistent Format `3`, with calibration invalid and safe defaults.

Using USART2 on COM5 (USB VID:PID `1A86:7523`, 115200 8N1), Modbus calibration
was completed with a 500 g standard mass. The calibration command sequence
returned `OK`, produced span mass `500000000 ug`, sequence `1`, and calibration
valid `1` in RAM.

The first Deferred SAVE returned mailbox `ACCEPTED` (token 251), but the
diagnostic terminal state became `INVALID_STATE` (`0x01C5-0x01C9 = [7,251,1,0,1]`)
and `dirty` remained set. The runtime fault mask was `0x00000040`
(`FAULT_CS1237_DATA_ERROR`). This is a blocking firmware/board state issue for
persistence validation; no manual power cycle or further write was attempted.

COM3 is identified as the CH579 UART1 log path; COM5 is the confirmed STM32
USART2 path. CH579 TCP `192.168.1.100:502` was reachable and raw port 5000 was
closed, but the full 0x0510 transparent regression was not run after the SAVE
failure.

Conclusion for this run: `STAGE 5K SOFTWARE READY; HARDWARE VALIDATION PARTIAL`.

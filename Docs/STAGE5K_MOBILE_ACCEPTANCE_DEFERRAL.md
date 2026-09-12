# Stage 5K Mobile Acceptance Deferral

Decision date: 2026-09-13

The WeChat client software gate passes 38/38 tests, and the BLE telemetry sequence-domain fix (`075862d8ed3c043ca92c0f12f8c2b9ef3326a6d2`) is an ancestor of the current client branch. Real-board monitoring with a Windows PC BLE Central passed the accepted V2 stability criteria for both RS485 and Modbus TCP PLC control paths.

The following mobile product-path tests were not executed:

- RS485 PLC + mobile WeChat BLE for 600 seconds.
- Modbus TCP PLC + mobile WeChat BLE for 600 seconds.

Consequently, no hardware evidence exists for a specific phone model, mobile operating system, WeChat BLE stack, or real-device mini-program session. PC BLE evidence is not mobile WeChat evidence. The user explicitly deferred these tests at this stage. The deferral does not block measurement-algorithm research, but mobile acceptance may still be completed independently before product release.

Stage 5K scoped conclusion:

`STAGE 5K CORE HARDWARE VALIDATED; MOBILE WECHAT HARDWARE ACCEPTANCE DEFERRED`

This conclusion covers STM32 firmware, persistence, RS232/RS485, Modbus TCP, CH579, PC client, PC BLE monitoring, and the single PLC control-path model. It does not claim mobile WeChat hardware validation, BLE zero-gap verification, or complete end-user mobile acceptance.

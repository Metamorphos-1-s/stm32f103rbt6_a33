# A33 Instrument Clients

Client Stage 0 contracts and protocol libraries for the A33 weighing instrument.
The STM32 protocol source of truth is the sibling repository
`stm32f103rbt6_a33` at commit `9e242c7649ad79ac4c3bae347b3259847f4bc09e`.

This repository contains a native TypeScript WeChat Mini Program core and a
.NET 10/WPF PC skeleton. Hardware communication is intentionally deferred to
Stage 1; protocol tests consume the JSON contracts under `contracts/`.

## Layout

- `contracts/`: machine-readable BLE V1 and Modbus map 0x0104 contracts.
- `apps/wechat-mini/`: TypeScript BLE adapter, stream parser and domain model.
- `apps/pc/`: C# Modbus codec, transport and WPF skeleton.
- `docs/`: architecture and firmware traceability.

## Verification

Run TypeScript tests with Node 20+ after installing dependencies in
`apps/wechat-mini` (`npm ci && npm test`). Build and test the PC solution with
.NET 10 on Windows (`dotnet build apps/pc/A33.Instrument.sln` and
`dotnet test apps/pc/tests/A33.Instrument.Protocol.Tests`). The executed
Stage 0-V gate is recorded in `docs/STAGE0_TOOLCHAIN_VALIDATION.md`; hardware
tests remain outside Stage 0.

The WeChat project uses the validated test AppID in
`apps/wechat-mini/project.config.json`. The DevTools simulator may emit
non-blocking preload advisories for its own `WAAutoService.js` and
`WAServiceMainContext.js` resources; these are documented in the validation
record and are not application errors.

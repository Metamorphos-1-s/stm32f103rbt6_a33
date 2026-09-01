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

## WeChat Stage 1

The mini program now provides read-only W02 scanning, BLE connection,
FAST/SLOW/CHECKWEIGH monitoring and bounded diagnostics. Only
GET_DEVICE_INFO and GET_ACTIVE_CONFIG are permitted on FFE2. Automated tests
and pending mobile hardware gates are documented in
`docs/WECHAT_STAGE1_BLE_MONITORING.md`.

Security status: the previously exposed AppSecret was rotated on 2026-09-01;
the replacement secret is not stored in this repository.

Xiaomi 15 Android validation is in progress. A post-fix run exceeded the
600-second gate by fixed-rate telemetry evidence and passed frame, command,
CRC, disconnect, stale-data and application-error criteria. Permission and
Bluetooth recovery, device deduplication and instrument-panel comparison now
pass. The permission-denial first-click fix needs one retest; Android version
and RF environment metadata remain open. iOS remains not run.

The first Xiaomi 15 Bluetooth-off recovery test exposed an adapter-state
recovery defect. Commit `47824bc` fixes adapter reopen, stale-error clearing
and listener de-duplication; its phone retest passed.

Commit `b65d245` removes BigInt exponentiation that the WeChat runtime lowered
to unsupported `Math.pow`; the post-fix monitor stability retest passed.

Commit `d8ba694` ensures every scan first reopens the adapter, avoiding a
transient `not init` error when permission is revoked. Its first-click denial
path remains to be confirmed on the Xiaomi 15.

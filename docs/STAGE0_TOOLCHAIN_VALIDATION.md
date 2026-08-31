# Stage 0-V Toolchain Validation

Validation date: 2026-09-01. Repository branch: `client-stage0-contracts`.
STM32 source of truth: `9e242c7649ad79ac4c3bae347b3259847f4bc09e`.

| Item | Result | Evidence |
| --- | --- | --- |
| .NET 10 SDK | PASS | SDK `10.0.400`, x64, installed at `C:\Users\meta\.dotnet10` |
| NuGet restore | PASS | All four projects restored (solution restore emitted a harmless SDK-style discovery warning; project restores completed) |
| C# Debug build | PASS | `dotnet build ... --configuration Debug`, 0 warnings, 0 errors |
| C# Release build | PASS | `dotnet build ... --configuration Release`, 0 warnings, 0 errors |
| C# Protocol tests | PASS | xUnit 11/11 passed, 0 failed, 0 skipped, 43 ms |
| WPF startup | PASS | Debug process launched, placeholder window process observed, no startup connection attempt; process terminated cleanly |
| TypeScript compile | PASS | TypeScript 5.9.3 compile via `pnpm test` |
| TypeScript tests | PASS | Node test runner 4/4 passed |
| Contract validation | PASS | `tools/validate-contracts/validate-contracts.ps1`; 32 definitions, 412 addresses, no overlaps |
| WeChat DevTools compile | PASS | WeChat Developer Tools `2.02.0`, project opened through CLI port `32845`; AppID `wxa909337693c03625` recognized |
| WeChat simulator startup | PASS | Diagnostics page rendered; `Connection state: CLOSED`, `Device: -` is the expected no-device Stage 0 state |
| BLE hardware | NOT RUN | Outside Stage 0-V scope |
| Modbus TCP hardware | NOT RUN | Outside Stage 0-V scope |
| Modbus RTU hardware | NOT RUN | Outside Stage 0-V scope |

No STM32 or CH579 files were changed. Build outputs remain excluded by
`.gitignore`.

## WeChat warning classification

The simulator console reported two messages:

| Message | Classification | Action |
| --- | --- | --- |
| `WAAutoService.js ... was preloaded ... but not used within a few seconds` | DevTools internal preload advisory | No action; not application code and does not affect startup or rendering |
| `WAServiceMainContext.js ... was preloaded ... but not used within a few seconds` | DevTools internal preload advisory | No action; not an exception, compile error, or runtime failure |

These are informational browser preload timing warnings emitted by the
simulator shell. They do not indicate a missing project resource, an
unhandled exception, a blank page, or a BLE/Modbus failure. Warning count for
the application itself is zero; the two messages are retained as non-blocking
DevTools warnings for traceability.

# Stage 5P Unified-R10C Host and stack audit

Baseline candidate: `0fe37c287e9aa55a2ab6fd6c90d88c183d82439b`.
No device connection or firmware/configuration write was made.

## Host toolchain search

The Windows PATH contains CMake 3.28.1 and Ninja 1.11.1, but no `gcc`,
`clang`, `clang-cl`, MSVC `cl`, `cc`, `make` or MinGW. A recursive search of
the available C/D/E volumes found no native host compiler. WSL reports an
Ubuntu distribution, but starting it fails with:

```text
HCS_E_SERVICE_NOT_AVAILABLE
```

Therefore the repository Host CTest executable cannot be compiled or run in
this environment. ARM cross-compilation and Python tests are explicitly not
counted as Host CTest.

**HOST: NOT RUN** (environment prerequisite unavailable).

## C-level focused coverage added

`Tests/host/stage5p_unified/test_defaults_battery.c` now exercises the real
`DeviceConfig_Validate()` path for the five target values, invalid recovery
and warning/critical ordering, and the intentional alarm-disabled legacy-zero
case. The test is wired into `stage5p_unified_tests`; it could not execute
because no native C compiler exists. The Python transaction contract remains
PASS, but is not a substitute for this C test.

The new C test does not claim mailbox integration coverage. Full native C
coverage of `ModbusRegisterModel_WriteMultiple`, generation/owner BUSY,
CONFIG_CANCEL, APPLY_RAM versus SAVE, save failure and simulated power-loss
recovery remains required before hardware use.

## ARM stack-usage build

Toolchain:

```text
GNU Tools for STM32 13.3.rel1.20240926-1715 (GCC 13.3.1)
CMake 3.28.1, Ninja 1.11.1
```

The reproducible build used a new directory and `-fstack-usage`:

```text
cmake -S . -B D:/Downloads/stage5p_battery_stack_build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS=-fstack-usage \
  -DA33_ENABLE_STAGE5P_UNIFIED=ON \
  -DA33_ENABLE_STAGE5P_BATTERY_TRANSACTION=ON \
  [the existing Stage 5P dependency flags] \
  -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake
cmake --build D:/Downloads/stage5p_battery_stack_build
```

ARM build: PASS. It produced 134 `.su` files. Relevant static frame sizes
include `CommandService_Execute=64 B`, `ModbusCommandMailbox_Write=56 B`,
`ModbusRegisterModel_WriteOne=56 B`, `ModbusRegisterModel_WriteMultiple=48 B`,
`ConfigApplication_ApplyInternal=40 B`, `BatteryAdc_Reconfigure=16 B`, and
`ModbusRegisterModel_ReadOne=248 B`. These are per-function compiler frames,
not a runtime high-water measurement. The relevant `.su` files are archived
under `Results/stage5p_unified/software/battery_transaction_stack/`.

The map reports `_ebss=0x200047D8`, RAM end `0x20005000`, and the existing
reserved heap/stack block. Static free RAM is 1,064 B before the established
512-B indirect-call/IRQ allowance, leaving a conservative 552 B. No DMA,
Modbus, BLE or reserved stack budget was reduced. Nested interrupt stack
interleaving and runtime watermark are not measured by `.su`.

**STACK: PASS for static conservative gate; runtime high-water: NOT RUN.**

## Image identity

The stack build generated the same candidate identity and bytes as the prior
candidate:

```text
0x0527 / 0x010F
BIN 97,196 B
BIN SHA-256 53C96D74C6830C266EEA4FCAC9A5AC60AC9F2345FF794ADC8E86CE99FEBC8F5B
ELF SHA-256 52108E8A83E516C9BADA5B46D60FDACD1E525BEC3014C74E15B0509F146FA26D
```

No product algorithm, V3 layout, calibration, R5 or checkweigh path changed.
The candidate remains software-only and is not ready for burn authorization
until native Host CTest and runtime stack evidence are supplied.

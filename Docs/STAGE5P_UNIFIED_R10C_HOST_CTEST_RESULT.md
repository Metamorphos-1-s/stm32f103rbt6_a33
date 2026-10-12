# Stage 5P Unified-R10C Host CTest result

Baseline: `ad49d1f17198417fd881dc2021033a35b337ec3f`.
No device access, firmware programming, SAVE, reset or power cycle occurred.

## Compiler provenance

No existing native compiler was installed. The official LLVM-MinGW release
asset was downloaded outside the repository:

```text
llvm-mingw-20261006-ucrt-x86_64.zip
SHA-256 317492C456AA27EE607A5919F1D2D38DCDC1112516A24D0BF4B00D078F52D17A
clang 23.1.3, target x86_64-w64-windows-gnu
```

The archive remains under `D:/Downloads/llvm-mingw-20261006`; it is not a
repository artifact. CMake 3.28.1 and Ninja 1.11.1 are the existing tools.

## Reproducible commands

```text
cmake -S Tests/host -B D:/Downloads/stage5p_host_debug2 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=D:/Downloads/llvm-mingw-20261006/llvm-mingw-20261006-ucrt-x86_64/bin/clang.exe
cmake --build D:/Downloads/stage5p_host_debug2 --target stage5p_unified_tests
ctest --test-dir D:/Downloads/stage5p_host_debug2 \
  -R stage5p_unified_defaults_and_startup --output-on-failure

cmake -S Tests/host -B D:/Downloads/stage5p_host_release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=D:/Downloads/llvm-mingw-20261006/llvm-mingw-20261006-ucrt-x86_64/bin/clang.exe
cmake --build D:/Downloads/stage5p_host_release --target stage5p_unified_tests
ctest --test-dir D:/Downloads/stage5p_host_release \
  -R stage5p_unified_defaults_and_startup --output-on-failure
```

The target is compiled with the complete unified product definitions,
including `A33_ENABLE_STAGE5P_BATTERY_TRANSACTION=1`. It includes the actual
C `DeviceConfig_Validate`, battery ADC, ConfigEdit, command service and
persistence sources.

## Results

| Build/test | Result |
| --- | --- |
| Host Debug `stage5p_unified_tests` build | PASS |
| Host Debug CTest `stage5p_unified_defaults_and_startup` | PASS, 1/1 |
| Host Release `stage5p_unified_tests` build | PASS |
| Host Release CTest `stage5p_unified_defaults_and_startup` | PASS, 1/1 |
| Full 47-test CTest execution | INCOMPLETE: only the focused target was built |

The focused C test now executes the battery default arithmetic/percentage and
60-sample warning confirmation, plus the new validator checks for all five
target fields, invalid threshold ordering and the alarm-disabled legacy-zero
tuple. The mailbox-level transaction helper remains covered by the Python
contract; full C-level mailbox/generation/owner/SAVE/power-loss integration
is not yet implemented in this test target and remains a risk before hardware.

The first full-build attempt also showed an existing Clang-only warning in
`menu_controller.c` (`s_original_unit` set but unused); the test target now
uses `-Wno-unused-but-set-variable` solely for this pre-existing source
warning. Product ARM warning flags and product sources were not changed.

## Image boundary

Only Host test build configuration changed; no product source or ARM build
inputs changed. The frozen 0x0527 image remains:

```text
97,196 B
53C96D74C6830C266EEA4FCAC9A5AC60AC9F2345FF794ADC8E86CE99FEBC8F5B
```

ASan/UBSan were not run because the portable bundle has no usable native
runtime setup. Device validation remains NOT RUN.

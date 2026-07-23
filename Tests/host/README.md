# Stage 2A Host Tests

These tests compile the production driver sources against HAL-free BSP mocks.
They cover CS1237 sign extension, configuration codec and fixed FIFO behavior;
TM1628 RAM, board segment and raw-key mappings; battery conversion; W02 timing;
and unsigned tick/cycle wraparound.

Run from a Visual Studio developer command prompt:

```powershell
cmake -S Tests/host -B Tests/host/out -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build Tests/host/out
ctest --test-dir Tests/host/out --output-on-failure
```

The target uses `/W4 /WX` with MSVC. `Tests/host/out/` is generated and ignored.

# Stage 2B Host Tests

These tests compile the production driver sources against HAL-free BSP mocks.
They cover Stage 2A driver logic plus RawMeasurement statistics, bounded FIFO
bridging, rate-limited raw events, diagnostic formatting/state transitions,
bounded outputs, W02 wrapping, and FAULT shutdown.

Run from a Visual Studio developer command prompt:

```powershell
cmake -S Tests/host -B Tests/host/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build Tests/host/build
ctest --test-dir Tests/host/build --output-on-failure
```

The target uses `/W4 /WX` with MSVC. `Tests/host/build/` is generated and ignored.

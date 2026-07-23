# Stage 3 Host Tests

These tests compile production Stage 2A/2B drivers and Stage 3 Domain/App
sources against HAL-free BSP and event mocks. Coverage includes checked integer
math, all filter modes, two-point calibration in both sensor directions,
stability and time wrap, zero/tare, `WeightEngine`, manager event rate limiting
and retry, the bounded bridge, internal REFOUT encoding, and synthetic scale
sequences.

Run from a Visual Studio developer command prompt:

```powershell
cmake -S Tests/host -B Tests/host/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build Tests/host/build
ctest --test-dir Tests/host/build --output-on-failure
```

The target is `stage3_host_tests` and uses `/W4 /WX` with MSVC.
`Tests/host/build/` is generated and ignored. Synthetic sequences are software
tests only and are NOT VERIFIED ON SCALE HARDWARE.

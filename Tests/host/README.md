# Stage 4A/4B/5 Host Tests

These tests compile production Stage 2A/2B drivers, Stage 3 metrology, and
Stage 4A UI/command/configuration/calibration logic sources against HAL-free BSP
and event mocks. Coverage includes checked integer
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

The targets use `/W4 /WX` with MSVC. Stage 4B directly tests the production CRC,
explicit V1 codec, A/B store, revision logic, and a fixed 4 KiB fault-injectable Flash model.
Its power-cut sweep is software simulation, not a physical brownout result.

`stage5b_host_tests` compiles the production Modbus CRC16, RTU timing, RTU
server, and Stage 5A register model with fake transport/time/RS485 boundaries.
It is a logic test, not a real serial-port or RS485 hardware test.
`Tests/host/build/` is generated and ignored. Synthetic sequences are software
tests only and are NOT VERIFIED ON SCALE HARDWARE.

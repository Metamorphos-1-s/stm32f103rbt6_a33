# Domain

This layer contains deterministic, HAL-free weighing logic. Stage 3 implements
integer math, raw filtering, calibration conversion, stability detection,
zero/tare state, configuration validation, and the unified `WeightEngine`.

Domain code does not depend on CS1237 types, GPIO, event queues, displays,
`SystemContext`, dynamic allocation, floating point, or STM32 HAL. App code is
responsible for adapting driver samples and publishing events.

# Command Service

This transport-independent service is the single entry for local keys and
future Modbus/BLE/USB clients. It maps commands to `MetrologyManager`, the RAM
configuration transaction, and `ConfigApplication`; it contains no weighing
math, display, GPIO, HAL, or wire protocol code.

Configuration and calibration commits update RAM and mark `config_dirty`.
Save and factory reset requests return `COMMAND_RESULT_STORAGE_UNAVAILABLE`;
Stage 4A never claims that data was persisted.

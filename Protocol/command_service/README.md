# Command Service

This transport-independent service is the single entry for local keys and
future Modbus/BLE/USB clients. It maps commands to `MetrologyManager`, the RAM
configuration transaction, and `ConfigApplication`; it contains no weighing
math, display, GPIO, HAL, or wire protocol code.

Configuration and calibration commits update RAM and mark `config_dirty`.
Stage 4B routes save and confirmed factory-reset requests through
`PersistenceManager`. Acceptance is asynchronous and is not completion; final
success, no-change, or failure is exposed by events and persistence status.

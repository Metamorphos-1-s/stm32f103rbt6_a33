# Key Service

Five physical TM1628 raw bits are translated through a validated `KeyMap` into
FUNCTION, TARE, ZERO, STAR, and HASH. The default bit0..bit4 mapping is a
DEVELOPMENT DEFAULT and must be VERIFIED ON HARDWARE.

Each key is independently debounced for 30 ms. The fixed 16-event FIFO reports
press, release, short, one long event at 1000 ms, and STAR/HASH repeats starting
at 600 ms every 150 ms. Full queues reject new events and count drops.

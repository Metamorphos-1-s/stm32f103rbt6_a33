# Run summary

Result: FAIL. The diagnostic control and counter/trace RAM were overwritten during the rate-switch completion path. Map and RAM evidence identify diagnostic stack collision: the old full metrology rebuild used a 1,048-byte replacement-engine frame while only about 1.3 KiB separated `.bss` from stack top. No valid 40 Hz rate can be calculated. Config Flash SHA-256 remained `D74C98D8D4221437773155E8D1ED75BC59D71AE2D285D5C2F18F6B494AA5DC86`; the exact product Release was immediately restored and verified.

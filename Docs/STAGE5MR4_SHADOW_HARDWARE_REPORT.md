# Stage 5M-R4 Shadow Hardware Report

Hardware shadow validation was not authorized because the frozen candidate
failed independent validation and no fixed-point parity artifact exists.

No diagnostic image was built or flashed. Product firmware remains 0x0510 with
Map 0x0104, Persistent Format V3, and Release SHA-256
`82E726F5B32A0DE36A5E686F62A937EC4FD9CBB488DB9733E83D2062673EF486`.
The final read-only probe after the 1 kg test showed dirty=0, active slot 1, and
storage sequence 7.

All shadow, PLC/BLE parallel, resource, and restore gates are **NOT RUN**.

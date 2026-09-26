# Realtime session archive

This session contains 185,910 read-only COM5/Modbus records from firmware
0x051C, Map 0x0104. The CSV and JSONL event log are stored directly. The raw
`frames.jsonl` is 223,649,867 bytes and is therefore archived losslessly as
`frames.jsonl.7z` to stay below normal single-file hosting limits. The archive
contains the original filename and restores byte-for-byte.

```powershell
7z t frames.jsonl.7z
7z x frames.jsonl.7z
Get-FileHash frames.jsonl -Algorithm SHA256
```

Expected restored SHA-256:

`75A87B361F293170042C564D6432EF58C577CCD784D4918B3EB7C9CE583F4FBC`

The full file hashes, UTC range, record count, and device state are in
`session_summary.json`. The recorder performed no writes or Flash operations.

# Stage 5K Canonical Persistent Format V3

Stage 5K separates the public configuration contract from the internal Flash
format. Public Device Config Schema remains `2` for Modbus/BLE clients. Internal
Flash records now use `CONFIG_STORE_FORMAT_V3` and `CONFIG_STORE_SCHEMA_V3`.

The A/B slot addresses and 2 KiB slot size are unchanged. Header, CRC32,
sequence comparison, commit-last programming, bounded verification and power
guards are unchanged. Records with the old V1/V2 format are unsupported: they
are never migrated or decoded as V3, never automatically overwritten, and cause
the startup loader to select safe defaults with calibration invalid. A later
explicit SAVE creates a V3 record.

## Canonical payload

The payload is 281 bytes, encoded explicitly little-endian:

| Block | Bytes |
| --- | ---: |
| Canonical metrology scalars and unit displays | 65 |
| Load-cell metadata | 17 |
| Two current weighing profiles | 50 |
| Active profile | 1 |
| Calibration current fields | 29 |
| Stability | 14 |
| Communication current fields | 24 |
| Bluetooth | 7 |
| Alarm current fields | 29 |
| Display | 2 |
| Battery | 29 |
| System | 3 |
| Persisted runtime view/tare | 11 |
| Reserved canonical byte | 1 |
| **Total** | **281** |

No V1 prefix, legacy count projection, migration marker or C-structure padding
is serialized. Signed values use bit-preserving `memcpy` conversions; bools and
reserved bytes are validated on decode. The codec tests cover round-trip,
truncation, invalid bools, unsupported schema, CRC, A/B selection, sequence
wrap and power-cut recovery.

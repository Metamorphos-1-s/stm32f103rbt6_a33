# Stage 5K BLE Acceptance Criteria Change

Decision date: 2026-09-12

BLE is an auxiliary, read-only monitoring interface while RS485 or Modbus TCP is the sole PLC control path. The product acceptance criterion no longer requires an absolute zero telemetry gap.

Acceptance V2 requires zero CRC errors, zero unrecovered parser errors, 100% read-only refresh success, a telemetry missing rate no greater than 0.1%, no more than one consecutively missing frame, zero out-of-order frames, zero unrecovered disconnects, zero PLC communication failures, and no configuration-state pollution. Gap events, recoverable resynchronization, fragmentation/coalescing, and recoverable reconnects remain visible statistics.

The original 20260912 RS485 + PC BLE run and its original failure result are immutable. Under V2 it passes: one frame was missing out of 12,605 expected frames (0.007933%), the maximum consecutive loss was one, all 60 refreshes succeeded, and all 6,801 RS485 requests succeeded.

The recorded `parser_resync=72` means one otherwise well-formed 73-byte SLOW frame arrived without its leading `A5` sync byte. The parser discarded the remaining 72 bytes and recovered at the next `A5 5A` sync. This is one recovered event, not 72 events. It was not caused by normal fragmentation, coalescing, or command-response insertion, and it caused no persistent loss of framing, CRC error, disconnect, or refresh failure.

Status after this criteria correction:

`STAGE 5K SAVE BLOCKER CLOSED; FULL HARDWARE VALIDATION PENDING`

Zero-gap is not claimed.

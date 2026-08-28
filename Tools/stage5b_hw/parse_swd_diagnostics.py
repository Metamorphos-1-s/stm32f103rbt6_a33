"""Parse Release-build Stage 5B statistics dumped from target RAM."""

import argparse
import json
import struct
from pathlib import Path

# Addresses for the current Release link map. Keep these synchronized with
# build/Release/stm32f103rbt6_a33.map when the firmware layout changes.
TX_LAST_ERROR = 0x20001BDB
TX_STATE = 0x20001BF0
TRANSPORT_RECOVERY_FAILURES = 0x20001BF1
TRANSPORT_RECEIVE_ERROR = 0x20001BF4
UART_STATS = 0x20001CC4
FRAMER2_STATS = 0x20000D78
FRAMER2_STATE = 0x20000C68
SERVER2_STATS = 0x200007A0
SERVER2_STATE = 0x20000780 + 0x1C
FRAMER3_STATS = 0x20000C20
FRAMER3_STATE = 0x20000AF8 + 0x14
SERVER3_STATS = 0x20000428
SERVER3_STATE = 0x20000408 + 0x1C
UART3_MODBUS_STATS = 0x20002108
CS1237_READ_ERRORS = 0x20001AFC
CS1237_BUFFER_OVERRUNS = 0x20001B00
CS1237_SAMPLE_COUNT = 0x20001B04
EVENT_QUEUE_DROPPED = 0x200031A0
FAULT_MASK = 0x200032E8
BLE_TRANSPORT_DIAGNOSTICS = 0x200033AC
BLE_CONNECTION_DIAGNOSTICS = 0x20003944
BLE_TELEMETRY_SCHEDULER = 0x20003A00
BLE_COMMAND_DIAGNOSTICS = 0x20003A54


def _slice(data, base, address, size):
    offset = address - base
    if offset < 0 or offset + size > len(data):
        raise ValueError("dump does not cover 0x%08X..0x%08X" %
                         (address, address + size - 1))
    return data[offset:offset + size]


def parse(data, base_address):
    uart_values = struct.unpack("<16I2H", _slice(data, base_address, UART_STATS, 68))
    uart_names = ["rx_byte_count", "rx_idle_count", "rx_half_count",
        "rx_wrap_count", "rx_dma_error_count", "rx_overrun_count",
        "rx_wrap_race_recovery_count",
        "rx_idle_queue_overflow_count",
        "uart_parity_error_count", "uart_frame_error_count",
        "uart_noise_error_count", "uart_overrun_error_count",
        "tx_request_count", "tx_complete_count", "tx_dma_error_count",
        "tx_timeout_count", "dma_write_position", "dma_read_position"]
    framer2_values = struct.unpack("<11IH2x", _slice(data, base_address, FRAMER2_STATS, 48))
    framer3_values = struct.unpack("<11IH2x", _slice(data, base_address, FRAMER3_STATS, 48))
    framer_names = ["frame_count", "short_frame_count",
        "inter_character_error_count", "overflow_count",
        "transport_error_count", "idle_event_count", "timer_event_count",
        "timer_race_count", "timer_t1_5_elapsed_count",
        "timer_t3_5_elapsed_count", "timer_start_failure_count",
        "current_frame_length"]
    server2_values = struct.unpack("<16I3Bx2H", _slice(data, base_address, SERVER2_STATS, 72))
    server3_values = struct.unpack("<16I3Bx2H", _slice(data, base_address, SERVER3_STATS, 72))
    server_names = ["valid_frame_count", "addressed_frame_count",
        "ignored_address_count", "broadcast_count", "crc_error_count",
        "length_error_count", "function03_count", "function06_count",
        "function16_count", "illegal_function_count",
        "exception_response_count", "tx_response_count", "tx_error_count",
        "tx_start_error_count", "tx_timeout_error_count",
        "protocol_violation_count", "last_request_address", "last_function",
        "last_exception", "last_request_length", "last_response_length"]
    ble_transport_values = struct.unpack(
        "<9I2HB3xI?3x",
        _slice(data, base_address, BLE_TRANSPORT_DIAGNOSTICS, 52))
    ble_transport_names = ["rx_bytes", "tx_bytes", "rx_overflow",
        "uart_error", "tx_error", "tx_complete", "transport_reset_count",
        "last_rx_ms", "last_tx_ms", "rx_pending", "tx_pending",
        "priority_pending", "priority_queue_full", "tx_busy"]
    ble_connection_values = struct.unpack(
        "<BB2x13I",
        _slice(data, base_address, BLE_CONNECTION_DIAGNOSTICS, 56))
    ble_connection_names = ["module_state", "link_state", "rx_bytes",
        "tx_bytes", "rx_frames", "tx_frames", "rx_overflow", "uart_error",
        "parse_error", "crc_error", "disconnect_count", "reconnect_count",
        "transport_reset_count", "last_rx_ms", "last_tx_ms"]
    telemetry_values = struct.unpack(
        "<15I",
        _slice(data, base_address, BLE_TELEMETRY_SCHEDULER + 16, 60))
    telemetry_names = ["frames_generated", "frames_sent",
        "frames_dropped_queue_full", "frames_dropped_transport_not_ready",
        "bytes_sent", "encode_errors", "fast_frames_generated",
        "fast_frames_sent", "fast_frames_dropped", "slow_frames_generated",
        "slow_frames_sent", "slow_frames_dropped",
        "checkweigh_frames_generated", "checkweigh_frames_sent",
        "checkweigh_frames_dropped"]
    command_values = struct.unpack(
        "<10I", _slice(data, base_address, BLE_COMMAND_DIAGNOSTICS, 40))
    command_names = ["requests_received", "responses_sent", "responses_queued",
        "response_queue_full", "duplicate_requests", "transaction_conflicts",
        "pending_save_requests", "parser_crc_errors", "parser_length_errors",
        "parser_resync_count"]
    return {
        "dump_base": "0x%08X" % base_address,
        "cs1237": {
            "read_error_count": struct.unpack("<I", _slice(
                data, base_address, CS1237_READ_ERRORS, 4))[0],
            "buffer_overrun_count": struct.unpack("<I", _slice(
                data, base_address, CS1237_BUFFER_OVERRUNS, 4))[0],
            "sample_count": struct.unpack("<I", _slice(
                data, base_address, CS1237_SAMPLE_COUNT, 4))[0],
        },
        "event_queue": {
            "dropped_count": struct.unpack("<I", _slice(
                data, base_address, EVENT_QUEUE_DROPPED, 4))[0],
        },
        "fault_mask": "0x%016X" % struct.unpack("<Q", _slice(
            data, base_address, FAULT_MASK, 8))[0],
        "ble_transport": dict(zip(ble_transport_names, ble_transport_values)),
        "ble_connection": dict(zip(ble_connection_names, ble_connection_values)),
        "ble_telemetry": dict(zip(telemetry_names, telemetry_values)),
        "ble_command": dict(zip(command_names, command_values)),
        "uart_dma": dict(zip(uart_names, uart_values)),
        "uart3_modbus": dict(zip(
            ["rx_byte_count", "rx_overrun_count", "rx_error_count",
             "tx_request_count", "tx_complete_count", "tx_error_count",
             "tx_timeout_count", "dma_write_position", "dma_read_position"],
            struct.unpack("<7I2H", _slice(data, base_address,
                                           UART3_MODBUS_STATS, 32)))),
        "framer2": dict(zip(framer_names, framer2_values)),
        "framer3": dict(zip(framer_names, framer3_values)),
        "server2": dict(zip(server_names, server2_values)),
        "server3": dict(zip(server_names, server3_values)),
        "framer": dict(zip(framer_names, framer2_values)),
        "server": dict(zip(server_names, server2_values)),
        "states": {
            "tx": _slice(data, base_address, TX_STATE, 1)[0],
            "tx_last_error": _slice(data, base_address, TX_LAST_ERROR, 1)[0],
            "transport_recovery_failures": _slice(
                data, base_address, TRANSPORT_RECOVERY_FAILURES, 1)[0],
            "transport_receive_error": _slice(
                data, base_address, TRANSPORT_RECEIVE_ERROR, 1)[0],
            "framer2": _slice(data, base_address, FRAMER2_STATE, 1)[0],
            "server2": _slice(data, base_address, SERVER2_STATE, 1)[0],
            "framer3": _slice(data, base_address, FRAMER3_STATE, 1)[0],
            "server3": _slice(data, base_address, SERVER3_STATE, 1)[0],
        },
    }


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("dump")
    parser.add_argument("--base-address", type=lambda value: int(value, 0),
                        default=TX_LAST_ERROR)
    parser.add_argument("--json")
    args = parser.parse_args(argv)
    result = parse(Path(args.dump).read_bytes(), args.base_address)
    output = json.dumps(result, indent=2)
    print(output)
    if args.json:
        Path(args.json).write_text(output + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

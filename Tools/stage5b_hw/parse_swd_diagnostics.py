"""Parse Release-build Stage 5B statistics dumped from target RAM."""

import argparse
import json
import struct
from pathlib import Path

TX_LAST_ERROR = 0x20000E5F
TX_STATE = 0x20000E74
TRANSPORT_RECOVERY_FAILURES = 0x20000E75
TRANSPORT_RECEIVE_ERROR = 0x20000E78
UART_STATS = 0x20000EB8
FRAMER_STATS = 0x200015C0
FRAMER_STATE = 0x200016DC
SERVER_STATS = 0x200019FC
SERVER_STATE = 0x20001A44


def _slice(data, base, address, size):
    offset = address - base
    if offset < 0 or offset + size > len(data):
        raise ValueError("dump does not cover 0x%08X..0x%08X" %
                         (address, address + size - 1))
    return data[offset:offset + size]


def parse(data, base_address):
    uart_values = struct.unpack("<15I2H", _slice(data, base_address, UART_STATS, 64))
    uart_names = ["rx_byte_count", "rx_idle_count", "rx_half_count",
        "rx_wrap_count", "rx_dma_error_count", "rx_overrun_count",
        "rx_wrap_race_recovery_count",
        "uart_parity_error_count", "uart_frame_error_count",
        "uart_noise_error_count", "uart_overrun_error_count",
        "tx_request_count", "tx_complete_count", "tx_dma_error_count",
        "tx_timeout_count", "dma_write_position", "dma_read_position"]
    framer_values = struct.unpack("<5IH2x", _slice(data, base_address, FRAMER_STATS, 24))
    framer_names = ["frame_count", "short_frame_count",
        "inter_character_error_count", "overflow_count",
        "transport_error_count", "current_frame_length"]
    server_values = struct.unpack("<16I3Bx2H", _slice(data, base_address, SERVER_STATS, 72))
    server_names = ["valid_frame_count", "addressed_frame_count",
        "ignored_address_count", "broadcast_count", "crc_error_count",
        "length_error_count", "function03_count", "function06_count",
        "function16_count", "illegal_function_count",
        "exception_response_count", "tx_response_count", "tx_error_count",
        "tx_start_error_count", "tx_timeout_error_count",
        "protocol_violation_count", "last_request_address", "last_function",
        "last_exception", "last_request_length", "last_response_length"]
    return {
        "dump_base": "0x%08X" % base_address,
        "uart_dma": dict(zip(uart_names, uart_values)),
        "framer": dict(zip(framer_names, framer_values)),
        "server": dict(zip(server_names, server_values)),
        "states": {
            "tx": _slice(data, base_address, TX_STATE, 1)[0],
            "tx_last_error": _slice(data, base_address, TX_LAST_ERROR, 1)[0],
            "transport_recovery_failures": _slice(
                data, base_address, TRANSPORT_RECOVERY_FAILURES, 1)[0],
            "transport_receive_error": _slice(
                data, base_address, TRANSPORT_RECEIVE_ERROR, 1)[0],
            "framer": _slice(data, base_address, FRAMER_STATE, 1)[0],
            "server": _slice(data, base_address, SERVER_STATE, 1)[0],
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

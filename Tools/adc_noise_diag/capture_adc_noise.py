#!/usr/bin/env python3
"""Capture deduplicated raw CS1237 samples through read-only Modbus FC03."""

import argparse
import csv
import datetime as dt
import logging
import os
from pathlib import Path
import sys
import time

HERE = Path(__file__).resolve().parent
STAGE5B = HERE.parent / "stage5b_hw"
sys.path.insert(0, str(STAGE5B))

import modbus_frame as frame  # noqa: E402
import register_map as reg  # noqa: E402
from serial_transport import SerialTransport  # noqa: E402

from diag_common import (CS1237_STATE_RUNNING, MODE_IDS, U32SequenceTracker,
                         U32TimeExtender, validate_diagnostic_identity)


CSV_FIELDS = [
    "host_time_iso", "host_monotonic_s", "elapsed_s", "test_id", "mode",
    "load_g", "sample_sequence", "device_timestamp_ms", "raw_count",
    "filtered_raw_count", "adc_state", "fifo_count", "overrun_count",
    "lost_samples", "config_register", "word_order", "excluded",
    "counts_per_g", "sensor_capacity_g", "sensor_sensitivity_mv_v",
    "excitation_v", "notes",
]


class CaptureError(RuntimeError):
    pass


class ModbusReader:
    def __init__(self, transport, slave):
        self.transport = transport
        self.slave = slave

    def read(self, address, count):
        exchange = self.transport.exchange(
            frame.build_read(self.slave, address, count))
        if not exchange.rx:
            raise CaptureError("Modbus response timeout")
        try:
            words = frame.registers_from_read_response(exchange.rx, self.slave)
        except (ValueError, frame.FrameError) as exc:
            raise CaptureError(str(exc)) from exc
        if len(words) != count:
            raise CaptureError("unexpected FC03 register count")
        return words


def read_with_retry(reader, address, count, attempts=3):
    error = None
    for attempt in range(1, attempts + 1):
        try:
            return reader.read(address, count)
        except (CaptureError, OSError) as exc:
            error = exc
            logging.warning("FC03 0x%04X attempt %d/%d failed: %s",
                            address, attempt, attempts, exc)
    raise CaptureError("FC03 0x%04X failed after %d attempts: %s" %
                       (address, attempts, error))


def decode_u32(words, word_order):
    ordered = list(words) if word_order == "high" else list(reversed(words))
    return (ordered[0] << 16) | ordered[1]


def read_identity(reader, requested_mode):
    identity = read_with_retry(reader, reg.REGISTER_MAP_ID, 2)
    if identity != [reg.REGISTER_MAP_VERSION, reg.FIRMWARE_VERSION]:
        raise CaptureError("unsupported map/firmware identity %r" % identity)
    word_order = "low" if read_with_retry(
        reader, reg.ACTIVE_WORD_ORDER, 1)[0] else "high"
    deadline = time.monotonic() + 10.0
    while True:
        diag = read_with_retry(reader, reg.RAW_VALUE,
                               reg.ADC_NOISE_DIAG_MODE - reg.RAW_VALUE + 1)
        mode = diag[reg.ADC_NOISE_DIAG_MODE - reg.RAW_VALUE]
        if mode != MODE_IDS[requested_mode]:
            raise CaptureError("diagnostic image mismatch: requested %s, device reports %d" %
                               (requested_mode, mode))
        state = diag[reg.CS1237_STATE - reg.RAW_VALUE]
        if state == CS1237_STATE_RUNNING:
            config_register = diag[
                reg.CS1237_CONFIG_REGISTER - reg.RAW_VALUE]
            try:
                decoded = validate_diagnostic_identity(
                    requested_mode, mode, config_register)
            except ValueError as exc:
                raise CaptureError(str(exc)) from exc
            return word_order, decoded, diag
        if time.monotonic() >= deadline:
            raise CaptureError("CS1237 did not enter RUNNING within 10 seconds")
        time.sleep(0.05)


def calibration_counts_per_g(reader, word_order):
    diag = read_with_retry(reader, 0x003B, 1)
    if diag[0] == 0:
        return None
    words = read_with_retry(reader, 0x0190, 8)
    raw_zero = frame.decode_i32_words(words[0:2], word_order)
    raw_span = frame.decode_i32_words(words[2:4], word_order)
    span_mass_ug = frame.decode_i64_words(words[4:8], word_order)
    if raw_span == raw_zero or span_mass_ug <= 0:
        return None
    return abs((raw_span - raw_zero) / (span_mass_ug / 1_000_000.0))


def configure_logging(output):
    log_path = output.with_suffix(output.suffix + ".log")
    logging.basicConfig(level=logging.INFO,
        format="%(asctime)s %(levelname)s %(message)s",
        handlers=[logging.FileHandler(log_path, encoding="utf-8"),
                  logging.StreamHandler()],
        force=True)


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--parity", choices=("N", "E", "O"), default="N")
    parser.add_argument("--stop-bits", type=int, choices=(1, 2), default=1)
    parser.add_argument("--slave", type=int, default=1)
    parser.add_argument("--mode", choices=sorted(MODE_IDS), required=True)
    parser.add_argument("--duration", type=float, required=True,
                        help="total capture seconds, including warmup")
    parser.add_argument("--warmup", type=float, default=60.0)
    parser.add_argument("--poll-ms", type=int, default=20)
    parser.add_argument("--test-id", required=True)
    parser.add_argument("--load-g", type=float, default=0.0)
    parser.add_argument("--sensor-capacity-g", type=float)
    parser.add_argument("--sensor-sensitivity-mv-v", type=float)
    parser.add_argument("--excitation-v", type=float)
    parser.add_argument("--counts-per-g", type=float)
    parser.add_argument("--notes", default="")
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    if args.duration <= 0 or args.warmup < 0 or args.warmup >= args.duration:
        raise SystemExit("duration must be positive and warmup must be smaller")
    if args.poll_ms <= 0 or args.counts_per_g is not None and \
            args.counts_per_g <= 0:
        raise SystemExit("poll-ms and counts-per-g must be positive")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    configure_logging(args.output)
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    tracker = U32SequenceTracker()
    time_extender = U32TimeExtender()
    accepted = 0
    interrupted = False
    failure = None
    start = time.monotonic()
    last_flush = start

    with temporary.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=CSV_FIELDS)
        writer.writeheader()
        try:
            with SerialTransport(args.port, args.baud, args.parity,
                    args.stop_bits, timeout_ms=max(300, args.poll_ms * 5)) as transport:
                transport.reset()
                reader = ModbusReader(transport, args.slave)
                word_order, decoded, _initial = read_identity(reader, args.mode)
                start = time.monotonic()
                last_flush = start
                counts_per_g = args.counts_per_g
                if counts_per_g is None:
                    counts_per_g = calibration_counts_per_g(reader, word_order)
                logging.info("validated mode=%s config=%s word_order=%s",
                             args.mode, decoded, word_order)
                next_poll = time.monotonic()
                while True:
                    now = time.monotonic()
                    elapsed = now - start
                    if elapsed >= args.duration:
                        break
                    if now < next_poll:
                        time.sleep(next_poll - now)
                    next_poll += args.poll_ms / 1000.0
                    block = read_with_retry(reader, reg.RAW_VALUE,
                        reg.ADC_NOISE_DIAG_MODE - reg.RAW_VALUE + 1)
                    adc_state = block[reg.CS1237_STATE - reg.RAW_VALUE]
                    config_register = block[
                        reg.CS1237_CONFIG_REGISTER - reg.RAW_VALUE]
                    reported_mode = block[reg.ADC_NOISE_DIAG_MODE - reg.RAW_VALUE]
                    validate_diagnostic_identity(
                        args.mode, reported_mode, config_register)
                    if adc_state != CS1237_STATE_RUNNING:
                        logging.info("waiting for CS1237 RUNNING, state=%d", adc_state)
                        continue
                    sequence = decode_u32(block[4:6], word_order)
                    update = tracker.update(sequence)
                    if not update.accepted:
                        continue
                    device_timestamp = decode_u32(block[6:8], word_order)
                    try:
                        time_extender.update(device_timestamp)
                    except ValueError as exc:
                        logging.warning("timestamp anomaly at sequence %u: %s",
                                        sequence, exc)
                    host_now = time.monotonic()
                    elapsed = host_now - start
                    row = {
                        "host_time_iso": dt.datetime.now().astimezone().isoformat(),
                        "host_monotonic_s": "%.9f" % host_now,
                        "elapsed_s": "%.6f" % elapsed,
                        "test_id": args.test_id, "mode": args.mode,
                        "load_g": args.load_g,
                        "sample_sequence": sequence,
                        "device_timestamp_ms": device_timestamp,
                        "raw_count": frame.decode_i32_words(block[0:2], word_order),
                        "filtered_raw_count": frame.decode_i32_words(
                            block[2:4], word_order),
                        "adc_state": adc_state,
                        "fifo_count": block[reg.CS1237_BUFFERED - reg.RAW_VALUE],
                        "overrun_count": decode_u32(block[17:19], word_order),
                        "lost_samples": tracker.total_lost,
                        "config_register": config_register,
                        "word_order": word_order,
                        "excluded": str(elapsed < args.warmup).lower(),
                        "counts_per_g": "" if counts_per_g is None else
                            "%.12g" % counts_per_g,
                        "sensor_capacity_g": args.sensor_capacity_g,
                        "sensor_sensitivity_mv_v": args.sensor_sensitivity_mv_v,
                        "excitation_v": args.excitation_v,
                        "notes": args.notes,
                    }
                    writer.writerow(row)
                    accepted += 1
                    if host_now - last_flush >= 1.0:
                        stream.flush()
                        os.fsync(stream.fileno())
                        last_flush = host_now
        except KeyboardInterrupt:
            interrupted = True
            logging.warning("capture interrupted by user; preserving CSV")
        except (CaptureError, OSError, ValueError) as exc:
            failure = exc
            logging.error("capture stopped; preserving CSV: %s", exc)
        finally:
            stream.flush()
            os.fsync(stream.fileno())
    os.replace(temporary, args.output)
    logging.info("saved %d unique samples, lost=%d, timestamp_anomalies=%d%s",
                 accepted, tracker.total_lost, time_extender.anomalies,
                 " (interrupted)" if interrupted else "")
    logging.shutdown()
    if failure is not None:
        raise failure
    return 130 if interrupted else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (CaptureError, OSError, ValueError) as exc:
        logging.error("capture failed: %s", exc)
        sys.exit(2)

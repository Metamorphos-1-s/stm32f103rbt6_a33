"""Read-only long-running Modbus polling test."""

import csv
import statistics
import time

import register_map as reg


def run(client, report, duration_s, interval_ms, max_timeouts=0,
        max_crc_errors=0, output_csv=None, timeout_retries=0):
    start = time.monotonic()
    rows, latencies = [], []
    timeouts = crc_errors = exceptions = 0
    retry_recoveries = retry_failures = 0
    timeout_details = []
    request_number = 0
    first_sequence = last_sequence = None
    while time.monotonic() - start < duration_s:
        request_number += 1
        row = {"timestamp": time.time(), "request_number": request_number}
        stage = "realtime"
        try:
            values, exchange = client.read(reg.REALTIME_FIRST, 0x20)
            stage = "sample_sequence"
            diag, _ = client.read(reg.SAMPLE_SEQUENCE, 2)
            stage = "storage_state"
            storage, _ = client.read(reg.STORAGE_STATE, 1)
            sequence = (diag[0] << 16) | diag[1]
            first_sequence = sequence if first_sequence is None else first_sequence
            last_sequence = sequence
            row.update({"tx_hex": exchange.tx.hex(), "rx_hex": exchange.rx.hex(),
                        "response_time_first_byte_ms": exchange.first_byte_ms,
                        "response_time_complete_ms": exchange.complete_ms,
                        "function": 3, "exception": "", "crc_valid": True,
                        "sample_sequence": sequence,
                        "status_flags": values[4] | (values[5] << 16),
                        "storage_state": storage[0], "communication_state": "not mapped"})
            latencies.append(exchange.complete_ms)
        except Exception as exc:
            text = str(exc)
            row["exception"] = text
            row["failure_stage"] = stage
            if "timeout" in text.lower():
                timeouts += 1
                detail = {"request_number": request_number, "stage": stage,
                          "recovered": False, "retry_attempts": 0}
                retry_address, retry_quantity = {
                    "realtime": (reg.REALTIME_FIRST, 0x20),
                    "sample_sequence": (reg.SAMPLE_SEQUENCE, 2),
                    "storage_state": (reg.STORAGE_STATE, 1),
                }[stage]
                for retry in range(timeout_retries):
                    detail["retry_attempts"] = retry + 1
                    try:
                        _, retry_exchange = client.read(retry_address, retry_quantity)
                        detail["recovered"] = True
                        detail["retry_complete_ms"] = retry_exchange.complete_ms
                        retry_recoveries += 1
                        break
                    except Exception as retry_exc:
                        detail["last_retry_error"] = str(retry_exc)
                if not detail["recovered"] and timeout_retries:
                    retry_failures += 1
                timeout_details.append(detail)
                row.update({"retry_recovered": detail["recovered"],
                            "retry_attempts": detail["retry_attempts"]})
            elif "crc" in text.lower():
                crc_errors += 1
            else:
                exceptions += 1
        rows.append(row)
        if timeouts > max_timeouts or crc_errors > max_crc_errors:
            break
        time.sleep(interval_ms / 1000.0)
    if output_csv:
        keys = sorted({key for row in rows for key in row})
        with open(output_csv, "w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, keys)
            writer.writeheader(); writer.writerows(rows)
    data = {"requests": request_number, "successes": len(latencies),
            "timeouts": timeouts, "crc_errors": crc_errors, "exceptions": exceptions,
            "retry_recoveries": retry_recoveries,
            "retry_failures": retry_failures,
            "timeout_details": timeout_details,
            "sample_sequence_growth": None if first_sequence is None else
                ((last_sequence - first_sequence) & 0xFFFFFFFF)}
    if latencies:
        ordered = sorted(latencies)
        percentile = lambda p: ordered[min(len(ordered) - 1, int((len(ordered) - 1) * p))]
        data.update({"latency_min_ms": min(latencies), "latency_max_ms": max(latencies),
                     "latency_mean_ms": statistics.mean(latencies),
                     "latency_p50_ms": percentile(.50), "latency_p95_ms": percentile(.95),
                     "latency_p99_ms": percentile(.99)})
    passed = timeouts <= max_timeouts and crc_errors <= max_crc_errors and bool(latencies)
    report.add("soak", "PASS" if passed else "FAIL", "%d requests" % request_number, data)
    report.add("DMA/UART/server counters", "NOT VERIFIED WITH CURRENT EQUIPMENT",
               "not mapped to Modbus; inspect Stage5BModbusDiagnosticSnapshot over SWD")
    return passed

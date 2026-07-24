"""Read-only Stage 5B smoke suite."""

from hw_common import probe_device
import register_map as reg


def run(client, report, count=100, interval_s=0.05):
    import time
    identity = probe_device(client)
    report.add("probe", "PASS", "identity and CRC validated", identity)
    last_sequence = None
    for index in range(count):
        values, exchange = client.read(reg.SAMPLE_SEQUENCE, 2)
        sequence = (values[0] << 16) | values[1]
        if last_sequence is not None and ((sequence - last_sequence) & 0xFFFFFFFF) >= 0x80000000:
            report.add("sample sequence", "FAIL", "sequence moved backwards")
            return False
        last_sequence = sequence
        report.latency({"request": index + 1, "complete_ms": exchange.complete_ms,
                        "first_byte_ms": exchange.first_byte_ms})
        if interval_s:
            time.sleep(interval_s)
    report.add("repeated FC03", "PASS", "%d reads" % count)
    return True

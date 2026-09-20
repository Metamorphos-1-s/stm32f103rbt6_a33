#!/usr/bin/env python3
import sys
import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))
import r5_beta_hw
from r5_beta_hw import decode_beta


class R5BetaHardwareToolTests(unittest.TestCase):
    def test_decode_high_word_first(self):
        words = [0] * 40
        words[0] = 0x55B5; words[1] = 1; words[2] = 2; words[3] = 5
        words[6:10] = [0, 0, 0, 123]
        words[30:32] = [0xFFFF, 0xFFCE]
        words[32:35] = [15, 300, 600]
        words[39] = 7
        value = decode_beta(words)
        self.assertEqual(value["application"], 1)
        self.assertEqual(value["offset_ug"], 123)
        self.assertEqual(value["correction_rate_milli_ug_per_s"], -50)
        self.assertEqual(value["observation_fill"], 600)
        self.assertEqual(value["save_request_count_low"], 7)

    def test_signature_required(self):
        with self.assertRaises(Exception): decode_beta([0] * 40)

    def test_r5e_identity_is_exact(self):
        self.assertEqual(r5_beta_hw.EXPECTED_FIRMWARE, 0x0512)
        self.assertNotEqual(r5_beta_hw.EXPECTED_FIRMWARE, 0x0511)

    def test_read_state_rejects_legacy_identity_and_requires_signature(self):
        class Client:
            def __init__(self, firmware=0x0512, signature=0x55B5):
                self.firmware = firmware
                self.signature = signature
            def read(self, address, quantity):
                values = [0] * quantity
                if address == 0:
                    values[14] = 0x0104
                    values[15] = self.firmware
                elif address == 0x20:
                    values[16] = 3
                    values[17] = 1
                    values[19] = 7
                    values[21] = 7
                elif address == 0x1C0:
                    values[0] = 3
                elif address == 0x280:
                    values[0] = self.signature
                return values, None
        self.assertEqual(r5_beta_hw.read_state(Client())["firmware"], "0x0512")
        with self.assertRaises(Exception):
            r5_beta_hw.read_state(Client(firmware=0x0511))
        with self.assertRaises(Exception):
            r5_beta_hw.read_state(Client(signature=0))

    def test_record_preserves_rows_on_interrupt(self):
        class Transport:
            def __init__(self, *args, **kwargs): pass
            def close(self): pass
        base = {"utc":"2026-09-16T00:00:00.000Z","firmware":"0x0512",
            "map":"0x0104","sample_sequence":1,"mode":0,"state":0,
            "application":0,"offset_ug":0}
        second = dict(base, utc="2026-09-16T00:00:01.000Z",
                      sample_sequence=3, mode=2, state=2)
        with tempfile.TemporaryDirectory() as directory:
            args = SimpleNamespace(output=str(Path(directory)/"run"),
                port="COM1",baud=115200,parity="N",stopbits=1,slave=1,
                timeout_ms=10,duration_s=3600,poll_interval_s=0,
                max_retries=0,retry_delay_s=0)
            with mock.patch.object(r5_beta_hw,"SerialTransport",Transport), \
                 mock.patch.object(r5_beta_hw,"ModbusClient",lambda *_:object()), \
                 mock.patch.object(r5_beta_hw,"read_state",
                    side_effect=[base,second,KeyboardInterrupt()]):
                self.assertEqual(r5_beta_hw.record(args),130)
            summary=json.loads((Path(args.output)/"summary.json").read_text())
            self.assertEqual(summary["status"],"INTERRUPTED")
            self.assertEqual(summary["records"],2)
            self.assertEqual(summary["unobserved_device_sequences"],1)
            self.assertGreater(summary["samples_length"],0)
            events=(Path(args.output)/"events.jsonl").read_text()
            self.assertIn("MODE_CHANGE",events)
            self.assertIn("HOST_INTERRUPT",events)


if __name__ == "__main__": unittest.main()

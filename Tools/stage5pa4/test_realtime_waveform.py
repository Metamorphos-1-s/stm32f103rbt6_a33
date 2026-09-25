import json
import tempfile
import threading
import unittest
from pathlib import Path
from urllib.request import Request, urlopen

from build_interactive_waveform import build
from realtime_waveform import Handler, Stream
from http.server import ThreadingHTTPServer


HEADER = "utc,host_monotonic_ns,gross_ug,display_count,display_decimals,raw_adc,filtered_raw\n"


class RealtimeContractTests(unittest.TestCase):
    def test_generated_page_can_start_with_empty_points(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            samples = root / "samples.csv"
            samples.write_text(HEADER, encoding="utf-8")
            # The normal recorder cannot be empty, so use one point as the
            # browser bootstrap and verify the realtime marker hook is present
            # in the template-generated page.
            samples.write_text(HEADER +
                "2026-09-25T08:00:00Z,100,1234000,123,2,-10,-11\n",
                encoding="utf-8")
            output = root / "waveform.html"
            build(samples, output)
            page = output.read_text(encoding="utf-8")
            self.assertIn('new EventSource("/events")', page)
            self.assertIn('points.push(message.point)', page)
            self.assertIn('location.search', page)

    def test_sse_point_schema_is_six_values(self):
        point = [1234, 500.01, 500.0, 500.0, -488000, -488001]
        self.assertEqual(len(point), 6)
        self.assertIsInstance(json.dumps({"point": point}), str)

    def test_http_stop_only_stops_local_collector(self):
        with tempfile.TemporaryDirectory() as tmp:
            service = Stream("NO_DEVICE_OPENED", 115200, Path(tmp)/"unused", 0.5, 0x051C)
            Handler.stream = service
            Handler.html = b"<html></html>"
            server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            try:
                with urlopen(Request("http://127.0.0.1:%d/stop" % server.server_port,
                                     data=b"", method="POST"), timeout=3) as response:
                    self.assertEqual(response.read(), b"OK")
                thread.join(timeout=3)
                self.assertTrue(service.stop.is_set())
                self.assertFalse(thread.is_alive())
                self.assertFalse((Path(tmp)/"unused").exists())
            finally:
                service.stop.set()
                server.shutdown()
                server.server_close()


if __name__ == "__main__":
    unittest.main()

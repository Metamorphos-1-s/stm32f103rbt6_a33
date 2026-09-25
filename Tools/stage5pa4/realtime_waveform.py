#!/usr/bin/env python3
"""Read-only COM5/Modbus streamer and local SSE waveform server."""

import argparse
import csv
import json
import queue
import threading
import time
from collections import deque
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parents[2]
import sys
sys.path.insert(0, str(ROOT / "Tools" / "stage5b_hw"))
sys.path.insert(0, str(ROOT / "Tools" / "stage5mr5b_beta"))
sys.path.insert(0, str(ROOT / "Tools" / "stage5na"))
from hw_common import ModbusClient  # noqa: E402
from serial_transport import SerialTransport  # noqa: E402
from r5_beta_hw import BETA_COUNT, BETA_FIRST, decode_beta  # noqa: E402
from stage5na2_hw import decode_display, decode_primary  # noqa: E402


class Stream:
    def __init__(self, port, baud, output, poll_s, expected_firmware):
        self.port = port
        self.baud = baud
        self.output = Path(output)
        self.poll_s = poll_s
        self.expected_firmware = expected_firmware
        self.clients = set()
        self.lock = threading.Lock()
        self.stop = threading.Event()
        self.ready = threading.Event()
        self.error = None
        self.history = deque(maxlen=50000)
        self.started_ns = None
        self.base_utc_ms = None
        self.transport = None

    def add_client(self):
        q = queue.Queue(maxsize=1024)
        with self.lock: self.clients.add(q)
        return q

    def remove_client(self, q):
        with self.lock: self.clients.discard(q)

    def broadcast(self, event, payload):
        message = "event: %s\ndata: %s\n\n" % (event, json.dumps(payload, ensure_ascii=False))
        with self.lock:
            for q in list(self.clients):
                try: q.put_nowait(message)
                except queue.Full:
                    self.clients.discard(q)

    @staticmethod
    def utc_ms():
        return round(datetime.now(timezone.utc).timestamp() * 1000)

    def run(self):
        self.output.mkdir(parents=True, exist_ok=False)
        sample_path = self.output / "samples.csv"
        events_path = self.output / "events.jsonl"
        frames_path = self.output / "frames.jsonl"
        self.started_ns = time.monotonic_ns()
        self.base_utc_ms = self.utc_ms()
        with events_path.open("a", encoding="utf-8") as events:
            events.write(json.dumps({"utc": datetime.now(timezone.utc).isoformat(),
                "event":"RECORDER_STARTED", "read_only":True})+"\n")
        transport = SerialTransport(self.port, self.baud, "N", 1, 300,
                                    frame_logger=lambda line: self._frame(frames_path, line))
        self.transport = transport
        client = ModbusClient(transport, 1)
        order = "low" if client.read(0x0103, 1)[0][0] else "high"
        fieldnames = ["utc", "host_monotonic_ns", "firmware", "map", "gross_ug",
                      "display_count", "conditioned_display_ug", "display_anchor_ug",
                      "raw_adc", "filtered_raw", "sample_sequence", "mcu_uptime_ms",
                      "dirty", "revision", "saved_revision", "fault_mask", "overrun_count",
                      "application", "mode", "state", "limited", "offset_ug", "reference_ug",
                      "save_request_count_low"]
        with sample_path.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=fieldnames, lineterminator="\n")
            writer.writeheader()
            errors = 0
            records = 0
            try:
                while not self.stop.is_set():
                    cycle = time.monotonic()
                    try:
                        primary = decode_primary(client.read(0, 64)[0], order,
                                                 self.expected_firmware)
                        display = decode_display(client.read(0x01E0, 17)[0], order)
                        state = decode_beta(client.read(BETA_FIRST, BETA_COUNT)[0], order)
                        errors = 0
                    except Exception as exc:
                        errors += 1
                        with events_path.open("a", encoding="utf-8") as events:
                            events.write(json.dumps({"utc": datetime.now(timezone.utc).isoformat(),
                                "event":"READ_ERROR", "error":str(exc), "consecutive":errors})+"\n")
                        if errors >= 10:
                            self.broadcast("fatal", {"error":str(exc)})
                            self.stop.set()
                            break
                        time.sleep(1)
                        continue
                    now_utc = self.utc_ms()
                    point = [now_utc - self.base_utc_ms, primary["gross_ug"] / 1e6,
                             primary["display_count"] / (10 ** primary["display_decimals"]),
                             display["conditioned_display_ug"] / 1e6,
                             primary["raw_adc"], primary["filtered_raw"]]
                    row = {"utc": datetime.fromtimestamp(now_utc/1000, timezone.utc).isoformat(),
                           "host_monotonic_ns": time.monotonic_ns(), "firmware": primary["firmware"],
                           "map": primary["map"], "gross_ug": primary["gross_ug"],
                           "display_count": primary["display_count"],
                           "conditioned_display_ug": display["conditioned_display_ug"],
                           "display_anchor_ug": display["display_anchor_ug"],
                           "raw_adc": primary["raw_adc"], "filtered_raw": primary["filtered_raw"],
                           "sample_sequence": primary["sample_sequence"],
                           "mcu_uptime_ms": primary["mcu_uptime_ms"],
                           "dirty": primary["dirty"], "revision": primary["revision"],
                           "saved_revision": primary["saved_revision"],
                           "fault_mask": primary["fault_mask"], "overrun_count": primary["overrun_count"],
                           "application": state.get("application", ""), "mode": state.get("mode", ""),
                           "state": state.get("state", ""), "limited": state.get("limited", ""),
                           "offset_ug": state.get("offset_ug", ""),
                           "reference_ug": state.get("reference_ug", ""),
                           "save_request_count_low": state.get("save_request_count_low", "")}
                    writer.writerow(row); stream.flush()
                    records += 1
                    with self.lock:
                        self.history.append(point)
                    self.ready.set()
                    self.broadcast("message", {"point": point})
                    delay = self.poll_s - (time.monotonic() - cycle)
                    if delay > 0: time.sleep(delay)
            finally:
                transport.close()
                with events_path.open("a", encoding="utf-8") as events:
                    events.write(json.dumps({"utc": datetime.now(timezone.utc).isoformat(),
                        "event":"RECORDER_STOPPED", "records":records,
                        "consecutive_read_errors":errors})+"\n")

    @staticmethod
    def _frame(path, line):
        with path.open("a", encoding="utf-8") as stream:
            stream.write(json.dumps({"utc": datetime.now(timezone.utc).isoformat(),
                                     "frame": line}) + "\n")


class Handler(BaseHTTPRequestHandler):
    stream = None
    html = b""
    def do_GET(self):
        if urlparse(self.path).path in ("/", "/index.html") and "realtime=1" not in self.path:
            self.send_response(302); self.send_header("Location", "/?realtime=1"); self.end_headers(); return
        if urlparse(self.path).path == "/events":
            q = self.stream.add_client()
            self.send_response(200); self.send_header("Content-Type", "text/event-stream; charset=utf-8")
            self.send_header("Cache-Control", "no-cache"); self.send_header("Connection", "keep-alive"); self.end_headers()
            try:
                with self.stream.lock:
                    snapshot = list(self.stream.history)
                self.wfile.write(("event: init\ndata: %s\n\n" % json.dumps({
                    "baseUtcMs": self.stream.base_utc_ms, "port": self.stream.port,
                    "points": snapshot})).encode()); self.wfile.flush()
                while not self.stream.stop.is_set():
                    try: message = q.get(timeout=10)
                    except queue.Empty: message = ": keepalive\n\n"
                    self.wfile.write(message.encode()); self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError): pass
            finally: self.stream.remove_client(q)
            return
        if urlparse(self.path).path == "/status":
            with self.stream.lock:
                count = len(self.stream.history)
            payload = json.dumps({"port":self.stream.port,"running":not self.stream.stop.is_set(),
                                  "retained_points":count,"output":str(self.stream.output)}).encode("utf-8")
            self.send_response(200); self.send_header("Content-Type","application/json; charset=utf-8")
            self.send_header("Content-Length",str(len(payload))); self.end_headers();self.wfile.write(payload)
            return
        if urlparse(self.path).path in ("/", "/index.html"):
            self.send_response(200); self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Cache-Control", "no-cache"); self.end_headers(); self.wfile.write(self.html); return
        self.send_error(404)
    def do_POST(self):
        if urlparse(self.path).path != "/stop":
            self.send_error(404);return
        self.stream.stop.set()
        self.send_response(200); self.send_header("Content-Length", "2"); self.end_headers()
        self.wfile.write(b"OK")
        threading.Thread(target=self.server.shutdown, daemon=True).start()
    def log_message(self, *_): pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM5")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--poll-s", type=float, default=0.5)
    parser.add_argument("--http-port", type=int, default=8765)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--output", type=Path,
        help="new, non-existent session folder; default creates a UTC-stamped folder")
    parser.add_argument("--expected-firmware", type=lambda v: int(v, 0), default=0x051C)
    args = parser.parse_args()
    if args.output is None:
        args.output = ROOT / "Results/stage5pa4/realtime_runs" / (
            datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "_0x%04X" %
            args.expected_firmware)
    if args.output.exists():
        parser.error("output folder already exists; use a new --output to preserve prior CSV")
    template = (ROOT / "Tools/stage5pa4/interactive_waveform_template.html").read_text(encoding="utf-8")
    html = template.replace("__WAVEFORM_DATA__", json.dumps({"baseUtcMs":0,"points":[],"events":[],"missing":[]}))
    stream = Stream(args.port, args.baud, args.output, args.poll_s, args.expected_firmware)
    Handler.stream = stream; Handler.html = html.encode("utf-8")
    server = ThreadingHTTPServer((args.host, args.http_port), Handler)
    thread = threading.Thread(target=stream.run, daemon=True); thread.start()
    if not stream.ready.wait(timeout=8):
        stream.stop.set(); server.server_close()
        raise RuntimeError("first 0x051C read failed; see acquisition stderr")
    print("Open http://%s:%d/?realtime=1" % (args.host, args.http_port), flush=True)
    try: server.serve_forever()
    except KeyboardInterrupt: pass
    finally: stream.stop.set(); server.shutdown(); thread.join(timeout=8); server.server_close()


if __name__ == "__main__": main()

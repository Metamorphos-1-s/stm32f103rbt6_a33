"""pyserial transport with bounded RTU response reads and raw frame logging."""

import time
from dataclasses import dataclass


class SerialUnavailable(RuntimeError):
    pass


@dataclass
class Exchange:
    tx: bytes
    rx: bytes
    first_byte_ms: float = None
    complete_ms: float = None


def _serial_modules():
    try:
        import serial
        from serial.tools import list_ports
    except ImportError as exc:
        raise SerialUnavailable("install dependencies: pip install -r requirements.txt") from exc
    return serial, list_ports


def list_serial_ports():
    _, list_ports = _serial_modules()
    result = []
    for port in list_ports.comports():
        result.append({
            "device": port.device, "description": port.description,
            "hwid": port.hwid, "vid": port.vid, "pid": port.pid,
            "manufacturer": port.manufacturer,
            "serial_number": port.serial_number, "location": port.location,
        })
    return result


def select_serial_port(requested, ports=None, input_fn=input):
    if requested and requested.lower() != "auto":
        return requested
    candidates = list(list_serial_ports() if ports is None else ports)
    usb = [item for item in candidates if item.get("vid") is not None or
           "USB" in (item.get("hwid") or "").upper()]
    if not usb:
        raise SerialUnavailable(
            "NO SERIAL PORT FOUND: connect the USB-RS485 adapter and check "
            "Device Manager, its driver, and the USB cable")
    if len(usb) == 1:
        return usb[0]["device"]
    print("Multiple USB serial ports found:")
    for index, item in enumerate(usb, 1):
        print("%d: %s - %s" % (index, item["device"], item.get("description", "")))
    choice = input_fn("Select USB-RS485 port number: ").strip()
    try:
        selected = int(choice)
    except ValueError as exc:
        raise SerialUnavailable("invalid serial port selection") from exc
    if not 1 <= selected <= len(usb):
        raise SerialUnavailable("serial port selection out of range")
    return usb[selected - 1]["device"]


def character_times(baud, parity="N", stop_bits=1):
    bits = 1 + 8 + (0 if parity == "N" else 1) + stop_bits
    char_us = (bits * 1000000 + baud - 1) // baud
    if baud <= 19200:
        t1_5 = (bits * 1500000 + baud - 1) // baud
        t3_5 = (bits * 3500000 + baud - 1) // baud
    else:
        t1_5, t3_5 = 750, 1750
    return char_us, t1_5, t3_5


class SerialTransport:
    def __init__(self, port, baud=115200, parity="N", stop_bits=1,
                 timeout_ms=300, frame_logger=None, verbose=False):
        serial, _ = _serial_modules()
        parity_map = {"N": serial.PARITY_NONE, "E": serial.PARITY_EVEN,
                      "O": serial.PARITY_ODD}
        stop_map = {1: serial.STOPBITS_ONE, 2: serial.STOPBITS_TWO}
        if parity not in parity_map or stop_bits not in stop_map:
            raise ValueError("parity must be N/E/O and stop bits 1/2")
        self.serial = serial.Serial(port=port, baudrate=baud,
            bytesize=serial.EIGHTBITS, parity=parity_map[parity],
            stopbits=stop_map[stop_bits], timeout=timeout_ms / 1000.0,
            write_timeout=timeout_ms / 1000.0)
        self.timeout_s = timeout_ms / 1000.0
        self.t3_5_us = character_times(baud, parity, stop_bits)[2]
        self.frame_logger = frame_logger
        self.verbose = verbose
        self.last_activity_ns = 0

    def close(self):
        if self.serial and self.serial.is_open:
            self.serial.close()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    def reset(self):
        self.serial.reset_input_buffer()
        self.serial.reset_output_buffer()
        time.sleep(self.t3_5_us / 1000000.0)
        self.last_activity_ns = time.perf_counter_ns()

    def _wait_for_silence(self):
        if not self.last_activity_ns:
            return
        remaining_ns = self.t3_5_us * 1000 - (time.perf_counter_ns() - self.last_activity_ns)
        if remaining_ns > 0:
            time.sleep(remaining_ns / 1000000000.0)

    def _log(self, direction, data):
        encoded = bytes(data).hex(" ").upper()
        line = direction + ((" " + encoded) if encoded else "")
        if self.verbose:
            print(line)
        if self.frame_logger:
            self.frame_logger(line)

    def exchange(self, request, expect_response=True):
        request = bytes(request)
        self._wait_for_silence()
        self.serial.reset_input_buffer()
        self._log("TX", request)
        self.serial.write(request)
        self.serial.flush()
        start = time.perf_counter_ns()
        if not expect_response:
            data = self.serial.read(1)
            self.last_activity_ns = time.perf_counter_ns()
            self._log("RX", data)
            return Exchange(request, data)
        first = self.serial.read(1)
        if not first:
            self.last_activity_ns = time.perf_counter_ns()
            self._log("RX", b"")
            return Exchange(request, b"")
        first_ns = time.perf_counter_ns()
        second = self.serial.read(1)
        if not second:
            data = first
        else:
            function = second[0]
            if function & 0x80:
                data = first + second + self.serial.read(3)
            elif function == 3:
                count = self.serial.read(1)
                data = first + second + count
                if count:
                    data += self.serial.read(count[0] + 2)
            elif function in (6, 16):
                data = first + second + self.serial.read(6)
            else:
                data = first + second + self.serial.read(254)
        complete_ns = time.perf_counter_ns()
        self.last_activity_ns = complete_ns
        self._log("RX", data)
        return Exchange(request, data, (first_ns - start) / 1e6,
                        (complete_ns - start) / 1e6)

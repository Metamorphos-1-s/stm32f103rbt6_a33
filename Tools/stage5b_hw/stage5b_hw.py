#!/usr/bin/env python3
"""Stage 5B-H Windows hardware-integration command line."""

import argparse
import json
import subprocess
import sys
from pathlib import Path

import modbus_frame as mf
import register_map as reg
from hw_common import (HardwareTestError, ModbusClient, load_config,
                       probe_device, require_authorization)
from register_map_check import check as check_register_map
from report_writer import ReportWriter, environment_info, git_identity, sha256_file
from serial_transport import SerialTransport, list_serial_ports

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]


def add_common(parser, serial=True):
    parser.add_argument("--config")
    if serial:
        parser.add_argument("--port")
        parser.add_argument("--interface", choices=("rs232", "rs485"))
        parser.add_argument("--baud", type=int, choices=sorted(reg.BAUD_ENUM))
        parser.add_argument("--parity", choices=("N", "E", "O"))
        parser.add_argument("--stopbits", type=int, choices=(1, 2))
        parser.add_argument("--slave", type=int)
        parser.add_argument("--timeout-ms", type=int)
    parser.add_argument("--json-report", nargs="?", const="")
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--no-color", action="store_true")
    parser.add_argument("--yes", action="store_true")
    for name in ("write", "actions", "flash", "comm-change", "factory-reset", "calibration"):
        parser.add_argument("--allow-" + name, action="store_true", default=None)
    parser.add_argument("--dangerous-power-loss-test", action="store_true", default=None)


def parser():
    result = argparse.ArgumentParser(description=__doc__)
    sub = result.add_subparsers(dest="command", required=True)
    p = sub.add_parser("list-ports"); add_common(p, False)
    p = sub.add_parser("flash"); add_common(p, False); p.add_argument("--elf"); p.add_argument("--programmer")
    p = sub.add_parser("dump-slots"); add_common(p, False); p.add_argument("--output-directory", required=True); p.add_argument("--programmer")
    p = sub.add_parser("probe"); add_common(p)
    p = sub.add_parser("hf2-status"); add_common(p)
    p = sub.add_parser("read"); add_common(p); p.add_argument("address", type=lambda x: int(x, 0)); p.add_argument("quantity", type=int)
    p = sub.add_parser("write-single"); add_common(p); p.add_argument("address", type=lambda x: int(x, 0)); p.add_argument("value", type=lambda x: int(x, 0))
    p = sub.add_parser("write-multiple"); add_common(p); p.add_argument("address", type=lambda x: int(x, 0)); p.add_argument("values", nargs="+", type=lambda x: int(x, 0))
    for name in ("smoke", "rs232", "rs485"):
        p = sub.add_parser(name); add_common(p); p.add_argument("--count", type=int, default=100); p.add_argument("--interval-ms", type=int)
        if name != "smoke":
            p.add_argument("--include-errors", action="store_true",
                           help="include guarded malformed FC06/FC16 tests")
    p = sub.add_parser("errors"); add_common(p)
    p = sub.add_parser("commands"); add_common(p); p.add_argument("--name", default="NOP", choices=sorted(reg.COMMANDS)); p.add_argument("--repeat-token", action="store_true"); p.add_argument("--arg0", type=lambda x: int(x, 0), default=0); p.add_argument("--arg1", type=lambda x: int(x, 0), default=0)
    p = sub.add_parser("config-apply"); add_common(p)
    p.add_argument("--communication", action="store_true")
    p.add_argument("--new-baud", type=int, choices=sorted(reg.BAUD_ENUM))
    p.add_argument("--new-parity", choices=("N", "E", "O"))
    p.add_argument("--new-stopbits", type=int, choices=(1, 2))
    p.add_argument("--new-slave", type=int)
    p = sub.add_parser("save"); add_common(p); p.add_argument("--manual-power-cycle", action="store_true")
    p = sub.add_parser("soak"); add_common(p); p.add_argument("--duration-s", type=float, required=True); p.add_argument("--interval-ms", type=int); p.add_argument("--max-timeouts", type=int, default=0); p.add_argument("--max-crc-errors", type=int, default=0); p.add_argument("--timeout-retries", type=int, default=0); p.add_argument("--output-csv")
    p = sub.add_parser("full"); add_common(p); p.add_argument("--count", type=int, default=100); p.add_argument("--duration-s", type=float, default=60)
    p = sub.add_parser("compare-interfaces"); add_common(p, False); p.add_argument("--rs232-port"); p.add_argument("--rs485-port")
    return result


def merge_config(args):
    config = load_config(args.config)
    mapping = {"baud": "baud_rate", "stopbits": "stop_bits", "slave": "slave_address"}
    for cli, key in mapping.items():
        if hasattr(args, cli) and getattr(args, cli) is not None:
            config[key] = getattr(args, cli)
    for key in ("port", "interface", "parity", "timeout_ms", "allow_write",
                "allow_actions", "allow_flash", "allow_comm_change",
                "allow_factory_reset", "allow_calibration"):
        if hasattr(args, key) and getattr(args, key) is not None:
            config[key] = getattr(args, key)
    if hasattr(args, "interval_ms") and args.interval_ms is not None:
        config["poll_interval_ms"] = args.interval_ms
    if not config.get("port") and config.get("interface"):
        config["port"] = config.get(config["interface"] + "_port")
    defaults = {"baud_rate": 115200, "parity": "N", "stop_bits": 1,
                "slave_address": 1, "timeout_ms": 300,
                "poll_interval_ms": 50, "interface": "unknown"}
    for key, value in defaults.items():
        config.setdefault(key, value)
    for key, value in config.items():
        setattr(args, key, value)
    if hasattr(args, "slave") and not 1 <= args.slave_address <= 247:
        raise HardwareTestError("slave address must be 1..247")
    return config


def make_report(args, config):
    enabled = args.command not in ("list-ports", "flash", "dump-slots") or args.json_report is not None
    report = ReportWriter(HERE / "reports", config.get("interface", "unknown"), enabled)
    if enabled:
        sanitized = {k: (Path(str(v)).name if any(word in k.lower() for word in ("elf", "path", "programmer")) else v)
                     for k, v in config.items() if "password" not in k.lower()}
        report.write_json("configuration.json", sanitized)
        port_info = None
        if config.get("port"):
            try:
                port_info = next((item for item in list_serial_ports()
                                  if item["device"].upper() == config["port"].upper()), None)
            except Exception:
                pass
        report.write_json("environment.json", environment_info(port_info))
        firmware = git_identity(ROOT)
        elf = config.get("firmware_elf")
        if elf:
            path = (HERE / elf).resolve() if not Path(elf).is_absolute() else Path(elf)
            firmware.update({"elf": path.name, "elf_sha256": sha256_file(path) if path.exists() else None})
        report.write_json("firmware.json", firmware)
    return report


def open_client(args, report):
    if not args.port:
        raise HardwareTestError("no serial port selected; use --port or a JSON port alias")
    interactive_frames = args.command in ("probe", "read", "write-single",
        "write-multiple", "errors", "commands", "config-apply", "save",
        "hf2-status")
    transport = SerialTransport(args.port, args.baud_rate, args.parity,
        args.stop_bits, args.timeout_ms, report.raw,
        args.verbose or interactive_frames)
    transport.reset()
    return transport, ModbusClient(transport, args.slave_address)


def authorize(args, action, flag, description, confirmation):
    require_authorization(args, action, flag, description, confirmation)


def authorize_raw_write(args, start, count):
    end = start + count - 1
    if end > 0xFFFF:
        raise HardwareTestError("write address range exceeds 0xFFFF")
    if start <= reg.EXECUTE <= end:
        raise HardwareTestError("raw mailbox EXECUTE is prohibited; use the commands subcommand")
    if start <= reg.COMM_BROADCAST_POLICY and end >= reg.COMM_ADDRESS:
        authorize(args, "communication_change", "allow_comm_change",
                  "raw write intersects communication parameters", "COMM_CHANGE")
    if start <= 0x019F and end >= reg.CALIBRATION_FIRST:
        authorize(args, "calibration", "allow_calibration",
                  "raw write intersects calibration registers", "CALIBRATION")


def run_communication_change(args, report, transport, client):
    import time
    old = client.read(reg.COMMUNICATION_FIRST, 9)[0]
    old_baud = {value: key for key, value in reg.BAUD_ENUM.items()}[old[2]]
    old_parity = {value: key for key, value in reg.PARITY_ENUM.items()}[old[3]]
    target = {
        "slave": args.new_slave or old[1],
        "baud": args.new_baud or old_baud,
        "parity": args.new_parity or old_parity,
        "stopbits": args.new_stopbits or old[4],
    }
    if not 1 <= target["slave"] <= 247:
        raise HardwareTestError("new slave address must be 1..247")
    if (target["slave"], target["baud"], target["parity"], target["stopbits"]) == \
            (old[1], old_baud, old_parity, old[4]):
        raise HardwareTestError("new communication parameters equal current parameters")

    def request_apply(active_client, values, token):
        active_client.write_multiple(reg.COMM_ADDRESS, values)
        active_client.write_multiple(reg.REQUEST_TOKEN,
                                     [token, reg.COMMANDS["COMMUNICATION_APPLY"]])
        # The FC06 acknowledgement is deliberately the final request at the old settings.
        active_client.write_single(reg.EXECUTE, reg.EXECUTE_VALUE)

    target_values = [target["slave"], reg.BAUD_ENUM[target["baud"]],
                     reg.PARITY_ENUM[target["parity"]], target["stopbits"]]
    token = int(time.time() * 1000) & 0xFFFF or 1
    request_apply(client, target_values, token)
    transport.close()
    time.sleep(0.1)
    new_transport = None
    try:
        new_transport = SerialTransport(args.port, target["baud"], target["parity"],
            target["stopbits"], args.timeout_ms, report.raw, args.verbose)
        new_transport.reset()
        new_client = ModbusClient(new_transport, target["slave"])
        changed = probe_device(new_client)
        report.add("communication apply", "PASS", "probe succeeded at new parameters", target)
        restore_values = [old[1], old[2], old[3], old[4]]
        request_apply(new_client, restore_values, (token + 1) & 0xFFFF or 1)
    except Exception as change_error:
        if new_transport: new_transport.close()
        # Bounded recovery: only the recorded old and explicitly requested new settings.
        for candidate in ({"slave": old[1], "baud": old_baud, "parity": old_parity,
                           "stopbits": old[4]}, target):
            recovery = None
            recovered = False
            try:
                recovery = SerialTransport(args.port, candidate["baud"], candidate["parity"],
                    candidate["stopbits"], args.timeout_ms, report.raw, args.verbose)
                recovery.reset()
                recovery_client = ModbusClient(recovery, candidate["slave"])
                probe_device(recovery_client)
                if candidate == target:
                    request_apply(recovery_client, [old[1], old[2], old[3], old[4]],
                                  (token + 2) & 0xFFFF or 1)
                    report.add("communication recovery", "PASS",
                               "target settings found and old settings requested")
                else:
                    report.add("communication recovery", "PASS", "device still at old settings")
                recovered = True
            except Exception:
                pass
            finally:
                if recovery: recovery.close()
            if recovered:
                raise HardwareTestError("communication change failed but recovery path responded: %s" % change_error)
        raise HardwareTestError("communication recovery failed; power-cycle to restore unsaved settings: %s" % change_error)
    finally:
        if new_transport: new_transport.close()
    time.sleep(0.1)
    restored_transport = SerialTransport(args.port, old_baud, old_parity, old[4],
        args.timeout_ms, report.raw, args.verbose)
    try:
        restored_transport.reset()
        restored = probe_device(ModbusClient(restored_transport, old[1]))
        report.add("communication restore", "PASS", "old parameters restored; SAVE not issued",
                   {"old": {"slave": old[1], "baud": old_baud,
                            "parity": old_parity, "stopbits": old[4]},
                    "new_probe": changed, "restored_probe": restored})
    finally:
        restored_transport.close()
    return True


def run_serial(args, report):
    import soak_test, test_commands, test_config_apply, test_protocol_errors
    import test_rs232, test_rs485, test_smoke, test_storage
    transport, client = open_client(args, report)
    try:
        if args.command == "probe":
            value = probe_device(client); print(json.dumps(value, indent=2)); report.add("probe", "PASS", "read-only identity", value); return True
        if args.command == "hf2-status":
            from hf2_status import read_status
            value = read_status(client); print(json.dumps(value, indent=2))
            report.add("HF2-R1 status", "PASS", "read-only telemetry", value)
            return True
        if args.command == "read":
            values, _ = client.read(args.address, args.quantity); print("PDU 0x%04X / PLC %d: %s" % (args.address, reg.plc_address(args.address), values)); return True
        if args.command == "write-single":
            authorize(args, "write_single", "allow_write", "FC06 PDU 0x%04X = 0x%04X" % (args.address, args.value), "WRITE_SINGLE")
            authorize_raw_write(args, args.address, 1)
            client.write_single(args.address, args.value); return True
        if args.command == "write-multiple":
            authorize(args, "write_multiple", "allow_write", "FC16 PDU 0x%04X, %d registers" % (args.address, len(args.values)), "WRITE_MULTIPLE")
            authorize_raw_write(args, args.address, len(args.values))
            client.write_multiple(args.address, args.values); return True
        interval = args.poll_interval_ms / 1000.0
        if args.command == "smoke": return test_smoke.run(client, report, args.count, interval)
        if args.command == "rs232":
            if args.include_errors:
                authorize(args, "protocol_writes", "allow_write",
                          "RS232 malformed FC06/FC16 regression", "PROTOCOL_ERRORS")
            return test_rs232.run(client, report, args.count, interval, args.include_errors)
        if args.command == "rs485":
            if args.include_errors:
                authorize(args, "protocol_writes", "allow_write",
                          "RS485 malformed FC06/FC16 regression", "PROTOCOL_ERRORS")
            return test_rs485.run(client, report, args.count, interval, args.include_errors)
        if args.command == "errors":
            authorize(args, "protocol_writes", "allow_write", "non-persistent malformed FC06/FC16 regression", "PROTOCOL_ERRORS")
            return test_protocol_errors.run(client, report)
        if args.command == "commands":
            authorize(args, "mailbox_write", "allow_write", "mailbox command %s" % args.name, "MAILBOX_WRITE")
            if args.name != "NOP": authorize(args, "actions", "allow_actions", "runtime action %s" % args.name, "RUN_ACTION")
            if "FACTORY" in args.name: authorize(args, "factory_reset", "allow_factory_reset", "factory reset", "FACTORY_RESET")
            if "CALIBRATION" in args.name: authorize(args, "calibration", "allow_calibration", "calibration command", "CALIBRATION")
            if args.name == "REQUEST_SAVE": authorize(args, "save", "allow_flash", "persistent SAVE", "SAVE_FLASH")
            if args.name == "COMMUNICATION_APPLY": authorize(args, "communication_change", "allow_comm_change", "communication apply", "COMM_CHANGE")
            return test_commands.run(client, report, args.name,
                                     args.repeat_token, args.arg0, args.arg1)
        if args.command == "config-apply":
            authorize(args, "config_apply", "allow_write", "temporary RAM brightness change and restore", "CONFIG_APPLY")
            if args.communication:
                authorize(args, "communication_change", "allow_comm_change",
                          "temporary communication parameter change and restore", "COMM_CHANGE")
                return run_communication_change(args, report, transport, client)
            return test_config_apply.run(client, report)
        if args.command == "save":
            authorize(args, "save", "allow_flash", "persistent SAVE (Flash erase/program possible)", "SAVE_FLASH")
            authorize(args, "mailbox_write", "allow_write", "mailbox REQUEST_SAVE", "MAILBOX_WRITE")
            if args.dangerous_power_loss_test:
                authorize(args, "power_loss", "dangerous_power_loss_test", "manual interruption during Flash write", "POWER_LOSS_TEST")
            return test_storage.run(client, report, args.manual_power_cycle,
                                    bool(args.dangerous_power_loss_test))
        if args.command == "soak":
            return soak_test.run(client, report, args.duration_s, args.poll_interval_ms,
                                 args.max_timeouts, args.max_crc_errors, args.output_csv,
                                 args.timeout_retries)
        if args.command == "full":
            if not test_smoke.run(client, report, args.count, interval): return False
            if args.allow_write:
                authorize(args, "protocol_writes", "allow_write", "non-persistent protocol write regression", "PROTOCOL_ERRORS")
                if not test_protocol_errors.run(client, report): return False
                authorize(args, "config_apply", "allow_write", "temporary RAM brightness test", "CONFIG_APPLY")
                if not test_config_apply.run(client, report): return False
                if args.allow_actions:
                    authorize(args, "mailbox_write", "allow_write", "NOP token deduplication", "MAILBOX_WRITE")
                    authorize(args, "actions", "allow_actions", "mailbox action suite", "RUN_ACTION")
                    if not test_commands.run(client, report, "NOP", True): return False
                else:
                    report.add("command action suite", "SKIPPED", "--allow-actions not supplied")
                if args.allow_flash:
                    authorize(args, "save", "allow_flash", "persistent SAVE", "SAVE_FLASH")
                    if not test_storage.run(client, report, False): return False
                else:
                    report.add("SAVE suite", "SKIPPED", "--allow-flash not supplied")
            else:
                report.add("write suites", "SKIPPED", "--allow-write not supplied")
            report.add("communication switch suite", "SKIPPED",
                       "run config-apply --communication with explicit new parameters")
            return soak_test.run(client, report, args.duration_s, args.poll_interval_ms, 0, 0)
    finally:
        transport.close()


def main(argv=None):
    args = parser().parse_args(argv)
    report = None
    try:
        config = merge_config(args)
        if args.command == "list-ports":
            print(json.dumps(list_serial_ports(), indent=2)); return 0
        if args.command == "flash":
            authorize(args, "flash_firmware", "allow_flash", "program, verify and reset ELF over SWD", "FLASH_STAGE5B")
            elf = args.elf or config.get("firmware_elf")
            if not elf: raise HardwareTestError("no ELF specified")
            if not args.elf and not Path(elf).is_absolute():
                elf = str((HERE / elf).resolve())
            command = ["powershell", "-ExecutionPolicy", "Bypass", "-File", str(HERE / "flash_firmware.ps1"), "-Elf", str(elf), "-AllowFlash", "-Yes"]
            if args.programmer: command += ["-Programmer", args.programmer]
            return subprocess.call(command, cwd=str(ROOT))
        if args.command == "dump-slots":
            command = ["powershell", "-ExecutionPolicy", "Bypass", "-File", str(HERE / "dump_config_flash.ps1"), "-OutputDirectory", args.output_directory]
            if args.programmer: command += ["-Programmer", args.programmer]
            return subprocess.call(command, cwd=str(ROOT))
        if args.command == "compare-interfaces":
            values = []
            for interface in ("rs232", "rs485"):
                args.interface = interface; config["interface"] = interface
                args.port = getattr(args, interface + "_port") or config.get(interface + "_port")
                input("Set hardware to %s, close other adapter, then press Enter: " % interface.upper())
                report = make_report(args, config)
                transport, client = open_client(args, report)
                try: values.append(probe_device(client))
                finally: transport.close(); report.finish()
            keys = ("register_map_version", "firmware_version", "schema_version")
            ok = all(values[0][key] == values[1][key] for key in keys)
            print(json.dumps({"match": ok, "rs232": values[0], "rs485": values[1]}, indent=2)); return 0 if ok else 2
        check_register_map()
        report = make_report(args, config)
        ok = run_serial(args, report)
        return 0 if ok else 2
    except KeyboardInterrupt:
        print("Interrupted; serial port closed.", file=sys.stderr); return 130
    except Exception as exc:
        if report: report.add("execution", "FAIL", str(exc))
        print("ERROR: %s" % exc, file=sys.stderr); return 2
    finally:
        if report:
            directory = report.finish()
            if directory: print("Report: %s" % directory)
            if args.json_report:
                Path(args.json_report).write_text(json.dumps(report.results, indent=2,
                    ensure_ascii=False), encoding="utf-8")


if __name__ == "__main__":
    raise SystemExit(main())

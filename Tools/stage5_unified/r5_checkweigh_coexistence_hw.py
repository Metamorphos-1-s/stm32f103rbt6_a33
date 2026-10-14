"""Historical failed 0x052A probe. Do not run; use coexistence_focus.py.

This first attempt reused response tokens and decoded the wrong R5 generation
registers. Its original raw evidence is preserved only for failure analysis.
"""
import json, time
from pathlib import Path
from hardware_shadow import SerialTransport, ModbusClient, probe
from hw_common import execute_command

OUT = Path("Results/stage5p_integrated/hardware/r5_checkweigh_coexistence_20261014.json")

def w64(value):
    value &= (1 << 64) - 1
    return [(value >> shift) & 0xffff for shift in (48, 32, 16, 0)]

def read_blocks(client):
    alarm, _ = client.read(0x220, 20)
    check, _ = client.read(0x2c0, 8)
    r5, _ = client.read(0x280, 40)
    return {"alarm": alarm, "checkweigh": check, "r5": r5}

def command(client, token, command_id, arg0=0, arg1=0, arg64=0, flags=0):
    result = execute_command(client, token, command_id, arg0=arg0,
                             arg1=arg1, arg64=arg64, flags=flags,
                             timeout_s=3.0)
    return result

def main():
    raise SystemExit("obsolete failed probe; use coexistence_focus.py")
    events = []
    with SerialTransport("COM5", 115200, "N", 1, 350) as port:
        client = ModbusClient(port, 1)
        events.append({"kind": "PRE", "utc": time.time(),
                       "probe": probe(client), "blocks": read_blocks(client)})
        # Begin complete RAM config transaction, then enable formal limits only.
        events.append({"kind": "CONFIG_BEGIN", "result": command(client, 3001, 9)})
        client.write_single(0x0240, 1)  # limit function enable
        client.write_single(0x0241, 0)  # net source
        client.write_multiple(0x0242, w64(-100000))
        client.write_multiple(0x0246, w64(100000))
        client.write_multiple(0x024A, w64(10000))
        client.write_single(0x024E, 0)
        client.write_single(0x024F, 0)
        client.write_single(0x0250, 0)
        events.append({"kind": "CONFIG_VALIDATE", "result": command(client, 3002, 10)})
        events.append({"kind": "CONFIG_APPLY_RAM", "result": command(client, 3003, 11),
                       "probe": probe(client), "blocks": read_blocks(client)})
        # Enable STATIC formal checkweigh while R5 remains ACTIVE+STATIC.
        check = read_blocks(client)["checkweigh"]
        check_generation = (check[2] << 16) | check[3]
        events.append({"kind": "CHECKWEIGH_STATIC", "generation": check_generation,
                       "result": command(client, 3010, 34, arg0=1,
                                          arg1=check_generation, flags=1)})
        time.sleep(1.0)
        events.append({"kind": "STATIC_OBSERVE", "probe": probe(client),
                       "blocks": read_blocks(client)})
        # Switch R5 to DOSING, then switch formal checkweigh to DYNAMIC.
        r5 = read_blocks(client)["r5"]
        r5_generation = (r5[33] << 16) | r5[34]
        events.append({"kind": "R5_DOSING", "generation": r5_generation,
                       "result": command(client, 3020, 36, arg0=1,
                                          arg1=1, arg64=r5_generation, flags=1)})
        time.sleep(0.5)
        events.append({"kind": "DOSING_STATIC_PENDING", "probe": probe(client),
                       "blocks": read_blocks(client)})
        check = read_blocks(client)["checkweigh"]
        check_generation = (check[2] << 16) | check[3]
        events.append({"kind": "CHECKWEIGH_DYNAMIC", "generation": check_generation,
                       "result": command(client, 3030, 34, arg0=2,
                                          arg1=check_generation, flags=1)})
        time.sleep(1.0)
        events.append({"kind": "DOSING_DYNAMIC", "probe": probe(client),
                       "blocks": read_blocks(client)})
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(events, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(events[-4:], indent=2))

if __name__ == "__main__":
    main()

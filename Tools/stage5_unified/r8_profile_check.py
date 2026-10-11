import json
import time
from hardware_shadow import SerialTransport, ModbusClient
from hw_common import execute_command

out = []
with SerialTransport('COM5', 115200, 'N', 1, 350) as port:
    client = ModbusClient(port, 1)
    for profile in (0, 1, 0):
        response = execute_command(client, 5000 + profile, 7, arg0=profile)
        time.sleep(2)
        diag, _ = client.read(0x20, 28)
        config, _ = client.read(0x100, 64)
        base = 0x20 if config[0x1f] else 0x2e
        out.append(dict(
            requested_profile=profile,
            response=response,
            active_profile=config[0x1f],
            sample_rate=config[base],
            filter_mode=config[base + 2],
            filter_strength=config[base + 3],
            sample_sequence=(diag[0] << 16) | diag[1],
            overrun=(diag[13] << 16) | diag[14],
            dirty=diag[18],
            revision=(diag[19] << 16) | diag[20],
            saved_revision=(diag[21] << 16) | diag[22]))
print(json.dumps(out, indent=2))

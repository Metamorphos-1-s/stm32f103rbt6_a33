import json
import time
from hardware_shadow import SerialTransport, ModbusClient, probe

with SerialTransport('COM5', 115200, 'N', 1, 350) as port:
    client = ModbusClient(port, 1)
    for _ in range(5):
        print(json.dumps(probe(client), indent=2))
        time.sleep(1)

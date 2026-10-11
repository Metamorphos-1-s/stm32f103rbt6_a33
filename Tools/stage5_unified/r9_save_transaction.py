"""Guarded ACTIVE+STATIC request and one-shot SAVE transaction."""
import json
import time
from pathlib import Path
from hardware_shadow import SerialTransport, ModbusClient, now, unsigned32
from hw_common import execute_command


class TransactionError(RuntimeError):
    pass


def read_state(client):
    diag, _ = client.read(0x20, 28)
    active, _ = client.read(0x340, 40)
    check, _ = client.read(0x2c0, 8)
    def u32(index):
        return (active[index] << 16) | active[index + 1]
    return dict(
        generation=unsigned32(active[2:4], 'high'),
        application=u32(8), mode=u32(34), state=u32(36),
        offset=u32(38),
        revision=unsigned32(diag[19:21], 'high'),
        saved_revision=unsigned32(diag[21:23], 'high'),
        dirty=diag[18], fault=unsigned32(diag[25:27], 'high'),
        overrun=unsigned32(diag[13:15], 'high'), checkweigh=check[1])


def guarded_pair_and_save(client, output):
    events = []
    def record(kind, **values):
        events.append(dict(kind=kind, utc=now(), **values))

    before = read_state(client); record('BEFORE', state=before)
    # Pair mode 2 is STATIC; profile switching must already be complete.
    generation = before['generation']
    for attempt in range(2):
        response = execute_command(client, 7000 + attempt, 36, arg0=1,
                                   arg1=2, arg64=generation, flags=1)
        after = read_state(client)
        record('PAIR_ATTEMPT', attempt=attempt, generation=generation,
               response=response, state=after)
        if response['result'] == 0:
            deadline = time.monotonic() + 8.0
            while time.monotonic() < deadline:
                after = read_state(client)
                record('PAIR_COMPLETION_POLL', state=after)
                if after['application'] == 1 and after['mode'] == 2 and after['checkweigh'] == 0:
                    break
                time.sleep(0.2)
            else:
                raise TransactionError('pair OK but runtime state did not complete')
            break
        if response['result'] != 9:
            raise TransactionError('pair rejected: ' + str(response))
        generation = after['generation']
        time.sleep(1.0)
    else:
        raise TransactionError('pair remained BUSY after one retry')

    ready = read_state(client)
    record('SAVE_PRECONDITION', state=ready)
    if not (ready['application'] == 1 and ready['mode'] == 2 and
            ready['checkweigh'] == 0 and ready['dirty'] and ready['fault'] == 0 and
            ready['overrun'] == 0 and ready['revision'] > ready['saved_revision']):
        raise TransactionError('SAVE precondition mismatch')
    save = execute_command(client, 7100, 13)
    record('SAVE_REQUEST', response=save, state=read_state(client))
    if save['result'] != 1:
        raise TransactionError('SAVE was not accepted')
    deadline = time.monotonic() + 8.0
    while time.monotonic() < deadline:
        state = read_state(client)
        save_result, _ = client.read(0x1c5, 1)
        record('SAVE_POLL', state=state, save_result=save_result[0])
        if state['saved_revision'] == state['revision'] and not state['dirty']:
            break
        time.sleep(0.2)
    else:
        raise TransactionError('SAVE did not complete')
    output.write_text(json.dumps(dict(result='PASS', events=events), indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    with SerialTransport('COM5', 115200, 'N', 1, 350) as port:
        guarded_pair_and_save(ModbusClient(port, 1), Path('r9_save_transaction.json'))

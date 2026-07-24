"""Parse Stage 4B A/B records using explicit little-endian offsets."""

import argparse
import json
import struct
import zlib
from pathlib import Path

SLOT_SIZE = 2048
MAGIC = 0x41333343
FORMAT_VERSION = 1
HEADER_SIZE = 32
CRC_OFFSET = 20
COMMIT_OFFSET = 2044
COMMIT_MARKER = 0x434F4D54
PAYLOAD_LENGTHS = {1: 164, 2: 344}


def sequence_newer(candidate, reference):
    if candidate == reference or candidate == 0xFFFFFFFF or reference == 0xFFFFFFFF:
        return False
    difference = (candidate - reference) & 0xFFFFFFFF
    return 0 < difference < 0x80000000


def parse_slot(data, name="?"):
    data = bytes(data)
    if len(data) != SLOT_SIZE:
        raise ValueError("slot %s must be exactly %d bytes" % (name, SLOT_SIZE))
    magic, = struct.unpack_from("<I", data, 0)
    format_version, schema, header_size, payload_length = struct.unpack_from("<HHHH", data, 4)
    sequence, flags, stored_crc = struct.unpack_from("<III", data, 12)
    commit_marker, = struct.unpack_from("<I", data, COMMIT_OFFSET)
    supported = (schema in PAYLOAD_LENGTHS and
                 payload_length == PAYLOAD_LENGTHS[schema])
    bounds_ok = header_size == HEADER_SIZE and payload_length <= SLOT_SIZE - HEADER_SIZE - 4
    calculated_crc = None
    if bounds_ok:
        covered = data[:CRC_OFFSET] + data[CRC_OFFSET + 4:HEADER_SIZE] + \
                  data[HEADER_SIZE:HEADER_SIZE + payload_length]
        calculated_crc = zlib.crc32(covered) & 0xFFFFFFFF
    valid = (magic == MAGIC and format_version == FORMAT_VERSION and
             supported and bounds_ok and stored_crc == calculated_crc and
             commit_marker == COMMIT_MARKER)
    return {"slot": name, "magic": "0x%08X" % magic,
            "record_format_version": format_version,
            "payload_schema_version": schema, "header_size": header_size,
            "payload_length": payload_length, "sequence": sequence,
            "record_flags": "0x%08X" % flags,
            "stored_crc32": "0x%08X" % stored_crc,
            "calculated_crc32": None if calculated_crc is None else "0x%08X" % calculated_crc,
            "crc_valid": stored_crc == calculated_crc,
            "commit_marker": "0x%08X" % commit_marker,
            "committed": commit_marker == COMMIT_MARKER,
            "supported": supported, "valid": valid}


def parse_dump(path, modbus_active_slot=None, modbus_sequence=None):
    data = Path(path).read_bytes()
    if len(data) == SLOT_SIZE * 2:
        slots = [parse_slot(data[:SLOT_SIZE], "A"), parse_slot(data[SLOT_SIZE:], "B")]
    elif len(data) == SLOT_SIZE:
        slots = [parse_slot(data, Path(path).stem)]
    else:
        raise ValueError("dump must contain one 2048-byte slot or the 4096-byte region")
    active = None
    if len(slots) == 2:
        valid = [slot for slot in slots if slot["valid"]]
        if len(valid) == 1:
            active = valid[0]["slot"]
        elif len(valid) == 2:
            active = "B" if sequence_newer(valid[1]["sequence"], valid[0]["sequence"]) else "A"
    result = {"source": Path(path).name, "slots": slots, "active_slot": active}
    if modbus_active_slot is not None or modbus_sequence is not None:
        active_record = next((slot for slot in slots if slot["slot"] == active), None)
        result["modbus_comparison"] = {
            "active_slot": modbus_active_slot,
            "sequence": modbus_sequence,
            "active_slot_matches": modbus_active_slot is None or str(modbus_active_slot).upper() == str(active).upper(),
            "sequence_matches": modbus_sequence is None or
                (active_record is not None and modbus_sequence == active_record["sequence"]),
        }
    return result


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("dump")
    parser.add_argument("--json")
    parser.add_argument("--modbus-active-slot", choices=("A", "B"))
    parser.add_argument("--modbus-sequence", type=lambda value: int(value, 0))
    args = parser.parse_args(argv)
    result = parse_dump(args.dump, args.modbus_active_slot, args.modbus_sequence)
    output = json.dumps(result, indent=2)
    print(output)
    if args.json:
        Path(args.json).write_text(output + "\n", encoding="utf-8")
    return 0 if all(slot["valid"] for slot in result["slots"]) else 2


if __name__ == "__main__":
    raise SystemExit(main())

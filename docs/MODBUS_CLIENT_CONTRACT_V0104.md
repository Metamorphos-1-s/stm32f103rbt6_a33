# Modbus Client Contract 0x0104

PDU addresses are zero based. Default unit/slave is 1, TCP port 502, RTU
115200 8N1. Supported functions are FC03, FC06 and FC16. RTU ADUs are capped
at 256 bytes and use CRC low byte then high byte. TCP validates MBAP protocol
0, exact length, matching transaction ID and preserved unit ID. Each transport
allows one request in flight. PLC display address is `40001 + PDU address`.

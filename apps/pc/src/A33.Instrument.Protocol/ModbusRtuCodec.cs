namespace A33.Instrument.Protocol;

public static class ModbusRtuCodec
{
    public const int MaximumAduLength = 256;
    public static byte[] Encode(byte unitId, ReadOnlySpan<byte> pdu)
    {
        if (unitId is 0 or > 247) throw new ArgumentOutOfRangeException(nameof(unitId));
        if (pdu.Length is 0 or > 253) throw new ArgumentOutOfRangeException(nameof(pdu));
        var result = new byte[pdu.Length + 3]; result[0] = unitId; pdu.CopyTo(result.AsSpan(1)); ModbusCrc16.Append(result); return result;
    }
    public static ReadOnlyMemory<byte> Decode(ReadOnlySpan<byte> adu, byte expectedUnit, byte expectedFunction)
    {
        if (adu.Length < 5 || adu.Length > MaximumAduLength) throw new ModbusFrameException(ModbusFrameError.Length, "Invalid RTU length.");
        if (!ModbusCrc16.Verify(adu)) throw new ModbusFrameException(ModbusFrameError.Crc, "RTU CRC mismatch.");
        if (adu[0] != expectedUnit) throw new ModbusFrameException(ModbusFrameError.UnitId, "RTU slave mismatch.");
        var function = adu[1];
        if (function == (expectedFunction | 0x80)) { if (adu.Length != 5) throw new ModbusFrameException(ModbusFrameError.Length, "Invalid RTU exception length."); throw new ModbusExceptionResponse(expectedFunction, adu[2]); }
        if (function != expectedFunction) throw new ModbusFrameException(ModbusFrameError.Function, "RTU function mismatch.");
        return adu[1..^2].ToArray();
    }
}

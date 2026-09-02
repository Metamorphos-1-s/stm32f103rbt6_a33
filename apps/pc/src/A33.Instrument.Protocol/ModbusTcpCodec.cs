namespace A33.Instrument.Protocol;

public static class ModbusTcpCodec
{
    public static byte[] Encode(ushort transactionId, byte unitId, ReadOnlySpan<byte> pdu)
    {
        if (pdu.Length is 0 or > 253) throw new ArgumentOutOfRangeException(nameof(pdu));
        var result = new byte[7 + pdu.Length];
        result[0] = (byte)(transactionId >> 8); result[1] = (byte)transactionId;
        result[4] = (byte)((pdu.Length + 1) >> 8); result[5] = (byte)(pdu.Length + 1);
        result[6] = unitId; pdu.CopyTo(result.AsSpan(7)); return result;
    }
    public static (ushort Tid, byte Unit, ReadOnlyMemory<byte> Pdu) Decode(ReadOnlySpan<byte> adu, ushort expectedTid, byte? expectedUnit = null)
    {
        if (adu.Length < 8) throw new ModbusFrameException(ModbusFrameError.Length, "Short MBAP response.");
        var tid = (ushort)(adu[0] << 8 | adu[1]);
        if (tid != expectedTid) throw new ModbusFrameException(ModbusFrameError.TransactionId, "Transaction ID mismatch.");
        if (adu[2] != 0 || adu[3] != 0) throw new ModbusFrameException(ModbusFrameError.ProtocolId, "Protocol ID must be zero.");
        var length = (ushort)(adu[4] << 8 | adu[5]);
        if (length < 2 || length + 6 != adu.Length) throw new ModbusFrameException(ModbusFrameError.Length, "MBAP length mismatch.");
        if (expectedUnit.HasValue && adu[6] != expectedUnit.Value) throw new ModbusFrameException(ModbusFrameError.UnitId, "Unit ID mismatch.");
        return (tid, adu[6], adu[7..].ToArray());
    }
}

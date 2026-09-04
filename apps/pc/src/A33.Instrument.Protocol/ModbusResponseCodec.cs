namespace A33.Instrument.Protocol;

public readonly record struct ModbusException(byte Function, byte Code);

public static class ModbusResponseCodec
{
    public static ushort[] ReadHolding(ReadOnlySpan<byte> pdu)
    {
        if (pdu.Length >= 2 && pdu[0] == 0x83) throw new ModbusExceptionResponse(3, pdu[1]);
        if (pdu.Length < 3 || pdu[0] != 3) throw new ModbusFrameException(ModbusFrameError.Function, "Expected FC03 response.");
        if ((pdu[1] & 1) != 0) throw new ModbusFrameException(ModbusFrameError.ByteCount, "FC03 byte count must be even.");
        if (pdu[1] != pdu.Length - 2) throw new ModbusFrameException(ModbusFrameError.Length, "FC03 length mismatch.");
        var result = new ushort[pdu[1] / 2];
        for (var i = 0; i < result.Length; i++) result[i] = (ushort)(pdu[2 + 2 * i] << 8 | pdu[3 + 2 * i]);
        return result;
    }
    public static void ValidateWriteSingle(ReadOnlySpan<byte> pdu){if(pdu.Length!=5||pdu[0]!=6)throw new FormatException("invalid FC06 response");}
    public static (ushort Address, ushort Quantity) ValidateWriteMultiple(ReadOnlySpan<byte> pdu){if(pdu.Length!=5||pdu[0]!=16)throw new FormatException("invalid FC16 response");return ((ushort)(pdu[1]<<8|pdu[2]),(ushort)(pdu[3]<<8|pdu[4]));}
    public static ModbusException? Exception(ReadOnlySpan<byte> pdu)=>pdu.Length>=2&&(pdu[0]&0x80)!=0?new ModbusException((byte)(pdu[0]&0x7f),pdu[1]):null;
}

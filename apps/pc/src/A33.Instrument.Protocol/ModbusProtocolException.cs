namespace A33.Instrument.Protocol;

public enum ModbusFrameError { Crc, TransactionId, ProtocolId, Length, UnitId, Function, ByteCount, Malformed }

public sealed class ModbusFrameException(ModbusFrameError error, string message) : FormatException(message)
{
    public ModbusFrameError Error { get; } = error;
}

public sealed class ModbusExceptionResponse(byte function, byte exceptionCode)
    : Exception($"Modbus exception 0x{exceptionCode:X2} for function 0x{function:X2}")
{
    public byte Function { get; } = function;
    public byte ExceptionCode { get; } = exceptionCode;
}

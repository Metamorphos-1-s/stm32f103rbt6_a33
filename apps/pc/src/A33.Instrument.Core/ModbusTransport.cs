using A33.Instrument.Protocol;

namespace A33.Instrument.Core;

public sealed record ModbusExchangeResult(ReadOnlyMemory<byte> Pdu, byte[] RequestAdu, byte[] ResponseAdu);

public interface IModbusTransport : IAsyncDisposable
{
    bool IsOpen { get; }
    string Endpoint { get; }
    Task OpenAsync(CancellationToken cancellationToken);
    Task CloseAsync();
    Task<ModbusExchangeResult> ExchangeAsync(byte unitId, ReadOnlyMemory<byte> pdu, CancellationToken cancellationToken);
}

public sealed class ReadOnlyModbusClient(IModbusTransport transport, byte unitId, TimeSpan requestTimeout)
{
    private readonly SemaphoreSlim gate = new(1, 1);
    public ModbusExchangeResult? LastExchange { get; private set; }

    public async Task<ushort[]> ReadHoldingAsync(ushort address, ushort count, CancellationToken cancellationToken = default)
    {
        await gate.WaitAsync(cancellationToken);
        try
        {
            using var timeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
            timeout.CancelAfter(requestTimeout);
            var pdu = ModbusRequestCodec.ReadHoldingPdu(address, count);
            LastExchange = await transport.ExchangeAsync(unitId, pdu, timeout.Token);
            var values = ModbusResponseCodec.ReadHolding(LastExchange.Pdu.Span);
            if (values.Length != count) throw new ModbusFrameException(ModbusFrameError.ByteCount, $"Expected {count} registers, received {values.Length}.");
            return values;
        }
        finally { gate.Release(); }
    }
}

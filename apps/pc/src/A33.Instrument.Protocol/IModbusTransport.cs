namespace A33.Instrument.Protocol;
public interface IModbusTransport : IAsyncDisposable { bool IsOpen {get;} Task OpenAsync(CancellationToken cancellationToken); Task CloseAsync(); Task<ReadOnlyMemory<byte>> ExchangeAsync(ReadOnlyMemory<byte> request,CancellationToken cancellationToken); }

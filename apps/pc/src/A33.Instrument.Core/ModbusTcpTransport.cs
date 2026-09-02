using System.Net.Sockets;
using A33.Instrument.Protocol;

namespace A33.Instrument.Core;

public sealed class ModbusTcpTransport(string host, int port) : IModbusTransport
{
    private TcpClient? client; private NetworkStream? stream; private ushort transactionId;
    public bool IsOpen => client?.Connected == true;
    public string Endpoint => $"{host}:{port}";
    public async Task OpenAsync(CancellationToken cancellationToken)
    {
        await CloseAsync(); client = new TcpClient(); await client.ConnectAsync(host, port, cancellationToken); stream = client.GetStream();
    }
    public async Task<ModbusExchangeResult> ExchangeAsync(byte unitId, ReadOnlyMemory<byte> pdu, CancellationToken cancellationToken)
    {
        if (stream is null) throw new InvalidOperationException("TCP transport is closed.");
        var tid = unchecked(++transactionId); var request = ModbusTcpCodec.Encode(tid, unitId, pdu.Span);
        await stream.WriteAsync(request, cancellationToken);
        var header = new byte[6]; await ReadExactlyAsync(stream, header, cancellationToken);
        var length = header[4] << 8 | header[5];
        if (length is < 2 or > 254) throw new ModbusFrameException(ModbusFrameError.Length, "Invalid MBAP length.");
        var remainder = new byte[length]; await ReadExactlyAsync(stream, remainder, cancellationToken);
        var response = header.Concat(remainder).ToArray(); var decoded = ModbusTcpCodec.Decode(response, tid, unitId);
        return new ModbusExchangeResult(decoded.Pdu, request, response);
    }
    public Task CloseAsync(){stream?.Dispose();stream=null;client?.Dispose();client=null;return Task.CompletedTask;}
    public async ValueTask DisposeAsync()=>await CloseAsync();
    private static async Task ReadExactlyAsync(Stream source, Memory<byte> destination, CancellationToken token){var offset=0;while(offset<destination.Length){var count=await source.ReadAsync(destination[offset..],token);if(count==0)throw new EndOfStreamException("TCP connection closed during response.");offset+=count;}}
}

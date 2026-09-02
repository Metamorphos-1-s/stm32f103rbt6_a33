using System.IO.Ports;
using A33.Instrument.Protocol;

namespace A33.Instrument.Core;

public sealed class ModbusRtuTransport : IModbusTransport
{
    private readonly SerialPort? serialPort; private Stream? stream; private readonly bool ownsStream;private readonly TimeSpan interFrameDelay;
    public ModbusRtuTransport(string portName,int baudRate=115200,int dataBits=8,Parity parity=Parity.None,StopBits stopBits=StopBits.One)
    { serialPort=new SerialPort(portName,baudRate,parity,dataBits,stopBits);ownsStream=true;interFrameDelay=TimeSpan.FromMilliseconds(Math.Max(1,Math.Ceiling(38500d/baudRate)));Endpoint=$"{portName} {baudRate} {dataBits}{parity.ToString()[0]}{(stopBits==StopBits.One?1:2)}"; }
    public ModbusRtuTransport(Stream testStream,string endpoint="test-stream"){stream=testStream;Endpoint=endpoint;interFrameDelay=TimeSpan.Zero;}
    public bool IsOpen => serialPort?.IsOpen ?? stream is not null;
    public string Endpoint { get; }
    public Task OpenAsync(CancellationToken cancellationToken){cancellationToken.ThrowIfCancellationRequested();if(serialPort is not null&&!serialPort.IsOpen){serialPort.Open();stream=serialPort.BaseStream;}return Task.CompletedTask;}
    public async Task<ModbusExchangeResult> ExchangeAsync(byte unitId,ReadOnlyMemory<byte> pdu,CancellationToken cancellationToken)
    {
        if(stream is null)throw new InvalidOperationException("RTU transport is closed.");if(interFrameDelay>TimeSpan.Zero)await Task.Delay(interFrameDelay,cancellationToken);serialPort?.DiscardInBuffer();var request=ModbusRtuCodec.Encode(unitId,pdu.Span);await stream.WriteAsync(request,cancellationToken);await stream.FlushAsync(cancellationToken);
        var prefix=new byte[3];await ReadExactlyAsync(stream,prefix,cancellationToken);var function=prefix[1];int remainderLength;
        if(function==(pdu.Span[0]|0x80))remainderLength=2;else if(function==3)remainderLength=prefix[2]+2;else remainderLength=2;
        if(prefix.Length+remainderLength>ModbusRtuCodec.MaximumAduLength)throw new ModbusFrameException(ModbusFrameError.Length,"RTU response exceeds 256 bytes.");
        var remainder=new byte[remainderLength];await ReadExactlyAsync(stream,remainder,cancellationToken);var response=prefix.Concat(remainder).ToArray();var decoded=ModbusRtuCodec.Decode(response,unitId,pdu.Span[0]);return new ModbusExchangeResult(decoded,request,response);
    }
    public Task CloseAsync(){if(serialPort?.IsOpen==true)serialPort.Close();if(ownsStream)stream?.Dispose();stream=serialPort is null?stream:null;return Task.CompletedTask;}
    public async ValueTask DisposeAsync(){await CloseAsync();serialPort?.Dispose();}
    private static async Task ReadExactlyAsync(Stream source,Memory<byte> destination,CancellationToken token){var offset=0;while(offset<destination.Length){var count=await source.ReadAsync(destination[offset..],token);if(count==0)throw new EndOfStreamException("RTU response ended early.");offset+=count;}}
}

using A33.Instrument.Protocol;
namespace A33.Instrument.Core;
public sealed record WeightSnapshot(long DisplayMassUg,long NetMassUg,long GrossMassUg,long TareMassUg,bool Stable,bool Overload,uint Sequence);
public sealed record DeviceInfo(ushort FirmwareVersion,byte ProtocolVersion,ushort SchemaVersion,ushort RegisterMapVersion);
public sealed class DeviceRepository(ModbusClient client) { public async Task<ushort> ReadMapVersionAsync(CancellationToken ct=default)=>(await client.ReadHoldingAsync(0x000e,1,ct))[0]; public static long DecodeMass(ReadOnlySpan<ushort> words,WordOrder order=WordOrder.HighWordFirst)=>RegisterValueCodec.Int64(words,order); }

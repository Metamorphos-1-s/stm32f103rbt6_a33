using A33.Instrument.Protocol;
using System.Text.Json;
using Xunit;
namespace A33.Instrument.Protocol.Tests;
public class ProtocolTests {
 [Fact] public void CrcReference(){Assert.Equal((ushort)0x0bc4,ModbusCrc16.Compute(new byte[]{1,3,0,0,0,2}));}
 [Fact] public void Fc03RequestAndResponse(){var r=ModbusRequestCodec.ReadHolding(1,0,2);Assert.Equal(new byte[]{1,3,0,0,0,2},r);Assert.Equal(new ushort[]{0x0104},ModbusResponseCodec.ReadHolding(new byte[]{3,2,1,4}));}
 [Fact] public void Fc06AndFc16(){Assert.Equal(new byte[]{1,6,0,14,0xa5,0x5a},ModbusRequestCodec.WriteSingle(1,14,0xa55a));var m=ModbusRequestCodec.WriteMultiple(1,0x10,new ushort[]{0x0104,0});Assert.Equal((byte)16,m[1]);Assert.Equal((ushort)2,ModbusResponseCodec.ValidateWriteMultiple(new byte[]{16,0,16,0,0,2}));}
 [Theory][InlineData(1)][InlineData(2)][InlineData(3)][InlineData(4)][InlineData(6)] public void ExceptionCodes(byte code){var e=ModbusResponseCodec.Exception(new byte[]{0x83,code});Assert.True(e.HasValue);Assert.Equal(code,e.Value.Code);}
 [Fact] public void TcpMbapAndValidation(){var pdu=ModbusRequestCodec.ReadHolding(1,14,1);var adu=ModbusTcpCodec.Encode(1,1,pdu.AsSpan(1));Assert.Equal("0001000000060103000e0001",Convert.ToHexString(adu).ToLowerInvariant());var decoded=ModbusTcpCodec.Decode(adu,1);Assert.Equal((byte)1,decoded.Unit);Assert.Throws<FormatException>(()=>ModbusTcpCodec.Decode(adu,2));var bad=adu.ToArray();bad[2]=1;Assert.Throws<FormatException>(()=>ModbusTcpCodec.Decode(bad,1));}
 [Fact] public void SignedInt32AndInt64AndWordOrder(){Assert.Equal(-1,RegisterValueCodec.Int32(new ushort[]{0xffff,0xffff}));Assert.Equal(-1,RegisterValueCodec.Int64(new ushort[]{0xffff,0xffff,0xffff,0xffff}));Assert.Equal(0x0002000100040003,RegisterValueCodec.Int64(new ushort[]{3,4,1,2},WordOrder.LowWordFirst));}
 [Fact] public void RegisterMapVersionAndNoOverlap(){Assert.Equal((ushort)0x0104,RegisterMap.Version);var used=new HashSet<int>();foreach(var d in RegisterMap.Definitions)for(var i=0;i<d.RegisterCount;i++)Assert.True(used.Add(d.Address+i),$"overlap {d.Name}");Assert.DoesNotContain(RegisterMap.Definitions,d=>d.Name=="capacity_ug"&&d.Access=="write");var dir=new DirectoryInfo(AppContext.BaseDirectory);while(dir is not null&&!File.Exists(Path.Combine(dir.FullName,"contracts","modbus-v0104","register-map.json")))dir=dir.Parent;Assert.NotNull(dir);using var json=JsonDocument.Parse(File.ReadAllText(Path.Combine(dir!.FullName,"contracts","modbus-v0104","register-map.json")));Assert.Equal(32,json.RootElement.GetArrayLength());}
}

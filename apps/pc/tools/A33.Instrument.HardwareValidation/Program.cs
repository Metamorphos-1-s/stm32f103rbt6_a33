using System.Diagnostics;
using System.Text.Json;
using A33.Instrument.Core;

var arguments = args.Select((value,index)=>(value,index)).Where(x=>x.value.StartsWith("--")).ToDictionary(x=>x.value[2..],x=>x.index+1<args.Length&&!args[x.index+1].StartsWith("--")?args[x.index+1]:"true",StringComparer.OrdinalIgnoreCase);
string Get(string name,string fallback)=>arguments.GetValueOrDefault(name,fallback);
var mode=Get("mode","tcp").Equals("rtu",StringComparison.OrdinalIgnoreCase)?TransportMode.Rtu:TransportMode.Tcp;
var durationSeconds=int.Parse(Get("duration","10"));var output=Get("output","");
var options=new MonitoringOptions(mode,Get("host","192.168.1.100"),int.Parse(Get("port","502")),Get("com","COM5"),int.Parse(Get("baud","115200")),byte.Parse(Get("unit","1")),int.Parse(Get("connect-timeout","3000")),int.Parse(Get("request-timeout","1000")),int.Parse(Get("poll","200")));
await using var service=new InstrumentMonitoringService();var stopwatch=Stopwatch.StartNew();double maxStaleMs=0;var samples=0;string status="PASS";string? failure=null;var terminalMonitoringState=MonitoringConnectionState.Disconnected;
try
{
    await service.ConnectAsync(options);await service.StartMonitoringAsync();
    while(stopwatch.Elapsed<TimeSpan.FromSeconds(durationSeconds)){await Task.Delay(100);if(service.Snapshot is not null){samples++;maxStaleMs=Math.Max(maxStaleMs,(DateTimeOffset.Now-service.Snapshot.CapturedAt).TotalMilliseconds);}}
    terminalMonitoringState=service.State;if(terminalMonitoringState==MonitoringConnectionState.Faulted){status="FAIL";failure="Monitoring entered FAULTED after bounded reconnect attempts.";}
    await service.StopMonitoringAsync();
}
catch(Exception error){status="FAIL";failure=$"{error.GetType().Name}: {error.Message}";}
finally{await service.DisconnectAsync();}
var snapshot=service.Snapshot;var diagnostics=service.Diagnostics;
var summary=new{status,mode=mode.ToString(),endpoint=mode==TransportMode.Tcp?$"{options.Host}:{options.Port}":options.SerialPort,options.UnitId,start_time=DateTimeOffset.Now-stopwatch.Elapsed,end_time=DateTimeOffset.Now,duration_s=stopwatch.Elapsed.TotalSeconds,map_version=$"0x{service.MapVersion:X4}",word_order=service.WordOrder.ToString(),samples,max_stale_ms=maxStaleMs,tx_requests=diagnostics.TxRequests,rx_responses=diagnostics.RxResponses,timeouts=diagnostics.Timeouts,crc_errors=diagnostics.CrcErrors,mbap_errors=diagnostics.MbapErrors,unit_errors=diagnostics.UnitErrors,modbus_exceptions=diagnostics.Exceptions,bad_frames=diagnostics.BadFrames,transport_errors=diagnostics.TransportErrors,reconnects=diagnostics.Reconnects,terminal_monitoring_state=terminalMonitoringState.ToString(),final_state=service.State.ToString(),write_function_observed=false,last_request=diagnostics.LastRequestHex,last_response=diagnostics.LastResponseHex,snapshot=snapshot is null?null:new{snapshot.DisplayMassUg,snapshot.NetMassUg,snapshot.GrossMassUg,snapshot.TareMassUg,snapshot.Unit,snapshot.DecimalPlaces,snapshot.Stable,snapshot.Zero,snapshot.Overload,snapshot.CheckweighState,snapshot.Raw,snapshot.FilteredRaw,snapshot.Sequence},failure};
var json=JsonSerializer.Serialize(summary,new JsonSerializerOptions{WriteIndented=true});Console.WriteLine(json);if(!string.IsNullOrWhiteSpace(output))await File.WriteAllTextAsync(output,json);return status=="PASS"?0:1;

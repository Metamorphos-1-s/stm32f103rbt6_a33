# Stage 5P-A4 standalone waveform viewer

From the A4 worktree, regenerate the self-contained HTML:

```powershell
python Tools/stage5pa4/build_interactive_waveform.py
```

Open `Results/stage5pa4/interactive_waveform.html` in a local browser. It
includes 10,244 original samples and the separately labeled inferred edge
and human confirmation markers. No serial connection, web server, account or
online JavaScript library is required. The generator reads the CSV/JSONL and
event timeline but does **not** edit the original data.

Use the mouse wheel over the plot to zoom the time axis around the pointer,
hold the left button and drag horizontally to pan, or select a quick window
for the first/second unloaded interval or second 500 g constant-load interval.
“全时段” resets the entire view. The visible vertical ranges rescale
automatically; hover to see the nearest sample's UTC, authoritative gross
weight, quantized panel reading, display conditioner, raw ADC and filtered ADC
counts. Checkbox switches only affect display, never the underlying data.

The upper axis is in grams and the lower axis in ADC counts; they must not
be equated. The recorder has no independent filtered *weight* in micrograms,
so none is fabricated. The first placement edge was not captured. Marked
physical edges are brackets from approximately 0.5 s host polls, not exact
ADC-ready electrical timing; operator messages arrived later.

To visualize another CSV with the same schema:

```powershell
python Tools/stage5pa4/build_interactive_waveform.py --input path/to/samples.csv --events path/to/events.jsonl --timeline path/to/event_timeline.csv --output path/to/waveform.html
```

Offline validation:

```powershell
python Tools/stage5pa4/test_interactive_waveform.py
node Tools/stage5pa4/qa_interactive_waveform.cjs Results/stage5pa4/interactive_waveform.html
```

The optional browser QA command requires an installed Playwright module and
Microsoft Edge; the viewer itself has no such dependency.

## 实时模式

启动只读 COM5 采集器和本地页面：

```powershell
python Tools/stage5pa4/realtime_waveform.py --port COM5 --expected-firmware 0x051C
```

然后打开 `http://127.0.0.1:8765/?realtime=1`。实时服务通过 SSE 推送新
记录，页面默认跟随最近 5 分钟；用户滚轮缩放或拖动后会停止自动跟随，
点击“全时段”可恢复。采集同时写入
`Results/stage5pa4/realtime/samples.csv`、`events.jsonl` 和 `frames.jsonl`。

默认每次在 `Results/stage5pa4/realtime_runs/` 下创建新的 UTC 会话；若手动
传入 `--output`，目标必须不存在，防止覆盖上次 CSV。服务是只读的：如果
固件身份、Map 或 Modbus 读取异常，页面会显示连接状态，采集错误写入事件
文件，不会执行 SAVE、配置写入、TARE 或设备复位。`/status` 返回采集健康信息。
可按 Ctrl+C，或从另一个终端执行：

```powershell
Invoke-WebRequest -Method Post -Uri http://127.0.0.1:8765/stop
```

上述 `/stop` 只停止本地服务和串口采集，不向仪表发送指令；CSV 保留已采样
数据。浏览器关闭或暂停跟随也不会停止服务端采集。

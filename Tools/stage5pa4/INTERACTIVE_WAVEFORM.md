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

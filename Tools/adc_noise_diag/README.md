# ADC Noise Diagnostic Tools

These Python 3 tools capture deduplicated raw CS1237 samples through the
existing read-only Modbus register model, analyze one run, and compare layered
experiments. Install dependencies from `Tools/stage5b_hw/requirements.txt`.

```powershell
python Tools\adc_noise_diag\capture_adc_noise.py --help
python Tools\adc_noise_diag\analyze_adc_noise.py --help
python Tools\adc_noise_diag\compare_noise_runs.py --help
```

The capture script refuses product or mismatched diagnostic images. See
`Docs/CS1237_NOISE_DIAGNOSTIC_GUIDE.md` before changing any analog wiring.

Status: SOFTWARE IMPLEMENTED. NOT TESTED ON HARDWARE.

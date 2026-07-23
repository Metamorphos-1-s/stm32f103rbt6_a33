# Zero And Tare

Three concepts remain separate:

- Calibration zero is the permanent two-point model's `raw_zero`.
- Daily zero is a volatile raw-count offset and never changes calibration.
- Tare stores the current unquantized gross weight; net is gross minus tare.

Daily zero requires valid calibration, stability, no active tare, and a weight
inside `zero_range`. Tare requires valid calibration, stability, and no
overload. Repeated tare updates the stored value; clear tare restores net to
gross. Stage 3 does not persist daily zero or write tare to Flash.

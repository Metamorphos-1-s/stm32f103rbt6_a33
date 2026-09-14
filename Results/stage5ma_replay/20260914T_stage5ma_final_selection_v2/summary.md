# Stage 5M-A final selection v2

The four current hardware baselines reproduce exactly under the frozen metric sources. The robust dual-IIR fixed-point reference reaches 0.00544 g static noise and 0.319 s loading 10-90% on holdout, with Python/C zero mismatch over 900 actual records. It fails the frozen 3.0 s stable-time target at 5.573 s and the slow-fill false-stable target at 24.56% versus 10% maximum.

No candidate is selected. The robust dual-IIR remains the closest research reference only. It is not linked into product firmware and is not authorized for Stage 5M-B hardware validation. Automatic zero/drift compensation is absent; 40 Hz remains contained.

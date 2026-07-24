# Display Model

The HAL-free model owns six logical segment masks, page, annunciators,
brightness, enable state, and a change revision. The formatter supports signed
integer `WeightValue`, 0..5 decimal places, decimal points, `INT32_MIN`, leading
blanking, and explicit HI/Lo overflow without `sprintf` or floating point.

`DisplayController` is the only adapter that maps logical segments and 12
annunciators to the TM1628 shadow RAM. Stage 2B diagnostics retain priority.

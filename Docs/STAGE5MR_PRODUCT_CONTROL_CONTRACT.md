# Stage 5M-R Product Control Contract

This is a future recommendation, not a public protocol change. Controls should be `dC OFF` and `dC ON`, with internal ENABLE, DISABLE and explicit RESET. ENABLE must be bumpless. Recommended DISABLE stops learning while retaining the current offset to avoid a display jump; RESET explicitly clears offset without changing zero, tare or calibration. Offset should never persist. PLC/menu/PC exposure requires a later versioned integration decision; no command, register or menu is added here.

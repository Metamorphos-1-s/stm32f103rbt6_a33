# W02 Driver

Reserved for W02 module control and UART framing. The PA8 signal is an active-low power-key pulse, not a steady power-enable output.

Stage 2A implements only the non-blocking PWRKEY pulse guard. AT commands,
connection state, BLE framing, and low-power policy remain out of scope.

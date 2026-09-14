# Stage 5M-R Requirements Correction

Stage 5M-A correctly failed its original combined targets and remains unchanged. The revised requirement separates protected static drift from response speed, stable latency and process-state signaling. Drops can be separated by 2-10 seconds of genuinely stable weight; `stable=true` does not mean the fill session ended. Fast response belongs to Stage 5M-F.

Protection priority is real-weight preservation, then conservative freezing, then slow drift improvement. Offset is runtime-only, power-on zero, never calibration/zero/tare modification, never persistent and never a rounded-display input.

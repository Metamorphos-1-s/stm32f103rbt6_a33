# A33 0x0542 候选：修复 R5 重建后一直等待阶跃结束

基线：`0x0541`（`5e93ef2`）。构建预设 `Stage5PR5ResetEpoch`，Modbus Map 仍为 `0x0110`，沿用 V4 配置。当前为源码候选，已做 Host 验证，尚未构建 ARM BIN 或上板。

## 现场证据与复现

2026-11-12 15:08:57–15:09:57（北京时间），COM5，`0x0541 / 0x0110`。独立校验 `frames.jsonl` 的 664 对 FC03 请求/应答 CRC 和寄存器解析。10 Hz 下重量样本序号 `520105 → 520693`，设备时间戳 `132932240 → 132992261 ms`，实测 9.797 Hz。61 帧 R5 诊断均为 `ACTIVE + STATIC`、状态 `3`（`STEP_PENDING`），实际施加量和内部补偿量均为 `0 µg`，施加标志为 `1`，与同帧毛重差一致。原始与滤后 ADC 峰峰值分别为 69 和 24 counts。

- 前 20.7 秒连续覆盖 203 个不同样本（`520105–520307`）。用同样本序号和时间戳对齐滤后 ADC 与未补偿毛重，实测斜率约 `1125.84 µg/count`。这段相邻样本最大变化 3 counts（约 `0.00338 g`），低于 10 Hz 的 `2 g` 明显阶跃门槛；但第 20 秒 R5 仍为状态 3，超过普通阶跃的 50 个样本冻结窗口。10 Hz 也排除了仅 40 Hz 使用的额外原始阶跃检查路径。
- 首帧设备时钟约为开机后 `36 h 55 min`（按未发生毫秒计数回绕计）；当前样本序号按观测速率折合约 `14 h 45 min`。约 `22 h 11 min` 的差值与开机后曾重建称重链路吻合。操作者确认本次开机后确实切换过滤波。若切换前后采样速率相近且称重持续，反推最后一次序号归零约在北京时间 `00:24`，仅为估计，不能当作菜单操作时间。

旧版的 `A13C_Reset()` 没有清除按样本序号计数的冻结截止点 `freeze_until`。发生过阶跃后，如果称重链路随后重建并让样本序号从头开始，旧截止点会重新落在新序号的未来，即使它在旧序号中已经到期。R5 会继续等待，不能建立新的参考。候选在 `A13C_Reset()` 清除此截止点；新阶跃、保存维护后的等待和正常参考建立窗口仍按原规则处理。

10 Hz 主机复现：静载运行约两分钟，施加 9 g 阶跃，等待旧冻结期限结束，再由 FILT1 切到 FILT3。重建后重量保持 509 g：旧版在 5.1、35.1、70 秒均为状态 3；候选依次为 4、5、7，70 秒时正常跟踪。10 Hz 和 40 Hz 集成测试均验证了重建后锁定参考、恒定载荷下维持零补偿，并对随后的模拟漂移产生非零补偿。使用当前源码重建 Host 集成目标，测试通过。

## 现场补证

现场证据强烈支持旧冻结截止点残留，但没有直接记录切换前的阶跃、旧序号或 `freeze_until`。若设备仍处于同一次运行，可单次 FC03 读 `0x0300` 起 35 个寄存器，查看状态 `0x0302`、原因 `0x0303`、参考锁定序号 `0x0321–0322`。复现中的旧版为 `3 / INITIAL(1) / 0`，可辅助比对。后续候选上板验收应在同一次开机内完成“正常跟踪 → 大阶跃 → 切换滤波 → 保持静载”，确认能重新锁定参考并进入状态 7；静载期间不要求补偿量非零。

## 本地构建与烧录验收

在已导入候选分支的 STM32 仓库根目录，使用 CMake ≥ 3.22、Ninja 和 GNU Arm Embedded GCC（包含 newlib-nano）。如使用 STM32CubeCLT，将其 GCC 和 CMake/Ninja 目录加入 PATH。以下命令只构建和检查文件：

```powershell
cmake --preset Stage5PR5ResetEpoch -B build/arm-0542 -DCMAKE_BUILD_TYPE=Release '-DCMAKE_C_FLAGS=-Wall -Wextra -Werror -fstack-usage -fcallgraph-info=su'
cmake --build build/arm-0542
arm-none-eabi-objcopy -O binary build/arm-0542/stm32f103rbt6_a33.elf build/arm-0542/A33_0542_MAP0110.bin
arm-none-eabi-objdump -h build/arm-0542/stm32f103rbt6_a33.elf
python Tools/stage5mr5c/analyze_ram_stack.py --elf build/arm-0542/stm32f103rbt6_a33.elf --map build/arm-0542/stm32f103rbt6_a33.map --callgraph-root build/arm-0542 --output build/arm-0542/ram
python Tools/stage5pa13d/nested_stack_gate.py --analysis build/arm-0542/ram --callgraph-root build/arm-0542 --output build/arm-0542/ram/nested_stack.json
Get-FileHash build/arm-0542/A33_0542_MAP0110.bin -Algorithm SHA256
```

核对 Flash 可加载段均落在应用区 `0x08000000–0x0801EFFF`，BIN 不超过 126,976 B，静态保守栈余量至少 512 B。独立运行 Host 专项时，使用本机 C 编译器配置 `cmake -S Tests/host -B build/host-0542 -G Ninja -DCMAKE_BUILD_TYPE=Debug`，构建 `stage5p_r5_reset_epoch_tests`，再运行 `ctest --test-dir build/host-0542 -R '^stage5p_r5_reset_epoch_integration$' --output-on-failure`。

测试仪上板时先备份 `0x0801F000–0x0801FFFF` 的完整 4 KiB 配置区，擦应用页 0–123，从 `0x08000000` 写入并 Verify。复位后先通过 COM5 确认 `0x0542 / 0x0110`，在任何菜单保存前回读配置并与备份逐字节比较。

在 10 Hz、FILT1、R5 ACTIVE + STATIC 下保持静载至少两分钟并读到跟踪状态。快放砝码，确认出现阶跃，再切换至 FILT3 并保持载荷。记录 `0x034A–034D` 的序号和时间戳、`0x0302` 的状态、`0x0321–0322` 的参考锁定序号；以切换生效后 90 秒内重新锁定参考且 `0x0302=7` 为验收目标。期间不重启、不置零、不去皮、不重新标定，以免绕过待验证的场景。

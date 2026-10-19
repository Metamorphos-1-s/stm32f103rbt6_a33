请继续 A33 Stage 5P 的 R5 跨负载/回零离线研究复核，按以下边界执行。

仓库：Metamorphos-1-s/stm32f103rbt6_a33。
分支：stage5p-r5-transition-research。
固定基线：e4a0b5161688a4ada95880befc2fb33013407536。
实际固件仍为 0x0530 / Map 0x010F。

本轮结论是 THREE AUTOMATIC CANDIDATES REJECTED / NO NEW FIRMWARE。
目前没有进入上板实测的合格改进候选；先复现并核对研究证据。
本任务不连接 COM5，不执行烧录、SWD、复位、ZERO、TARE、标定、SAVE、
R5 模式切换或设备配置操作。不要把 Host-only paired_reference 候选集成进产品。

1. 检查本地未提交改动，fetch 研究分支 stage5p-r5-transition-research，建立独立工作区。
   记录实际 HEAD/tree，确认固定基线 e4a0b5161688a4ada95880befc2fb33013407536 是祖先。
   本工作包的代码、报告和 210,657 B 合成证据归档已经用户明确授权向指定仓库的独立研究分支发布。
2. 阅读 Docs/STAGE5P_R5_TRANSITION_RESEARCH.md 和 Docs/STAGE5P_R5_EMPTY_CONTEXT_CONTRACT_DRAFT.md。
3. 核对 Results/stage5p_r5_transition_research/manifest.json 的文件长度与 SHA；
   核对 selected_traces_and_build_logs.tar.gz 包内文件与 selected_archive_manifest.json。
   归档保存关键完整追踪及构建日志；全部 64 次数值及输入/输出哈希在 analysis.json 中，
   其余原始合成 CSV 可从生成器重建。原始实板证据不变。
4. 配置独立 Host 构建：
   cmake -S Tests/host -B build/r5-transition -DCMAKE_BUILD_TYPE=Release -DA33_BUILD_R5_TRANSITION_RESEARCH=ON
   cmake --build build/r5-transition --config Release --parallel 6
   ctest --test-dir build/r5-transition -C Release --output-on-failure
   Windows 若无 MSVC 编译环境，使用已安装的原生 C 编译器选择合适的生成器；不要使用 ARM 交叉编译器运行 Host 目标。
5. 运行现有零返回归与新增研究检查：
   python -m unittest discover -s Tools/stage5_unified -p "test_zero_return*py" -v
   python -m unittest discover -s Tools/stage5_unified -p "test_r5_transition_research.py" -v
   预期分别 33 和 9 项，Host CTest 52/52。测试使用文件或 fake clients，不打开真实串口。
6. 在一个新输出目录复现 16 场景、4 策略共 64 次运行：
   python Tools/stage5_unified/research_r5_transition.py --build build/r5-transition/stage5p_r5_transition --output Results/r5_transition_local_repro
   如使用多配置生成器，把 --build 指向实际包含四个 stage5p_r5_transition_*.exe 的目录（通常再加 Release）。
   每次输出必须 48,000 行、25 ms 完整物理样本，共 3,072,000 行；不将实板低频记录插值后喂入此测试。
7. 用 analysis.json 的每次输入/输出 SHA 和数值指标交叉核对。CSV 输出以 LF 规范化。
   不要求本地编译器的 ELF 哈希与 Linux 宿主相同；若数值或 CSV 哈希不同，保存差异、工具链信息及原因，不能改参考结果把差异抹掉。
8. 必须确认：hold60/hold120 有恒载或漂移退步；paired_reference 虽改善模拟机械偏移，
   但在真实 40 mg 残留中吞掉约 30 mg。两种物理世界的原始 ADC 和输出分别完全一致。
   必须同时报告基线 STATIC 的 20 mg/秒合成加料保留量误差约 −0.092668 g，
   与匹配 DOSING 的约 −0.000094 g；这些是仿真，不是本轮实板结果。
9. 不因单个场景变好就晋级。不更改 Firmware 版本，不构建或烧录新 ARM 候选。
   归档复现结果和工具链差异，提交/推送到同仓库的独立审查分支，不改 main，不覆盖历史原始证据。
   本次授权覆盖上述研究工作包的外部归档发布，不包含实板操作或把失败候选集成到产品。

完成复核后明确输出：NO NEW FIRMWARE; HARDWARE ENTRY NOT READY。
若下一步要求主动恢复旧空载参考，先取得“PLC 可确认卸空 / 仅人工确认 / 必须全自动”的
业务事实，再针对该事实设计后续方案。缺少该信息时，保留空载契约为草案，
不擅自增加协议命令、自动清零或恢复参考。滴水与停顿的实板项继续 NOT RUN。

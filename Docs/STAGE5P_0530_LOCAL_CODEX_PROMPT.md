# 给本地 Codex 的执行提示词：Stage 5P / 0x0530 聚焦实板验收

请在我的 Windows 电脑上执行下面的任务。云端软件审查已经完成，现在开始本地实板阶段。

任务授权范围：在正确的 STM32F103RBT6 仪表上执行只读预检、备份、仅应用区烧录、显式复位、只读采集，以及必要时恢复本次备份的应用区。不要重复询问同一范围的烧录授权。人工加卸载、滴水、接通电源等动作必须等我实际完成，不能自行假定。

## 1. 取得正确代码与固定候选

仓库：https://github.com/Metamorphos-1-s/stm32f103rbt6_a33
分支：stage5p-display-anchor-follow-v3
PR：https://github.com/Metamorphos-1-s/stm32f103rbt6_a33/pull/1

优先检查 D:/Documents/stm32f103rbt6_a33_stage5p_052c_save，但必须用 git remote 确认仓库。保留所有既有工作和证据。拉取上述分支后固定 FETCH_HEAD，建立独立 worktree 和硬件验收分支；不要重置原工作区，不合并 main，不在测试中途再次更新代码。

先阅读：
- Docs/STAGE5P_0530_SOFTWARE_REVIEW.md
- Results/stage5p_0530_software_review/software/manifest.json
- Results/stage5p_0530_software_review/software/qualification.json

本轮只能使用：
- Firmware：0x0530；Map：0x010F
- BIN：Results/stage5p_0530_software_review/software/firmware_0x0530.bin
- BIN 大小：98,796 B
- BIN SHA-256：8ADB103085629F9A0F645CA25F324C6B53DDB395C714E62D1C0306ED7CBCBA34

先检查 Git 工作区、BIN 大小和 SHA、向量表、manifest 中的源码哈希。源码哈希按固定提交的 Git blob 原始字节核对，不按可能经过 Windows CRLF 转换的工作区文本字节核对；可用 Python subprocess.check_output 调用 git show 获取字节，避免 PowerShell 重编码。向量表初始 SP 应在 STM32 的 20 KiB SRAM 范围内并正确对齐，复位向量必须是应用区内的 Thumb 地址。任一不符就停止，不烧录。
manifest 的 source_blobs 保存 Git blob 对象 ID。优先一次读取 git ls-tree -r --full-tree <固定SHA>，按路径核对所有对象 ID，不必逐个启动数千次 git show。

本轮优先烧录这个已验证的 BIN。不要调用 Tools/stage5b_hw/build_and_flash.ps1，它构建的普通 Release 不能代表这个候选。
若需要本地重建，必须使用完整标识包含 GNU Tools for STM32 13.3.rel1.20240926-1715 的编译器和 Stage5PDisplayReview preset；不能把 Arm GNU Toolchain 13.3.1 当成同一发行版。重建结果不匹配上述 BIN 时，停止解释差异，不得静默替换候选。

软件验证命令示例（替换为已确认的本地路径；输出目录必须是新的）：
    python Tools/stage5_unified/qualify_display_0530.py --arm-bin "<ST工具链的bin目录>" --host-cc "<LLVM-MinGW clang或MinGW gcc>" --cmake "<cmake路径>" --ninja "<ninja路径>" --output "<新的软件验证目录>"

完整 Host Debug/Release 应各为 51/51，显示专项为 78 个场景；本轮新增的采集工具契约测试全部是假串口，不访问设备。

## 2. 只读预检；此前不擦写

先创建唯一的新证据目录，保存 UTC 时间、本地时区、固定 Git SHA、dirty 状态、工具路径/完整版本、Windows USB/串口设备清单、适配器 VID/PID/序列号和 ST-LINK 序列号。

历史端口 COM5 是 STM32 USART2，COM3 是 CH579 日志口。COM5 可能重编号：必须确认适配器归属，不能向所有串口遍历发包。只保留一个访问仪表串口的程序；不要擅自终止无关进程。

区分并分别归档：
- PORT_OPEN_FAILURE：端口不存在、被占用、拒绝访问；
- MODBUS_TIMEOUT：串口已打开，但没有有效 Modbus 回复；
- SWD_TARGET_UNAVAILABLE：没有找到目标或目标未上电。
这些不能直接写成“新固件启动失败”，尤其此时尚未烧录。

按已知 115200/N/1、Unit ID 1 做 FC03 预检，并确认实际配置。可以使用：
    python Tools/stage5pa13c/hardware_shadow.py --port "<已确认端口>" --output "<新目录>/preflight_before"

归档真实 Firmware/Map、字序、分度/小数位、当前单位/NET-GROSS、采样率/滤波、标定有效性、R5 请求与运行状态、偏移、检重模式、revision/saved、dirty/fault/overrun、槽位与 sequence。
历史最后报告是 0x052E、40 Hz/filt1/strength3、R5 ACTIVE+STATIC、检重 STATIC，但这不是当前设备的既定事实。sequence 23 和 500 g 在秤台上的历史状态也必须重新确认。

在原固件不变的情况下，完成 100 次 FC03 只读通信，打开失败、超时、CRC、异常分别统计，目标 100/100 有效。
若通讯不稳，先排除 USB、端口占用、线缆和电源，不烧录。

SWD 确认是正确仪表、MCU ID 0x410、128 KiB Flash，并记录供电及连接信息。若有多个探针，明确指定正确探针。
若 dirty 非零、revision/saved 不一致、标定无效、fault/overrun 异常或固件版本超出已知基线，先停止并说明实际差异，不擅自 SAVE、清故障、降级或改变配置。
若已经是相同 0x0530，先读回验证是否就是本候选；相同则跳过重复烧录。

## 3. 先备份，再取得原版本的静载对照

保存并校验长度、SHA：
- 应用区：0x08000000，长度 0x1F000（126,976 B）。
- 完整 V3 配置区：0x0801F000，长度 0x1000（4,096 B）。

解析双槽：
    python Tools/stage5b_hw/parse_config_slots.py "<config_before.bin>" --json "<slots_before.json>"
并与刚读出的活动槽和 sequence 核对；保留 CRC/commit 结果及原始数据。

SWD 读回若暂停了内核，必须恢复运行并记录复位/启动过程。复位会清除易失 R5 偏移并重建参考，不得把这当成配置丢失。等待启动状态恢复后，再取对照数据。

烧录准备期间保持当前秤台载荷不动。询问并记录真实载荷，不假定仍放着 500 g。
在主称重界面取得原版本静载 120 s 对照，保存我的目视反馈原文。采集期间不能另开串口预检或上位机。

    python Tools/stage5_unified/record_display_diag_052c.py --port "<端口>" --word-order "<preflight中的high或low>" --firmware "<刚读出的原Firmware>" --duration 120 --output "<新目录>/baseline_static"

不要新建 ZERO、TARE 或标定来“修正”500 g 附近的绝对读数；本轮比较相对显示行为。

## 4. 应用区烧录与完整验证

任何擦写之前确认备份完整、BIN 匹配，并再次检查配置区 SHA 未变化。
STM32_Programmer_CLI 路径优先查 E:/ST/STM32CubeCLT_1.18.0/STM32CubeProgrammer/bin，使用实际发现的版本。

只允许擦除应用页 0–123；124–127 是配置页。
严禁 mass erase、-e all、修改 option bytes、写配置区、换标定、恢复出厂或默认配置。

使用已经成功验证过的 Normal SWD 方式。关键命令应保持以下参数语义；PowerShell 中 '[0 123]' 必须是一个完整参数：
    <CLI> -c port=SWD mode=Normal -e '[0 123]' -w "<已校验BIN>" 0x08000000 -v
    <CLI> -c port=SWD mode=Normal -rst

若指定探针，在每条命令中保持同一个已确认的探针。逐条检查退出码和完整输出；不能忽略擦除报错后继续声称成功。

随后独立读回：
- 整个 126,976 B 应用区：前 98,796 B 与候选 BIN 完全一致，剩余字节全为 FF。
- 整个 4,096 B 配置区：与本次 preflash 备份 SHA 完全一致。
- 再解析 V3 双槽并核对活动槽、sequence、CRC/commit。
- 如读回操作暂停了内核，再显式 reset，随后才开始 COM 预检。

只读确认 Firmware 0x0530 / Map 0x010F、配置与标定恢复、revision/saved 与配置槽不变、dirty/fault/overrun 为 0。R5 易失 offset 重新从 0 建立是预期行为，但不能擅自切换它的运行模式来让测试通过。

COM 超时时，先区分端口打不开和打开后无响应。保存烧录、复位和读回证据，做一次有限的主机链路排查；不要反复擦写候选。

## 5. 聚焦显示实测

所有采集使用同一套串口程序、实际端口和字序，Firmware 参数为 0x0530。可以使用 --stop-file 优雅停止；停止并释放串口后才能运行其他读取工具。
每阶段独立目录，保留 frames、samples.csv、errors.jsonl、summary、人工事件和反馈。

A. 保持原载荷：
- 先取 120 s 与原版本对照，再继续静置观察至少 10 min。
- 记录显示变化次数、持续往返区间、raw/filtered ADC、权威重量、display_state、locked、official stable、R5 offset、fault/overrun。
- 如目视看到抖动，记录发生时段和读数；不要立即改参数。

采集示例：
    python Tools/stage5_unified/record_display_diag_052c.py --port "<端口>" --word-order "<字序>" --firmware 0x0530 --duration 600 --output "<新目录>/candidate_static"
    python Tools/stage5_unified/analyze_display_diag_052c.py "<采集目录>/samples.csv" --output "<采集目录>/analysis.json"

B. 加载/卸载：
- 需要我实际操作。确认当前量程允许后，用现有 500 g 砝码完成 3 轮卸载/加载，每次保持 30–60 s。
- 明显加载和卸载应快速跟随，不能长时间保持上一次载荷的旧数字。
- 操作回复只作确认，不当作精确动作时间；用数据给出事件时间区间。
- 不执行 ZERO/TARE、不改变滤波、采样率、单位或检重/R5 模式。

C. 每秒一滴及停顿：
- 我准备稳定容器和滴头，水不能接触电路。等待我确认准备完成再开始。
- 连续约每秒一滴 20–30 s，记录实际滴数；再做“少量滴加—停 2 s—继续”和“少量滴加—停 10 s—继续”，至少各 3 次。
- 用 N 滴总增量估计本次实际单滴质量，不假定每滴一定是 0.05 g。
- 小于噪声带或分度的单滴不要求每滴可见；要求累计增重能跟随，停止滴加后能接受新重量，不因滴间停顿而一直保持旧值。
- 软件参考时序是：九点中位数、>=3e 同侧 300 ms 或非零同侧 1500 ms，另有 >8e 累计大偏差释放。观察明确新平台后约 2.5 s 仍保持旧值的情况，标记疑似冻结并保留证据，不自动放宽条件。
- 额外反应时间应相对权威重量变化判断，不能把 ADC/滤波响应全部计成显示延迟。

D. 最后再取空载/恒载短窗，读回配置区校验 SHA，确认 SWD 操作后内核已恢复运行；若需显式 reset，记录 R5 易失偏移重新建立，再执行最终 COM 只读 preflight。不能留下被调试器暂停的设备。

## 6. 证据边界与停止条件

本采集器由多个 FC03 请求组成，summary 已明确 snapshot_atomic=false。它是稀疏 Host 记录，不是完整 40 Hz 样本流，也不是 LED 段码电气读回。
不能用一个跨请求 CSV 行直接证明“同一样本偏差超过 8e”；有疑点时结合持续时间、序列、多个相邻记录和实际面板观察。不要声称这轮补齐了全部动态同样本关系或运行时栈水位。

显示 locked 与 official stable 独立：显示保持不能被写成官方判稳通过。绝对称重误差、长期漂移、跨传感器、10 Hz 硬件若未测，分别保持待验收；不要改配置去补所有历史项目。

必须报告：
- 烧录/读回/启动是否通过；
- 配置保护是否通过；
- 静载目视与数字往返是否改善或变差；
- 加卸载是否跟随；
- 滴加与 2–10 s 停顿是否出现旧值冻结；
- 所有读错、fault、overrun 和缺失的人工反馈。
缺少我的反馈时写 NOT OBSERVED / INSTALLED_PENDING_VISUAL，不能宣称面板完整 PASS。

出现启动失败、应用区校验失败、明显显示回归、新 fault/overrun 或滴加冻结时，停止后续验收，保留失败证据，并恢复“本次上板前备份的应用区”，不是随便选择旧 Release：
- 仍只擦除 0–123；
- 写入 application_before.bin，Verify，显式 reset；
- 完整读回应用区并与备份比较；配置 SHA 仍须一致；
- 恢复通信后核对原 Firmware/配置。
如果配置 SHA 已变化，立即保全 before/after，不能擅自擦写 124–127；标记 CONFIG_CHANGED / MANUAL_RECOVERY_REQUIRED，说明应用区恢复不能代替配置恢复。

只有真实失败触发回滚；单纯缺少人工反馈时可保持候选等待观察并明确状态。若受端口、供电或人工动作阻挡，只说明需要的具体动作，不重复询问已经授权的应用区烧录操作。

## 7. 收尾

在独立硬件验收分支整理报告与 SHA-256 manifest，重新从磁盘核验文件完整性。保存成功和失败尝试，不覆盖历史证据。提交并推送到同一仓库，不合并 main、不自动关闭 PR #1。

最终给出一句准确结论及当前设备固件/配置/载荷状态、分支 HEAD、报告位置。软件 READY 与聚焦硬件 PASS 分开陈述；明确所有 NOT RUN / NOT OBSERVED / INCONCLUSIVE 项。

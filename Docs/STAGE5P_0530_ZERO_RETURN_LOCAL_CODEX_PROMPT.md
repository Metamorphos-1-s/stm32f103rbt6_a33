# 给本地 Codex：0x0530 回零专项只读补证

请执行以下任务。本任务是在保留 0x0530 固件和当前已保存参数的前提下，
补充回零诊断证据。授权范围为识别串口、只读 FC03 采集、指导用户装卸砝码、
离线分析以及本地 Git 提交。远端发布需用户明确授权仓库、分支和证据内容后执行。

本任务不包含烧录、SWD、复位、配置写入、R5 切换、ZERO、TARE、标定或 SAVE。
不要调用旧的烧录/恢复/邮箱执行脚本。发现状态不符时保留证据并停止，
不能为了达到预期状态自行修改仪表。

## 1. 从交付包获取并固定审查版本

仓库：`https://github.com/Metamorphos-1-s/stm32f103rbt6_a33`。
交付包：`A33_0530_Zero_Return_Review.zip`，包含完整审查文件、
`review.patch` 和 `bundle_manifest.json`。本流程不依赖远端审查分支。
补丁基线必须为 `5913167f904a8783b3547ea33ab0d471bd80789d`。
固件生产源码仍固定在 `a2174ede65a1927ae491e73289c95bdcaa97deb5`。

先把交付包解压到仓库之外的新目录，读取 `bundle_manifest.json`，核对补丁的
SHA-256。找到已有仓库，记录当前分支、HEAD 和未提交内容，使用独立 worktree。
在新 worktree 上应用补丁，不覆盖旧工作区，也不合并 main。PowerShell 示例：

```powershell
$bundleDir = "<本次交付包已解压的完整路径>"
$bundle = Get-Content (Join-Path $bundleDir "bundle_manifest.json") -Raw | ConvertFrom-Json
$patchPath = Join-Path $bundleDir "review.patch"
if ((Get-FileHash $patchPath -Algorithm SHA256).Hash -ne $bundle.review_patch_sha256) { throw "补丁摘要不符" }
if ($bundle.base_commit -ne "5913167f904a8783b3547ea33ab0d471bd80789d") { throw "补丁基线不符" }
git fetch origin $bundle.base_commit
$taskStamp = Get-Date -Format yyyyMMddTHHmmss
$repoRoot = (git rev-parse --show-toplevel).Trim()
$taskDir = Join-Path (Split-Path $repoRoot) "a33_zero_return_$taskStamp"
git worktree add --detach $taskDir $bundle.base_commit
Set-Location $taskDir
git switch -c "stage5p-zero-return-readonly-$taskStamp"
git apply --check $patchPath
git apply --index $patchPath
if ((git write-tree).Trim() -ne $bundle.review_tree) { throw "审查文件树不符" }
git commit -m "Import verified 0x0530 zero-return review for local evidence capture"
$reviewHead = (git rev-parse HEAD).Trim()
```

逐条检查命令成功，失败立即停止；不要忽略补丁或文件树校验错误。
使用已有 Git 身份，记录 `$reviewHead` 和 `$bundle.review_tree`，后续不自动切换版本。
阅读 `Docs/STAGE5P_0530_ZERO_RETURN_R5_REVIEW.md` 和本提示词。
使用 Git 规范字节核对 `Results/stage5p_0530_zero_return_review/software/manifest.json`
中的源文件 blob ID / 证据摘要；不要把 Windows 换行转换误当作内容变更。
确认生产源码路径相对固件固定 HEAD 无变化。

先运行模拟测试，它们不访问串口：

```powershell
python -m unittest discover -s Tools/stage5_unified -p "test_zero_return*.py" -v
```

应为 19 项通过。若依赖缺失，仅安装本地 Python 所需的 pyserial；
不更换仪表固件或配置。无需 ARM 构建，也无需重新下载/烧录 BIN。

## 2. 现场识别与新鲜预检

历史 COM5 是 CH340（VID 1A86/PID 7523），COM3 曾是 CH579；
本次重新枚举端口并结合描述、VID/PID、序列号识别，不盲开 COM3。
一次只允许一个串口客户端，使用实际确认端口、115200/N/1、Unit 1。
区分端口打开失败、Modbus 超时、CRC/异常及身份不符。

让用户确认秤台当前物理空载、传感器/秤台稳定、500 g 砝码可安全加载。
不要依据历史消息认定砝码仍在或已移走。空载有约 0.3–0.4 g 读数也不执行 ZERO。

用下述只读预检得到新鲜身份，输出必须为新的目录：

```powershell
$port = "<本次已确认的实际串口>"
$evidenceDir = Join-Path "Results/stage5p_zero_return_readonly" $taskStamp
python Tools/stage5pa13c/hardware_shadow.py --port $port --output "$evidenceDir/initial_probe"
```

要求：0x0530 / Map 0x010F，word order=high，Profile 0、40 Hz/filt1/strength3、gain3，
R5 ACTIVE+STATIC 且非 limited，检重 STATIC，calibration valid，
revision=saved、dirty/fault/overrun=0。记录实际配置、标定系数、sequence、
revision、SAVE 计数和 R5 offset，不能假定仍是 sequence/revision 23。
如果和审查基线配置有差异，解释并报告，不自行改回。
若当前已不是 0x0530 或字序不符，停止并报告。

## 3. 启动连续只读采集，再指导物理动作

```powershell
$stopFile = Join-Path $evidenceDir "STOP"
python Tools/stage5_unified/capture_zero_return_0530.py --port $port --duration 3600 --output "$evidenceDir/capture" --stop-file $stopFile
```

让该进程在本地 Codex 可继续交互的独立终端/后台任务中运行，持续检查其输出、
错误文件和进程状态。不要再启动第二个串口客户端。
该工具只用 FC03，单独保存原始帧及 JSONL：

- `0x0340 / 40`：同响应权威未补偿毛重、补偿后毛重/净重、tare、applied offset、
  apply、generation、sequence、MCU 时间及当前模式；逐行验证两条计量等式。
- 其余实时、诊断、显示、R5 和 A13C 请求各自保留字段与时间，整体不标原子。
- 高字在前是工具明确支持的本次冻结范围；不支持时停止，不改仪表字序。

动作顺序必须从空载开始，并最终回到空载：

1. 用户确认空载后先记录 **120 s** 基线。
2. 第 1 次放置 500 g，用户回复已放置后记录 **120 s**。
3. 第 1 次卸载，用户回复已空载后记录 **120 s**。
4. 第 2 次加载/卸载，各记录 **120 s**。
5. 第 3 次加载/卸载，各记录 **120 s**。
6. 最后一轮空载连续记录至卸载后 **600 s**，包含前述 120 s，不额外加载。

每次动作前清楚告诉用户当前请求，等用户确认后再进入后续阶段。
用户回复只作为消息接收时间标记；实际重量边沿由连续数据识别。
如用户暂时不能配合，保留现有记录并标 `NOT RUN/INCOMPLETE`，不虚构动作。
保持同一个砝码、同一放置位置和支撑条件；若需扶秤、改线或移动秤体，单独标记。

可用现有工具写入消息标记（不访问串口）：

```powershell
python Tools/stage5_unified/mark_hardware_event_0530.py --events "$evidenceDir/operator_events.jsonl" --event EMPTY_INITIAL_CONFIRMED --message "用户确认当前空载"
```

加载/卸载分别用 `LOAD_1_CONFIRMED`、`UNLOAD_1_CONFIRMED` 等唯一事件名。
有温度/上电时间记录就保留，没有则标未知，不能编造。
本轮不补做滴水或 2/10 s 停顿，不更改此前 NOT RUN 结论。

完成后创建 stop 文件并等待正常退出：

```powershell
New-Item -ItemType File -Path $stopFile
```

工具会在退出前做只读 postflight。若读错、身份/配置/模式/tare 改变、
fault/overrun 或算术不一致，停止动作、保留完整记录并报告；不要自动重试整轮、
复位、恢复配置或改固件。没有设备写入时也不执行“回滚”。

## 4. 离线分析及结束状态

检查原始 FC03 帧 CRC/长度、日志完整性、实际采样数量、重复/省略/倒退，
保存输入 SHA-256。主机轮询缺少的转换是未观察样本，不插值为完整 40 Hz。

以同一 `authority` 响应计算：

`gross = uncompensated - (apply ? applied_offset : 0)`

`net = gross - tare`

每轮比较加载前 `[-45,-15)` 秒与卸载后 `[15,45)`、`[60,90)`、`[90,120)` 秒。
输出每个平台完整覆盖/间断/稳定状态、分布、中位数与均值；不只保留 stable 行。
各窗口从同一 authority 字段计算均值分解，勿用“多个中位数必然可加”来证明等式。
独立请求中的显示、raw/filtered ADC、R5 参考和 A13C event 只作带时间边界的诊断。

至少输出三轮：加载前空载、加载后重量与净增量、卸载后空载差、
Δ未补偿重量、Δapplied offset、Δ补偿后重量、显示/权威差，以及最后空载变化。
没有覆盖完整的窗口要标不完整。没有砝码证书、环境约束和正式容差时，
给出实测值与证据范围，不擅自宣布准确度、漂移或回零资格 PASS。
本轮没有 OFF 对照；不要据此声称 R5 的因果改善/恶化已证明。

确认结束时仪表身份、配置/revision/SAVE 计数、R5 请求模式、fault/overrun 与预检
一致，记录实际空载/砝码状态。offset 的正常运行变化不等于配置被修改。
如果用户未完成最后卸载，真实记录最终载荷状态，不自行填空载。

生成 `Docs/STAGE5P_0530_ZERO_RETURN_READONLY_HARDWARE.md`，将原始帧、JSONL、
动作标记、前后预检、摘要和 SHA 清单放到本轮独立 Results 目录。
提交到本地任务分支。只有用户明确授权向上述仓库发布本轮源码、审查报告和
硬件证据后，才推送本任务分支并验证本地/远端 HEAD；不合并 main。
最终报告执行/未执行项、设备最终状态、完整闭环数、窗口结果与仍开放的归因问题。

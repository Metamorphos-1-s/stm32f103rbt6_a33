# Stage 5P-A9 — 新接线两轮装卸归因开发记录

## 结论与边界

`MODEL DEVELOPMENT / ATTRIBUTION COMPLETE; MODEL SELECTION INCOMPLETE`。本次新数据不是独立 holdout；只有先在开发数据上明确并冻结候选，再另采独立数据才能做资格验证。本阶段未修改产品 R5、未烧录、未执行 SAVE/ZERO/TARE 或重新标定，也未启动 12 小时、极慢加料、10/40 Hz 或检重全矩阵。设备实际 R5 全程 `SHADOW + OFF`，offset=0，所有补偿数值均为**事后离线回放**。

研究基线 `58938bd4e2049e6d32bdb35bcb58f4e904217152`；分支 `stage5pa9-attribution-capture`。A8/A8R 原报告与结果保持原状。旧 185,910 行 CSV SHA-256 `10545968A92A23688162A68D82DCFCF20C10FDD0ABDF3EC58203F6E549D0C3D2` 是已打开的开发数据，不能当作本次新数据或 holdout。

## 设备预检（本次实板只读）

`Results/stage5pa9/preflight_readonly_20260927/environment.json`、`summary.json` 与原始 `samples.csv`：2026-09-27 06:07:20–06:07:28 UTC；77 条有效记录，0 读取错误、2 个未观测 sample sequence。`0x051C / Map 0x0104`；采样率配置 `0`（设备产品配置解释为 10 Hz），活动滤波 `filt1/strength3`，gain3；标定 raw zero `41868`、raw span `485780`、对应 `500000000 ug`，calibration sequence5/valid1；Persistent Format3、配置槽1/序号19；revision/saved=`19/19`；dirty/fault/overrun=0；R5 `SHADOW+OFF`，offset/reference=0。这里的 strength3 是**本次实际读回**，不能倒推旧 185,910 行 CSV 的采集期 strength。旧 CSV 采集前 S+/S− 改正、重新标定、filt1 属于用户现场确认；准确改线和标定时间仍未确认。

温度：未测量（无同步温度传感器或人工温度读数）。设备采集结束后未再次连接 COM5，最终设备状态依据最后一条正式记录，并非新一次独立探测。

## 连续原始采集与质量

`Results/stage5pa9/20260927T060806Z_development_capture/`：2026-09-27 06:08:06.508–09:07:33.730 UTC，`host_monotonic_ns` 跨度 `10767.219 s`；107,491 条不同 sample sequence 的有效记录，0 个序号缺口，覆盖率按设备序号为 100%；另有 149,914 次读取到重复序号被记录器跳过，不是缺失样本。读取错误0、主机最大有效记录间隔0.157秒、uptime/sequence 回退0、UTC 相邻时钟跳变>1秒为0。不能将不同 Modbus 块误称为同一 ADC 时刻；原始 ADC 边沿 bracket 也不等于 DRDY 电气时间。

记录的字段包括 UTC、host monotonic、MCU uptime、sample sequence、raw/filtered ADC **counts**、权威 gross/net、面板 count 和 conditioned display、official stable、R5 状态/offset、revision、fault/overrun/SAVE 等。`filtered_raw` 不是滤波后质量；R5 OFF 因而权威 gross 可作为本次未补偿重量，但没有独立的 filtered mass 字段。

记录状态全程：firmware/config 不变，revision/saved `19/19`，dirty0，R5 `SHADOW+OFF`，offset始终0，fault/overrun0，SAVE 请求计数始终10。稳定标志有效样本占比约99.852%。记录器报告无写入或 Flash 操作。CSV SHA-256：

```text
E683609BA0DB7EA090D101CE38569A432EC319982E7D41A9D2176A62B623128C
```

原始 CSV 为 31,001,524 bytes；`attribution_analysis.json` 完整保存文件哈希、逐边沿 ADC/权威质量区间、逐字段中位数、窗口有效性、用户事件标记和质量统计。

## 人工标记与数据推定边沿

人工“开始操作”是给用户的动作指令；“完成”是用户回复时间，均**不是**准确物理边沿。所有事件窗口使用 monotonic；UTC 仅供定位。

| 事件 | 指令 UTC | 权威 gross 阈值 crossing UTC（宿主采样区间） | 用户确认 UTC | 阶段时长 |
|---|---|---|---|---:|
| 装载 1 | 06:18:34.278 | 06:21:06.094–06:21:06.172（79 ms monotonic） | 06:21:30.937 | 后续恒载56分24秒 |
| 卸载 1 | 06:51:39.245 | 07:17:30.489–07:17:30.593（93 ms monotonic） | 07:17:53.539 | 空载31分19秒 |
| 装载 2 | 07:48:17.594 | 07:48:49.069–07:48:49.175（94 ms monotonic） | 07:49:11.486 | 恒载31分48秒 |
| 卸载 2 | 08:20:15.871 | 08:20:37.228–08:20:37.306（78 ms） | 08:20:59.500 | 空载46分57秒 |

空载前置基线约12分59秒。原始 ADC、滤波 ADC、权威 gross 使用各自的阈值及各自 Modbus 读取区间推定边沿，不能把它们的先后顺序解释为传感器固有延迟；见 `attribution_analysis.json` 的 `raw_adc_edge`、`filtered_adc_edge`、`gross_edge`。

## 直接实测中位数

每阶段早期参考为数据推定边沿后 15–45 秒，末端为该阶段最后约5分钟；加载跨度=装载前空载最后5分钟到装载后早期窗口；卸载跨度=卸载前恒载最后5分钟到卸载后早期窗口。检查点是从边沿后第 N 分钟开始的完整 60 秒窗口，绝不跨越下一个边沿。下表是**权威 gross 相对本阶段早期参考的变化**，单位 g，不是面板保持值：

| 阶段 | 早期 gross | 1 min | 2 min | 5 min | 10 min | 15 min | 30 min | 45 min | 末5分钟变化 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 装载1 | 500.159942 | +0.059696 | +0.078844 | +0.102497 | +0.107003 | +0.100245 | +0.102497 | +0.087855 | +0.068707 |
| 卸载1 | 0.202743 | -0.018022 | -0.025906 | -0.041675 | -0.057444 | -0.061949 | -0.083350 | NOT AVAILABLE | -0.083350 |
| 装载2 | 500.140794 | +0.030411 | +0.049559 | +0.061949 | +0.056317 | +0.070960 | +0.063075 | NOT AVAILABLE | +0.057443 |
| 卸载2 | 0.181342 | -0.015769 | -0.029285 | -0.046180 | -0.054065 | -0.060823 | -0.074339 | -0.077718 | -0.078844 |

初始空载首5分钟中位数 `0.113761 g`，装载前最后5分钟 `0.135162 g`（先行热机空载变化 +0.021401 g）；空载变化并非同时进行的未载对照，不能机械扣除后声称识别载荷蠕变。

对应物理跨度：装载1早期+`500.024780 g`、装载2早期+`500.021401 g`；卸载1早期-`500.025906 g`、卸载2早期-`500.016895 g`。末端跨度随零点及载荷演化变化，非独立“砝码质量偏差”。raw ADC 与 filtered ADC counts 同样移动，但不是滤波质量。装载1早期/末端 raw `485924/485983` counts，装载2 `485904.5/485956`；卸载1 `42048/41974`，卸载2 `42029/41959`。面板早期相对权威重量的差约 `-0.009942/-0.010794/-0.002743/+0.008658 g`（依装载1/装载2/卸载1/卸载2排序），量化与保持不用于拟合蠕变。

两个加载后权威重量都在早期升高、卸载后都负向移动；幅度并不一致。这支持“与装卸事件有关的变化”，但不是传感器蠕变的因果证明：公共零点热漂、机械夹具恢复、环境温度、真实慢速质量变化在单个 gross 观测中仍有混杂。

## 离线反事实与决策

`Tools/stage5pa9/a9_model_review.py` 使用旧 CSV 做过离线修订；新的 `Tools/stage5pa9/analyze_capture.py` 同时生成本次**全新开发数据**的只读、事后反事实。冻结 R5 Python `ReferenceLock` 全程单一 STATIC 状态实例，1秒中位数输入，识别4次自动阶跃重建、最大10秒 offset 变化287 ug。事件 oracle 使用事后已知边沿，提前进 DOSING、事件后60秒切 STATIC，从每次事件当时继承的真实模拟 offset 建立15–45秒 corrected reference；最大10秒 offset 变化500 ug，刚好是冻结上限。合成非零初值 `120000 ug` 仅用于单元测试，不是现场 offset；本次现场 offset 一直为0。

在本次装载1/2，冻结 R5 回放的第5分钟变化约+0.0974/+0.0636 g，事后 oracle 约+0.0846/+0.0506 g；至30分钟 oracle 约+0.0108/+0.0047 g。但其优势取决于未来边沿预知，不能据此选择可部署候选。卸载1/2 到30分钟，冻结 R5 回放相对早期为-0.0632/-0.0567 g，oracle 约-0.0026/+0.0006 g；第一卸载45分钟窗口无效。不能用这些事后窗口结果推断实际 R5 已补偿。本次没有温度或同条件空载传感器对照；真慢速加料与漂移仍不可辨。

**MODEL SELECTION INCOMPLETE**；不进入固定点 C、不启用产品补偿，不以这批新数据宣称独立算法验证。下一步若要选型，先补可区分输入（温度或同步空载对照、操作状态/慢速加料标记），在开发数据上冻结规则与参数，再对另一组未参与选型的数据进行独立 holdout。

## 回归、证据与后续

```text
python Tools/stage5pa9/test_a9_model_review.py
python Tools/stage5pa9/test_analyze_capture.py
python Tools/stage5pa9/a9_model_review.py
python Tools/stage5pa9/analyze_capture.py Results/stage5pa9/20260927T060806Z_development_capture
```

回归结果：`A9 FOUR NUMERICAL STATE/WINDOW TESTS PASS`、`A9 CAPTURE ANALYSIS SYNTHETIC PASS`。已保存 `Results/stage5pa9/opened_replay_trajectory.csv`：逐秒输入、冻结/事后 oracle 参考、offset、模式和重建原因；它属于旧数据回放，不是现场 R5 输出。新原始数据、事件标记、环境、summary、归因 JSON 和 SHA 归档在同一目录。尚未运行的独立 holdout、定点 C、温度归因、计量资格明确为 `NOT RUN`。

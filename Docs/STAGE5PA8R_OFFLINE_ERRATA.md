# Stage 5P-A8R：离线分析勘误

## 结论

本勘误从 Stage 5P-A8 提交 `6bdae58be1e1430a44df013a211b41b50d826895` 开始，未访问设备、未启动采集、未修改产品 R5、未烧录、未执行 SAVE/ZERO/TARE。

结论仍为：

```text
MODEL SELECTION INCOMPLETE
```

原 A8 报告保留为历史记录；本文件只记录勘误后的结果。

## SHA 勘误

原始 CSV 未修改：

```text
Results/stage5pa4/realtime_runs/20260925T121451Z_0x051C/samples.csv
```

正确 SHA-256：

```text
10545968A92A23688162A68D82DCFCF20C10FDD0ABDF3EC58203F6E549D0C3D2
```

A8 使用的预期字符串少了末尾 `2`。A8R 已修正脚本、结果和测试，当前 `sha_match=true`。

## 时间基准勘误

使用 `host_monotonic_ns`：

```text
monotonic span = 93187.109 s
```

UTC 标签跨度约为：

```text
93195.801 s
```

发现两次明显 UTC 跳变，示例：

```text
2026-09-25T13:00:28.607Z → 13:00:32.641Z
monotonic 约 0.5 s，UTC 约 4.034 s

2026-09-26T14:05:13.828Z → 14:05:19.487Z
monotonic 约 0.5 s，UTC 约 5.659 s
```

所有事件、阶段和检查点均使用 monotonic；UTC 仅作标签。

## 事件复核

基于 `gross_ug` 穿越 250 g 阈值，共识别：

- 3 次加载；
- 4 次卸载；
- 最长加载阶段 `65998.015 s`，约 18 小时 20 分钟；
- 每个边沿 bracket 约 0.500–0.516 s。

没有人工事件标记，边沿是事后数据推定。

## 检查点截断勘误

所有检查点现在都截断到下一次边沿之前。若某检查点起始时刻已越过下一边沿，则为 `null/NOT AVAILABLE`。

特别是第一次卸载的 45 分钟窗口现在明确不可用，不再输出下一次加载造成的约 500 g 数值。

有效的最长恒载和回零结果：

| 阶段 | 早期参考跨度 | 阶段末端跨度 |
|---|---:|---:|
| 第三次加载、最长恒载 | `500.025906 g` | `500.368316 g`（末 5 分钟相对装载前窗口） |
| 最后一次卸载 | `-500.026469 g` | `-500.123898 g` |

第三次加载相对装载后 15–45 秒参考的权威 gross 变化：

```text
1 min   +0.027 g
2 min   +0.037 g
5 min   +0.063 g
10 min  +0.060 g
30 min  +0.058 g
```

最后一次卸载相对卸载后 15–45 秒参考：

```text
1 min   -0.0107 g
2 min   -0.0197 g
5 min   -0.0355 g
10 min  -0.0434 g
15 min  -0.0529 g
30 min  -0.0805 g
45 min  -0.0929 g
```

`raw_adc`、`filtered_raw`、`gross_ug` 和显示字段均保留在逐事件 JSON；`filtered_raw` 仍只是 ADC counts，不是滤波后质量。

## 候选状态机勘误

### 事件建参 oracle 回放

A8 的旧逻辑在约 62 秒退出 DOSING 时已经不满足 `abs(edge-sec)<=60`，导致计算出的 15–45 秒参考实际未被使用。

A8R 修正为：

1. 边沿前后进入 DOSING；
2. DOSING 期间 offset 严格冻结；
3. 退出 DOSING 时锁定该边沿后 15–45 秒窗口的参考；
4. 参考使用补偿后重量 `gross - existing_offset`；
5. 仅在 STATIC 下按 50 ug/s 修正。

合成测试验证了：已有非零 offset 时参考值来自指定窗口，DOSING 不改变 offset，且窗口不跨事件。

### 冻结 R5 基线

A8R 已加入真正的 Python `ReferenceLock` 模型：

- 同一个状态实例贯穿全部周期；
- 包含真实阶跃识别和参考重建；
- 保留 automatic step、状态转换和 offset；
- 不再使用“从文件开头固定一个参考、跨 500 g 阶跃慢慢追赶”的简化器。

### 回放结果

冻结 R5 Python 基线：

```text
最大 10 s offset 变化：416 ug
```

事后已知边沿的 oracle 事件建参候选：

```text
修正速度：50 ug/s
最大 10 s offset 变化：500 ug
初始 offset：120000 ug（用于验证非零 offset 参考建立）
```

两者都只是同一已打开 CSV 上的开发回放。oracle 回放知道边沿，冻结 R5 不知道未来边沿；二者不能当作同类资格结果。

候选仍无法同时兼顾：

- 前 5 分钟改善；
- 18 小时级长载残差；
- 卸载后负向回零；
- 真实慢速加料不被吞掉。

因此不进入 C 实现，不启用正式补偿输出。

## 合成测试

新增测试：

```text
Tools/stage5pa8r/test_a8r_errata.py
```

覆盖：

- 已有非零 offset 的事件建参；
- 指定 15–45 秒参考窗口确实被锁定；
- DOSING 期间 offset 严格冻结；
- 冻结 R5 的真实阶跃状态机；
- 事件窗口不得跨下一事件；
- 10 秒修正限制的实际数值。

运行结果：

```text
STAGE5PA8R SYNTHETIC TEST PASS
```

## 交付物

```text
Tools/stage5pa8r/a8r_errata.py
Tools/stage5pa8r/test_a8r_errata.py
Results/stage5pa8r/review.json
Docs/STAGE5PA8R_OFFLINE_ERRATA.md
```

原 A8 文件和报告没有覆盖或删除：

```text
Docs/STAGE5PA8_NEW_WIRING_CREEP_REVIEW.md
Results/stage5pa8/review.json
```

## 后续数据边界

下一组装卸记录应定位为“解决模型选择问题的开发数据”，不能直接叫 holdout。

只有在候选参数和状态机先冻结后，另采的一组新数据才能作为独立 holdout。当前不启动新采集。

现场关于 S+/S− 已改正、已重新标定、使用 filt1 的信息仍是用户确认；改线时间、标定时间以及 filt1 strength 没有同步设备证据。

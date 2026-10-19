请在本机继续 A33 Stage 5P 0x0530 的离线证据复核与归档。无需再次询问常规文件读取、测试、文档勘误及已授权仓库内的独立分支提交/推送。

仓库：Metamorphos-1-s/stm32f103rbt6_a33。
原硬件证据提交：f5137c0f1eb6171d4f2e459bd28539f51fb8896f。
独立勘误分支：stage5p-zero-return-independent-audit。
原本机目录：D:/Documents/stm32f103rbt6_a33_stage5p_zero_return。
原证据目录：Results/stage5p_zero_return_readonly/20261019T033815Z/。

本任务只操作已有文件及 fake-client 测试，不连接 COM5、不采集新硬件数据。
不执行烧录、SWD、复位、ZERO、TARE、标定、SAVE、R5 切换或任何设备配置操作。

1. 检查现有工作区；保护未提交改动。fetch 原硬件分支与独立勘误分支，基于勘误分支建一个独立工作区/分支。记录实际 HEAD、tree 与远端同步情况，确认原证据提交是祖先，不覆盖其他工作区。
2. 阅读 Docs/STAGE5P_0530_ZERO_RETURN_INDEPENDENT_REVIEW.md 和勘误后的 Docs/STAGE5P_0530_ZERO_RETURN_READONLY_HARDWARE.md。复核三轮分解：
   - 第 1 轮：Δu +0.047520 g，Δoffset +0.037359 g，Δnet +0.010161 g。
   - 第 2 轮：Δu +0.083681 g，Δoffset +0.041590 g，Δnet +0.042091 g。
   - 第 3 轮：Δu +0.014969 g，Δoffset +0.022778 g，Δnet −0.007809 g。
   以上为均值；净重中位数差仍是 +0.010605 / +0.042973 / −0.009129 g。
3. 对归档中的两段 JSONL 和 frames 执行独立审计，把新结果写到一个新目录，不覆盖原始证据：
   python Tools/stage5_unified/audit_zero_return_atomic_0530.py --evidence Results/stage5p_zero_return_readonly/20261019T033815Z --output audit_out/analysis.json
   python -m unittest discover -s Tools/stage5_unified -p "test_zero_return*py" -v
   预期 27,043 行、6 个边沿、3 轮、33 个测试通过。若存在原本机补齐日志，审计的 missing_manifest_entries 可以从 5 变为 0；逐项按原清单核对长度和 SHA，不能只按文件名认定通过。
4. 原 manifest 列出但远端未提交的五份日志为：capture_stdout.log、capture_stderr.log、capture_tail_extension_stdout.log、capture_tail_extension_stderr.log、host_simulation_19_tests.log。仅在原本机查找已有原文件，与原 manifest 的 bytes/SHA256 全部相符才补交。Git 忽略 .log 时只对这五个经核验的明确路径使用 git add -f，保留原始字节。不添加其他日志、凭据或无关文件。
5. 找不到或哈希不符的项继续标为 MISSING / HASH_MISMATCH，不伪造，不修改原 manifest 以删除缺项，也不拿新运行日志冒充旧日志。保留新测试日志的独立日期和哈希。
6. 原始 JSONL、RTU 帧、analysis_local.json、原 manifest 均保持不变。把本轮结果、日志补齐状态、复现命令写入补充报告；新派生文件另建清单。将报告及核验通过的历史日志提交/推送到独立归档分支，给出最终 HEAD、tree、同步状态及缺项。

结论边界：原报告的 R5 分解表误用了旧数据，现已勘误；三轮早期中位数可复现。主记录末次卸载后覆盖约 283.563 秒，补采前缺口 238.359 秒，连续 600 秒仍 INCOMPLETE。即使日志补齐也不能改成连续 600 秒 PASS。不把同一输入轨迹的算术分解当成 OFF 物理对照，不宣布回零或计量资格通过。

归档完成即停止。本轮不开发新固件，也不重新安排三轮实板试验；滴水与 2/10 秒停顿继续 NOT RUN。

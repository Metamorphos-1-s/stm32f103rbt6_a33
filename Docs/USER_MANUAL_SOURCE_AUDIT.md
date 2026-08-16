# User Manual V0.9 Source Audit

## 1. 审计范围

本审计对应：

- 用户说明书：`Docs/User_Manual_V0.9.md`
- 文档版本：V0.9
- 适用固件：`stage5f-ui-tested`
- Firmware baseline：`0e10a53dd1b89eafc34b8dc3a95964394bc1c01b`
- Git tag object：`f409849e4a42a121eaefc0e89e5d96df30dcb809`
- Modbus Register Map：`0x0103`
- BLE Protocol：V1
- Config Schema：V2 / 344 B
- 审计日期：2026-08-16

状态定义：

- **Confirmed**：由当前源码、当前协议文档、Stage 5E/5F 实板记录或用户明确输入确认。
- **Constrained**：已确认功能语义，但正式产品数值或外观资料尚未冻结，说明书使用了限制性表述。
- **TBD**：必须等待 Stage 6 或正式产品资料，不得在 V0.9 中补全。
- **Reference only**：只参考说明书结构/表达，不作为本产品参数来源。

## 2. 基线与版本

| 章节/主题 | 事实 | 主要来源 | 状态 |
|---|---|---|---|
| 封面 | Manual V0.9、适用 `stage5f-ui-tested` | 本轮 Documentation Stage D1 要求 | Confirmed |
| 封面 | baseline `0e10a53`、tag object `f409849` | `git rev-parse`、`git cat-file -p stage5f-ui-tested` | Confirmed |
| 封面/附录 E | Firmware reported value `0x050A` | `Config/project_config.h`、`Docs/BLE_PROTOCOL_V1.md` | Confirmed |
| 封面/附录 E | Modbus map `0x0103` | `Docs/MODBUS_REGISTER_MAP_V1.md`、Stage 5E/5F 验证 | Confirmed |
| 封面/附录 E | BLE Protocol V1 | `Protocol/ble/ble_frame_codec.h`、`Docs/BLE_PROTOCOL_V1.md` | Confirmed |
| 封面/附录 E | Schema V2 / 344 B | `Services/config_store/persistent_schema.h`、Stage 5E/5F 验证 | Confirmed |
| 产品型号/公司名称 | 当前未冻结 | Documentation Stage D1 要求 | TBD |

## 3. 功能与硬件接口

| 章节/主题 | 事实 | 主要来源 | 状态 |
|---|---|---|---|
| 产品功能 | 六位显示、两点标定、ZERO、TARE、NET/GROSS、单位、保存、检重、通讯 | `Docs/PROJECT_STAGE_STATUS.md`、Stage 5E/5F 文档 | Confirmed |
| 传感器接口 | 逻辑信号 E+/E-/S+/S- | 用户 Documentation Stage D1 要求；称重传感器逻辑定义 | Confirmed |
| 端子位置 | 以硬件丝印为准 | 尚无正式外壳/端子排图 | Constrained |
| MCU/内部引脚 | CS1237、W02、RGY、蜂鸣器内部连接 | `CONFIGURATION_SUMMARY.md`、`Core/Inc/main.h` | Confirmed；正文避免暴露内部引脚 |
| RS232/RS485 | 共享 USART2 数据、地址和寄存器模型 | `Docs/MODBUS_REGISTER_MAP_V1.md` | Confirmed |
| BLE 模块 | W02 位于内部，无需普通用户接线 | 用户要求、`Docs/STAGE5C_A_BLE_TRANSPORT.md` | Confirmed |
| 电源额定范围 | 当前正式产品资料未提供 | Documentation Stage D1 禁止发明电气指标 | TBD |
| 产品外观/端子/铭牌 | 仅设置图片占位 | Documentation Stage D1 要求 | TBD |

## 4. 面板、按键与菜单

| 章节/主题 | 事实 | 主要来源 | 状态 |
|---|---|---|---|
| 五键名称 | FUNCTION、TARE、ZERO、STAR、HASH | `UI/key_service/key_types.h` | Confirmed |
| 正常页 FUNCTION | NET→GROSS→TARE→BATTERY；长按进菜单 | `App/app_main.c` | Confirmed |
| 正常页 TARE | 短按 TARE，长按 CLEAR TARE | `App/app_main.c`、`Domain/zero_tare/zero_tare.c` | Confirmed |
| 正常页 ZERO | 短按 ZERO，长按 RESET ZERO | `App/app_main.c`、`Domain/zero_tare/zero_tare.c` | Confirmed |
| 正常页 HASH | NET/GROSS 快速切换 | `App/app_main.c` | Confirmed |
| 正常页 STAR | 手动输出请求无外部传输；长按状态页 | `App/app_main.c`、`Protocol/command_service/command_service.c` | Confirmed |
| 长按时间 | 约 1.5 s | `Config/project_config.h` | Confirmed |
| STAR/HASH 重复 | 600 ms 后每 150 ms固定重复 | `Config/project_config.h`、`UI/key_service/key_service.c` | Confirmed |
| 无长按加速 | 固定重复，无加速 | Stage 5F 范围、KeyService 源码 | Confirmed |
| 菜单超时 | 30 s，取消未确认编辑并退出 | `Config/project_config.h`、`UI/menu_controller/menu_controller.c` | Confirmed |
| 普通菜单 | UnIt/PrOF/briGHt/trrEt/SAUE/EHIt | `UI/menu_controller/menu_controller.c::s_ordinary` | Confirmed |
| 高级入口 | UnIt 页 STAR/HASH/STAR/HASH | `UI/menu_controller/menu_controller.c::HandleAdvancedSequence` | Confirmed |
| 高级菜单顺序 | 当前 MenuItem 枚举和 `s_labels` 全量 | `UI/menu_controller/menu_types.h`、`menu_controller.c` | Confirmed |
| SPd/GAIn | 面板只读，显示 `rEAd` | `UI/menu_controller/menu_controller.c` | Confirmed |
| 恢复默认 | rESEt? 后 FUNCTION 长按确认，TARE 取消 | `UI/menu_controller/menu_controller.c`、`App/persistence_manager.c` | Confirmed |

## 5. 六位编辑与显示

| 章节/主题 | 事实 | 主要来源 | 状态 |
|---|---|---|---|
| 数字位顺序 | ZERO：右1→右2→右3→右4→右5→右6→右1 | `Docs/STAGE5F_SIX_DIGIT_EDIT_BLINK.md`、`UI/numeric_edit_cursor` | Confirmed |
| STAR/HASH 方向 | STAR 减小，HASH 增大 | `UI/menu_controller/menu_controller.c`、Stage 5F 实板验证 | Confirmed |
| 闪烁周期 | 250 ms 可见 / 250 ms 隐藏 | `Docs/STAGE5F_SIX_DIGIT_EDIT_BLINK.md` | Confirmed |
| 前导空白 | 0/空白闪烁提示选中位置 | Stage 5F 文档与实板验证 | Confirmed |
| 小数点 | 闪烁期间保持 | Stage 5F 文档与实板验证 | Confirmed |
| dIU | 1/2/5 | `UI/menu_controller/menu_controller.c`、`Services/config_edit/config_edit.c` | Confirmed |
| dP | 0～5，需通过完整配置验证 | 同上 | Confirmed |
| FILt | 0 None、1 Average、2 IIR、3 Median3+IIR | `Config/device_config.h`、菜单源码 | Confirmed |
| StAb 面板项 | 编辑当前配置档 stability_hold_ms，10～10000 ms有效 | 菜单源码、`metrology_config_validator.c` | Confirmed |
| briGHt | 0～7 | `Services/config_edit/config_edit.c` | Confirmed |
| 显示小数位≠准确度 | Stage 6 未完成 | Documentation Stage D1、`Docs/METROLOGY_REQUIREMENTS_V1.md` | Confirmed restriction |

## 6. ZERO、TARE 与保存

| 章节/主题 | 事实 | 主要来源 | 状态 |
|---|---|---|---|
| ZERO 条件 | 标定有效、稳定、无皮重、范围内、ZrnG>0 | `Domain/zero_tare/zero_tare.c` | Confirmed |
| TARE 条件 | 标定有效、稳定、不过载 | `Domain/zero_tare/zero_tare.c` | Confirmed |
| ZERO 持久性 | ZERO/RESET ZERO 为 RAM-only | `Docs/STAGE5A_CLOSEOUT_CONFIG_DIRTY.md` | Confirmed |
| TARE 持久性 | 由 trrEt 控制；变化使 dirty，需后续 SAVE | `Docs/STAGE5A_CLOSEOUT_CONFIG_DIRTY.md` | Confirmed |
| 参数确认 | RAM 立即生效并显示 rAnonL | `UI/menu_controller/menu_controller.c` | Confirmed |
| SAVE | 独立异步 Flash 操作 | `App/persistence_manager.c`、Stage 5E 验证 | Confirmed |
| SAVE 提示 | SAUE、donE、noCHG、ErrSAU | `UI/display_model/display_codes.c`、`persistence_manager.c` | Confirmed |
| Flash A/B | 两个 2 KiB 配置槽，CRC/序列恢复 | `persistent_schema.h`、Stage 4B/5E 文档 | Confirmed；正文不暴露地址 |

## 7. 标定

| 章节/主题 | 事实 | 主要来源 | 状态 |
|---|---|---|---|
| 标定类型 | 两点：空载零点 + 已知加载点 | `App/calibration_controller.c`、`Domain/calibration` | Confirmed |
| 本地流程 | UnLoAd→CAL 0→质量编辑→LoAd→CAL SP→donE→rAnonL | `calibration_controller.c`、`display_codes.c`、Stage 5C-D 验证 | Confirmed |
| 标定取消 | 任意活动阶段 TARE 取消 | `CalibrationController_HandleKeyEvent` | Confirmed |
| 标定质量 | >0、≤CAP、按单位/dP/dIU准确往返 | `calibration_controller.c`、`Docs/BLE_PROTOCOL_V1.md` | Confirmed |
| APPLY 与 SAVE | APPLY 只改 RAM；SAVE 才持久化 | Stage 5C-D、BLE Protocol V1 | Confirmed |
| 500 g 示例 | 仅操作示例，不作为通用推荐 | Stage 5C-D/5E 实板使用 500 g；D1 限制 | Constrained |
| 推荐砝码比例 | 未冻结 | Documentation Stage D1 | TBD |
| 标定稳定时间规格 | 未冻结；内部流程窗口不等于产品稳定时间 | D1 限制、Stage 5C-D | TBD |

## 8. 检重与报警

| 章节/主题 | 事实 | 主要来源 | 状态 |
|---|---|---|---|
| 基本边界 | `<Lo` LOW；`Lo..Hi` 含边界 OK；`>Hi` HIGH | `Domain/limit_checker/limit_checker.c`、Stage 5E 实板边界 | Confirmed |
| 灯色 | LOW 黄、OK 绿、HIGH/OL/FAULT 红 | `alarm_output_manager.c`、Stage 5E 实板 | Confirmed |
| HyS | LOW 恢复 Lo+HyS；HIGH 恢复 Hi-HyS | `limit_checker.c`、Stage 5E 实板 | Confirmed |
| HyS 校验 | ≥0 且≤(Hi-Lo)/2 | `Config/alarm_config_validation.c` | Confirmed |
| Src | NET/GROSS | `limit_checker.c`、Stage 5E 实板 | Confirmed |
| bIn/bEH/bOK | 独立启用 | `menu_controller.c`、`alarm_output_manager.c` | Confirmed |
| OK 短鸣 | 约 100 ms，仅新进入 OK | `alarm_output_manager.c`、Stage 5E 实板 | Confirmed |
| 报警节拍 | HIGH/OL/FAULT 约 250 ms ON/OFF | `alarm_output_manager.c`、Stage 5E 实板 | Confirmed |
| 稳定门控 | 无历史分类不稳定时禁用；有历史时可保持 | `limit_checker.c`、Stage 5E 快载验证 | Confirmed |
| 慢速变化稳定特性 | 当前短窗口可能持续 STABLE | Stage 5E P2 characterization | Confirmed；不写成缺陷/指标 |

## 9. Modbus

| 章节/主题 | 事实 | 主要来源 | 状态 |
|---|---|---|---|
| RTU 默认链路 | 地址1、115200、8N1、0 ms延时 | `Config/default_config.c`、Modbus文档 | Confirmed（可配置） |
| 功能码 | FC03、FC06、FC16 | `modbus_rtu_server.c`、Modbus文档 | Confirmed |
| 版本寄存器 | 000E=`0x0103`、000F=`0x050A` | Modbus文档、Stage 5E验证 | Confirmed |
| 多寄存器字序 | 当前配置决定，默认高字在前 | `default_config.c`、Modbus文档 | Confirmed |
| RS232/485一致模型 | 共用 USART2 字节和协议模型 | Modbus文档 | Confirmed |
| 配置事务 | staging→validate→apply→save | Modbus文档、CommandService | Confirmed |
| 64位质量写入 | 必须 FC16 原子四寄存器 | Modbus文档、Stage 5E 验证 | Confirmed |

## 10. BLE

| 章节/主题 | 事实 | 主要来源 | 状态 |
|---|---|---|---|
| W02 基础 | BLE 5.2、UART、FFE0/FFE1/FFE2、默认 UART 9600/8/N/1 | 用户 Documentation Stage D1 明确提供 | Confirmed by user |
| MCU 链路 | USART1 9600 8N1 | `Docs/BLE_PROTOCOL_V1.md`、Stage 5E 验证 | Confirmed |
| GATT | FFE0 服务、FFE1 Notify、FFE2 Write | Stage 5C-A 实板验证、D1输入 | Confirmed |
| 遥测类型 | FAST 0x01/5Hz、SLOW 0x02/1Hz、CHECKWEIGH 0x03/1Hz | BLE Protocol V1 | Confirmed |
| 命令 | REQUEST 0x80、RESPONSE 0x81 | BLE Protocol V1 | Confirmed |
| 支持操作 | 查询、称重操作、配置、保存和标定 0x30..0x36 | BLE Protocol V1 | Confirmed |
| 不支持操作 | Factory reset、通信 apply、OTA、AT、FF12、Runtime Drift控制 | BLE Protocol V1 | Confirmed |
| 分帧语义 | Notify/Write 边界不是协议帧边界 | BLE Protocol V1 | Confirmed |
| 测试设备名/地址 | 只作测试示例，不是产品固定值 | Stage 5C-A/5E 验证 | Constrained |

## 11. 状态、维护与 Stage 6

| 章节/主题 | 事实 | 主要来源 | 状态 |
|---|---|---|---|
| 显示代码 | 当前可达的 noCAL/UnStAb/ZrErr/trErr/OL/标定/SAVE/错误提示 | `UI/display_model/display_codes.c` 及各调用点 | Confirmed |
| 字形拼写 | InUALd、SAUE、ErrSAU、rAnonL 等为六位字形 | `display_codes.c` | Confirmed |
| 更换传感器后重标定 | 标定映射依赖传感器/机械结构 | 当前标定模型与维护常识 | Confirmed |
| 正式校验周期 | 未由产品/法规输入冻结 | D1 事实来源规则 | TBD |
| Stage 6 指标 | 准确度、重复性、线性、计量迟滞、温度、预热、稳定时间等 | D1 明确禁止提前填写 | TBD |
| Runtime Drift | 默认关闭、仅工程遥测、未计量验证 | `project_config.h`、Modbus/BLE文档 | Confirmed restriction |

## 12. 参考资料使用审计

| 资料 | 用途 | 参数是否复制 | 状态 |
|---|---|---|---|
| SJ101CX/SJ101T2 等同类手册 | 仅参考用户手册章节组织、操作步骤和表格形式 | 否 | Reference only |
| W02 厂商资料 | 确认用户明确提供的模块基础信息 | 仅使用 D1 明确确认的 BLE 5.2/UART/UUID/默认 UART | Reference only + user confirmed facts |
| 早期 Stage 4A/5A 文档 | 理解演进和语义 | 仅在当前源码/5E/5F复核后使用 | Reference only |

说明：当前 Codex 附件目录中没有可单独读取的 SJ101 或 W02 PDF/DOCX 文件，因此本稿未引用或复制任何参考产品的电源、量程、精度、传感器或环境参数。W02 基础信息只采用本轮 D1 文本中由用户明确提供的内容，并由当前项目 BLE 文档/实板记录交叉核对。

## 13. 审计结论

- 当前菜单、按键、六位选位、标定、检重、SAVE、Modbus `0x0103` 和 BLE V1 已与 `stage5f-ui-tested` 源码/验证记录核对。
- V0.9 未把 Stage 6 未完成项目写成已验证产品指标。
- 正式产品型号、公司名称、外观、端子图、电源/环境规格、计量性能和推荐参数仍为 TBD。
- 发现说明书与源码冲突时，应修改说明书；本轮不得修改固件以迎合文档。

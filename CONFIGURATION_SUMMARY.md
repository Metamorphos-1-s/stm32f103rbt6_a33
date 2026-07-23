# STM32F103RBT6 A33 配置摘要

## 完整引脚分配表

| 引脚 | 功能 | User Label | 模式/备注 |
|---|---|---|---|
| PA1 | RS485方向控制 | MCU_DE | GPIO推挽输出，低速，无上下拉，初始低 |
| PA2 | USART2_TX | MCU_TX | 异步串口发送 |
| PA3 | USART2_RX | MCU_RX | 异步串口接收 |
| PA8 | W02 PWR按键/唤醒 | W02_PWRKEY | GPIO开漏输出，低速，无上下拉，初始高（释放） |
| PA9 | USART1_TX | MCU_BLE_TX | 异步串口发送 |
| PA10 | USART1_RX | MCU_BLE_RX | 异步串口接收 |
| PA11 | USB_DM预留 | USB_DM_RESERVE | 当前未配置，不作为GPIO使用 |
| PA12 | USB_DP预留 | USB_DP_RESERVE | 当前未配置，不作为GPIO使用 |
| PA13 | SWDIO | - | Serial Wire调试保留 |
| PA14 | SWCLK | - | Serial Wire调试保留 |
| PA15 | 预留GPIO | - | 当前未配置；关闭JTAG后可作GPIO |
| PB3 | 预留GPIO | - | 当前未配置；关闭JTAG后可作GPIO |
| PB4 | 预留GPIO | - | 当前未配置；关闭JTAG后可作GPIO |
| PB5 | 板载蜂鸣器 | MCU_BUZZER | GPIO推挽输出，低速，无上下拉，初始低 |
| PB6 | 绿色灯珠 | MCU_RGY_G | GPIO推挽输出，低速，无上下拉，初始低 |
| PB7 | 红色灯珠 | MCU_RGY_R | GPIO推挽输出，低速，无上下拉，初始低 |
| PB8 | 黄色灯珠 | MCU_RGY_Y | GPIO推挽输出，低速，无上下拉，初始低 |
| PB9 | 外部蜂鸣器 | MCU_RGY_BUZZER | GPIO推挽输出，低速，无上下拉，初始低 |
| PB10 | AD串行时钟 | MCU_AD_SCLK | GPIO推挽输出，低速，无上下拉，初始低 |
| PB11 | AD数据输出 | MCU_AD_DOUT | GPIO输入，无上下拉 |
| PB12 | AD使能 | MCU_AD_EN | GPIO推挽输出，低速，无上下拉，初始低 |
| PC0 | ADC1_IN10 | MCU_VBAT_AD | 模拟输入，外部VCC四分之一分压 |
| PC7 | TM1628A DIO | MCU_TM_DIO | GPIO开漏输出，低速，无上下拉，初始高（释放） |
| PC8 | TM1628A CLK | MCU_TM_CLK | GPIO开漏输出，低速，无上下拉，初始高（释放） |
| PC9 | TM1628A STB | MCU_TM_STB | GPIO开漏输出，低速，无上下拉，初始高（释放） |
| PC14 | LSE_IN预留 | - | 当前未配置，LSE关闭 |
| PC15 | LSE_OUT预留 | - | 当前未配置，LSE关闭 |
| PD0 | HSE_IN | - | 8 MHz晶振输入 |
| PD1 | HSE_OUT | - | 8 MHz晶振输出 |

## CubeMX外设配置摘要

- SYS：Serial Wire，仅保留SWD；JTAG关闭以释放PA15/PB3/PB4。
- RCC：HSE晶体8 MHz；LSE关闭。
- USART1：PA9/PA10，异步，115200，8-N-1，无流控，16倍过采样。
- USART2：PA2/PA3，异步，115200，8-N-1，无流控，16倍过采样。
- ADC1：单通道ADC1_IN10，软件触发，单次转换，右对齐，239.5周期采样。
- 未启用USART3、SPI、I2C、USB、中间件、RTOS、DMA通道、定时器或网络栈。
- 外设初始化分别生成在 `gpio.c/.h`、`usart.c/.h`、`adc.c/.h`。

## 时钟树配置摘要

| 时钟 | 配置/频率 |
|---|---|
| HSE | 8 MHz晶体 |
| PLL | HSE x 9 |
| SYSCLK/HCLK | 72 MHz |
| APB1 | 36 MHz（HCLK / 2） |
| APB2 | 72 MHz（HCLK / 1） |
| ADC | 12 MHz（PCLK2 / 6） |
| Flash | 2 Wait States，Prefetch启用 |
| LSE | Disable |

## GPIO初始状态表

| 初始低 | 初始高（开漏释放） | 输入 |
|---|---|---|
| PA1 MCU_DE | PC7 MCU_TM_DIO | PB11 MCU_AD_DOUT（浮空） |
|  | PA8 W02_PWRKEY | PC0 MCU_VBAT_AD（模拟） |
|  | PC8 MCU_TM_CLK |  |
| PB5 MCU_BUZZER | PC9 MCU_TM_STB |  |
| PB6 MCU_RGY_G |  |  |
| PB7 MCU_RGY_R |  |  |
| PB8 MCU_RGY_Y |  |  |
| PB9 MCU_RGY_BUZZER |  |  |
| PB10 MCU_AD_SCLK |  |  |
| PB12 MCU_AD_EN |  |  |

初始化代码在配置GPIO模式之前先写输出数据寄存器，以减少上电瞬态误动作。

PB12 `MCU_AD_EN` 已由硬件负责人确认为高电平使能、低电平关闭；初始低电平为安全关闭状态。
TM1628 的 GRID1～GRID6 已确认为面板从左到右第1～第6位。

## ADC分压与换算

- 分压：`VADC = VCC x 10k / (30k + 10k) = VCC / 4`
- 反算：`VCC = VADC x 4`
- 12位ADC：`VADC = ADC_RAW / 4095 x VDDA`
- 最终：`VCC = ADC_RAW / 4095 x VDDA x 4`

业务代码应使用实测VDDA、VREFINT估算值或软件校准值，并考虑30k/10k电阻误差，不应固定假设VDDA精确为3.300 V。ADC在初始化后仅校准一次。

## TM1628A开漏接口说明

PC7、PC8、PC9均配置为开漏、No Pull、初始高。这里的高电平表示STM32输出管关闭，由TM1628A内部上拉产生高电平。读取DIO前必须先将PC7输出置1释放总线，或临时改为浮空输入；TM1628A驱动DIO时STM32不得继续拉低。接口供电不得超过5 V，超过5 V必须增加电平转换。

## 编译结果

- 工具链：GNU Arm Embedded GCC 13.3.1
- 配置：Debug/CMake/Ninja
- 结果：零错误完成链接
- FLASH：8356 B / 128 KB（6.38%）
- RAM：1776 B / 20 KB（8.67%）
- ELF：`build/Debug/stm32f103rbt6_a33.elf`

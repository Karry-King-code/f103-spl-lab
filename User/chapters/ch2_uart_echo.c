/**
  ******************************************************************************
  * @file    main.c
  * @brief   04 章：串口收发——USART1 115200-8-N-1，收什么回什么
  * @note    PA9=TX（复用推挽）、PA10=RX（浮空输入），经板载 CH340N 或外接
  *          USB-TTL 模块连电脑；每收到 1 字节回显 1 字节并翻转绿灯（PC13）。
  *          页码注释约定：RM0008 = 《STM32F103xx参考手册(中文参考)》，页码为 PDF 阅读器页码
  ******************************************************************************
  */
#include "stm32f10x.h"

volatile uint32_t g_rx_count = 0;            /* 已收字节数（可用调试器从内存读出验证） */

void delay_ms(uint32_t ms)
{
    uint32_t tick = SystemCoreClock / 8 / 1000;   /* SysTick 是内核外设，见 PM0056 第4章（RM0008 里没有） */
    SysTick->LOAD = tick - 1;                     /* 重装值：数到 0 自动重装（含 0 所以写 次数-1） */
    SysTick->VAL  = 0;                            /* 写任意值清当前计数与 COUNTFLAG */
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk;      /* bit0=1 启动（时钟源=HCLK/8） */
    while (ms--)
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0)
        {                                         /* bit16=COUNTFLAG：数到 0 置位，读即清 = 1ms 到 */
        }
    SysTick->CTRL = 0;
}

/* ---------- 发送：等 TXE 再写 DR ---------- */
void uart1_send(uint8_t b)
{
    USART_SendData(USART1, b);                                    /* 底层=写 USART1->DR 数据寄存器(基址0x40013800+偏移0x04)；寄存器组表 RM0008 中文版 PDF 第517页 */
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)  /* 底层=读 USART1->SR 的 bit7(TXE 发送数据寄存器空)；位表 RM0008 中文版 PDF 第521页 */
    {
    }
}
void uart1_str(const char *s)
{
    while (*s)
        uart1_send((uint8_t)*s++);
}

int main(void)
{
    GPIO_InitTypeDef  gpio;
    USART_InitTypeDef usart;

    SystemCoreClockUpdate();
    /* USART1 挂 APB2，和 GPIOA/GPIOC 同一条时钟线 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC |
                           RCC_APB2Periph_USART1, ENABLE);        /* 底层=一次置 RCC->APB2ENR 的 bit2(IOPAEN)+bit4(IOPCEN)+bit14(USART1EN)；地址=0x40021000+0x18；RM0008 中文版 PDF 第77页 */

    /* PA9=USART1_TX：复用推挽（引脚控制权交给 USART 外设） */
    gpio.GPIO_Pin   = GPIO_Pin_9;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;                            /* 复用推挽 = CNF=10 + MODE=11(50MHz) = 0b1011，填进 CRH[7:4]；RM0008 中文版 PDF 第113页(编码表)/第114页(CRH位表) */
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);
    /* PA10=USART1_RX：浮空输入（电平由对方决定） */
    gpio.GPIO_Pin   = GPIO_Pin_10;
    gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;                      /* 浮空输入 = CNF=01 + MODE=00 = 0b0100，填进 CRH[11:8]（错到 [7:4] 就会把 PA9 改成输入，TX 直接哑） */
    GPIO_Init(GPIOA, &gpio);
    /* PC13 绿灯：回显反馈 */
    gpio.GPIO_Pin   = GPIO_Pin_13;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &gpio);
    GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);                   /* 底层=GPIOC->BSRR 置位（高=灭） */

    /* 115200-8-N-1，收+发，无流控 */
    usart.USART_BaudRate            = 115200;                     /* 底层=算 BRR：72MHz/16/115200=39.0625 → 整数39(0x27)进[15:4]、小数1进[3:0] = 0x271；RM0008 中文版 PDF 第525页 */
    usart.USART_WordLength          = USART_WordLength_8b;        /* 底层=CR1 的 M 位(bit12)=0 表示 8 位 */
    usart.USART_StopBits            = USART_StopBits_1;           /* 底层=CR2 的 STOP 位段=00 表示 1 位停止 */
    usart.USART_Parity              = USART_Parity_No;            /* 底层=CR1 的 PCE(bit10)=0 无校验 */
    usart.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;  /* 底层=CR1 的 TE(bit3)+RE(bit2) */
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART1, &usart);                                   /* 底层=写 USART1->BRR/CR1/CR2；组表 RM0008 中文版 PDF 第517页、CR1 位表第518页 */
    USART_Cmd(USART1, ENABLE);                                    /* 底层=置 USART1->CR1 的 bit13(UE 总使能)；PDF 第518页。注意：标准库内部先置 UE 再置 TE/RE 顺序安全 */

    uart1_str("\r\n=== STM32F103 USART1 READY (115200-8-N-1) ===\r\n");
    uart1_str("Type anything, I will echo it back. LED toggles per byte.\r\n");

    while (1)
    {
        if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET)   /* 底层=读 SR 的 bit5(RXNE 读数据寄存器非空)；PDF 第523页 */
        {
            uint8_t b = (uint8_t)USART_ReceiveData(USART1);          /* 底层=读 USART1->DR —— 这个读动作本身就自动清 RXNE */
            uart1_send(b);                   /* 读 DR 会自动清 RXNE */
            GPIO_WriteBit(GPIOC, GPIO_Pin_13,
                (BitAction)!GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_13));
            g_rx_count++;
        }
    }
}

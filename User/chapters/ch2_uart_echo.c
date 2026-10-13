/**
  ******************************************************************************
  * @file    main.c
  * @brief   04 章：串口收发——USART1 115200-8-N-1，收什么回什么
  * @note    PA9=TX（复用推挽）、PA10=RX（浮空输入），经板载 CH340N 或外接
  *          USB-TTL 模块连电脑；每收到 1 字节回显 1 字节并翻转绿灯（PC13）。
  ******************************************************************************
  */
#include "stm32f10x.h"

volatile uint32_t g_rx_count = 0;            /* 已收字节数（可用调试器从内存读出验证） */

void delay_ms(uint32_t ms)
{
    uint32_t tick = SystemCoreClock / 8 / 1000;
    SysTick->LOAD = tick - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk;
    while (ms--)
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0)
        {
        }
    SysTick->CTRL = 0;
}

/* ---------- 发送：等 TXE 再写 DR ---------- */
void uart1_send(uint8_t b)
{
    USART_SendData(USART1, b);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
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
                           RCC_APB2Periph_USART1, ENABLE);

    /* PA9=USART1_TX：复用推挽（引脚控制权交给 USART 外设） */
    gpio.GPIO_Pin   = GPIO_Pin_9;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);
    /* PA10=USART1_RX：浮空输入（电平由对方决定） */
    gpio.GPIO_Pin   = GPIO_Pin_10;
    gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);
    /* PC13 绿灯：回显反馈 */
    gpio.GPIO_Pin   = GPIO_Pin_13;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &gpio);
    GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);

    /* 115200-8-N-1，收+发，无流控 */
    usart.USART_BaudRate            = 115200;
    usart.USART_WordLength          = USART_WordLength_8b;
    usart.USART_StopBits            = USART_StopBits_1;
    usart.USART_Parity              = USART_Parity_No;
    usart.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART1, &usart);
    USART_Cmd(USART1, ENABLE);

    uart1_str("\r\n=== STM32F103 USART1 READY (115200-8-N-1) ===\r\n");
    uart1_str("Type anything, I will echo it back. LED toggles per byte.\r\n");

    while (1)
    {
        if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET)
        {
            uint8_t b = (uint8_t)USART_ReceiveData(USART1);
            uart1_send(b);                   /* 读 DR 会自动清 RXNE */
            GPIO_WriteBit(GPIOC, GPIO_Pin_13,
                (BitAction)!GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_13));
            g_rx_count++;
        }
    }
}

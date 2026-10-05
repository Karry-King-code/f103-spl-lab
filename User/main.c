/**
  ******************************************************************************
  * @file    main.c
  * @brief   07 章：光敏模块 DO 读取 + 串口打印（烟雾传感器替身，标准库版）
  * @note    光敏 DO = PB13（比较器推挽输出，电平由模块电位器阈值决定）
  *          MQ-2 烟雾模块同为 DO 输出——代码只差"名字"，买回接同脚即用。
  *          电平变化即打印（事件驱动）；'?' 立即查询；SWD 读 IDR 可交叉验证。
  ******************************************************************************
  */
#include "stm32f10x.h"

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

static void uart1_send(uint8_t b)
{
    USART_SendData(USART1, b);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
    {
    }
}
static void uart1_str(const char *s)
{
    while (*s)
        uart1_send((uint8_t)*s++);
}

int main(void)
{
    GPIO_InitTypeDef  gpio;
    USART_InitTypeDef usart;
    uint8_t last = 0xFF;                     /* 强制首次打印 */
    uint16_t n = 0;

    SystemCoreClockUpdate();
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_GPIOC | RCC_APB2Periph_USART1, ENABLE);

    /* PB13 = 光敏 DO：浮空输入（模块比较器推挽输出，无需上拉） */
    gpio.GPIO_Pin  = GPIO_Pin_13;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &gpio);
    /* PC13 心跳灯 */
    gpio.GPIO_Pin   = GPIO_Pin_13;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &gpio);
    GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);

    /* USART1 115200-8-N-1 */
    gpio.GPIO_Pin  = GPIO_Pin_9;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);
    gpio.GPIO_Pin  = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);
    usart.USART_BaudRate            = 115200;
    usart.USART_WordLength          = USART_WordLength_8b;
    usart.USART_StopBits            = USART_StopBits_1;
    usart.USART_Parity              = USART_Parity_No;
    usart.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART1, &usart);
    USART_Cmd(USART1, ENABLE);

    uart1_str("\r\n=== CH07 LIGHT SENSOR (SPL) READY ===\r\n");
    uart1_str("Prints on level change. '?'=query. PB13=DO\r\n");

    while (1)
    {
        uint8_t now = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13);

        if (now != last)                     /* 电平变化 -> 事件打印 */
        {
            last = now;
            uart1_str("SMOKE(CH07 LIGHT) DO=");
            uart1_send((uint8_t)('0' + now));
            uart1_str(now ? "  (normal/bright)\r\n" : "  (ALARM/dark!)\r\n");
        }

        if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET)
        {
            uint8_t b = (uint8_t)USART_ReceiveData(USART1);
            if (b == '?')
            {
                uart1_str("[QUERY] DO=");
                uart1_send((uint8_t)('0' + GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13)));
                uart1_str("\r\n");
            }
        }

        delay_ms(10);                        /* 节拍 + 心跳灯 */
        if (++n >= 50)
        {
            n = 0;
            GPIO_WriteBit(GPIOC, GPIO_Pin_13,
                (BitAction)!GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_13));
        }
    }
}

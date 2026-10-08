/**
  ******************************************************************************
  * @file    main.c
  * @brief   05 章：按键控制蜂鸣器+继电器（标准库版）
  * @note    KEY1=PB7 蜂鸣器开关；KEY2=PB6 继电器开关（消抖+等释放，同 03 章）
  *          蜂鸣器=PC15（NPN 驱动，高电平响，有源⚠️待实物确认）
  *          继电器=PC14（**实测低电平吸合**，与原理图 NPN 拓扑相反，以实物为准）
  *          串口（115200）同步打印状态；另支持串口命令 B/R/? 远程控制（验证用）
  ******************************************************************************
  */
#include "stm32f10x.h"

volatile uint32_t g_rx_count = 0;

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

/* ---------- 串口（04 章同款） ---------- */
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

/* ---------- 蜂鸣器/继电器：宏定义让"有效电平"一处可改 ---------- */
#define BUZZ_ON()   GPIO_WriteBit(GPIOC, GPIO_Pin_15, Bit_SET)    /* 高电平响【原理图 NPN】 */
#define BUZZ_OFF()  GPIO_WriteBit(GPIOC, GPIO_Pin_15, Bit_RESET)
#define RELAY_ON()  GPIO_WriteBit(GPIOC, GPIO_Pin_14, Bit_RESET)  /* 低电平吸合【实测】 */
#define RELAY_OFF() GPIO_WriteBit(GPIOC, GPIO_Pin_14, Bit_SET)

static uint8_t buzz_state = 0, relay_state = 0;

static void print_state(void)
{
    uart1_str("[STATE] BUZZER=");
    uart1_str(buzz_state ? "ON" : "OFF");
    uart1_str("  RELAY=");
    uart1_str(relay_state ? "ON(closed)" : "OFF(open)");
    uart1_str("  (KEY1=buzz KEY2=relay, serial cmd: B/R/?=status)\r\n");
}

static void toggle_buzz(void)
{
    buzz_state ^= 1;
    if (buzz_state)
        BUZZ_ON();
    else
        BUZZ_OFF();
    uart1_str("[KEY1/CMD B] ");
    print_state();
}
static void toggle_relay(void)
{
    relay_state ^= 1;
    if (relay_state)
        RELAY_ON();
    else
        RELAY_OFF();
    uart1_str("[KEY2/CMD R] ");
    print_state();
}

/* ---------- 按键：沿检测+双消抖+等释放（03 章版本 B 同款） ---------- */
static uint8_t key_pressed(uint16_t pin)
{
    static uint8_t last1 = 1, last2 = 1;
    uint8_t *last = (pin == GPIO_Pin_7) ? &last1 : &last2;
    uint8_t now = GPIO_ReadInputDataBit(GPIOB, pin);
    uint8_t evt = 0;

    if (*last == 1 && now == 0)
    {
        delay_ms(10);
        if (GPIO_ReadInputDataBit(GPIOB, pin) == 0)
        {
            evt = 1;
            while (GPIO_ReadInputDataBit(GPIOB, pin) == 0)
            {
            }
            delay_ms(10);
        }
    }
    *last = now;
    return evt;
}

int main(void)
{
    GPIO_InitTypeDef  gpio;
    USART_InitTypeDef usart;

    SystemCoreClockUpdate();
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_GPIOC | RCC_APB2Periph_USART1, ENABLE);

    /* 按键 PB7/PB6：上拉输入 */
    gpio.GPIO_Pin  = GPIO_Pin_7 | GPIO_Pin_6;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &gpio);

    /* PC13 心跳灯 + PC14 继电器 + PC15 蜂鸣器：全部推挽输出（CRH 三格一次配） */
    gpio.GPIO_Pin   = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &gpio);
    GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);   /* 灯灭 */
    RELAY_OFF();                                   /* 继电器初态=释放（高） */
    BUZZ_OFF();                                    /* 蜂鸣器初态=不响（低） */

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

    uart1_str("\r\n=== CH05 BUZZER+RELAY (SPL) READY ===\r\n");
    print_state();

    while (1)
    {
        if (key_pressed(GPIO_Pin_7))
            toggle_buzz();
        if (key_pressed(GPIO_Pin_6))
            toggle_relay();

        if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET)
        {
            uint8_t b = (uint8_t)USART_ReceiveData(USART1);
            g_rx_count++;
            switch (b)
            {
            case 'B':
            case 'b':
                toggle_buzz();
                break;
            case 'R':
            case 'r':
                toggle_relay();
                break;
            case '?':
                print_state();
                break;
            default:
                break;                               /* 其他字符忽略 */
            }
        }
    }
}

/**
  ******************************************************************************
  * @file    main.c
  * @brief   06 章：DHT11 温湿度读取 + 串口打印（标准库版）
  * @note    DHT11 数据线 = PB15（单总线：MCU 拉低 20ms 起始 -> 释放 ->
  *          DHT 回 80us 低+80us 高 -> 40 位数据，每位 50us 低 + 高宽 27us(0)/70us(1)）
  *          微秒延时用 DWT 周期计数器（内核外设，三条腿通用）。
  *          每 2 秒读一次并打印；串口 '?' 立即读一次。校验和失败打印 FAIL。
  ******************************************************************************
  */
#include "stm32f10x.h"

/* ---------- DWT 微秒延时（内核 32 位周期计数器，本工程 CMSIS 较老无结构体，裸地址定义） ---------- */
#define DEMCR      (*(volatile uint32_t *)0xE000EDFC)   /* 调试异常与监视控制寄存器 */
#define DWT_CTRL   (*(volatile uint32_t *)0xE0001000)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004)   /* 周期计数器（32 位回绕） */
static void dwt_init(void)
{
    DEMCR |= (1u << 24);            /* TRCENA：打开跟踪 */
    DWT_CYCCNT = 0;
    DWT_CTRL |= 1u;                 /* CYCCNTENA：计数器跑起来 */
}
static void delay_us(uint32_t us)
{
    uint32_t start = DWT_CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    while ((DWT_CYCCNT - start) < ticks)
    {
    }
}
void delay_ms(uint32_t ms)
{
    while (ms--)
        delay_us(1000);
}

/* ---------- 串口（04 章同款） ---------- */
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
static void uart1_dec(uint8_t v)
{
    uart1_send((uint8_t)('0' + v / 100));
    uart1_send((uint8_t)('0' + v / 10 % 10));
    uart1_send((uint8_t)('0' + v % 10));
}

/* ---------- DHT11 单总线 ---------- */
#define DHT_OUT_LOW()  do { GPIO_InitTypeDef g = { GPIO_Pin_15, GPIO_Speed_2MHz, GPIO_Mode_Out_PP }; \
                            GPIO_WriteBit(GPIOB, GPIO_Pin_15, Bit_RESET); GPIO_Init(GPIOB, &g); } while (0)
#define DHT_RELEASE()  do { GPIO_InitTypeDef g = { GPIO_Pin_15, GPIO_Speed_2MHz, GPIO_Mode_IPU }; \
                            GPIO_Init(GPIOB, &g); } while (0)
#define DHT_READ()     GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_15)

static uint8_t g_stage = 0;      /* 诊断：记录当前协议阶段 */
static uint32_t g_edge_t[128];   /* 诊断：边沿时间戳 */
static uint8_t  g_edge_lv[128];
static uint8_t  g_edge_n = 0;

/* 等待线变为 level，超时返回 1（约 250us 上限），并记录边沿 */
static uint8_t wait_line(uint8_t level)
{
    uint32_t t0 = DWT_CYCCNT;
    while (DHT_READ() != level)
        if ((DWT_CYCCNT - t0) > 5000 * 72)
            return 1;
    if (g_edge_n < 128)
    {
        g_edge_t[g_edge_n] = DWT_CYCCNT;
        g_edge_lv[g_edge_n] = level;
        g_edge_n++;
    }
    return 0;
}

/* 读 40 位；返回 0=成功 1=超时 2=校验和错 */
static uint8_t dht11_read(uint8_t d[5])
{
    uint8_t i, j;

    for (i = 0; i < 5; i++)
        d[i] = 0;

    /* ① 起始：拉低 >=18ms 再释放 */
    DHT_OUT_LOW();
    delay_ms(20);
    DHT_RELEASE();
    delay_us(50);
    DHT_OUT_LOW();           /* 第二次起始：兼容部分慢克隆 */
    delay_ms(20);
    g_edge_n = 0;
    {
        uint32_t t0 = DWT_CYCCNT;
        DHT_RELEASE();
        g_edge_t[0] = t0;        /* 起点=释放时刻 */
        g_edge_lv[0] = 0xFF;     /* 特殊标记 */
        g_edge_n = 1;
    }

    /* ② DHT 响应：80us 低 + 80us 高 */
    g_stage = 2;
    if (wait_line(0)) return 1;
    if (wait_line(1)) return 1;
    if (wait_line(0)) return 1;

    /* ③ 40 位：每位 50us 低起头，随后高宽 27us=0 / 70us=1 */
    g_stage = 3;
    for (i = 0; i < 5; i++)
    {
        for (j = 0; j < 8; j++)
        {
            if (wait_line(1)) return 1;      /* 越过 50us 低电平 */
            delay_us(40);                    /* 站在 40us 处采样 */
            if (DHT_READ())
            {
                d[i] = (uint8_t)(d[i] << 1) | 1u;   /* 还在高 = 1（70us 宽） */
                if (wait_line(0)) return 1;         /* 等这位结束 */
            }
            else
            {
                d[i] = (uint8_t)(d[i] << 1);        /* 已变低 = 0（27us 宽） */
            }
        }
    }
    /* ④ 校验：前 4 字节之和低 8 位 == 第 5 字节 */
    if ((uint8_t)(d[0] + d[1] + d[2] + d[3]) != d[4])
        return 2;
    return 0;
}

int main(void)
{
    GPIO_InitTypeDef  gpio;
    USART_InitTypeDef usart;
    uint8_t d[5];
    uint32_t blink = 0;

    SystemCoreClockUpdate();
    dwt_init();

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_GPIOC | RCC_APB2Periph_USART1, ENABLE);

    /* PC13 心跳灯 */
    gpio.GPIO_Pin   = GPIO_Pin_13;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &gpio);
    GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);

    /* PB15 先保持释放（上拉输入，模块侧有上拉） */
    DHT_RELEASE();

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

    uart1_str("\r\n=== CH06 DHT11 (SPL) READY ===\r\n");
    uart1_str("Reading every 2s; send '?' to read now.\r\n");

    while (1)
    {
        uint8_t err = dht11_read(d);
        if (err == 0)
        {
            uart1_str("DHT11  HUMI=");
            uart1_dec(d[0]);
            uart1_send('.');
            uart1_dec(d[1]);
            uart1_str("%  TEMP=");
            uart1_dec(d[2]);
            uart1_send('.');
            uart1_dec(d[3]);
            uart1_str("C\r\n");
        }
        else
        {
            if (err == 1)
            {
                uint8_t k;
                uint32_t prev = g_edge_t[0];
                uart1_str("DHT11 FAIL stage=");
                uart1_send((uint8_t)('0' + g_stage));
                uart1_str(" edges=");
                uart1_send((uint8_t)('0' + g_edge_n / 10));
                uart1_send((uint8_t)('0' + g_edge_n % 10));
                uart1_str(" [");
                for (k = 1; k < g_edge_n && k < 20; k++)
                {
                    uint32_t d = (g_edge_t[k] - prev) / 72;
                    uart1_send(g_edge_lv[k] ? 'H' : 'L');
                    uart1_send((uint8_t)('0' + d / 100));
                    uart1_send((uint8_t)('0' + d / 10 % 10));
                    uart1_send((uint8_t)('0' + d % 10));
                    uart1_send(' ');
                    prev = g_edge_t[k];
                }
                uart1_str("]\r\n");
            }
            else
                uart1_str("DHT11 CHECKSUM ERROR\r\n");
        }

        /* 心跳灯翻转 + 等 2 秒（期间响应 '?'） */
        {
            uint16_t n;
            for (n = 0; n < 200; n++)
            {
                delay_ms(10);
                if (++blink >= 50)
                {
                    blink = 0;
                    GPIO_WriteBit(GPIOC, GPIO_Pin_13,
                        (BitAction)!GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_13));
                }
                if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET)
                {
                    uint8_t b = (uint8_t)USART_ReceiveData(USART1);
                    if (b == '?')
                        break;                   /* 立即进入下一轮读取 */
                }
            }
        }
    }
}

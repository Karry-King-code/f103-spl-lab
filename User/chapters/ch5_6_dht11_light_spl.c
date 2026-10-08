/**
  ******************************************************************************
  * @file    main.c
  * @brief   任务五+六（标准库版）：DHT11 温湿度 + 光敏 串口打印
  * @note    DHT11 数据线 = PB14（B14 飞线，B15 孔磨损已弃用）
  *          光敏 DO = PB12，必须「上拉输入」（开漏输出，无上拉读不出高电平）
  *          串口 USART1 PA9/PA10 115200-8-N-1；同时把结果写 RAM 0x20004000
  *          供无串口时用 ST-Link 直接读（RES[0]=光敏 RES[1]=err RES[2]=湿度
  *          RES[3]=温度 RES[4]=成功计数）
  ******************************************************************************
  */
#include "stm32f10x.h"

#define RES ((volatile uint8_t *)0x20004000)

/* ---------- DWT 微秒延时（DHT 时序要微秒级） ---------- */
#define DEMCR      (*(volatile uint32_t *)0xE000EDFC)
#define DWT_CTRL   (*(volatile uint32_t *)0xE0001000)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004)
static void dwt_init(void){ DEMCR |= (1u<<24); DWT_CYCCNT=0; DWT_CTRL |= 1u; }
static void delay_us(uint32_t us){ uint32_t s=DWT_CYCCNT; while((DWT_CYCCNT-s) < us*72){} }
static void delay_ms(uint32_t ms){ while(ms--) delay_us(1000); }

/* ---------- 串口 ---------- */
static void uart1_send(uint8_t b)
{
    USART_SendData(USART1, b);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) { }
}
static void uart1_str(const char *s){ while (*s) uart1_send((uint8_t)*s++); }
static void uart1_num(uint8_t v){ uart1_send((uint8_t)('0'+v/100)); uart1_send((uint8_t)('0'+v/10%10)); uart1_send((uint8_t)('0'+v%10)); }

/* ---------- DHT11 单总线（PB14） ---------- */
static void dht_out_low(void){ GPIOB->CRH=(GPIOB->CRH&0xF0FFFFFFU)|0x02000000U; GPIOB->ODR&=~(1u<<14); }
static void dht_rel(void){ GPIOB->CRH=(GPIOB->CRH&0xF0FFFFFFU)|0x08000000U; GPIOB->ODR|=(1u<<14); }
#define DHT_READ() ((GPIOB->IDR>>14)&1u)

static uint8_t wait_line(uint8_t lv)
{
    uint32_t t0 = DWT_CYCCNT;
    while (DHT_READ() != lv)
        if ((DWT_CYCCNT - t0) > 5000u*72) return 1;   /* 5ms 超时 */
    return 0;
}
/* 返回 0=成功 1=超时 2=校验错 */
static uint8_t dht11_read(uint8_t d[5])
{
    uint8_t i, j;
    for(i=0;i<5;i++) d[i]=0;
    dht_out_low(); delay_ms(20); dht_rel();          /* 起始：拉低 20ms 再释放 */
    if(wait_line(0)) return 1;                        /* DHT 应答：低 */
    if(wait_line(1)) return 1;                        /* 应答低结束 -> 高 */
    if(wait_line(0)) return 1;                        /* 应答高结束=首个数据位低 ★关键一步 */
    for(i=0;i<5;i++){
        for(j=0;j<8;j++){
            if(wait_line(1)) return 1;                /* 每位 50us 低结束 */
            delay_us(40);                             /* 40us 处采样：高=1 低=0 */
            d[i] = (uint8_t)(d[i]<<1);
            if(DHT_READ()){ d[i] |= 1; if(wait_line(0)) return 1; }
        }
    }
    if((uint8_t)(d[0]+d[1]+d[2]+d[3]) != d[4]) return 2;
    return 0;
}

int main(void)
{
    GPIO_InitTypeDef  gpio;
    USART_InitTypeDef usart;
    uint8_t d[5], err, light, ok_cnt = 0;

    SystemCoreClockUpdate();
    dwt_init();
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_GPIOC | RCC_APB2Periph_USART1, ENABLE);

    /* PB12 光敏 DO：上拉输入（开漏输出必须上拉！标准库用 GPIO_Mode_IPU） */
    gpio.GPIO_Pin  = GPIO_Pin_12;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &gpio);
    /* PB14 DHT 数据线：先浮空输入（读取时函数内部切换） */
    gpio.GPIO_Pin  = GPIO_Pin_14;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &gpio);
    /* PC13 心跳灯 */
    gpio.GPIO_Pin   = GPIO_Pin_13;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &gpio);
    GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);

    /* USART1 */
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

    uart1_str("\r\n=== TASK5+6 SPL: DHT11(PB14) + LIGHT(PB12) ===\r\n");

    while (1)
    {
        light = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12);
        err = dht11_read(d);

        /* 串口打印 */
        uart1_str("LIGHT=");  uart1_send((uint8_t)('0'+light));
        if (err == 0) {
            uart1_str("  HUMI="); uart1_num(d[0]); uart1_send('.'); uart1_send((uint8_t)('0'+d[1])); uart1_send('%');
            uart1_str("  TEMP="); uart1_num(d[2]); uart1_send('.'); uart1_send((uint8_t)('0'+d[3])); uart1_str("C\r\n");
            ok_cnt++;
        } else {
            uart1_str(err==1 ? "  DHT11 TIMEOUT (power-cycle VCC if stuck)\r\n" : "  DHT11 CHECKSUM ERR\r\n");
        }

        /* 写 RAM 供 ST-Link 直接读 */
        RES[0] = light; RES[1] = err; RES[2] = d[0]; RES[3] = d[2]; RES[4] = ok_cnt;

        delay_ms(2000);
        GPIO_WriteBit(GPIOC, GPIO_Pin_13, (BitAction)!GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_13));
    }
}

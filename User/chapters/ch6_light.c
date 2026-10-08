/**
  ******************************************************************************
  * @file    main.c
  * @brief   08 章预览：OLED(SSD1306 0x3C) 软件I2C显示测试（标准库版）
  * @note    PB8=SCL PB9=SDA 软件模拟 I2C；显示 F103 SMART ENV + 实时光敏值。
  *          光敏 DO=PB13，屏幕第二行实时刷新；PC13 心跳。
  ******************************************************************************
  */
#include "stm32f10x.h"

/* ---------- 5x7 字库（ASCII 0x20~0x5F，列序） ---------- */
static const uint8_t FONT5X7[] = {
0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x5F,0x00,0x00,
0x00,0x07,0x00,0x07,0x00, 0x14,0x7F,0x14,0x7F,0x14,
0x24,0x2A,0x7F,0x2A,0x12, 0x23,0x13,0x08,0x64,0x62,
0x36,0x49,0x55,0x22,0x50, 0x00,0x05,0x03,0x00,0x00,
0x00,0x1C,0x22,0x41,0x00, 0x00,0x41,0x22,0x1C,0x00,
0x14,0x08,0x3E,0x08,0x14, 0x08,0x08,0x3E,0x08,0x08,
0x00,0x50,0x30,0x00,0x00, 0x08,0x08,0x08,0x08,0x08,
0x00,0x60,0x60,0x00,0x00, 0x20,0x10,0x08,0x04,0x02,
0x3E,0x51,0x49,0x45,0x3E, 0x00,0x42,0x7F,0x40,0x00,
0x42,0x61,0x51,0x49,0x46, 0x21,0x41,0x45,0x4B,0x31,
0x18,0x14,0x12,0x7F,0x10, 0x27,0x45,0x45,0x45,0x39,
0x3C,0x4A,0x49,0x49,0x30, 0x01,0x71,0x09,0x05,0x03,
0x36,0x49,0x49,0x49,0x36, 0x06,0x49,0x49,0x29,0x1E,
0x00,0x36,0x36,0x00,0x00, 0x00,0x56,0x36,0x00,0x00,
0x08,0x14,0x22,0x41,0x00, 0x14,0x14,0x14,0x14,0x14,
0x00,0x41,0x22,0x14,0x08, 0x02,0x01,0x51,0x09,0x06,
0x32,0x49,0x79,0x41,0x3E, 0x7E,0x11,0x11,0x11,0x7E,
0x7F,0x49,0x49,0x49,0x36, 0x3E,0x41,0x41,0x41,0x22,
0x7F,0x41,0x41,0x22,0x1C, 0x7F,0x49,0x49,0x49,0x41,
0x7F,0x09,0x09,0x09,0x01, 0x3E,0x41,0x49,0x49,0x7A,
0x7F,0x08,0x08,0x08,0x7F, 0x00,0x41,0x7F,0x41,0x00,
0x20,0x40,0x41,0x3F,0x01, 0x7F,0x08,0x14,0x22,0x41,
0x7F,0x40,0x40,0x40,0x40, 0x7F,0x02,0x0C,0x02,0x7F,
0x7F,0x04,0x08,0x10,0x7F, 0x3E,0x41,0x41,0x41,0x3E,
0x7F,0x09,0x09,0x09,0x06, 0x3E,0x41,0x51,0x21,0x5E,
0x7F,0x09,0x19,0x29,0x46, 0x46,0x49,0x49,0x49,0x31,
0x01,0x01,0x7F,0x01,0x01, 0x3F,0x40,0x40,0x40,0x3F,
0x1F,0x20,0x40,0x20,0x1F, 0x3F,0x40,0x38,0x40,0x3F,
0x63,0x14,0x08,0x14,0x63, 0x07,0x08,0x70,0x08,0x07,
0x61,0x51,0x49,0x45,0x43
};

/* ---------- 软件 I2C（PB8=SCL PB9=SDA） ---------- */
static void dly(void){ volatile uint32_t n=2000; while(n--){} }
static void sda_lo(void){ GPIO_InitTypeDef g; g.GPIO_Pin=GPIO_Pin_9; g.GPIO_Speed=GPIO_Speed_2MHz; g.GPIO_Mode=GPIO_Mode_Out_PP; GPIO_WriteBit(GPIOB,GPIO_Pin_9,Bit_RESET); GPIO_Init(GPIOB,&g); }
static void sda_rel(void){ GPIO_WriteBit(GPIOB,GPIO_Pin_9,Bit_SET); GPIO_InitTypeDef g; g.GPIO_Pin=GPIO_Pin_9; g.GPIO_Speed=GPIO_Speed_2MHz; g.GPIO_Mode=GPIO_Mode_IPU; GPIO_Init(GPIOB,&g); }  /* ODR=1 先置位！CNF=10+ODR=0=下拉（ch03 陷阱） */
static uint8_t sda_rd(void){ return (uint8_t)GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_9); }
static void scl_hi(void){ GPIO_WriteBit(GPIOB,GPIO_Pin_8,Bit_SET); }
static void scl_lo(void){ GPIO_WriteBit(GPIOB,GPIO_Pin_8,Bit_RESET); }

static void i2c_start(void){ sda_rel(); dly(); scl_hi(); dly(); sda_lo(); dly(); scl_lo(); dly(); }
static void i2c_stop(void){ sda_lo(); dly(); scl_hi(); dly(); sda_rel(); dly(); }
static uint8_t i2c_wr(uint8_t b){ uint8_t i,ack;
  for(i=0;i<8;i++){ if(b&0x80) sda_rel(); else sda_lo(); b<<=1; dly(); scl_hi(); dly(); scl_lo(); }
  sda_rel(); dly(); scl_hi(); dly(); ack=sda_rd(); scl_lo(); return ack; }

static void oled_cmd(uint8_t c){ i2c_start(); i2c_wr(0x78); i2c_wr(0x00); i2c_wr(c); i2c_stop(); }
static void oled_data(uint8_t d){ i2c_start(); i2c_wr(0x78); i2c_wr(0x40); i2c_wr(d); i2c_stop(); }

static void oled_init(void)
{
    uint8_t i;
    for(i=0;i<200;i++) dly();
    oled_cmd(0xAE); oled_cmd(0xD5); oled_cmd(0x80);
    oled_cmd(0xA8); oled_cmd(0x3F); oled_cmd(0xD3); oled_cmd(0x00);
    oled_cmd(0x40); oled_cmd(0x8D); oled_cmd(0x14); oled_cmd(0x20);
    oled_cmd(0x02); oled_cmd(0xA1); oled_cmd(0xC8); oled_cmd(0xDA);
    oled_cmd(0x12); oled_cmd(0x81); oled_cmd(0xCF); oled_cmd(0xD9);
    oled_cmd(0xF1); oled_cmd(0xDB); oled_cmd(0x40); oled_cmd(0xA4);
    oled_cmd(0xA6); oled_cmd(0xAF);
}
static void oled_pos(uint8_t page, uint8_t col)
{
    oled_cmd((uint8_t)(0xB0 | page));
    oled_cmd((uint8_t)(0x00 | (col & 0x0F)));
    oled_cmd((uint8_t)(0x10 | (col >> 4)));
}
static void oled_print(uint8_t page, uint8_t col, const char *s)
{
    oled_pos(page, col);
    while (*s)
    {
        uint8_t c = (uint8_t)*s++;
        uint8_t i;
        if (c < 0x20 || c > 0x5F) c = ' ';
        for (i = 0; i < 5; i++)
            oled_data(FONT5X7[(c - 0x20) * 5 + i]);
        oled_data(0x00);
    }
}
static void oled_clear_page(uint8_t page)
{
    uint8_t i;
    oled_pos(page, 0);
    for (i = 0; i < 128; i++) oled_data(0x00);
}

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

int main(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef u;
    uint8_t disp = 0xFF;
    uint16_t n = 0;

    SystemCoreClockUpdate();
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_GPIOC | RCC_APB2Periph_USART1, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_8;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &gpio);
    scl_hi(); sda_rel();
    gpio.GPIO_Pin = GPIO_Pin_13;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOC, &gpio);
    GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);

    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);
    u.USART_BaudRate = 115200; u.USART_WordLength = USART_WordLength_8b;
    u.USART_StopBits = USART_StopBits_1; u.USART_Parity = USART_Parity_No;
    u.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    u.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART1, &u); USART_Cmd(USART1, ENABLE);

    delay_ms(100);
    oled_init();
    /* 翻车#11 已修：sda_rel() 之前漏置 ODR9=1，内部下拉拖死 SDA（ch03 同款陷阱） */
    oled_clear_page(0); oled_clear_page(2); oled_clear_page(4);
    oled_print(0, 0, "F103 SMART ENV");
    oled_print(2, 0, "LIGHT:");
    oled_print(4, 0, "HUMI:--  TEMP:--");
    {
        const char *m = "\r\n=== CH08 OLED ONLINE (0x3C) ===\r\n";
        while (*m) { USART_SendData(USART1, (uint8_t)*m++); while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {} }
    }

    while (1)
    {
        uint8_t now = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12);
        if (now != disp)
        {
            disp = now;
            oled_print(2, 48, now ? "OK  " : "DARK");
        }
        delay_ms(10);
        if (++n >= 50)
        {
            n = 0;
            GPIO_WriteBit(GPIOC, GPIO_Pin_13,
                (BitAction)!GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_13));
        }
    }
}

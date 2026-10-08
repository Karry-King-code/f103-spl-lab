/**
  ******************************************************************************
  * @file    main.c
  * @brief   任务八（标准库版）：SPI W25Q128 读 JEDEC ID + 读数据
  * @note    板载 W25Q128JV（16MB）挂在 SPI2：CS=PB12 SCK=PB13 MISO=PB14 MOSI=PB15
  *          SPI2 在 APB1(36MHz)，分频 8 → 4.5MHz，模式 0（CPOL=0 CPHA=0）
  *          读 ID 命令 0x9F：返回 厂商(EF) + 类型(40) + 容量(18=128Mbit)
  *          结果写 RAM 0x20004000：[0]=0xAA 表示读到厂商ID [1..3]=ID [4..7]=首4字节
  *          ⚠️ 烧录前请拔掉光敏(DO线)和温湿度(DAT线)——它们和 Flash 共用 PB12/PB14
  ******************************************************************************
  */
#include "stm32f10x.h"

#define RES ((volatile uint8_t *)0x20004000)

static void uart1_send(uint8_t b){ USART_SendData(USART1, b); while(USART_GetFlagStatus(USART1, USART_FLAG_TXE)==RESET){} }
static void uart1_str(const char *s){ while(*s) uart1_send((uint8_t)*s++); }
static void uart1_hex(uint8_t v){ const char *h="0123456789ABCDEF"; uart1_send((uint8_t)h[v>>4]); uart1_send((uint8_t)h[v&15]); }

/* ---- 片选：PB12 手动控制（低=选中） ---- */
static void cs_low(void){ GPIO_WriteBit(GPIOB, GPIO_Pin_12, Bit_RESET); }
static void cs_high(void){ GPIO_WriteBit(GPIOB, GPIO_Pin_12, Bit_SET); }

/* ---- SPI2 收发一个字节 ---- */
static uint8_t spi_xfer(uint8_t b)
{
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET) {}
    SPI_I2S_SendData(SPI2, b);
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET) {}
    return (uint8_t)SPI_I2S_ReceiveData(SPI2);
}

int main(void)
{
    GPIO_InitTypeDef  gpio;
    SPI_InitTypeDef   spi;
    USART_InitTypeDef usart;
    uint8_t id[3], data0[4], sr;
    uint8_t i;

    SystemCoreClockUpdate();
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_GPIOC | RCC_APB2Periph_USART1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    /* PB12 片选：推挽输出，空闲拉高 */
    gpio.GPIO_Pin = GPIO_Pin_12;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &gpio);
    cs_high();
    /* PB13=SCK / PB15=MOSI 复用推挽；PB14=MISO 浮空输入 */
    gpio.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_14;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &gpio);

    /* SPI2：主机 8 位 模式0 软件片选 4.5MHz */
    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;
    spi.SPI_CPOL = SPI_CPOL_Low;
    spi.SPI_CPHA = SPI_CPHA_1Edge;
    spi.SPI_NSS = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
    spi.SPI_FirstBit = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial = 7;
    SPI_Init(SPI2, &spi);
    SPI_Cmd(SPI2, ENABLE);

    /* USART1 115200 */
    gpio.GPIO_Pin = GPIO_Pin_9;  gpio.GPIO_Mode = GPIO_Mode_AF_PP;          GPIO_Init(GPIOA, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_10; gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;    GPIO_Init(GPIOA, &gpio);
    usart.USART_BaudRate = 115200; usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1; usart.USART_Parity = USART_Parity_No;
    usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART1, &usart); USART_Cmd(USART1, ENABLE);

    uart1_str("\r\n=== TASK8 SPL: W25Q128 SPI READ ID ===\r\n");

    /* ① 读状态寄存器 0x05（bit0=1 表示忙） */
    cs_low(); spi_xfer(0x05); sr = spi_xfer(0xFF); cs_high();
    uart1_str("STATUS=0x"); uart1_hex(sr); uart1_str(sr==0 ? " (idle)\r\n" : " (BUSY!)\r\n");

    /* ② 读 JEDEC ID 0x9F：厂商 + 类型 + 容量 */
    cs_low();
    spi_xfer(0x9F);
    id[0] = spi_xfer(0xFF);
    id[1] = spi_xfer(0xFF);
    id[2] = spi_xfer(0xFF);
    cs_high();
    uart1_str("JEDEC ID: "); uart1_hex(id[0]); uart1_send(' '); uart1_hex(id[1]); uart1_send(' '); uart1_hex(id[2]);
    if (id[0] == 0xEF && id[1] == 0x40 && id[2] == 0x18) uart1_str("  -> W25Q128 (16MB) OK\r\n");
    else if (id[0] == 0xEF && id[1] == 0x40 && id[2] == 0x17) uart1_str("  -> W25Q64 (8MB) OK\r\n");
    else uart1_str("  -> 非 Winbond 或读取失败\r\n");

    /* ③ 读地址 0x000000 起 4 字节（0x03 读命令 + 24 位地址） */
    cs_low();
    spi_xfer(0x03);
    spi_xfer(0x00); spi_xfer(0x00); spi_xfer(0x00);
    for(i=0;i<4;i++) data0[i] = spi_xfer(0xFF);
    cs_high();
    uart1_str("DATA[0..3]: ");
    for(i=0;i<4;i++){ uart1_hex(data0[i]); uart1_send(' '); }
    uart1_str("\r\n");

    /* 写 RAM 供 ST-Link 直接读 */
    RES[0] = (id[0]==0xEF) ? 0xAA : 0x00;
    RES[1]=id[0]; RES[2]=id[1]; RES[3]=id[2];
    RES[4]=data0[0]; RES[5]=data0[1]; RES[6]=data0[2]; RES[7]=data0[3];

    while(1){ }
}

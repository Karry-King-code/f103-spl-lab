/**
  ******************************************************************************
  * @file    main.c
  * @brief   任务八（标准库版）：SPI W25Q 完整测试——擦除/写入/读回/断电不丢
  * @note    板载 Flash 挂 SPI2：CS=PB12 SCK=PB13 MISO=PB14 MOSI=PB15
  *          流程：①读ID ②读地址0的5字节 → 若等于 "STM32" 说明数据还在（断电不丢！）
  *                否则：擦除扇区 → 写 "STM32" → 读回比对
  *          结果写 RAM 0x20004000：[0]=0xAA(ID OK) [1]=0xBB(写成功) [2]=0xCC(数据持久)
  *          ⚠️ 做写入测试必须拔掉光敏(PB12)和温湿度(PB14)的线
  ******************************************************************************
  */
#include "stm32f10x.h"

#define RES ((volatile uint8_t *)0x20004000)
#define MAGIC "STM32"
#define ADDR  0x000000u          /* 测试用地址（从 0 开始，共 2MB 空间） */

static void uart1_send(uint8_t b){ USART_SendData(USART1, b); while(USART_GetFlagStatus(USART1, USART_FLAG_TXE)==RESET){} }
static void uart1_str(const char *s){ while(*s) uart1_send((uint8_t)*s++); }
static void uart1_hex(uint8_t v){ const char *h="0123456789ABCDEF"; uart1_send((uint8_t)h[v>>4]); uart1_send((uint8_t)h[v&15]); }

static void cs_low(void){ GPIO_WriteBit(GPIOB, GPIO_Pin_12, Bit_RESET); }
static void cs_high(void){ GPIO_WriteBit(GPIOB, GPIO_Pin_12, Bit_SET); }

static uint8_t spi_xfer(uint8_t b)
{
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET) {}
    SPI_I2S_SendData(SPI2, b);
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET) {}
    return (uint8_t)SPI_I2S_ReceiveData(SPI2);
}

/* 等芯片不忙（读状态寄存器 0x05，bit0=1 表示正在擦/写） */
static void wait_ready(void)
{
    uint8_t sr;
    do {
        cs_low(); spi_xfer(0x05); sr = spi_xfer(0xFF); cs_high();
    } while (sr & 0x01);
}

/* 读 5 字节到 buf */
static void flash_read(uint32_t addr, uint8_t *buf, uint8_t n)
{
    uint8_t i;
    cs_low();
    spi_xfer(0x03);
    spi_xfer((uint8_t)(addr>>16)); spi_xfer((uint8_t)(addr>>8)); spi_xfer((uint8_t)addr);
    for(i=0;i<n;i++) buf[i] = spi_xfer(0xFF);
    cs_high();
}

int main(void)
{
    GPIO_InitTypeDef  gpio;
    SPI_InitTypeDef   spi;
    USART_InitTypeDef usart;
    uint8_t id[3], buf[5], i, same;
    const char *magic = MAGIC;

    SystemCoreClockUpdate();
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_GPIOC | RCC_APB2Periph_USART1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    /* 引脚：PB12=CS 输出；PB13/15=SPI 复用推挽；PB14=输入 */
    gpio.GPIO_Pin = GPIO_Pin_12; gpio.GPIO_Mode = GPIO_Mode_Out_PP; gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &gpio); cs_high();
    gpio.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15; gpio.GPIO_Mode = GPIO_Mode_AF_PP; gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_14; gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &gpio);

    /* SPI2：主机 8位 模式0 4.5MHz */
    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;
    spi.SPI_CPOL = SPI_CPOL_Low;
    spi.SPI_CPHA = SPI_CPHA_1Edge;
    spi.SPI_NSS = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
    spi.SPI_FirstBit = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial = 7;
    SPI_Init(SPI2, &spi); SPI_Cmd(SPI2, ENABLE);

    /* 串口 */
    gpio.GPIO_Pin = GPIO_Pin_9;  gpio.GPIO_Mode = GPIO_Mode_AF_PP;       GPIO_Init(GPIOA, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_10; gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING; GPIO_Init(GPIOA, &gpio);
    usart.USART_BaudRate = 115200; usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1; usart.USART_Parity = USART_Parity_No;
    usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART1, &usart); USART_Cmd(USART1, ENABLE);

    /* PC13 心跳灯 */
    gpio.GPIO_Pin = GPIO_Pin_13; gpio.GPIO_Mode = GPIO_Mode_Out_PP; gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &gpio);

    uart1_str("\r\n=== TASK8 SPL: FLASH WRITE/PERSIST TEST ===\r\n");

    /* ① 读 ID */
    cs_low(); spi_xfer(0x9F);
    id[0]=spi_xfer(0xFF); id[1]=spi_xfer(0xFF); id[2]=spi_xfer(0xFF);
    cs_high();
    uart1_str("JEDEC ID: "); uart1_hex(id[0]); uart1_send(' '); uart1_hex(id[1]); uart1_send(' '); uart1_hex(id[2]);
    uart1_str(" (W25Q 系列)\r\n");
    RES[0] = (id[0]==0xEF) ? 0xAA : 0x00;

    /* ② 先读地址0的5字节 */
    flash_read(ADDR, buf, 5);
    uart1_str("READ before: ");
    for(i=0;i<5;i++){ uart1_hex(buf[i]); uart1_send(' '); }
    uart1_str("\r\n");

    same = 1;
    for(i=0;i<5;i++) if(buf[i] != (uint8_t)magic[i]) same = 0;

    if (same) {
        /* 数据还在 → 上一次写入经过复位依然保留 → 这就是 Flash 的意义！ */
        uart1_str("PERSIST OK: \"STM32\" survived reset! (flash keeps data)\r\n");
        RES[2] = 0xCC;
    } else {
        /* ③ 擦除扇区（0x20，4KB） */
        uart1_str("Erasing sector...\r\n");
        cs_low(); spi_xfer(0x06); cs_high();           /* 写使能 */
        cs_low();
        spi_xfer(0x20);
        spi_xfer((uint8_t)(ADDR>>16)); spi_xfer((uint8_t)(ADDR>>8)); spi_xfer((uint8_t)ADDR);
        cs_high();
        wait_ready();
        uart1_str("Erase done\r\n");

        /* ④ 写入 "STM32"（0x02 页编程） */
        cs_low(); spi_xfer(0x06); cs_high();           /* 每次写前都要写使能 */
        cs_low();
        spi_xfer(0x02);
        spi_xfer((uint8_t)(ADDR>>16)); spi_xfer((uint8_t)(ADDR>>8)); spi_xfer((uint8_t)ADDR);
        for(i=0;i<5;i++) spi_xfer((uint8_t)magic[i]);
        cs_high();
        wait_ready();
        uart1_str("Write done\r\n");

        /* ⑤ 读回比对 */
        flash_read(ADDR, buf, 5);
        uart1_str("READ after : ");
        for(i=0;i<5;i++){ uart1_hex(buf[i]); uart1_send(' '); }
        same = 1;
        for(i=0;i<5;i++) if(buf[i] != (uint8_t)magic[i]) same = 0;
        uart1_str(same ? " -> VERIFY PASS\r\n" : " -> VERIFY FAIL\r\n");
        RES[1] = same ? 0xBB : 0x00;
        if (same) uart1_str("Now press RESET: data should stay (persist test)\\r\\n");
    }

    while(1){ }
}

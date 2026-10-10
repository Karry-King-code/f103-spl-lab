/**
  ******************************************************************************
  * @file    main.c
  * @brief   任务九 终版：DHT11(PB14)+光敏(PB12) -> OLED(PB8/PB9)+串口1+OneNET云端 三处一致
  * @note    代码来源(全部是烧录验证过的,不引入新写法):
  *            - WiFi+TCP+手工MQTT  = 本文件上一版(实测 code:200)
  *            - DHT11单总线(PB14)  = SPL腿 ch5_6_dht11_light_spl.c (带5ms超时保护)
  *            - 光敏 DO(PB12,上拉) = SPL腿 ch5_6_dht11_light_spl.c
  *            - OLED 软件 I2C      = 寄存器腿 ch5_6_7_dht11_light_oled_reg.c
  *              (PB8=SCL, PB9=SDA, 屏幕四行, 已在寄存器腿实测)
  *
  *   数据流: DHT11/光敏 -> 全局值 -> (1)OLED刷新 (2)串口1打印 (3)MQTT上传云端
  *   上传策略: 只有拿到过一次成功的 DHT11 读数才发布(绝不传假值);
  *             DHT 失败时 OLED 显示 "--", 云端保持上一次成功值(OneJSON可只传light).
  *   周期: 每轮约2秒(读DHT+刷新OLED), 每3轮发布一次到云端(约6~8秒一条)。
  *   串口打印一律 ASCII(中文只写注释), 否则 AC5 报 #870-D 警告且乱码。
  ******************************************************************************
  */
#include "stm32f10x.h"

#define RES   ((volatile uint32_t *)0x20004000)

/* ---------------- 账号参数(改这里就能换设备/换WiFi) ---------------- */
#define WIFI_SSID   "1"
#define WIFI_PWD    "987654321"

#define MQTT_HOST   "mqtts.heclouds.com"
#define MQTT_PORT   "1883"
#define MQTT_CLIENT "dev1"
#define MQTT_USER   "T67q8WAChA"
#define MQTT_PASS   "version=2018-10-31&res=products/T67q8WAChA/devices/dev1&et=2106900860&method=md5&sign=fKgrFdzCemZ72jpNBVxrHA=="
#define KEEPALIVE   60

#define TOPIC_POST  "$sys/T67q8WAChA/dev1/thing/property/post"
#define TOPIC_SUB   "$sys/T67q8WAChA/dev1/thing/property/post/reply"

/* ================= DWT 微秒延时(DHT11 时序必须微秒级) ================= */
#define DEMCR      (*(volatile uint32_t *)0xE000EDFC)
#define DWT_CTRL   (*(volatile uint32_t *)0xE0001000)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004)
static void dwt_init(void){ DEMCR |= (1u<<24); DWT_CYCCNT=0; DWT_CTRL |= 1u; }
static void delay_us(uint32_t us){ uint32_t s=DWT_CYCCNT; while((DWT_CYCCNT-s) < us*72){} }
static void delay_ms(uint32_t ms){ while(ms--) delay_us(1000); }

/* ---------------- 串口环形缓冲区 + 中断接收 ----------------
   环形: head/tail 是"总计数", 下标取模。旧版线性缓冲区收满1500就聋了(踩过坑) */
#define RXCAP 2000
volatile uint32_t rx_head = 0, rx_tail = 0, ore_n = 0;
volatile uint8_t  rx_buf[RXCAP];
static uint8_t  saw_prompt = 0;

void USART2_IRQHandler(void)
{
    uint32_t sr = USART2->SR;
    uint32_t dr = USART2->DR;
    if (sr & USART_SR_ORE) ore_n++;
    if (sr & USART_SR_RXNE)
    {
        rx_buf[rx_head % RXCAP] = (uint8_t)dr;
        rx_head++;
    }
}

/* ---------------- 串口1: 打印 ---------------- */
static void uart1_init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef u;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);
    gpio.GPIO_Pin = GPIO_Pin_9;  gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;  GPIO_Init(GPIOA, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_10; gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);
    u.USART_BaudRate = 115200; u.USART_WordLength = USART_WordLength_8b;
    u.USART_StopBits = USART_StopBits_1; u.USART_Parity = USART_Parity_No;
    u.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    u.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &u);
    USART_Cmd(USART1, ENABLE);
}
static void u1ch(char c)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) { }
    USART_SendData(USART1, (uint16_t)c);
}
static void u1str(const char *s) { while (*s) u1ch(*s++); }
static void u1num(uint32_t v)
{
    char b[11]; int i = 0;
    if (v == 0) { u1ch('0'); return; }
    while (v) { b[i++] = (char)('0' + v % 10); v /= 10; }
    while (i--) u1ch(b[i]);
}

/* ---------------- 串口2: 发字节给 ESP ---------------- */
static void u2put(char c)
{
    uint32_t t = 2000000;
    while (!(USART2->SR & USART_SR_TXE) && t) t--;
    if (t) USART2->DR = (uint16_t)c;
}
static void u2str(const char *s) { while (*s) u2put(*s++); }

static void flush_rx(void)
{
    uint32_t h = rx_head;
    while (rx_tail < h)
    {
        uint8_t c = rx_buf[rx_tail % RXCAP];
        rx_tail++;
        if (c == '>') saw_prompt = 1;
        if (c == '\n') { u1ch('\r'); u1ch('\n'); }
        else if (c == '\r') { }
        else if (c >= 32 && c < 127) u1ch((char)c);
        else { u1ch('['); u1num(c); u1ch(']'); }
    }
}

static void pump(uint32_t ms)
{
    uint32_t k;
    for (k = 0; k < ms; k++) { flush_rx(); delay_ms(1); }
    flush_rx();
}

static void at_cmd(const char *cmd, uint32_t ms)
{
    u1str("\r\n===== SEND: "); u1str(cmd); u1str(" =====\r\n");
    flush_rx();
    rx_tail = rx_head;                     /* 丢弃上一条的尾巴, 本段只看本条回复 */
    u2str(cmd); u2put('\r'); u2put('\n');
    pump(ms);
}

/* 在环形缓冲区里找特征字节串(按总计数下标, 取模访问) */
static long find_pat(uint32_t from, const uint8_t *pat, uint32_t plen)
{
    uint32_t i, j, h = rx_head;
    if (h < plen || from > h - plen) return -1;
    for (i = from; i + plen <= h; i++)
    {
        for (j = 0; j < plen; j++)
            if (rx_buf[(i + j) % RXCAP] != pat[j]) break;
        if (j == plen) return (long)i;
    }
    return -1;
}

static const uint8_t PAT_OK[2] = { 'O', 'K' };

static void wait_ready(void)
{
    uint32_t i, base;
    for (i = 0; i < 10; i++)
    {
        base = rx_head;
        at_cmd("AT", 1200);
        if (find_pat(base, PAT_OK, 2) >= 0)
        {
            u1str("[+] module ready (got OK)\r\n");
            return;
        }
        u1str("[..] no OK yet, retry\r\n");
    }
    u1str("[!] module never answered OK - check power / TX-RX wiring\r\n");
}

/* ================= OLED 软件 I2C(PB8=SCL, PB9=SDA) =================
   来自寄存器腿 ch5_6_7(实测), 寄存器写法在 SPL 工程里同样可用 */
static void dly(void){ volatile uint32_t n=800; while(n--){} }
#define SCL_BIT  (1u<<8)
#define SDA_BIT  (1u<<9)
static void cfg_scl_out(void){ GPIOB->CRH=(GPIOB->CRH&0xFFFFFFF0U)|0x00000002U; GPIOB->ODR|=SCL_BIT; }
static void cfg_sda_in(void){ GPIOB->CRH=(GPIOB->CRH&0xFFFFFF0FU)|0x00000080U; GPIOB->ODR|=SDA_BIT; }
static void cfg_sda_lo(void){ GPIOB->CRH=(GPIOB->CRH&0xFFFFFF0FU)|0x00000020U; GPIOB->ODR&=~SDA_BIT; }
static void scl_hi(void){ GPIOB->ODR |= SCL_BIT; }
static void scl_lo(void){ GPIOB->ODR &= ~SCL_BIT; }
static uint8_t sda_rd(void){ return (uint8_t)((GPIOB->IDR & SDA_BIT)?1:0); }
static uint8_t wr(uint8_t b){ uint8_t i,ack;
  for(i=0;i<8;i++){ if(b&0x80) cfg_sda_in(); else cfg_sda_lo(); b<<=1; dly(); scl_hi(); dly(); scl_lo(); }
  cfg_sda_in(); dly(); scl_hi(); dly(); ack=sda_rd(); scl_lo(); return ack; }
static void start(void){ cfg_sda_in(); dly(); scl_hi(); dly(); cfg_sda_lo(); dly(); scl_lo(); dly(); }
static void stop(void){ cfg_sda_lo(); dly(); scl_hi(); dly(); cfg_sda_in(); dly(); }

static const uint8_t F57[] = {
0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x5F,0x00,0x00, 0x00,0x07,0x00,0x07,0x00, 0x14,0x7F,0x14,0x7F,0x14,
0x24,0x2A,0x7F,0x2A,0x12, 0x23,0x13,0x08,0x64,0x62, 0x36,0x49,0x55,0x22,0x50, 0x00,0x05,0x03,0x00,0x00,
0x00,0x1C,0x22,0x41,0x00, 0x00,0x41,0x22,0x1C,0x00, 0x14,0x08,0x3E,0x08,0x14, 0x08,0x08,0x3E,0x08,0x08,
0x00,0x50,0x30,0x00,0x00, 0x08,0x08,0x08,0x08,0x08, 0x00,0x60,0x60,0x00,0x00, 0x20,0x10,0x08,0x04,0x02,
0x3E,0x51,0x49,0x45,0x3E, 0x00,0x42,0x7F,0x40,0x00, 0x42,0x61,0x51,0x49,0x46, 0x21,0x41,0x45,0x4B,0x31,
0x18,0x14,0x12,0x7F,0x10, 0x27,0x45,0x45,0x45,0x39, 0x3C,0x4A,0x49,0x49,0x30, 0x01,0x71,0x09,0x05,0x03,
0x36,0x49,0x49,0x49,0x36, 0x06,0x49,0x49,0x29,0x1E, 0x00,0x36,0x36,0x00,0x00, 0x00,0x56,0x36,0x00,0x00,
0x08,0x14,0x22,0x41,0x00, 0x14,0x14,0x14,0x14,0x14, 0x00,0x41,0x22,0x14,0x08, 0x02,0x01,0x51,0x09,0x06,
0x32,0x49,0x79,0x41,0x3E, 0x7E,0x11,0x11,0x11,0x7E, 0x7F,0x49,0x49,0x49,0x36, 0x3E,0x41,0x41,0x41,0x22,
0x7F,0x41,0x41,0x22,0x1C, 0x7F,0x49,0x49,0x49,0x41, 0x7F,0x09,0x09,0x09,0x01, 0x3E,0x41,0x49,0x49,0x7A,
0x7F,0x08,0x08,0x08,0x7F, 0x00,0x41,0x7F,0x41,0x00, 0x20,0x40,0x41,0x3F,0x01, 0x7F,0x08,0x14,0x22,0x41,
0x7F,0x40,0x40,0x40,0x40, 0x7F,0x02,0x0C,0x02,0x7F, 0x7F,0x04,0x08,0x10,0x7F, 0x3E,0x41,0x41,0x41,0x3E,
0x7F,0x09,0x09,0x09,0x06, 0x3E,0x41,0x51,0x21,0x5E, 0x7F,0x09,0x19,0x29,0x46, 0x46,0x49,0x49,0x49,0x31,
0x01,0x01,0x7F,0x01,0x01, 0x3F,0x40,0x40,0x40,0x3F, 0x1F,0x20,0x40,0x20,0x1F, 0x3F,0x40,0x38,0x40,0x3F,
0x63,0x14,0x08,0x14,0x63, 0x07,0x08,0x70,0x08,0x07, 0x61,0x51,0x49,0x45,0x43 };
static void o_cmd(uint8_t c){ start(); wr(0x78); wr(0x00); wr(c); stop(); }
static void o_data(uint8_t d){ start(); wr(0x78); wr(0x40); wr(d); stop(); }
static void o_pos(uint8_t pg, uint8_t col){ o_cmd(0xB0|pg); o_cmd(0x00|(col&0x0F)); o_cmd(0x10|(col>>4)); }
static void o_print(uint8_t pg, uint8_t col, const char *s){
  o_pos(pg,col);
  while(*s){ uint8_t c=(uint8_t)*s++; uint8_t i;
    if(c<0x20||c>0x5F) c=' ';
    for(i=0;i<5;i++) o_data(F57[(c-0x20)*5+i]);
    o_data(0x00); }
}
static void o_clear(uint8_t pg){ uint8_t i; o_pos(pg,0); for(i=0;i<128;i++) o_data(0x00); }
static void o_init(void){
  uint8_t i; for(i=0;i<100;i++) dly();
  start(); wr(0x78); wr(0x00);
  wr(0xAE); wr(0xD5); wr(0x80); wr(0xA8); wr(0x3F); wr(0xD3); wr(0x00);
  wr(0x40); wr(0x8D); wr(0x14); wr(0x20); wr(0x02); wr(0xA1); wr(0xC8);
  wr(0xDA); wr(0x12); wr(0x81); wr(0xCF); wr(0xD9); wr(0xF1); wr(0xDB);
  wr(0x40); wr(0xA4); wr(0xA6); wr(0xAF);
  stop();
}

/* ================= DHT11 单总线(PB14, 带5ms超时, 永不卡死) ================= */
static void dht_out_low(void){ GPIOB->CRH=(GPIOB->CRH&0xF0FFFFFFU)|0x02000000U; GPIOB->ODR&=~(1u<<14); }
static void dht_rel(void){ GPIOB->CRH=(GPIOB->CRH&0xF0FFFFFFU)|0x08000000U; GPIOB->ODR|=(1u<<14); }
#define DHT_READ() ((GPIOB->IDR>>14)&1u)
static uint8_t wait_line(uint8_t lv){
  uint32_t t0 = DWT_CYCCNT;
  while (DHT_READ() != lv)
    if ((DWT_CYCCNT - t0) > 5000u*72) return 1;
  return 0;
}
/* 返回 0=成功 1=超时 2=校验错 */
static uint8_t dht11_read(uint8_t d[5]){
  uint8_t i, j;
  for(i=0;i<5;i++) d[i]=0;
  dht_out_low(); delay_ms(20); dht_rel();
  if(wait_line(0)) return 1;
  if(wait_line(1)) return 1;
  if(wait_line(0)) return 1;
  for(i=0;i<5;i++){
    for(j=0;j<8;j++){
      if(wait_line(1)) return 1;
      delay_us(40);
      d[i] = (uint8_t)(d[i]<<1);
      if(DHT_READ()){ d[i] |= 1; if(wait_line(0)) return 1; }
    }
  }
  if((uint8_t)(d[0]+d[1]+d[2]+d[3]) != d[4]) return 2;
  return 0;
}

/* ================= MQTT 报文拼装 ================= */
static uint8_t body[420];
static uint8_t mq[560];

static uint32_t put_str(uint8_t *p, const char *s)
{
    uint32_t n = 0, i;
    while (s[n]) n++;
    p[0] = (uint8_t)(n >> 8);
    p[1] = (uint8_t)(n & 0xFF);
    for (i = 0; i < n; i++) p[2 + i] = (uint8_t)s[i];
    return 2 + n;
}

static uint32_t pack(uint8_t type, uint32_t blen)
{
    uint32_t rl = blen, i = 1, k;
    mq[0] = type;
    do {
        uint8_t d = (uint8_t)(rl & 0x7F);
        rl >>= 7;
        if (rl) d |= 0x80;
        mq[i++] = d;
    } while (rl);
    for (k = 0; k < blen; k++) mq[i + k] = body[k];
    return i + blen;
}

static uint32_t build_connect(void)
{
    uint32_t b = 0;
    b += put_str(body + b, "MQTT");
    body[b++] = 0x04;
    body[b++] = 0xC2;
    body[b++] = (uint8_t)(KEEPALIVE >> 8);
    body[b++] = (uint8_t)(KEEPALIVE & 0xFF);
    b += put_str(body + b, MQTT_CLIENT);
    b += put_str(body + b, MQTT_USER);
    b += put_str(body + b, MQTT_PASS);
    return pack(0x10, b);
}

static uint32_t build_subscribe(void)
{
    uint32_t b = 0;
    body[b++] = 0x00; body[b++] = 0x01;
    b += put_str(body + b, TOPIC_SUB);
    body[b++] = 0x00;
    return pack(0x82, b);
}

/* ---- 动态 payload: 用真实传感器值拼 OneJSON ---- */
static char    payload[200];
static uint32_t plen = 0;
static uint32_t msg_id = 0;

static void p_add(const char *s){ while (*s) payload[plen++] = *s++; }
static void p_u8(uint8_t v)
{
    if (v >= 100) payload[plen++] = (char)('0' + v / 100);
    if (v >= 10)  payload[plen++] = (char)('0' + v / 10 % 10);
    payload[plen++] = (char)('0' + v % 10);
}

static uint32_t build_publish(uint8_t t_i, uint8_t t_f, uint8_t h_i, uint8_t h_f, uint8_t light)
{
    uint32_t b = 0, i;
    plen = 0;
    msg_id++;
    p_add("{\"id\":\"");      p_u8((uint8_t)msg_id);
    p_add("\",\"version\":\"1.0\",\"params\":{");
    p_add("\"temperature\":{\"value\":");  p_u8(t_i); payload[plen++]='.'; p_u8(t_f); p_add("},");
    p_add("\"humidity\":{\"value\":");     p_u8(h_i); payload[plen++]='.'; p_u8(h_f); p_add("},");
    p_add("\"light\":{\"value\":");        p_u8(light); p_add("}}}");
    payload[plen] = 0;
    /* PUBLISH 报文体 = 2字节topic长度+topic+payload, 必须重新装进 body[]! */
    b += put_str(body + b, TOPIC_POST);
    for (i = 0; i < plen; i++) body[b++] = (uint8_t)payload[i];
    return pack(0x30, b);
}

/* 发一包 MQTT 报文 */
static void send_packet(const char *name, uint32_t n, uint32_t wait_ms)
{
    char cmd[32];
    uint32_t i, k;

    u1str("\r\n----- "); u1str(name); u1str(" : "); u1num(n);
    u1str(" bytes -----\r\n");

    cmd[0] = 'A'; cmd[1] = 'T'; cmd[2] = '+'; cmd[3] = 'C'; cmd[4] = 'I';
    cmd[5] = 'P'; cmd[6] = 'S'; cmd[7] = 'E'; cmd[8] = 'N'; cmd[9] = 'D';
    cmd[10] = '='; i = 11;
    if (n >= 100) cmd[i++] = (char)('0' + n / 100);
    if (n >= 10)  cmd[i++] = (char)('0' + (n / 10) % 10);
    cmd[i++] = (char)('0' + n % 10);
    cmd[i] = 0;

    saw_prompt = 0;
    u2str(cmd); u2put('\r'); u2put('\n');

    for (k = 0; k < 5000; k++)
    {
        flush_rx();
        if (saw_prompt) break;
        delay_ms(1);
    }
    if (!saw_prompt)
    {
        u1str("[!] no '>' prompt, skip this packet\r\n");
        pump(1500);
        return;
    }
    for (i = 0; i < n; i++) u2put((char)mq[i]);

    {   /* 必须等到 SEND OK 才算模块真发出去了 */
        static const uint8_t PAT_SENDOK[7] = { 'S','E','N','D',' ','O','K' };
        uint32_t base = rx_head;
        uint8_t ok = 0, kk;
        for (kk = 0; kk < 40; kk++)
        {
            flush_rx();
            if (find_pat(base, PAT_SENDOK, 7) >= 0) { ok = 1; break; }
            delay_ms(100);
        }
        u1str(ok ? "[ok] SEND OK confirmed\r\n" : "[!] no SEND OK!\r\n");
    }
    pump(wait_ms);
}

/* ================= 全局传感器值(云/OLED/串口三处共用同一份) ================= */
static uint8_t g_light   = 0;
static uint8_t g_t_i = 0, g_t_f = 0, g_h_i = 0, g_h_f = 0;
static uint8_t g_dht_ok = 0;          /* 拿到过一次成功读数才允许上云 */
static uint32_t g_ok_cnt = 0;

int main(void)
{
    uint32_t brr = (36000000UL + 115200 / 2) / 115200;
    long pos;
    uint32_t n;
    uint8_t  d[5], err, lastv = 0xFF;
    uint8_t  disp_h = 0xFF, disp_t = 0xFF;
    uint32_t cycle = 0;
    static const uint8_t PAT_CONNACK[2] = { 0x20, 0x02 };
    static const uint8_t PAT_SUBACK[2]  = { 0x90, 0x03 };

    RES[0] = 0x22222701;
    SystemCoreClockUpdate();
    dwt_init();
    uart1_init();

    {
        GPIO_InitTypeDef gpio;
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                               RCC_APB2Periph_GPIOC, ENABLE);
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
        /* USART2 PA2/PA3 -> ESP8266 */
        gpio.GPIO_Pin = GPIO_Pin_2;  gpio.GPIO_Mode = GPIO_Mode_AF_PP;
        gpio.GPIO_Speed = GPIO_Speed_50MHz;  GPIO_Init(GPIOA, &gpio);
        gpio.GPIO_Pin = GPIO_Pin_3;  gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        GPIO_Init(GPIOA, &gpio);
        /* PB12 光敏 DO: 上拉输入(开漏输出必须上拉!) */
        gpio.GPIO_Pin  = GPIO_Pin_12;
        gpio.GPIO_Mode = GPIO_Mode_IPU;
        GPIO_Init(GPIOB, &gpio);
        /* PC13 心跳灯 */
        gpio.GPIO_Pin   = GPIO_Pin_13;
        gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
        gpio.GPIO_Speed = GPIO_Speed_2MHz;
        GPIO_Init(GPIOC, &gpio);
        GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);
    }
    /* PB14 DHT / PB8 PB9 OLED 的方向由各自函数在用时切换 */

    USART2->CR2 = 0;
    USART2->CR3 = 0;
    USART2->BRR = brr;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    NVIC_SetPriority(USART2_IRQn, 1);
    NVIC_EnableIRQ(USART2_IRQn);
    USART2->CR1 |= USART_CR1_RXNEIE;

    /* OLED 先亮起来(不等网络) */
    cfg_scl_out();                       /* PB8=SCL 推挽输出(漏配则屏幕全黑) */
    cfg_sda_in();                        /* PB9=SDA 释放为输入(靠上拉) */
    delay_ms(100);
    o_init();
    { uint8_t p; for(p=0;p<8;p++) o_clear(p); }
    o_print(0, 0, "F103 SMART ENV");
    o_print(2, 0, "LIGHT:");
    o_print(4, 0, "HUMI:");
    o_print(6, 0, "TEMP:");

    u1str("\r\n\r\n########## TASK9 FINAL: sensors+OLED+MQTT ##########\r\n");
    delay_ms(300);
    while (USART2->SR & USART_SR_RXNE) { (void)USART2->DR; }
    flush_rx();
    rx_tail = rx_head;                     /* 丢掉开机垃圾 */

    /* ---- 第一步: 连 WiFi ---- */
    at_cmd("AT+RST",               5000);
    wait_ready();
    at_cmd("AT+CWAUTOCONN=0",      1500);
    at_cmd("AT+CWMODE=1",          1500);
    at_cmd("AT+CIPMUX=0",          1500);
    at_cmd("AT+CIPMODE=0",         1500);
    at_cmd("AT+CWJAP=\"" WIFI_SSID "\",\"" WIFI_PWD "\"", 13000);
    at_cmd("AT+CIFSR",             2500);

    /* ---- 第二步: TCP 连 OneNET ---- */
    at_cmd("AT+CIPSTART=\"TCP\",\"" MQTT_HOST "\"," MQTT_PORT, 6000);
    {
        static const uint8_t PAT_CONNECTED[7] = { 'C','O','N','N','E','C','T' };
        pos = find_pat(0, PAT_CONNECTED, 7);
        if (pos >= 0) u1str("\r\n[+] TCP CONNECTED\r\n");
        else          u1str("\r\n[!] TCP maybe failed\r\n");
    }

    /* ---- 第三步: MQTT CONNECT ---- */
    n = build_connect();
    pos = (long)rx_head;
    send_packet("MQTT CONNECT", n, 5000);
    pos = find_pat((uint32_t)pos, PAT_CONNACK, 2);
    if (pos >= 0)
    {
        u1str("\r\n[+] CONNACK rc="); u1num(rx_buf[pos + 3]); u1str("\r\n");
        o_print(0, 108, "NET");          /* 屏幕右上角亮 NET = 云已连上 */
    }
    else u1str("\r\n[!] no CONNACK\r\n");

    /* ---- 第四步: SUBSCRIBE 回执主题 ---- */
    n = build_subscribe();
    pos = (long)rx_head;
    send_packet("MQTT SUBSCRIBE", n, 3000);
    pos = find_pat((uint32_t)pos, PAT_SUBACK, 2);
    if (pos >= 0) u1str("\r\n[+] SUBACK ok\r\n");
    else          u1str("\r\n[!] no SUBACK\r\n");

    RES[0] = 0x22222702;

    /* ================= 主循环: 传感 -> OLED -> 串口 -> 云端 ================= */
    while (1)
    {
        /* 1) 读光敏 */
        g_light = (uint8_t)((GPIOB->IDR >> 12) & 1u);
        if (g_light != lastv)
        {
            lastv = g_light;
            o_print(2, 48, g_light ? "OK  " : "DARK");
        }

        /* 2) 读 DHT11 */
        err = dht11_read(d);
        if (err == 0)
        {
            g_t_i = d[2]; g_t_f = d[3]; g_h_i = d[0]; g_h_f = d[1];
            g_dht_ok = 1; g_ok_cnt++;
            if (d[0] != disp_h)
            {
                disp_h = d[0];
                o_print(4, 40, "      ");
                { char b[8];
                  b[0]=(char)('0'+d[0]/100); b[1]=(char)('0'+d[0]/10%10); b[2]=(char)('0'+d[0]%10);
                  b[3]='.'; b[4]=(char)('0'+d[1]); b[5]='%'; b[6]=0;
                  o_print(4, 40, b); }
            }
            if (d[2] != disp_t)
            {
                disp_t = d[2];
                o_print(6, 40, "      ");
                { char b[8];
                  b[0]=(char)('0'+d[2]/100); b[1]=(char)('0'+d[2]/10%10); b[2]=(char)('0'+d[2]%10);
                  b[3]='.'; b[4]=(char)('0'+d[3]); b[5]='C'; b[6]=0;
                  o_print(6, 40, b); }
            }
        }
        else
        {
            o_print(4, 40, " --  ");
            o_print(6, 40, " --  ");
            disp_h = 0xFF; disp_t = 0xFF;
            u1str("[..] DHT11 ");
            u1str(err == 1 ? "TIMEOUT\r\n" : "CHECKSUM ERR\r\n");
        }

        /* 3) 串口打印(和屏幕同一份值) */
        u1str("LIGHT="); u1num(g_light);
        if (err == 0)
        {
            u1str("  HUMI="); u1num(g_h_i); u1ch('.'); u1num(g_h_f);
            u1str("%  TEMP="); u1num(g_t_i); u1ch('.'); u1num(g_t_f); u1str("C\r\n");
        }
        else u1str("\r\n");

        /* 4) 上云(每3轮一次, 且必须拿到过真实读数) */
        cycle++;
        if (g_dht_ok && (cycle % 3u) == 0u)
        {
            n = build_publish(g_t_i, g_t_f, g_h_i, g_h_f, g_light);
            u1str("publish: "); u1str(payload); u1str("\r\n");
            send_packet("PUBLISH", n, 4000);
        }
        else pump(800);

        /* 5) 心跳 */
        GPIO_WriteBit(GPIOC, GPIO_Pin_13,
            (BitAction)!GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_13));

        RES[1] = rx_head; RES[2] = ore_n; RES[3] = g_ok_cnt;
    }
}

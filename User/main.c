#include "stm32f10x.h"
/* DHT11 RAM 诊断（SWD 读，无需串口）
   0x20000000: [0]=PB15 IPD [1]=PB15 IPU [2]=协议阶段(2=等应答超时 3=读位超时 4=OK) [3]=边沿数
   0x20000004: 5 字节数据（校验通过时有效）                                             */
#define RES ((volatile uint8_t *)0x20004000)   /* RAM 高段，链接器不管理 */

#define DEMCR      (*(volatile uint32_t *)0xE000EDFC)
#define DWT_CTRL   (*(volatile uint32_t *)0xE0001000)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004)

static void dwt_init(void){ DEMCR |= (1u<<24); DWT_CYCCNT = 0; DWT_CTRL |= 1u; }
static void delay_us(uint32_t us){ uint32_t s=DWT_CYCCNT; while((DWT_CYCCNT-s) < us*72){} }
static void delay_ms(uint32_t ms){ while(ms--) delay_us(1000); }

static void dht_out_low(void){ GPIO_InitTypeDef g; g.GPIO_Pin=GPIO_Pin_15; g.GPIO_Speed=GPIO_Speed_2MHz; g.GPIO_Mode=GPIO_Mode_Out_PP; GPIO_WriteBit(GPIOB,GPIO_Pin_15,Bit_RESET); GPIO_Init(GPIOB,&g); }
static void dht_rel(void){ GPIO_InitTypeDef g; g.GPIO_Pin=GPIO_Pin_15; g.GPIO_Speed=GPIO_Speed_2MHz; g.GPIO_Mode=GPIO_Mode_IPU; GPIO_Init(GPIOB,&g); }
#define DHT_READ() GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_15)

static uint8_t wait_line(uint8_t lv)
{
    uint32_t t0 = DWT_CYCCNT;
    while (DHT_READ() != lv)
        if ((DWT_CYCCNT - t0) > 5000u*72) return 1;
    if (RES[3] < 250) RES[3]++;
    return 0;
}

int main(void)
{
    uint8_t i, j, err;
    SystemCoreClockUpdate();
    dwt_init();
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN|RCC_APB2ENR_IOPBEN|RCC_APB2ENR_USART1EN;
    dht_rel();

    /* ① 双读：判断线连接状态 */
    GPIOB->ODR &= ~(1u<<15);
    GPIOB->CRH = (GPIOB->CRH & 0x0FFFFFFFU) | 0x80000000U;
    delay_ms(2);
    RES[0] = (uint8_t)((GPIOB->IDR>>15)&1);
    GPIOB->ODR |= (1u<<15);
    delay_ms(2);
    RES[1] = (uint8_t)((GPIOB->IDR>>15)&1);

    while(1){
        /* ② 协议尝试 */
        RES[2] = 1; RES[3] = 0;
        for(i=4;i<9;i++) RES[i]=0;
        dht_out_low();
        delay_ms(20);
        RES[2] = 2;
        dht_rel();
        if(wait_line(0)) { goto done; }
        if(wait_line(1)) { goto done; }
        RES[2] = 3;
        if(wait_line(0)) { goto done; }   /* 等应答高电平结束(80us)：此前漏了这步导致数据错位一位 */
        for(i=0;i<5;i++){
            for(j=0;j<8;j++){
                if(wait_line(1)) { goto done; }
                delay_us(40);
                RES[4+i] = (uint8_t)(RES[4+i]<<1);
                if(DHT_READ()){ RES[4+i] |= 1; if(wait_line(0)) { goto done; } }
            }
        }
        RES[2] = 4;
        done:
        if(RES[2]==4 && (uint8_t)(RES[4]+RES[5]+RES[6]+RES[7])!=RES[8])
            RES[2] = 5;   /* 校验和错 */
        delay_ms(1500);
    }
}

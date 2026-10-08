/**
  ******************************************************************************
  * @file    main.c
  * @brief   03 章：版本 C——按键开关闪烁（按一下开始 1s 闪烁，再按一下停止）
  * @note    KEY1=PB7（上拉输入，按下=0）；绿灯=PC13（低电平亮）
  *          核心思想：把 1 秒的等待切成 100 个 10ms 小片，每片开头查一次按键
  *          ——闪烁和按键"并行"，谁也不卡谁（非阻塞分片）。
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

/* 查一次按键：只捕捉"从松开到按下"的沿（带消抖+等释放）。
   返回 1 = 发生了一次完整按压；0 = 没事发生 */
uint8_t key_pressed(void)
{
    static uint8_t last = 1;                 /* 上一次的按键状态（static: 下次进来还记得） */
    uint8_t now = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7);
    uint8_t evt  = 0;

    if (last == 1 && now == 0)               /* 1 -> 0 = 捕捉到按下沿 */
    {
        delay_ms(10);                        /* 按下消抖：跳过 5~20ms 的抖动期 */
        if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7) == 0)   /* 抖完还是 0 = 真按下 */
        {
            evt = 1;
            while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7) == 0)
            {
            }                                /* 等松手（不松手不算完整一次） */
            delay_ms(10);                    /* 释放消抖：松手也会弹 */
        }
    }
    last = now;                              /* 记住本次状态，下次比较用 */
    return evt;
}

int main(void)
{
    GPIO_InitTypeDef gpio;
    uint8_t  running = 0;                    /* 闪烁开关：0=停 1=闪 */
    uint16_t slice   = 0;                    /* 10ms 小片计数器 */

    SystemCoreClockUpdate();
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);

    gpio.GPIO_Pin  = GPIO_Pin_7;             /* PB7 = KEY1 上拉输入 */
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &gpio);

    gpio.GPIO_Pin   = GPIO_Pin_13;           /* PC13 = 绿灯 推挽输出 */
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &gpio);

    GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);   /* 初始：灭 */

    while (1)
    {
        /* ① 每个周期开头，先照看一次按键 */
        if (key_pressed())
        {
            running = !running;              /* 按一下：开 <-> 停 切换 */
            if (running == 0)
            {
                GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);  /* 停止：灯灭 */
                slice = 0;
            }
        }

        /* ② 闪烁：不睡大觉，靠小片计数 */
        if (running)
        {
            slice++;
            if (slice >= 50)                 /* 50 片 x 10ms = 500ms */
            {
                slice = 0;
                /* 翻转灯：读当前输出电平，取反写回 */
                GPIO_WriteBit(GPIOC, GPIO_Pin_13,
                    (BitAction)!GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_13));
            }
        }

        delay_ms(10);                        /* ③ 每个周期固定 10ms —— 全局的节拍器 */
    }
}

/**
  ******************************************************************************
  * @file    main.c
  * @brief   03 章：按键控制 LED——版本 A（按住亮，松开灭，电平跟随）
  * @note    实测硬件：KEY1=PB7（按键按下接 GND，低电平有效，无外部上拉）
  *          绿灯=PC13（低电平亮）
  *          核心新知识：GPIO 输入模式（上拉输入）+ 读引脚（IDR）
  ******************************************************************************
  */
#include "stm32f10x.h"

int main(void)
{
    GPIO_InitTypeDef gpio;

    SystemCoreClockUpdate();

    /* GPIOB(按键) 和 GPIOC(灯) 都在 APB2，一次同时开 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);

    /* ---- 配按键脚：PB7 = 上拉输入 ----
     * 按键一端接 PB7，另一端接 GND：
     *   按下  -> PB7 直接连地        -> 读到 0
     *   松开  -> 引脚悬空？不行！靠内部上拉电阻拉到 3.3V -> 读到 1
     * 所以必须配"上拉输入"，否则松开时引脚悬空，读到乱跳的随机值 */
    gpio.GPIO_Pin  = GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_IPU;              /* Input Pull Up 上拉输入 */
    GPIO_Init(GPIOB, &gpio);

    /* ---- 配灯脚：PC13 推挽输出（02 章已学） ---- */
    gpio.GPIO_Pin   = GPIO_Pin_13;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &gpio);

    GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);  /* 上电先灭 */

    while (1)
    {
        /* 读按键：0 = 按下（低电平有效），1 = 松开 */
        if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7) == 0)
        {
            GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_RESET);   /* 按住 -> 亮 */
        }
        else
        {
            GPIO_WriteBit(GPIOC, GPIO_Pin_13, Bit_SET);     /* 松开 -> 灭 */
        }
    }
}

/**
  ******************************************************************************
  * @file    stm32f10x_it.c
  * @brief   中断服务函数（ISR）统一放在本文件
  * @note    Handler 名字必须与启动文件 startup_stm32f10x_md.s 里的向量表
  *          标号一致（弱定义覆盖）。具体外设中断在对应章节实现。
  ******************************************************************************
  */
#include "stm32f10x_it.h"

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
    /* 硬件错误：跑飞/非法访问，调试时在此断住 */
    while (1)
    {
    }
}

void MemManage_Handler(void)
{
    while (1)
    {
    }
}

void BusFault_Handler(void)
{
    while (1)
    {
    }
}

void UsageFault_Handler(void)
{
    while (1)
    {
    }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
    /* HAL_Delay/SPL 延时依赖的 1ms 心跳在此章节实现 */
}

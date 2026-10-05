/**
  ******************************************************************************
  * @file    stm32f10x_conf.h
  * @brief   标准库配置头文件：决定哪些外设驱动参与编译
  * @note    被 stm32f10x.h 末尾的 #include "stm32f10x_conf.h" 引入（前提是
  *          定义了 USE_STDPERIPH_DRIVER 宏，见 Keil 工程的 Define 选项）。
  *          规则：用哪个外设，就放开哪个 #include，并把对应的 .c 加入工程。
  *          脚手架阶段只启用 RCC + GPIO + misc（NVIC）。
  ******************************************************************************
  */
#ifndef __STM32F10x_CONF_H
#define __STM32F10x_CONF_H

#include "stm32f10x.h"

/* 脚手架启用的外设驱动（后续教程章节按需追加） */
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "misc.h"

/* 暂未使用的外设（需要时放开 include 并把 src 下对应 .c 加入工程） */
/*#include "stm32f10x_adc.h"    */
/*#include "stm32f10x_bkp.h"    */
/*#include "stm32f10x_can.h"    */
/*#include "stm32f10x_crc.h"    */
/*#include "stm32f10x_dac.h"    */
/*#include "stm32f10x_dbgmcu.h" */
/*#include "stm32f10x_dma.h"    */
/*#include "stm32f10x_exti.h"   */
/*#include "stm32f10x_flash.h"  */
/*#include "stm32f10x_fsmc.h"   */
/*#include "stm32f10x_i2c.h"    */
/*#include "stm32f10x_iwdg.h"   */
/*#include "stm32f10x_pwr.h"    */
/*#include "stm32f10x_rtc.h"    */
/*#include "stm32f10x_sdio.h"   */
/*#include "stm32f10x_spi.h"    */
/*#include "stm32f10x_tim.h"    */
#include "stm32f10x_usart.h"
/*#include "stm32f10x_wwdg.h"   */

/* SPL 参数检查宏：
   默认关闭(空宏)；调试期想开启参数检查，在工程 Define 里加 USE_FULL_ASSERT，
   并实现 void assert_failed(uint8_t* file, uint32_t line) */
#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line);
#else
#define assert_param(expr) ((void)0)
#endif

#endif /* __STM32F10x_CONF_H */

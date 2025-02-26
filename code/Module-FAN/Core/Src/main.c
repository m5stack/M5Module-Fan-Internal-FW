/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "iwdg.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include "i2c_ex.h"
#include "flash.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define I2C_BASS_ADDR       (0x18)
#define FIRMWARE_VERSION    (1)
#define DUTY_CYCLE_BASE     (0)
#define SAMPLE_INTERVAL     (100)
#define APPLICATION_ADDRESS ((uint32_t)0x08001000)
// 寄存器地址
#define FAN_CONTROL_REG_ADDR          (0x00)
#define PWM_FREQUENCY_REG_ADDR        (0x10)
#define PWM_DUTY_CYCLE_REG_ADDR       (0x20)
#define FAN_RPM_REG_ADDR              (0x30)
#define FAN_SIGNAL_FREQUENCY_REG_ADDR (0x40)
#define FLASH_WRITE_BACK              (0xF0)
#define IAP_UPDATE_ADDR               (0xFD)
#define FIRMWARE_VERAION_REG_ADDR     (0xFE)
#define I2C_ADDRESS_REG_ADDR          (0xFF)
// 寄存器
__IO static uint8_t fan_control_reg             = 0;
__IO uint8_t pwm_frequency_reg                  = 0;
__IO static uint8_t pwm_duty_cycle_reg          = 0;
__IO static uint8_t fan_rpm_reg[2]              = {0};
__IO static uint8_t fan_signal_frequency_reg[2] = {0};
__IO static uint8_t firmware_version_reg        = 0;
__IO uint8_t i2c_addr_reg                       = I2C_BASS_ADDR;

static uint16_t CCR1 = 0, CCR2 = 0, CCR3 = 0;
static uint8_t measure_flag              = 0;
static uint32_t freq                     = 0;
static uint8_t duty                      = 0;
static uint16_t fan_freq                 = 0;
static uint16_t fan_rpm                  = 0;
static uint32_t start_time               = 0;
volatile uint32_t i2c_stop_timeout_delay = 0;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void iap_set()
{
    uint8_t i;
    uint32_t *pVecTab = (uint32_t *)(0x20000000);
    for (i = 0; i < 48; i++) {
        *(pVecTab++) = *(__IO uint32_t *)(APPLICATION_ADDRESS + (i << 2));
    }
    /* Enable the SYSCFG peripheral clock*/
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_SYSCFG_REMAPMEMORY_SRAM();
}

// 寄存器初始化
void reg_init(void)
{
    uint8_t flag = 0;
    if (get_fan_control() == 0xFF) {
        fan_control_reg = FAN_ON;
        flag++;
        //		set_fan_control(FAN_ON);
    } else {
        fan_control_reg = get_fan_control();
    }
    if (get_pwm_freq() == 0xFF) {
        pwm_frequency_reg = PWM_24KHZ;
        flag++;
        //		set_pwm_freq(PWM_24KHZ);
    } else {
        pwm_frequency_reg = get_pwm_freq();
    }
    if (get_pwm_duty_cycle() == 0xFF) {
        pwm_duty_cycle_reg = DUTY_CYCLE_BASE;
        flag++;
        //		set_pwm_duty_cycle(DUTY_CYCLE_BASE);
    } else {
        pwm_duty_cycle_reg = get_pwm_duty_cycle();
    }
    if (flag != 0) {
        write_flash(fan_control_reg, pwm_frequency_reg, pwm_duty_cycle_reg);
    }
    firmware_version_reg = FIRMWARE_VERSION;
    if (get_i2c_addr() == 0xFF) {
        i2c_addr_reg = I2C_BASS_ADDR;
        set_i2c_addr(I2C_BASS_ADDR);
    } else {
        i2c_addr_reg = get_i2c_addr();
    }
}

// 风扇状态更新
void fan_status_update(void)
{
    if (fan_control_reg == FAN_OFF) {
        LL_TIM_OC_SetCompareCH1(TIM3, 0);
    } else if (fan_control_reg == FAN_ON) {
        LL_TIM_OC_SetCompareCH1(TIM3, pwm_duty_cycle_reg * 10);
    }
}

// PWM频率更新
void pwm_frequency_update(void)
{
    LL_TIM_DisableCounter(TIM3);
    time_init();
    fan_status_update();
}

// I2C从机回调函数
void Slave_Complete_Callback(uint8_t *rx_data, uint16_t len)
{
    if (len > 1) {
        if (rx_data[0] == FAN_CONTROL_REG_ADDR && len == 2) {
            if (rx_data[1] == FAN_OFF || rx_data[1] == FAN_ON) {
                if (fan_control_reg != rx_data[1]) {
                    fan_control_reg = rx_data[1];
                    //					set_fan_control(rx_data[1]);
                    fan_status_update();
                }
            }
        } else if (rx_data[0] == PWM_FREQUENCY_REG_ADDR && len == 2) {
            if (rx_data[1] == PWM_1KHZ || rx_data[1] == PWM_12KHZ || rx_data[1] == PWM_24KHZ ||
                rx_data[1] == PWM_48KHZ) {
                if (pwm_frequency_reg != rx_data[1]) {
                    pwm_frequency_reg = rx_data[1];
                    //					set_pwm_freq(rx_data[1]);
                    pwm_frequency_update();
                }
            }
        } else if (rx_data[0] == PWM_DUTY_CYCLE_REG_ADDR && len == 2) {
            if (rx_data[1] >= 0 && rx_data[1] <= 100) {
                if (pwm_duty_cycle_reg != rx_data[1]) {
                    pwm_duty_cycle_reg = rx_data[1];
                    //				set_pwm_duty_cycle(rx_data[1]);
                    fan_status_update();
                }
            }
        } else if (rx_data[0] == I2C_ADDRESS_REG_ADDR && len == 2) {
            if (rx_data[1] >= 0x08 && rx_data[1] <= 0x77) {
                if (i2c_addr_reg != rx_data[1]) {
                    i2c_addr_reg = rx_data[1];
                    set_i2c_addr(rx_data[1]);
                    user_i2c_init();
                }
            }
        } else if (rx_data[0] == IAP_UPDATE_ADDR && len == 2) {
            if (rx_data[1] >= 0) {
                NVIC_SystemReset();
            }
        } else if (rx_data[0] == FLASH_WRITE_BACK && len == 2) {
            if (rx_data[1] == 1) {
                if (fan_control_reg != get_fan_control() || pwm_frequency_reg != get_pwm_freq() ||
                    pwm_duty_cycle_reg != get_pwm_duty_cycle()) {
                    write_flash(fan_control_reg, pwm_frequency_reg, pwm_duty_cycle_reg);
                }
            }
        }
    }
    if (len == 1) {
        switch (rx_data[0]) {
            case FAN_CONTROL_REG_ADDR:
                i2c1_set_send_data((uint8_t *)&fan_control_reg, 1);
                break;
            case PWM_FREQUENCY_REG_ADDR:
                i2c1_set_send_data((uint8_t *)&pwm_frequency_reg, 1);
                break;
            case PWM_DUTY_CYCLE_REG_ADDR:
                i2c1_set_send_data((uint8_t *)&pwm_duty_cycle_reg, 1);
                break;
            case FAN_RPM_REG_ADDR:
                i2c1_set_send_data((uint8_t *)fan_rpm_reg, 2);
                break;
            case (FAN_RPM_REG_ADDR + 1):
                i2c1_set_send_data(((uint8_t *)fan_rpm_reg) + 1, 1);
                break;
            case FAN_SIGNAL_FREQUENCY_REG_ADDR:
                i2c1_set_send_data((uint8_t *)fan_signal_frequency_reg, 2);
                break;
            case (FAN_SIGNAL_FREQUENCY_REG_ADDR + 1):
                i2c1_set_send_data(((uint8_t *)fan_signal_frequency_reg) + 1, 1);
                break;
            case FIRMWARE_VERAION_REG_ADDR:
                i2c1_set_send_data((uint8_t *)&firmware_version_reg, 1);
                break;
            case I2C_ADDRESS_REG_ADDR:
                i2c1_set_send_data((uint8_t *)&i2c_addr_reg, 1);
                break;
            default:
                break;
        }
    }
}

// 定时器捕获中断函数
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    static uint8_t measure_cnt = 1;
    if (htim == &htim14) {
        if (measure_cnt == 1) {
            CCR1 = HAL_TIM_ReadCapturedValue(&htim14, TIM_CHANNEL_1);
            __HAL_TIM_SET_CAPTUREPOLARITY(&htim14, TIM_CHANNEL_1, TIM_INPUTCHANNELPOLARITY_FALLING);
            measure_cnt = 2;
        } else if (measure_cnt == 2) {
            CCR2 = HAL_TIM_ReadCapturedValue(&htim14, TIM_CHANNEL_1);
            __HAL_TIM_SET_CAPTUREPOLARITY(&htim14, TIM_CHANNEL_1, TIM_INPUTCHANNELPOLARITY_RISING);
            measure_cnt = 3;
        } else if (measure_cnt == 3) {
            CCR3 = HAL_TIM_ReadCapturedValue(&htim14, TIM_CHANNEL_1);
            HAL_TIM_IC_Stop_IT(&htim14, TIM_CHANNEL_1);
            measure_cnt  = 1;
            measure_flag = 1;
        }
    }
}

// 捕获函数
void capture(void)
{
    uint16_t diff1 = 0, diff2 = 0;
    freq = 0;
    duty = 0;
    if (CCR1 < CCR2) {
        diff1 = CCR2 - CCR1;
    } else {
        diff1 = 0xffff + 1 + CCR2 - CCR1;
    }
    if (CCR1 < CCR3) {
        diff2 = CCR3 - CCR1;
    } else {
        diff2 = 0xffff + 1 + CCR3 - CCR1;
    }
    freq = (72000000 / 72) / diff2;
    duty = diff1 * 100 / diff2;
    if (freq <= 600 && freq > 0) {
        fan_freq                    = freq;
        fan_rpm                     = freq * 30;
        fan_signal_frequency_reg[0] = (fan_freq & 0xFF);
        fan_signal_frequency_reg[1] = ((fan_freq >> 8) & 0xFF);
        fan_rpm_reg[0]              = (fan_rpm & 0xFF);
        fan_rpm_reg[1]              = ((fan_rpm >> 8) & 0xFF);
    } else {
        fan_freq                    = 0;
        fan_rpm                     = 0;
        fan_signal_frequency_reg[0] = 0;
        fan_signal_frequency_reg[1] = 0;
        fan_rpm_reg[0]              = 0;
        fan_rpm_reg[1]              = 0;
    }
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
    /* USER CODE BEGIN 1 */
    iap_set();
    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    //  MX_I2C1_Init();
    //  MX_TIM3_Init();
    MX_TIM14_Init();
    MX_IWDG_Init();
    /* USER CODE BEGIN 2 */
    reg_init();
    time_init();
    fan_status_update();
    user_i2c_init();
    i2c1_it_enable();
    start_time = HAL_GetTick();
    HAL_TIM_IC_Start_IT(&htim14, TIM_CHANNEL_1);
    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1) {
        if ((HAL_GetTick() - start_time) > SAMPLE_INTERVAL) {
            capture();
            CCR1 = 0, CCR2 = 0, CCR3 = 0;
            HAL_TIM_IC_Start_IT(&htim14, TIM_CHANNEL_1);
            start_time = HAL_GetTick();
        }
        i2c_timeout_counter = 0;
        if (i2c_stop_timeout_flag) {
            if (i2c_stop_timeout_delay < HAL_GetTick()) {
                i2c_stop_timeout_counter++;
                i2c_stop_timeout_delay = HAL_GetTick() + 10;
            }
        }
        if (i2c_stop_timeout_counter > 50) {
            LL_I2C_DeInit(I2C1);
            LL_I2C_DisableAutoEndMode(I2C1);
            LL_I2C_Disable(I2C1);
            LL_I2C_DisableIT_ADDR(I2C1);
            user_i2c_init();
            i2c1_it_enable();
            HAL_Delay(500);
        }
        LL_IWDG_ReloadCounter(IWDG);
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_1);
    while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_1) {
    }
    LL_RCC_HSI_Enable();

    /* Wait till HSI is ready */
    while (LL_RCC_HSI_IsReady() != 1) {
    }
    LL_RCC_HSI_SetCalibTrimming(16);
    LL_RCC_LSI_Enable();

    /* Wait till LSI is ready */
    while (LL_RCC_LSI_IsReady() != 1) {
    }
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI_DIV_2, LL_RCC_PLL_MUL_12);
    LL_RCC_PLL_Enable();

    /* Wait till PLL is ready */
    while (LL_RCC_PLL_IsReady() != 1) {
    }
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

    /* Wait till System clock is ready */
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL) {
    }
    LL_SetSystemCoreClock(48000000);

    /* Update the time base */
    if (HAL_InitTick(TICK_INT_PRIORITY) != HAL_OK) {
        Error_Handler();
    }
    LL_RCC_SetI2CClockSource(LL_RCC_I2C1_CLKSOURCE_SYSCLK);
    LL_RCC_ConfigMCO(LL_RCC_MCO1SOURCE_SYSCLK, LL_RCC_MCO1_DIV_1);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1) {
    }
    /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

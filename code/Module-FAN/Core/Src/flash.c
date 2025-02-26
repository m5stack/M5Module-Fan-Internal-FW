#include "flash.h"

/**
 * @brief  修改8byte的某一字节
 * @param  data 要修改的数据
 * @param  byte_index 数据下标
 * @param  new_value 修改值
 * @retval 修改之后的数
 */
static void set_byte_in_uint64(uint64_t *data, uint8_t byte_index, uint8_t new_value)
{
    *data &= ~((uint64_t)(0xFF) << (byte_index * 8));
    // 然后设置新的字节值
    *data |= (uint64_t)new_value << (byte_index * 8);
}

/**
 * @brief  读取字节
 * @param  address 内存地址
 * @retval 返回值
 */
static uint64_t my_flash_read_byte(uint32_t address)
{
    return *((__IO uint8_t *)(address));
}

/**
 * @brief  读取双字
 * @param  address 内存地址
 * @retval 返回值
 */
static uint64_t my_flash_read_double_word(uint32_t address)
{
    return *((__IO uint64_t *)(address));
}

/**
 * @brief  页擦除
 * @param  address 内存地址
 * @retval 是否成功
 */
static bool my_flash_earse_pages(void)
{
    uint32_t page_error = 0;  // 设置Page_error,如果出现错误这个变量会被设置为出错的FLASH地址
    FLASH_EraseInitTypeDef my_flash;               // 页擦除所在的位置
    my_flash.TypeErase   = FLASH_TYPEERASE_PAGES;  // 标明Flash执行页面只做擦除操作
    my_flash.PageAddress = STM32F0xx_FLASH_PAGE15_STARTADDR;
    my_flash.NbPages     = 1;  // 说明要擦除的页数
    HAL_FLASH_Unlock();
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&my_flash, &page_error);
    HAL_FLASH_Lock();
    if (status == HAL_OK) {
        return true;
    }
    return false;
}

/**
 * @brief  写双字
 * @param  address 内存地址
 * @param  data 要写入的数据
 * @retval 是否成功
 */
static bool my_flash_write_double_word(uint64_t data)
{
    HAL_FLASH_Unlock();
    HAL_StatusTypeDef status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, STM32F0xx_FLASH_PAGE15_STARTADDR, data);
    HAL_FLASH_Lock();
    if (status == HAL_OK) {
        return true;
    }
    return false;
}

/**
 * @brief  设置风扇工作状态
 * @param  修改的数据
 * @retval 是否成功
 */
bool set_fan_control(uint8_t data)
{
    uint64_t temp = my_flash_read_double_word(STM32F0xx_FLASH_PAGE15_STARTADDR);
    set_byte_in_uint64(&temp, 0, data);
    my_flash_earse_pages();
    my_flash_write_double_word(temp);
    if (data == get_fan_control()) {
        return true;
    }
    return false;
}

/**
 * @brief  获得风扇工作状态
 * @param  无
 * @retval 无
 */
uint8_t get_fan_control(void)
{
    return my_flash_read_byte(FAN_CONTROL_ADDR);
}

/**
 * @brief  修改PWM频率
 * @param  修改的数据
 * @retval 是否成功
 */
bool set_pwm_freq(uint8_t data)
{
    uint64_t temp = my_flash_read_double_word(STM32F0xx_FLASH_PAGE15_STARTADDR);
    set_byte_in_uint64(&temp, 1, data);
    my_flash_earse_pages();
    my_flash_write_double_word(temp);
    if (data == get_pwm_freq()) {
        return true;
    }
    return false;
}

/**
 * @brief  获得PWM频率
 * @param  无
 * @retval 返回PWM频率
 */
uint8_t get_pwm_freq(void)
{
    return my_flash_read_byte(PWM_FREQUENCY_ADDR);
}

/**
 * @brief  修改PWM占空比
 * @param  修改的数据
 * @retval 是否成功
 */
bool set_pwm_duty_cycle(uint8_t data)
{
    uint64_t temp = my_flash_read_double_word(STM32F0xx_FLASH_PAGE15_STARTADDR);
    set_byte_in_uint64(&temp, 2, data);
    my_flash_earse_pages();
    my_flash_write_double_word(temp);
    if (data == get_pwm_duty_cycle()) {
        return true;
    }
    return false;
}

/**
 * @brief  获得PWM占空比
 * @param  无
 * @retval 返回PWM占空比
 */
uint8_t get_pwm_duty_cycle(void)
{
    return my_flash_read_byte(PWM_DUTY_CYCLE_ADDR);
}

/**
 * @brief  修改i2c地址
 * @param  修改的数据
 * @retval 是否成功
 */
bool set_i2c_addr(uint8_t data)
{
    uint64_t temp = my_flash_read_double_word(STM32F0xx_FLASH_PAGE15_STARTADDR);
    set_byte_in_uint64(&temp, 3, data);
    my_flash_earse_pages();
    my_flash_write_double_word(temp);
    if (data == get_i2c_addr()) {
        return true;
    }
    return false;
}

/**
 * @brief  获得i2c地址
 * @param  无
 * @retval 返回i2c地址
 */
uint8_t get_i2c_addr(void)
{
    return my_flash_read_byte(I2C_ADDR);
}

bool write_flash(uint8_t control, uint8_t frequency, uint8_t duty)
{
    __disable_irq();
    uint64_t temp = my_flash_read_double_word(STM32F0xx_FLASH_PAGE15_STARTADDR);
    set_byte_in_uint64(&temp, 0, control);
    set_byte_in_uint64(&temp, 1, frequency);
    set_byte_in_uint64(&temp, 2, duty);
    my_flash_earse_pages();
    my_flash_write_double_word(temp);
    __enable_irq();
    if (control == get_fan_control() && frequency == get_pwm_freq() && duty == get_pwm_duty_cycle()) {
        return true;
    }
    return false;
}

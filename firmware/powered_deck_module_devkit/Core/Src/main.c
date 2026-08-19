/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
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
#include "fdcan.h"
#include "gpio.h"
#include "icache.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "protocol.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// incoming_data[0] selects the public API port the command targets.
typedef enum {
    PORT_A = 0,
    PORT_B = 1,
    PORT_LEDS = 2,
} GpioPort;

// LED ids are zero-indexed to match the public API (incoming_data[1] == 0 -> LED1).
typedef enum {
    LED1 = 0,
    LED2 = 1,
    LED3 = 2,
} UserLeds;

/* Mode encoding used on the wire. */
typedef enum { PIN_MODE_INPUT = 0, PIN_MODE_OUTPUT = 1 } GpioMode;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
bool valid_pinmode = true;

// Only these GPIO pins are exposed for user control. Reserved pins are excluded
// here so the same validation is used for both pin writes and mode changes.
static const uint16_t user_pin_masks[] = {
    [PORT_A] = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_6 |
               GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12,
    [PORT_B] = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 |
               GPIO_PIN_10 | GPIO_PIN_12,
};

uint8_t incoming_data[64];
bool processed_incoming_data = false;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
static void setGpio(UserLeds pin, GPIO_PinState state);
static bool resolveUserPin(uint8_t port, uint8_t pin, GPIO_TypeDef** gpio_port, uint16_t* gpio_pin);
static void applyPinCommand(uint8_t port, uint8_t pin, uint8_t state_byte);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* MPU Configuration--------------------------------------------------------*/
    MPU_Config();

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
    MX_FDCAN2_Init();
    MX_ICACHE_Init();
    /* USER CODE BEGIN 2 */
    CanCommsInit();
    setGpio(LED1, GPIO_PIN_RESET);
    setGpio(LED2, GPIO_PIN_SET);
    setGpio(LED3, GPIO_PIN_RESET);
    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1) {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */

        if (receivedCanMessage) {
            handleCanMessage();
        }

        if (!processed_incoming_data) {
            // incoming_data[0] = port (0=GPIOA, 1=GPIOB, 2=LEDs)
            // incoming_data[1] = pin within the port (LED pins are 0-based)
            // incoming_data[2] = state (0 = LOW, non-zero = HIGH)
            applyPinCommand(incoming_data[0], incoming_data[1], incoming_data[2]);
            processed_incoming_data = true;
        }

        // Sleep until the next interrupt (CAN RX or SysTick) instead of busy-waiting
    }
    /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
     */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
    }

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLL1_SOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 4;
    RCC_OscInitStruct.PLL.PLLN = 31;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 2;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1_VCIRANGE_3;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1_VCORANGE_WIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 2048;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 |
                                  RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_PCLK3;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }

    /** Configure the programming delay
     */
    __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_2);
}

/* USER CODE BEGIN 4 */

static void setGpio(UserLeds pin, GPIO_PinState state) {
    switch (pin) {
    case LED1:
        HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, state);
        break;
    case LED2:
        HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, state);
        break;
    case LED3:
        HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, state);
        break;
    default:
        // Handle invalid pin if necessary
        break;
    }
}

bool readGpio(uint8_t port, uint8_t pin, uint8_t* state) {
    if (state == 0) {
        return false;
    }

    GPIO_TypeDef* gpio_port;
    uint16_t gpio_pin;
    if (!resolveUserPin(port, pin, &gpio_port, &gpio_pin)) {
        return false;
    }

    *state = (HAL_GPIO_ReadPin(gpio_port, gpio_pin) == GPIO_PIN_SET) ? 1U : 0U;
    return true;
}

static bool resolveUserPin(uint8_t port, uint8_t pin, GPIO_TypeDef** gpio_port,
                           uint16_t* gpio_pin) {
    if (port > PORT_B || pin >= 16U) {
        return false;
    }

    uint16_t candidate = (uint16_t)(1U << pin);
    if ((user_pin_masks[port] & candidate) == 0U) {
        return false;
    }

    *gpio_port = (port == PORT_A) ? GPIOA : GPIOB;
    *gpio_pin = candidate;
    return true;
}

bool setGpioMode(uint8_t port, uint8_t pin, uint8_t mode) {
    if (mode != PIN_MODE_INPUT && mode != PIN_MODE_OUTPUT) {
        valid_pinmode = false;
        return false;
    }

    GPIO_TypeDef* gpio_port;
    uint16_t gpio_pin;
    if (!resolveUserPin(port, pin, &gpio_port, &gpio_pin)) {
        valid_pinmode = false;
        return false;
    }

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = gpio_pin;
    GPIO_InitStruct.Mode = (mode == PIN_MODE_OUTPUT) ? GPIO_MODE_OUTPUT_PP : GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(gpio_port, &GPIO_InitStruct);

    valid_pinmode = true;
    return true;
}
// Apply a single pin command decoded from an incoming CAN message.
//   port       -> GpioPort (0 = GPIOA, 1 = GPIOB, 2 = LEDs)
//   pin        -> pin number within the port (LEDs are 0-indexed)
//   state_byte -> 0 drives the pin LOW, any other value drives it HIGH
// GPIO pins not present in user_pin_masks are silently ignored so a bad request
// cannot disturb reserved pins (CAN standby, SWD, etc.).
static void applyPinCommand(uint8_t port, uint8_t pin, uint8_t state_byte) {
    GPIO_PinState gpioState = state_byte ? GPIO_PIN_SET : GPIO_PIN_RESET;

    switch (port) {
    case PORT_LEDS:
        switch ((UserLeds)pin) {
        case LED1:
        case LED2:
        case LED3:
            setGpio((UserLeds)pin, gpioState);
            break;
        default:
            break; // Unlisted LED: ignore
        }
        break;

    case PORT_A:
    case PORT_B: {
        GPIO_TypeDef* gpio_port;
        uint16_t gpio_pin;

        if (resolveUserPin(port, pin, &gpio_port, &gpio_pin)) {
            HAL_GPIO_WritePin(gpio_port, gpio_pin, gpioState);
        }
        break;
    }

    default:
        break; // Unknown port: ignore
    }
}

/* USER CODE END 4 */

/* MPU Configuration */

void MPU_Config(void) {
    MPU_Region_InitTypeDef MPU_InitStruct = {0};
    MPU_Attributes_InitTypeDef MPU_AttributesInit = {0};

    /* Disables the MPU */
    HAL_MPU_Disable();

    /** Initializes and configures the Region 0 and the memory to be protected
     */
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress = 0x08FFF000;
    MPU_InitStruct.LimitAddress = 0x08FFFFFF;
    MPU_InitStruct.AttributesIndex = MPU_ATTRIBUTES_NUMBER0;
    MPU_InitStruct.AccessPermission = MPU_REGION_ALL_RO;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;

    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    /** Initializes and configures the Attribute 0 and the memory to be protected
     */
    MPU_AttributesInit.Number = MPU_ATTRIBUTES_NUMBER0;
    MPU_AttributesInit.Attributes = INNER_OUTER(MPU_NOT_CACHEABLE);

    HAL_MPU_ConfigMemoryAttributes(&MPU_AttributesInit);
    /* Enables the MPU */
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
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
void assert_failed(uint8_t* file, uint32_t line) {
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

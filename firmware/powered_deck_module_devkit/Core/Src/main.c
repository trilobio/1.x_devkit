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
#include "icache.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "protocol.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// incoming_data[0] selects which group of pins the command targets.
typedef enum {
  GROUP_LED = 0,
  GROUP_PORT_A = 1,
  GROUP_PORT_B = 2,
} PinGroup;

// LED ids are 1-indexed to match the wire protocol (incoming_data[1] == 1 -> LED1).
typedef enum {
  LED1 = 1,
  LED2 = 2,
  LED3 = 3,
} UserLeds;

// Port A/B enum values equal the GPIO pin number, which is also the bit index used by
// GPIO_PIN_n (== 1 << n) and the value carried in incoming_data[1]. Only the pins listed
// here are exposed for control; everything else is ignored. Omitted on purpose:
// PA5 (STBY, CAN transceiver), PA13/PA14 (SWD), PB3 (SWD), PB4/PB13/PB14 (reserved).
typedef enum {
  PA0 = 0, PA1 = 1, PA2 = 2, PA3 = 3, PA4 = 4,
  PA6 = 6, PA7 = 7, PA8 = 8, PA9 = 9, PA10 = 10, PA11 = 11, PA12 = 12,
} UserPortAPins;

typedef enum {
  PB0 = 0, PB1 = 1, PB2 = 2, PB6 = 6, PB7 = 7,
  PB8 = 8, PB10 = 10, PB12 = 12, PB15 = 15,
} UserPortBPins;

typedef enum {
  LOW = 0,
  HIGH = 1,
} PinState;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t incoming_data[64];
bool processed_incoming_data = false;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
void setGpio(UserLeds pin, PinState state);
static void applyPinCommand(uint8_t group, uint8_t pin, uint8_t state_byte);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

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
  setGpio(LED1, LOW);
  setGpio(LED2, HIGH);
  setGpio(LED3, LOW);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    
    if (receivedCanMessage) {
        handleCanMessage();
    }

    if (!processed_incoming_data) {
        // incoming_data[0] = group (0=LED, 1=port A, 2=port B)
        // incoming_data[1] = pin within the group
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
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

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
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the programming delay
  */
  __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_2);
}

/* USER CODE BEGIN 4 */

void setGpio(UserLeds pin, PinState state){
  GPIO_PinState gpioState = (state == HIGH) ? GPIO_PIN_SET : GPIO_PIN_RESET;

  switch(pin) {
    case LED1:
      HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, gpioState);
      break;
    case LED2:
      HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, gpioState);
      break;
    case LED3:
      HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, gpioState);
      break;
    default:
      // Handle invalid pin if necessary
      break;
  }
}

// Apply a single pin command decoded from an incoming CAN message.
//   group      -> PinGroup (0 = LED, 1 = port A, 2 = port B)
//   pin        -> pin selector within the group (see the Port A/B enums; LEDs are 1-indexed)
//   state_byte -> 0 drives the pin LOW, any other value drives it HIGH
// Pins not listed in the enums are silently ignored so a bad request cannot disturb
// reserved pins (CAN standby, SWD, etc.).
static void applyPinCommand(uint8_t group, uint8_t pin, uint8_t state_byte) {
  PinState state = state_byte ? HIGH : LOW;
  GPIO_PinState gpioState = (state == HIGH) ? GPIO_PIN_SET : GPIO_PIN_RESET;

  switch (group) {
    case GROUP_LED:
      switch ((UserLeds)pin) {
        case LED1:
        case LED2:
        case LED3:
          setGpio((UserLeds)pin, state);
          break;
        default:
          break; // Unlisted LED: ignore
      }
      break;

    case GROUP_PORT_A:
      switch ((UserPortAPins)pin) {
        case PA0: case PA1: case PA2: case PA3: case PA4:
        case PA6: case PA7: case PA8: case PA9:
        case PA10: case PA11: case PA12:
          HAL_GPIO_WritePin(GPIOA, (uint16_t)(1U << pin), gpioState);
          break;
        default:
          break; // Unlisted / reserved port A pin: ignore
      }
      break;

    case GROUP_PORT_B:
      switch ((UserPortBPins)pin) {
        case PB0: case PB1: case PB2: case PB6: case PB7:
        case PB8: case PB10: case PB12: case PB15:
          HAL_GPIO_WritePin(GPIOB, (uint16_t)(1U << pin), gpioState);
          break;
        default:
          break; // Unlisted / reserved port B pin: ignore
      }
      break;

    default:
      break; // Unknown group: ignore
  }
}

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
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
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
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

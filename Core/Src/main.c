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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "wizchip_port.h"
#include "usart1.h"
#include "socket.h"
#include "wizchip_conf.h"
#include "gpio.h"
#include "string.h"
#include "mq2_sensor.h"
#include <stdio.h>
#include "string.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define TCP_SERVER_SOCKET   0
#define TCP_SERVER_PORT     5000
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

SPI_HandleTypeDef hspi1;

/* Definitions for TcpReceiveTask */
osThreadId_t TcpReceiveTaskHandle;
const osThreadAttr_t TcpReceiveTask_attributes = {.name = "TcpReceiveTask", .stack_size = 512 * 4, .priority =
    (osPriority_t) osPriorityNormal, };
/* Definitions for LedControlTask */
osThreadId_t LedControlTaskHandle;
const osThreadAttr_t LedControlTask_attributes = {.name = "LedControlTask", .stack_size = 512 * 4, .priority =
    (osPriority_t) osPriorityNormal1, };
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_ADC1_Init(void);
void StartTcpReceiveTask(void *argument);
void StartLedControlTask(void *argument);

/* USER CODE BEGIN PFP */

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
  MX_SPI1_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE BEGIN 2 */
  USART1_Init();

  if(MQ2_Init(&hadc1) != HAL_OK)
  {
    USART1_SendString("MQ-2 Init FAILED!\r\n");
    Error_Handler();
  }

  USART1_SendString("MQ-2 Sensor Initialized\r\n");
  USART1_SendString("Testing ADC...\r\n");
  for(int i = 0; i < 3; i++)
  {
    uint32_t raw = MQ2_ReadRawADC();
    float voltage = (raw * 3.3f) / 4095.0f;
    char msg[60];
    sprintf(msg, "ADC: %lu = %.2fV\r\n", raw, voltage);
    USART1_SendString(msg);
    HAL_Delay(500);
  }

  /* Step 2: Warm up */
  USART1_SendString("\r\nWarming up MQ-2 (30 sec)...\r\n");
  for(int i = 30; i > 0; i--)
  {
    char count_msg[20];
    sprintf(count_msg, "%d seconds left\r\n", i);
    USART1_SendString(count_msg);
    HAL_Delay(1000);
  }

  /* Step 3: Calibrate */
  USART1_SendString("\r\nCalibrating...\r\n");
  if(MQ2_Calibrate(50))
  {
    USART1_SendString("Calibration SUCCESSFUL!\r\n");

    /* Show calibration results */
    float voltage = MQ2_GetVoltage();
    float ppm = MQ2_GetPPM();
    const char *level = MQ2_GetLevelString();

    char result_msg[300];
    sprintf(result_msg, "\r\n=== Calibration Results ===\r\n"
        "Voltage: %.2f V\r\n"
        "PPM: %.0f\r\n"
        "Level: %s\r\n"
        "\r\nExpected (clean air):\r\n"
        "Voltage: 0.1-0.3V | PPM: <100 | Level: NORMAL\r\n"
        "\r\nNow test with gas (lighter without flame)!\r\n"
        "You should see voltage and PPM increase.\r\n", voltage, ppm, level);
    USART1_SendString(result_msg);

    /* Step 4: Continuous monitoring for 30 seconds */
    USART1_SendString("\r\n=== Monitoring for 30 seconds ===\r\n");
    for(int i = 0; i < 15; i++)
    {
      float v = MQ2_GetVoltage();
      float p = MQ2_GetPPM();
      const char *lvl = MQ2_GetLevelString();

      char monitor_msg[80];
      sprintf(monitor_msg, "[%d] %.2fV | %.0fppm | %s\r\n", i + 1, v, p, lvl);
      USART1_SendString(monitor_msg);

      HAL_Delay(2000);
    }
  }
  else
  {
    USART1_SendString("Calibration FAILED!\r\n");
    USART1_SendString("Check:\r\n");
    USART1_SendString("- Voltage divider (10k+20k)\r\n");
    USART1_SendString("- PA0 connected correctly\r\n");
    USART1_SendString("- Sensor warmed up\r\n");
  }

  USART1_SendString("\r\n=== Starting TCP Server ===\r\n");

  if(W5500_Init() != 0)
  {
    Error_Handler();
  }

  // Create socket
  if(socket(TCP_SERVER_SOCKET, Sn_MR_TCP, TCP_SERVER_PORT, 0) != TCP_SERVER_SOCKET)
  {
    USART1_SendString("Socket open failed\r\n");
  }

  // Start listening
  if(listen(TCP_SERVER_SOCKET) != SOCK_OK)
  {
    USART1_SendString("Listen failed\r\n");
    close(TCP_SERVER_SOCKET);
  }

  USART1_SendString("TCP Server with LED Control on port ");
  USART1_SendNumber(TCP_SERVER_PORT);
  USART1_SendString("\r\n");

  // Initialize LED
  LED_init();
  LED_OFF();  // Start with LED off

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of TcpReceiveTask */
  TcpReceiveTaskHandle = osThreadNew(StartTcpReceiveTask, NULL, &TcpReceiveTask_attributes);

  /* creation of LedControlTask */
  LedControlTaskHandle = osThreadNew(StartLedControlTask, NULL, &LedControlTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while(1)
  {
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
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if(HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
 * @brief ADC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
   */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if(HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
   */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
  if(HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
 * @brief SPI1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if(HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, RESET_Pin | CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : RESET_Pin CS_Pin */
  GPIO_InitStruct.Pin = RESET_Pin | CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartTcpReceiveTask */
/**
 * @brief  Function implementing the TcpReceiveTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartTcpReceiveTask */
void StartTcpReceiveTask(void *argument)
{
  /* USER CODE BEGIN 5 */

  uint16_t available_data = 0;
  uint8_t buffer[256];
#define BUFFER_SIZE 256

  /* Infinite loop */
  for(;;)
  {
    uint8_t status = getSn_SR(TCP_SERVER_SOCKET);
    switch(status)
    {
      case SOCK_ESTABLISHED:
        // Check for data
        available_data = getSn_RX_RSR(TCP_SERVER_SOCKET);

        if(available_data > 0)
        {
          int32_t ret = recv(TCP_SERVER_SOCKET, buffer, (available_data < BUFFER_SIZE) ? available_data : BUFFER_SIZE);

          if(ret > 0)
          {
            buffer[ret] = '\0';
            USART1_SendString("Received: ");
            USART1_SendString((char*) buffer);
            USART1_SendString("\r\n");

            // Remove newline characters for comparison
            for(int i = 0; i < ret; i++)
            {
              if(buffer[i] == '\n' || buffer[i] == '\r')
              {
                buffer[i] = '\0';
                break;
              }
            }

            // Parse command and control LED
            if(strncmp((char*) buffer, "ON", 2) == 0 || strncmp((char*) buffer, "on", 2) == 0)
            {
              osThreadFlagsSet(LedControlTaskHandle, 0x01); // Flag to turn LED ON
            }
            else if(strncmp((char*) buffer, "OFF", 3) == 0 || strncmp((char*) buffer, "off", 3) == 0)
            {
              osThreadFlagsSet(LedControlTaskHandle, 0x02); // Flag to turn LED OFF
            }
          }
        }
        break;

      case SOCK_CLOSE_WAIT:
        USART1_SendString("\r\nClient disconnected\r\n");
        disconnect(TCP_SERVER_SOCKET);
        break;

      case SOCK_CLOSED:
        USART1_SendString("Waiting for new client...\r\n");
        if(socket(TCP_SERVER_SOCKET, Sn_MR_TCP, TCP_SERVER_PORT, 0) == TCP_SERVER_SOCKET)
        {
          listen(TCP_SERVER_SOCKET);
        }
        break;

      default:
        break;
    }

    osDelay(1);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartLedControlTask */
/**
 * @brief Function implementing the LedControlTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartLedControlTask */
void StartLedControlTask(void *argument)
{
  /* USER CODE BEGIN StartLedControlTask */
  /* Infinite loop */

  uint8_t buffer[256];
  for(;;)
  {
    uint32_t flag = osThreadFlagsWait(0x01 | 0x02, osFlagsWaitAny, osWaitForever);
    if(flag == 0x01) // Turn LED ON
    {
      LED_ON();
      strcpy((char*) buffer, "LED Turned ON\r\n");
      send(TCP_SERVER_SOCKET, buffer, strlen((char*) buffer));
    }
    else if(flag == 0x02) // Turn LED OFF
    {
      LED_OFF();
      strcpy((char*) buffer, "LED Turned OFF\r\n");
      send(TCP_SERVER_SOCKET, buffer, strlen((char*) buffer));
    }
    osDelay(1);
  }
  /* USER CODE END StartLedControlTask */
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM2 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if(htim->Instance == TIM2)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
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
  while(1)
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

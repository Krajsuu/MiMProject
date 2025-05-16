/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "SSD1331.h"
#include "aht30.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi3;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
float temp_threshold = 27.0;
float hum_threshold = 60.0;
#define ADC_BUF_LEN 2
uint16_t adc_buf[ADC_BUF_LEN]; // DMA wypełnia ten bufor
volatile uint8_t screenMode = 1;
volatile uint32_t screenTimer = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI3_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);
/* USER CODE BEGIN PFP */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_0 ) { // Debounce 200 ms
        if(screenMode == 0) { // Jeśli ekran był wyłączony
            screenMode = 1;
            SSD1331_DisplayON();
            screenTimer = HAL_GetTick(); // Resetuj timer przy KAŻDYM włączeniu
        } else {
            screenMode = 0;
            SSD1331_DisplayOFF();
        }
    }
}
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
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_SPI3_Init();
  MX_I2C1_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buf, ADC_BUF_LEN);
  HAL_Delay(100);
  SSD1331_init();
  HAL_Delay(100);
  SSD1331_drawFrame(0, 0, RGB_OLED_WIDTH - 1, RGB_OLED_HEIGHT - 1, COLOR_BLACK, COLOR_BLACK);
  HAL_Delay(150);
  SSD1331_SetXY(RGB_OLED_WIDTH/2 - 40, RGB_OLED_HEIGHT/2 - 10);
  char *wiadomosc = "WELCOME";
  SSD1331_FStr(FONT_2X, (unsigned char*) wiadomosc, COLOR_WHITE, COLOR_BLACK);
  SSD1331_SetXY(RGB_OLED_WIDTH/2 - 38, RGB_OLED_HEIGHT/2 +15);
  char *wiadomosc2 = "Projekt KZ,KT";
  SSD1331_FStr(FONT_1X, (unsigned char*) wiadomosc2, COLOR_WHITE, COLOR_BLACK);
  HAL_Delay(3000);
  SSD1331_drawFrame(0, 0, RGB_OLED_WIDTH - 1, RGB_OLED_HEIGHT - 1, COLOR_BLACK, COLOR_BLACK);
  HAL_Delay(100);
  screenMode = 1;
  screenTimer = HAL_GetTick();
  /*
  char buf[16];
  sprintf(buf, "%d", 123);
  SSD1331_SetXY(RGB_OLED_WIDTH/2 - 10, RGB_OLED_HEIGHT/2 - 1, 10);
  SSD1331_FStr(FONT_1X, (unsigned char*)buf, COLOR_WHITE, COLOR_BLACK);*/
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  float temp, hum;
  uint32_t led_tick = 0;
  uint8_t led_temp_state = 0;
  uint8_t led_hum_state = 0;


  while (1)
  {
	  printf("Timer wynosi: %lu\r\n", HAL_GetTick()-screenTimer);
	  if(screenMode && (HAL_GetTick() - screenTimer > 30000)) {
	          screenMode = 0;
	          SSD1331_DisplayOFF();
	   }
	  if(screenMode){
	          uint16_t xValue = adc_buf[0]; // PC1 (nieużywany, ale gotowy do rozbudowy)
	          uint16_t yValue = adc_buf[1]; // PC2

	          // Wyczyszczenie ekranu
	          SSD1331_drawFrame(0, 0, RGB_OLED_WIDTH - 1, RGB_OLED_HEIGHT - 1, COLOR_BLACK, COLOR_BLACK);
	          HAL_Delay(100);
	          if (HAL_GetTick() - led_tick >= 500) {
	          	        led_tick = HAL_GetTick();

	          	        if (AHT30_Read(&temp, &hum) == HAL_OK) {
	          	          char *tempTag = "Temperatura :";
	          	          char *humTag =  "Wilgotnosc : ";
	          	          char temp_val[16];
	          	          char hum_val[16];

	          	        sprintf(temp_val, "%.2f st. C",temp);
	          	        sprintf(hum_val, "%.2f %%" ,hum);
	          	        SSD1331_drawFrame(0, 0, RGB_OLED_WIDTH - 1, RGB_OLED_HEIGHT - 1, COLOR_BLACK, COLOR_BLACK);

	          	          if (xValue < 1000)
	          	          {
	          	        	 SSD1331_SetXY(RGB_OLED_WIDTH/2 - 40, RGB_OLED_HEIGHT/2 - 10);
	          	             SSD1331_FStr(FONT_1X, (unsigned char*)tempTag, COLOR_RED, COLOR_BLACK);
	          	             SSD1331_SetXY(RGB_OLED_WIDTH/2 - 35, RGB_OLED_HEIGHT/2 +6);
	          	           	 SSD1331_FStr(FONT_1X, (unsigned char*)temp_val, COLOR_RED, COLOR_BLACK);
	          	           if (temp > temp_threshold) {
	          	           	    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET); // LED ON
	          	           } else if (temp < temp_threshold) {
	          	           	    led_temp_state = !led_temp_state;
	          	           	    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, led_temp_state);
	          	           } else {
	          	           	    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
	          	           }
	          	          }
	          	          else if (xValue > 3000)
	          	          {
	          	        	SSD1331_SetXY(RGB_OLED_WIDTH/2 - 32, RGB_OLED_HEIGHT/2 - 10);
	          	        	SSD1331_FStr(FONT_1X, (unsigned char*)humTag, COLOR_BLUE, COLOR_BLACK);
	          	        	SSD1331_SetXY(RGB_OLED_WIDTH/2 - 20, RGB_OLED_HEIGHT/2 + 6);
	          	        	SSD1331_FStr(FONT_1X, (unsigned char*)hum_val, COLOR_BLUE, COLOR_BLACK);
	          	           if (hum > hum_threshold) {
	          	           	 HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
	          	           } else if (hum < hum_threshold) {
	          	           	 led_hum_state = !led_hum_state;
	          	           	 HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, led_hum_state);
	          	           } else {
	          	           	 HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
	          	           }
	          	          }
	          	          // Możesz dodać else, by czyścić ekran lub wyświetlać inne stany

	          	          HAL_Delay(1400); // Odświeżanie co 150 ms
	          	          }
	          }

	  /*
	  if (HAL_GetTick() - led_tick >= 500) {
	        led_tick = HAL_GetTick();

	        if (AHT30_Read(&temp, &hum) == HAL_OK) {
	          char temp_str[16];
	          char hum_str[16];

	        	    // Format values with 1 decimal place
	          sprintf(temp_str, "Temp: %.2f C", temp);
	          sprintf(hum_str, "Hum : %.2f %%", hum);

	        	    // Clear display
	          SSD1331_drawFrame(0, 0, RGB_OLED_WIDTH - 1, RGB_OLED_HEIGHT - 1, COLOR_BLACK, COLOR_BLACK);

	        	    // Display temperature on first line
	          SSD1331_SetXY(0, 0);
	          SSD1331_FStr(FONT_1X, (unsigned char*)temp_str, COLOR_RED, COLOR_BLACK);

	        	    // Display humidity on second line
	          SSD1331_SetXY(0, 16); // Position for second line
	          SSD1331_FStr(FONT_1X, (unsigned char*)hum_str, COLOR_BLUE, COLOR_BLACK);

	        	    // Shorter delay or consider removing delay and using timer interrupts
	          HAL_Delay(2000);
	          // TEMP LED logic
	          if (temp > temp_threshold) {
	            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_13, GPIO_PIN_SET); // LED ON
	          } else if (temp < temp_threshold) {
	            led_temp_state = !led_temp_state;
	            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_13, led_temp_state);
	          } else {
	            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_13, GPIO_PIN_RESET);
	          }

	          // HUM LED logic
	          if (hum > hum_threshold) {
	            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
	          } else if (hum < hum_threshold) {
	            led_hum_state = !led_hum_state;
	            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, led_hum_state);
	          } else {
	            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
	          }
	        }
	      }
	    }*/
	  }
	  HAL_Delay(100);
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
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
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 2;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_7;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.SamplingTime = ADC_SAMPLETIME_181CYCLES_5;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_8;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00201D2B;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI3_Init(void)
{

  /* USER CODE BEGIN SPI3_Init 0 */

  /* USER CODE END SPI3_Init 0 */

  /* USER CODE BEGIN SPI3_Init 1 */

  /* USER CODE END SPI3_Init 1 */
  /* SPI3 parameter configuration*/
  hspi3.Instance = SPI3;
  hspi3.Init.Mode = SPI_MODE_MASTER;
  hspi3.Init.Direction = SPI_DIRECTION_2LINES;
  hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi3.Init.NSS = SPI_NSS_SOFT;
  hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi3.Init.CRCPolynomial = 7;
  hspi3.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi3.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI3_Init 2 */

  /* USER CODE END SPI3_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 38400;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, CS_Pin|RESET_Pin|DC_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PC0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : CS_Pin RESET_Pin DC_Pin */
  GPIO_InitStruct.Pin = CS_Pin|RESET_Pin|DC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
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
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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

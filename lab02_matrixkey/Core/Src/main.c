/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : 3×4 矩阵键盘扫描 + 8421BCD 码显示
  ******************************************************************************
  * @attention
  *
  * 硬件：行线 PE4~PE6（开漏输出），列线 PE0~PE3（上拉输入），
  *       显示用 PF0~PF7 八颗 LED（低电平点亮）。
  *       键值 K1~K12 按行优先编号：第 1 行 K1~K4、第 2 行 K5~K8、第 3 行 K9~K12。
  *
  * 结构：key 驱动（matrix_key.c）负责扫描与消抖，main 只处理"按下"事件，
  *       两者都不阻塞，主循环可以继续挂载其它任务。
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "matrix_key.h"
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

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
  * @brief  把键值以 8421BCD 码显示到 8 颗 LED 上
  * @param  key：键值 0~12（0 表示不显示）
  * @note   PF4~PF7 显示十位，PF0~PF3 显示个位；LED 低电平点亮。
  *         用 BSRR 一次写完整口低 8 位，避免误改 PF8~PF15。
  */
static void KeyDisplay_Show(uint8_t key)
{
  uint16_t bcd;
  uint16_t lit;
  uint16_t off;

  bcd = (uint16_t)(((key / 10U) << 4) | (key % 10U));   /* 十位放高 4 位，个位放低 4 位 */
  lit = (uint16_t)(bcd & 0x00FFU);                       /* 1 = 点亮（输出低）           */
  off = (uint16_t)(~lit & 0x00FFU);                      /* 1 = 熄灭（输出高）           */

  GPIOF->BSRR = ((uint32_t)lit << 16) | (uint32_t)off;
}
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
  /* USER CODE BEGIN 2 */
  MatrixKey_Init();                     /* 释放所有行线               */
  KeyDisplay_Show(MATRIX_KEY_NONE);     /* 上电先显示 0（8 颗灯全灭）  */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint8_t          key = MATRIX_KEY_NONE;
    MatrixKeyEvent_t ev  = MatrixKey_GetEvent(&key);

    /* 只有"消抖后的按下"才刷新显示，抖动和长按都不会重复触发 */
    if (ev == MATRIX_KEY_EVT_DOWN)
    {
      KeyDisplay_Show(key);
    }

    /* 默认每 1ms 扫描一次键盘（单次约 60~80µs，CPU 占用 < 10%），
       没有 HAL_Delay() 阻塞，可以在这里继续添加其它任务 */
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/*
 * 键值分配（行优先，与实验板丝印 K1~K12 一致）：
 *
 *              PE0(COL0)  PE1(COL1)  PE2(COL2)  PE3(COL3)
 *   PE4(ROW0)      1          2          3          4        <- K1 K2 K3 K4
 *   PE5(ROW1)      5          6          7          8        <- K5 K6 K7 K8
 *   PE6(ROW2)      9         10         11         12        <- K9 K10 K11 K12
 *
 * 显示：PF4~PF7 = 十位 BCD，PF0~PF3 = 个位 BCD，均为低电平点亮。
 *       例如按下 K12 → 十位 1(PF4 亮) + 个位 2(PF1 亮)。
 */

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

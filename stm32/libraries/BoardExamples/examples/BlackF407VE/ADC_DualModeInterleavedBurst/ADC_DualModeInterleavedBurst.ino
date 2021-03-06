/**
 * STM32GENERIC - untested
 * 
 * 8 bit samples, ADC1 & 2, DMA
 * Interrupt processes can eat all the CPU time, so long sample times and big buffers to reduce the number of interrupts.
 * Example uses software trigger for 8 bit sampling in bursts. 
 * Conversion can be kept running by moving ADC/DMA start to setup().
 * 
 * Current HAL_ADCEx_MultiModeConfigChannel() always configures 32 bit transfers. 
 * High 16 bits of each quad-byte DMA transfer will always be zero.
 * 
  ******************************************************************************
  * @file    ADC/ADC_DualModeInterleaved/Src/main.c 
  * @author  MCD Application Team
  * @version V1.4.0
  * @date    17-February-2017
  * Modified 9-May-2017 RP for Arduino
  * @brief   This example provides a short description of how to use the ADC 
  *          peripheral to convert a regular channel in Dual interleaved mode.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2017 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"


/** @addtogroup STM32F4xx_HAL_Examples
  * @{
  */

/** @addtogroup ADC_DualModeInterleaved
  * @{
  */ 

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* ADC handler declaration */
ADC_HandleTypeDef    AdcHandle1;
ADC_HandleTypeDef    AdcHandle2;
ADC_MultiModeTypeDef   ADCmode;
/* Variable used to get converted value */
__IO uint16_t uhADCDualConvertedValue = 0;
#define NSAMPLES 512
__IO uint8_t convBuffer[NSAMPLES][4]; // 2 samples per cycle. DMA transfers at 32 bit boundaries, half are empty. 
#define NPRINT 64   // just display the first few pairs of samples

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void Error_Handler(void);
static void ADC_Config(void);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
void setup()
{
  SERIALX.stm32SetTX(PC10); 
  SERIALX.stm32SetRX(PC11);  
  SERIALX.begin(115200);
  delay(200);
  SERIALX.println("\nADC Demo - Dual Mode 8 bit Interleaved Conversion, DMA Mode 3: ADC1 & ADC2 on PA1");
  
  HAL_Init();
  
  /* Configure LED1 (status) and LED3 (error) */
  BSP_LED_Init(LED1);
  BSP_LED_Init(LED3);
  BSP_LED_Off(LED1);
  BSP_LED_Off(LED3);
}

volatile uint32_t started, finished;
int32_t elapsed;
float tsamp;
void loop(void){ 
/* Stopping the transfer when the busrt is complete leaves the conversion buffer unchanged while it's being printed/processed.
 *  
 * ADC/DMA can be left running for continuous conversion. 
 * Move the ADC_Config() & HAL_ADC_Start() code to setup()  
 * Remove the HAL_ADCEx_MultiModeStop_DMA() code from HAL_ADC_ConvCpltCallback() 
 * ADC_ResetToIndep() can also be deleted. It is only required if stopping/restarting Interleaved ADC/DMA.
 */
  SERIALX.println("ADC Reset & Config"); 

  // Reset ADC to Independent mode before triggering a new DMA burst. 
  // Not required for continuous conversions.
  ADC_ResetToIndep(); 
  /*##-1- Configure ADC1 and ADC2 peripherals ################################*/
  ADC_Config();

  /*##-2- Enable ADC2 ########################################################*/
  started = micros();
  if(HAL_ADC_Start(&AdcHandle2) != HAL_OK)
  {
    /* Start Error */
    Error_Handler(1); 
  }
  
  /*##-3- Start ADC1 and ADC2 multimode conversion process and enable DMA ####*/
  /* Note: Considering IT occurring after each number of ADC conversions      */
  /*       (IT by DMA end of transfer), select sampling time and ADC clock    */
  /*       with sufficient duration to not create an overhead situation in    */
  /*        IRQHandler. */
  if(HAL_ADCEx_MultiModeStart_DMA(&AdcHandle1, (uint32_t*)convBuffer, NSAMPLES) != HAL_OK)
  {
    /* Start Error */
    Error_Handler(2); 
  } 
    delay(100); // wait for sampling to finish
 
    /*    
    SERIALX.print("Started ");
    SERIALX.print(started);
    SERIALX.print(", finished ");
    SERIALX.println(finished);
    */
    elapsed = finished - started;
    // SERIALX.print(started);
    SERIALX.print("Capture took ");
    SERIALX.print(elapsed);
    SERIALX.print(" us, time per sample ");
    tsamp = (float)elapsed/(2*NSAMPLES);
    SERIALX.print(tsamp);
    SERIALX.print(" us, sample frequency ");
    SERIALX.print(1/tsamp);
    SERIALX.print(" MHz\nValues:\n");
    for(int i=0; i < min(NPRINT, NSAMPLES); i++){ // only print the first few samples to speed things up
       SERIALX.print(convBuffer[i][0]);
       SERIALX.print(", ");
       SERIALX.print(convBuffer[i][1]);
       SERIALX.print(", ");
       if(i % 4 == 3) SERIALX.println("");
    }
    
    SERIALX.println("***");
    delay(1000);
    BSP_LED_Off(LED1); // turn off indicator that DMA is completed
    delay(1000);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
static void Error_Handler(int eVal)
{
/* Sequence of short pulses, then gap. Repeat forever. */
    while(1) { 
      for(int i = 0; i < eVal; i++){
         BSP_LED_On(LED3);
         delay (200);
         BSP_LED_Off(LED3);
         delay (200);
      } 
      delay(1000);
   }
}

/**
  * @brief  ADC configuration
  * @note   This function Configure the ADC peripheral  
            1) Enable peripheral clocks
            2) Configure ADC Channel 12 pin as analog input
            3) DMA2_Stream0 channel2 configuration
            4) Configure ADC1 Channel 12
            5) Configure ADC2 Channel 12             
  * @param  None
  * @retval None
  */
static void ADC_Config(void)
{
  ADC_ChannelConfTypeDef sConfig;

  
  /*##-1- Configure the ADC2 peripheral ######################################*/
  AdcHandle2.Instance          = ADCy;
  
  AdcHandle2.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV2;
  AdcHandle2.Init.Resolution = ADC_RESOLUTION_8B;
  AdcHandle2.Init.ScanConvMode = ENABLE;
  AdcHandle2.Init.ContinuousConvMode = ENABLE;
  AdcHandle2.Init.DiscontinuousConvMode = DISABLE;
  AdcHandle2.Init.NbrOfDiscConversion = 0;
  AdcHandle2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  AdcHandle2.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T1_CC1;
  AdcHandle2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  AdcHandle2.Init.NbrOfConversion = 1;
  AdcHandle2.Init.DMAContinuousRequests = ENABLE;
  AdcHandle2.Init.EOCSelection = ENABLE;
      
  if(HAL_ADC_Init(&AdcHandle2) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler(4); 
  }
  
  // Interleaved mode: settings same for both channels
  sConfig.Channel = ADCxy_CHANNEL;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES; //ADC_SAMPLETIME_84CYCLES; //ADC_SAMPLETIME_3CYCLES
  sConfig.Offset = 0;
  
  /*##-2- Configure ADC2 regular channel #####################################*/   
  if(HAL_ADC_ConfigChannel(&AdcHandle2, &sConfig) != HAL_OK)
  {
    /* Channel Configuration Error */
    Error_Handler(3); 
  }
  
  /*##-3- Configure the ADC1 peripheral ######################################*/
  AdcHandle1.Instance          = ADCx;
  
  AdcHandle1.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV2;
  AdcHandle1.Init.Resolution = ADC_RESOLUTION_8B;
  AdcHandle1.Init.ScanConvMode = DISABLE;
  AdcHandle1.Init.ContinuousConvMode = ENABLE;
  AdcHandle1.Init.DiscontinuousConvMode = DISABLE;
  AdcHandle1.Init.NbrOfDiscConversion = 0;
  AdcHandle1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  AdcHandle1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T1_CC1;
  AdcHandle1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  AdcHandle1.Init.NbrOfConversion = 1;
  AdcHandle1.Init.DMAContinuousRequests = ENABLE;
  AdcHandle1.Init.EOCSelection = ENABLE;
      
  if(HAL_ADC_Init(&AdcHandle1) != HAL_OK)
  {
    /* Initialization Error */
    Error_Handler(5); 
  }
  
  /*##-4- Configure ADC1 regular channel #####################################*/  
  if(HAL_ADC_ConfigChannel(&AdcHandle1, &sConfig) != HAL_OK)
  {
    /* Channel Configuration Error */
    Error_Handler(6); 
  }
  
  /*##-5- Configure Multimode ################################################*/
  ADCmode.Mode = ADC_DUALMODE_INTERL;
  ADCmode.DMAAccessMode = ADC_DMAACCESSMODE_3;
  ADCmode.TwoSamplingDelay = ADC_TWOSAMPLINGDELAY_5CYCLES; 
  if(HAL_ADCEx_MultiModeConfigChannel(&AdcHandle1, &ADCmode) != HAL_OK)
  {
    /* Channel Configuration Error */
    Error_Handler(7); 
  }
}
 /*
  * If the conversion sequence is interrupted (for instance when DMA end of transfer occurs),
  * the multi-ADC sequencer must be reset by configuring it in independent mode first (bits
  * DUAL[4:0] = 00000) before reprogramming the interleaved mode. 32F407 Ref Manual, p410, Note in "Dual ADC mode"
  */
void ADC_ResetToIndep(void){  
  /* Re-configuration of HAL ADC Mode structure to Independent mode.
   * Reset ADC (see above) before triggering another ADC/DMA burst.
   */

  ADCmode.Mode = ADC_MODE_INDEPENDENT;
  ADCmode.DMAAccessMode = ADC_DMAACCESSMODE_DISABLED;
  ADCmode.TwoSamplingDelay = ADC_TWOSAMPLINGDELAY_6CYCLES; 
  if(HAL_ADCEx_MultiModeConfigChannel(&AdcHandle1, &ADCmode) != HAL_OK)
  {
    /* Channel Configuration Error */
    Error_Handler(7); 
  }
}

/**
  * @brief  Conversion complete callback in non blocking mode 
  * @param  hadc : hadc handle
  * @note   This example shows a simple way to report end of conversion, and 
  *         you can add your own implementation.    
  * @retval None
  */
  int counter;
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
   finished = micros();
  /* Turn LED1 on: Transfer process is correct */
  BSP_LED_On(LED1);

  /* Stop ADC and DMA */
  if (HAL_ADCEx_MultiModeStop_DMA(hadc) != HAL_OK){
    /* Start Error */
    Error_Handler(9); 
  }
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */


   SERIALX.print("Assert failed! ");
   SERIALX.print("File '");
   SERIALX.print((char*)file);
   SERIALX.print("', Line ");
   SERIALX.println(line);
  
  /* Rapid blink */
   while(1) { 
     BSP_LED_On(LED3);
     delay (100);
     BSP_LED_Off(LED3);
     delay (100);
    } 
}
#endif

/**
  * @}
  */

/**
  * @brief ADC MSP Initialization 
  *        This function configures the hardware resources used in this example: 
  *           - Peripheral's clock enable
  *           - Peripheral's GPIO Configuration  
  * @param huart: UART handle pointer
  * @retval None
  */
void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc)
{
  GPIO_InitTypeDef          GPIO_InitStruct;
  static DMA_HandleTypeDef  hdma_adc;
  
  /*##-1- Enable peripherals and GPIO Clocks #################################*/
  /* Enable GPIO clock */
  ADCxy_CHANNEL_GPIO_CLK_ENABLE();
  /* ADCx Periph clock enable */
  ADCx_CLK_ENABLE();
  /* ADCy Periph clock enable */
  ADCy_CLK_ENABLE();
  /* Enable DMA2 clock */
  DMAxy_CLK_ENABLE(); 
  
  /*##-2- Configure peripheral GPIO ##########################################*/ 
  /* ADCx and ADCy Channel12 GPIO pin configuration */
  GPIO_InitStruct.Pin = ADCxy_CHANNEL_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ADCxy_CHANNEL_GPIO_PORT, &GPIO_InitStruct);
  
  /*##-3- Configure the DMA streams ##########################################*/
  /* Set the parameters to be configured */
  hdma_adc.Instance = ADCxy_DMA_STREAM;
  
  hdma_adc.Init.Channel  = ADCxy_DMA_CHANNEL;
  hdma_adc.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdma_adc.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_adc.Init.MemInc = DMA_MINC_ENABLE;
  hdma_adc.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  hdma_adc.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
  hdma_adc.Init.Mode = DMA_CIRCULAR;
  hdma_adc.Init.Priority = DMA_PRIORITY_HIGH;
  hdma_adc.Init.FIFOMode = DMA_FIFOMODE_DISABLE;         
  hdma_adc.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_HALFFULL;
  hdma_adc.Init.MemBurst = DMA_MBURST_SINGLE;
  hdma_adc.Init.PeriphBurst = DMA_PBURST_SINGLE; 

  HAL_DMA_Init(&hdma_adc);
    
  /* Associate the initialized DMA handle to the the UART handle */
  __HAL_LINKDMA(hadc, DMA_Handle, hdma_adc);

  /*##-4- Configure the NVIC for DMA #########################################*/
  /* NVIC configuration for DMA transfer complete interrupt */
  HAL_NVIC_SetPriority(ADCxy_DMA_IRQn, 0, 0);   
  HAL_NVIC_EnableIRQ(ADCxy_DMA_IRQn);
}
  
/**
  * @brief ADC MSP De-Initialization 
  *        This function frees the hardware resources used in this example:
  *          - Disable the Peripheral's clock
  *          - Revert GPIO to their default state
  * @param hadc: ADC handle pointer
  * @retval None
  */
void HAL_ADC_MspDeInit(ADC_HandleTypeDef *hadc)
{
  static DMA_HandleTypeDef  hdma_adc;
  
  /*##-1- Reset peripherals ##################################################*/
  ADCxy_FORCE_RESET();
  ADCxy_RELEASE_RESET();

  /*##-2- Disable peripherals and GPIO Clocks ################################*/
  /* De-initialize the ADC3 Channel8 GPIO pin */
  HAL_GPIO_DeInit(ADCxy_CHANNEL_GPIO_PORT, ADCxy_CHANNEL_PIN);
  
  /*##-3- Disable the DMA Streams ############################################*/
  /* De-Initialize the DMA Stream associate to transmission process */
  HAL_DMA_DeInit(&hdma_adc); 
    
  /*##-4- Disable the NVIC for DMA ###########################################*/
  HAL_NVIC_DisableIRQ(ADCxy_DMA_IRQn);
}

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

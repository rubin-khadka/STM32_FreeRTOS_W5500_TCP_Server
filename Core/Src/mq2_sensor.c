/*
 * mq2_sensor.c
 *
 *  Created on: Apr 4, 2026
 *      Author: Rubin Khadka
 */

#include "mq2_sensor.h"
#include "usart1.h"
#include <stdio.h>

/* Private variables */
static ADC_HandleTypeDef *mq2_adc = NULL;

/* Initialize sensor */
HAL_StatusTypeDef MQ2_Init(ADC_HandleTypeDef *hadc)
{
  if(hadc == NULL)
    return HAL_ERROR;
  mq2_adc = hadc;
  return HAL_OK;
}

/* Get scaled voltage (0-3.3V) */
float MQ2_GetVoltage(void)
{
  if(mq2_adc == NULL)
    return 0;

  uint32_t adc_value = 0;

  HAL_ADC_Start(mq2_adc);
  if(HAL_ADC_PollForConversion(mq2_adc, 100) == HAL_OK)
  {
    adc_value = HAL_ADC_GetValue(mq2_adc);
  }
  HAL_ADC_Stop(mq2_adc);

  /* Convert to voltage and scale for your sensor */
  float voltage = (adc_value * 3.3f) / 4095.0f;
  voltage = voltage * 8.0f; /* Scale factor for your sensor */

  if(voltage > 3.3f)
    voltage = 3.3f;
  if(voltage < 0.0f)
    voltage = 0.0f;

  return voltage;
}

/* Get PPM - optimized for your readings */
float MQ2_GetPPM(void)
{
  float voltage = MQ2_GetVoltage();

  /* Adjusted formula for 0.5V = 250ppm range */
  float ppm = 50.0f + (voltage - 0.20f) * 500.0f;

  if(ppm < 30)
    ppm = 30;
  if(ppm > 5000)
    ppm = 5000;

  return ppm;
}

/* Alarm threshold */
bool MQ2_IsAlarm(void)
{
  return (MQ2_GetVoltage() >= 2.5f);
}

/* Get level string */
const char* MQ2_GetLevelString(void)
{
  float voltage = MQ2_GetVoltage();

  if(voltage < 0.5f)
    return "NORMAL";
  if(voltage < 1.2f)
    return "LOW";
  if(voltage < 2.0f)
    return "MEDIUM";
  if(voltage < 3.0f)
    return "HIGH";
  return "CRITICAL";
}

/* Raw ADC read (for debugging) */
uint32_t MQ2_ReadRawADC(void)
{
  if(mq2_adc == NULL)
    return 0;

  uint32_t adc_value = 0;

  HAL_ADC_Start(mq2_adc);
  if(HAL_ADC_PollForConversion(mq2_adc, 100) == HAL_OK)
  {
    adc_value = HAL_ADC_GetValue(mq2_adc);
  }
  HAL_ADC_Stop(mq2_adc);

  return adc_value;
}

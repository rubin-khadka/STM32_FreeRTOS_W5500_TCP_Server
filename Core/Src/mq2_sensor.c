/*
 * mq2_sensor.c
 *
 *  Created on: Apr 4, 2026
 *      Author: Rubin Khadka
 */

#include "mq2_sensor.h"
#include "usart1.h"      /* ADD THIS - for USART1_SendString */
#include "math.h"
#include <stdio.h>

/* Private variables */
static ADC_HandleTypeDef *mq2_adc = NULL;
static float mq2_r0 = 0.0f;
static bool mq2_calibrated = false;

/* Private function to calculate PPM */
static float MQ2_CalculatePPM(float ratio)
{
  /* Simplified formula for LPG/propane */
  if(ratio <= 0)
    return 0;
  return powf(10.0f, ((log10f(ratio) - log10f(0.4f)) / -0.32f));
}

/* Initialize sensor */
HAL_StatusTypeDef MQ2_Init(ADC_HandleTypeDef *hadc)
{
  if(hadc == NULL)
    return HAL_ERROR;
  mq2_adc = hadc;
  mq2_calibrated = false;
  return HAL_OK;
}

/* Calibrate in clean air */
bool MQ2_Calibrate(uint16_t samples)
{
  if(mq2_adc == NULL || samples == 0)
    return false;

  float sum_voltage = 0.0f;

  for(uint16_t i = 0; i < samples; i++)
  {
    sum_voltage += MQ2_GetVoltage();
    HAL_Delay(10);
  }

  float avg_voltage = sum_voltage / samples;

  /* Calculate Rs = RL * (Vc - Vout)/Vout */
  float rs_air = MQ2_RL_VALUE * (MQ2_VC - avg_voltage) / avg_voltage;

  /* R0 = Rs_air / 9.8 (clean air ratio from datasheet) */
  mq2_r0 = rs_air / 9.8f;
  mq2_calibrated = true;

  /* Debug output - now using proper include */
  char debug_msg[100];
  sprintf(debug_msg, "Calibration: avg_voltage=%.2fV, rs_air=%.0f, R0=%.0f\r\n", avg_voltage, rs_air, mq2_r0);
  USART1_SendString(debug_msg);

  return true;
}

/* Get sensor voltage */
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

  /* Convert to voltage at STM32 pin (0-3.3V) */
  float voltage_stm32 = (adc_value * 3.3f) / 4095.0f;

  /* Scale back to sensor voltage (with voltage divider 10k/20k = 1.5x) */
  float voltage_sensor = voltage_stm32 * 1.5f;

  if(voltage_sensor > 5.0f)
    voltage_sensor = 5.0f;
  if(voltage_sensor < 0.0f)
    voltage_sensor = 0.0f;

  return voltage_sensor;
}

/* Get PPM concentration */
float MQ2_GetPPM(void)
{
  if(!mq2_calibrated || mq2_r0 <= 0)
    return 0;

  float voltage = MQ2_GetVoltage();
  if(voltage <= 0.001f)
    return 0;

  float rs = MQ2_RL_VALUE * (MQ2_VC - voltage) / voltage;
  float ratio = rs / mq2_r0;

  /* Use the standard formula instead of linear approximation */
  float ppm = MQ2_CalculatePPM(ratio);

  if(ppm < 0)
    ppm = 0;
  if(ppm > 10000)
    ppm = 10000;

  return ppm;
}

/* Check if alarm threshold exceeded */
bool MQ2_IsAlarm(void)
{
  float voltage = MQ2_GetVoltage();
  return (voltage >= 3.0f); /* Alarm at 3V (~2000ppm) */
}

/* Get level as string (no enum to save space) */
const char* MQ2_GetLevelString(void)
{
  float voltage = MQ2_GetVoltage();

  if(voltage < 0.5f)
    return "NORMAL";
  if(voltage < 1.5f)
    return "LOW";
  if(voltage < 2.5f)
    return "MEDIUM";
  if(voltage < 3.5f)
    return "HIGH";
  return "CRITICAL";
}

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

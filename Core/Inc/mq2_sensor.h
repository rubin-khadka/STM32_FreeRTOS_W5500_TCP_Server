/*
 * mq2_sensor.h
 *
 *  Created on: Apr 4, 2026
 *      Author: Rubin Khadka
 */

#ifndef INC_MQ2_SENSOR_H_
#define INC_MQ2_SENSOR_H_

#include "main.h"
#include <stdbool.h>

#define MQ2_ADC_CHANNEL     ADC_CHANNEL_0
#define MQ2_RL_VALUE        10000.0f     /* 10kΩ load resistor */
#define MQ2_VC              5.0f         /* Circuit voltage */

/* Exported functions */
HAL_StatusTypeDef MQ2_Init(ADC_HandleTypeDef *hadc);
bool MQ2_Calibrate(uint16_t samples);
float MQ2_GetVoltage(void);
float MQ2_GetPPM(void);
bool MQ2_IsAlarm(void);
const char* MQ2_GetLevelString(void);
uint32_t MQ2_ReadRawADC(void);

#endif /* INC_MQ2_SENSOR_H_ */

#ifndef INC_AHT30_H_
#define INC_AHT30_H_

#include "main.h"

HAL_StatusTypeDef AHT30_Read(float *temperature, float *humidity);
HAL_StatusTypeDef AHT30_Init(void);
#endif /* INC_AHT30_H_ */

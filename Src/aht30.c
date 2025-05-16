#include "aht30.h"

#include "main.h"
//extern I2C_HandleTypeDef hi2c1;

#define AHT30_ADDR (0x38 << 1)

HAL_StatusTypeDef AHT30_Init(void) {
    uint8_t cmd = 0xBA; // Soft reset command
    HAL_StatusTypeDef ret;

    // Send soft reset
    ret = HAL_I2C_Master_Transmit(&hi2c1, AHT30_ADDR, &cmd, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK) return ret;

    // Wait for reset to complete (20ms as per datasheet)
    HAL_Delay(20);

    return HAL_OK;
}

HAL_StatusTypeDef AHT30_Read(float *temperature, float *humidity) {
    uint8_t cmd[3] = {0xAC, 0x33, 0x00};
    uint8_t data[7]; // Increase to 7 bytes to include checksum

    // Send measurement command
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(&hi2c1, AHT30_ADDR, cmd, 3, HAL_MAX_DELAY);
    if (ret != HAL_OK) return ret;

    // Wait for measurement completion (80ms as per datasheet)
    HAL_Delay(80);

    // Read data including status, measurement data and checksum
    ret = HAL_I2C_Master_Receive(&hi2c1, AHT30_ADDR, data, 7, HAL_MAX_DELAY);
    if (ret != HAL_OK) return ret;

    // Check status byte - Bit[7] should be 0 indicating measurement complete
    if (data[0] & 0x80) {
        return HAL_ERROR; // Busy state, measurement not complete
    }

    // Check if calibration was successful - Bit[3] should be 1
    if (!(data[0] & 0x08)) {
        // Consider initializing calibration if needed
        return HAL_ERROR;
    }

    // Extract raw humidity (20 bits)
    uint32_t hum_raw = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);

    // Extract raw temperature (20 bits)
    uint32_t temp_raw = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];

    // Standard conversion formulas for AHT30
    *humidity = (float)hum_raw * 100.0f / 1048576.0f;
    *temperature = (float)temp_raw * 200.0f / 1048576.0f - 50.0f;

    return HAL_OK;
}

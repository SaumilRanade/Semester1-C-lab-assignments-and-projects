#ifndef BME280_H
#define BME280_H

#include "main.h"

typedef struct
{
    I2C_HandleTypeDef *hi2c;
    uint16_t addr;          /* 7-bit address already shifted left by 1 (HAL style) */
    uint8_t  chip_id;       /* 0x60 = BME280, 0x58 = BMP280 (no humidity) */

    /* Calibration coefficients, read once from the sensor's NVM */
    uint16_t dig_T1;
    int16_t  dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    uint8_t  dig_H1, dig_H3;
    int16_t  dig_H2, dig_H4, dig_H5;
    int8_t   dig_H6;

    int32_t  t_fine;        /* shared between temperature, pressure, humidity maths */
} BME280_t;

typedef struct
{
    int32_t  temp_c100;     /* temperature in 0.01 degC: 2534 = 25.34 C */
    uint32_t press_pa;      /* pressure in Pa: 101325 = 1013.25 hPa */
    uint32_t hum_q10;       /* humidity in Q22.10: divide by 1024 for %RH */
} BME280_Data_t;

/* addr7 is the plain 7-bit address: 0x76 (SDO to GND) or 0x77 (SDO to 3V3) */
HAL_StatusTypeDef BME280_Init(BME280_t *dev, I2C_HandleTypeDef *hi2c, uint8_t addr7);
HAL_StatusTypeDef BME280_Read(BME280_t *dev, BME280_Data_t *out);

#endif

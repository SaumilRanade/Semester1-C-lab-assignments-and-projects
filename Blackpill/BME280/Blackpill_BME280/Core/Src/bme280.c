#include "bme280.h"

/* Register map (from the BME280 datasheet) */
#define REG_ID          0xD0
#define REG_RESET       0xE0
#define REG_CTRL_HUM    0xF2
#define REG_CTRL_MEAS   0xF4
#define REG_CONFIG      0xF5
#define REG_DATA        0xF7   /* press[3], temp[3], hum[2] */

#define BME280_CHIP_ID  0x60
#define I2C_TIMEOUT_MS  100

/* ---------- Low-level helpers: thin wrappers around HAL ---------- */

static HAL_StatusTypeDef rd(BME280_t *d, uint8_t reg, uint8_t *buf, uint16_t n)
{
    return HAL_I2C_Mem_Read(d->hi2c, d->addr, reg, I2C_MEMADD_SIZE_8BIT,
                            buf, n, I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef wr(BME280_t *d, uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(d->hi2c, d->addr, reg, I2C_MEMADD_SIZE_8BIT,
                             &val, 1, I2C_TIMEOUT_MS);
}

/* ---------- Calibration ---------- */

static HAL_StatusTypeDef read_calibration(BME280_t *d)
{
    uint8_t b[24], e[7];
    HAL_StatusTypeDef st;

    /* 0x88..0x9F: temperature and pressure coefficients, little-endian */
    st = rd(d, 0x88, b, 24);
    if (st != HAL_OK) return st;

    d->dig_T1 = (uint16_t)((b[1]  << 8) | b[0]);
    d->dig_T2 = (int16_t) ((b[3]  << 8) | b[2]);
    d->dig_T3 = (int16_t) ((b[5]  << 8) | b[4]);
    d->dig_P1 = (uint16_t)((b[7]  << 8) | b[6]);
    d->dig_P2 = (int16_t) ((b[9]  << 8) | b[8]);
    d->dig_P3 = (int16_t) ((b[11] << 8) | b[10]);
    d->dig_P4 = (int16_t) ((b[13] << 8) | b[12]);
    d->dig_P5 = (int16_t) ((b[15] << 8) | b[14]);
    d->dig_P6 = (int16_t) ((b[17] << 8) | b[16]);
    d->dig_P7 = (int16_t) ((b[19] << 8) | b[18]);
    d->dig_P8 = (int16_t) ((b[21] << 8) | b[20]);
    d->dig_P9 = (int16_t) ((b[23] << 8) | b[22]);

    /* Humidity coefficients are scattered: 0xA1, then 0xE1..0xE7 */
    st = rd(d, 0xA1, &d->dig_H1, 1);
    if (st != HAL_OK) return st;

    st = rd(d, 0xE1, e, 7);
    if (st != HAL_OK) return st;

    d->dig_H2 = (int16_t)((e[1] << 8) | e[0]);
    d->dig_H3 = e[2];
    /* H4 and H5 are 12-bit values packed across a shared byte (0xE5) */
    d->dig_H4 = (int16_t)(((int8_t)e[3]) * 16 + (e[4] & 0x0F));
    d->dig_H5 = (int16_t)(((int8_t)e[5]) * 16 + (e[4] >> 4));
    d->dig_H6 = (int8_t)e[6];

    return HAL_OK;
}

/* ---------- Compensation formulas (Bosch datasheet, 32-bit integer versions) ---------- */

/* Returns temperature in 0.01 degC and sets t_fine, which the other two need. */
static int32_t comp_temp(BME280_t *d, int32_t adc_T)
{
    int32_t var1, var2;

    var1 = ((((adc_T >> 3) - ((int32_t)d->dig_T1 << 1))) * ((int32_t)d->dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)d->dig_T1)) *
              ((adc_T >> 4) - ((int32_t)d->dig_T1))) >> 12) *
            ((int32_t)d->dig_T3)) >> 14;

    d->t_fine = var1 + var2;
    return (d->t_fine * 5 + 128) >> 8;
}

/* Returns pressure in Pa */
static uint32_t comp_press(BME280_t *d, int32_t adc_P)
{
    int32_t var1, var2;
    uint32_t p;

    var1 = (((int32_t)d->t_fine) >> 1) - (int32_t)64000;
    var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((int32_t)d->dig_P6);
    var2 = var2 + ((var1 * ((int32_t)d->dig_P5)) << 1);
    var2 = (var2 >> 2) + (((int32_t)d->dig_P4) << 16);
    var1 = (((d->dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) +
            ((((int32_t)d->dig_P2) * var1) >> 1)) >> 18;
    var1 = ((((32768 + var1)) * ((int32_t)d->dig_P1)) >> 15);

    if (var1 == 0) return 0;   /* avoid division by zero */

    p = (((uint32_t)(((int32_t)1048576) - adc_P) - (var2 >> 12))) * 3125;
    if (p < 0x80000000)
        p = (p << 1) / ((uint32_t)var1);
    else
        p = (p / (uint32_t)var1) * 2;

    var1 = (((int32_t)d->dig_P9) * ((int32_t)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
    var2 = (((int32_t)(p >> 2)) * ((int32_t)d->dig_P8)) >> 13;
    p = (uint32_t)((int32_t)p + ((var1 + var2 + d->dig_P7) >> 4));

    return p;
}

/* Returns humidity in Q22.10 format (1024 = 1 %RH) */
static uint32_t comp_hum(BME280_t *d, int32_t adc_H)
{
    int32_t v;

    v = (d->t_fine - ((int32_t)76800));
    v = (((((adc_H << 14) - (((int32_t)d->dig_H4) << 20) -
            (((int32_t)d->dig_H5) * v)) + ((int32_t)16384)) >> 15) *
         (((((((v * ((int32_t)d->dig_H6)) >> 10) *
              (((v * ((int32_t)d->dig_H3)) >> 11) + ((int32_t)32768))) >> 10) +
            ((int32_t)2097152)) * ((int32_t)d->dig_H2) + 8192) >> 14));
    v = (v - (((((v >> 15) * (v >> 15)) >> 7) * ((int32_t)d->dig_H1)) >> 4));
    v = (v < 0 ? 0 : v);
    v = (v > 419430400 ? 419430400 : v);

    return (uint32_t)(v >> 12);
}

/* ---------- Public API ---------- */

HAL_StatusTypeDef BME280_Init(BME280_t *dev, I2C_HandleTypeDef *hi2c, uint8_t addr7)
{
    HAL_StatusTypeDef st;

    dev->hi2c = hi2c;
    dev->addr = (uint16_t)(addr7 << 1);   /* HAL wants the address shifted */

    st = rd(dev, REG_ID, &dev->chip_id, 1);
    if (st != HAL_OK) return st;                       /* no ACK: wiring or address */
    if (dev->chip_id != BME280_CHIP_ID) return HAL_ERROR;  /* 0x58 means BMP280 */

    st = wr(dev, REG_RESET, 0xB6);                     /* soft reset */
    if (st != HAL_OK) return st;
    HAL_Delay(10);                                     /* wait for NVM copy */

    st = read_calibration(dev);
    if (st != HAL_OK) return st;

    /* Humidity oversampling x1. Takes effect only after CTRL_MEAS is written. */
    st = wr(dev, REG_CTRL_HUM, 0x01);
    if (st != HAL_OK) return st;

    /* Standby 1000 ms, filter off */
    st = wr(dev, REG_CONFIG, 0xA0);
    if (st != HAL_OK) return st;

    /* Temp x1 (001), press x1 (001), normal mode (11) -> 0b001_001_11 = 0x27 */
    st = wr(dev, REG_CTRL_MEAS, 0x27);
    if (st != HAL_OK) return st;

    HAL_Delay(20);   /* let the first conversion finish */
    return HAL_OK;
}

HAL_StatusTypeDef BME280_Read(BME280_t *dev, BME280_Data_t *out)
{
    uint8_t r[8];
    HAL_StatusTypeDef st = rd(dev, REG_DATA, r, 8);   /* one burst read, 0xF7..0xFE */
    if (st != HAL_OK) return st;

    /* Pressure and temperature are 20-bit, humidity is 16-bit */
    int32_t adc_P = ((int32_t)r[0] << 12) | ((int32_t)r[1] << 4) | (r[2] >> 4);
    int32_t adc_T = ((int32_t)r[3] << 12) | ((int32_t)r[4] << 4) | (r[5] >> 4);
    int32_t adc_H = ((int32_t)r[6] << 8)  |  r[7];

    /* Temperature first: it computes t_fine, which the others depend on */
    out->temp_c100 = comp_temp(dev, adc_T);
    out->press_pa  = comp_press(dev, adc_P);
    out->hum_q10   = comp_hum(dev, adc_H);

    return HAL_OK;
}

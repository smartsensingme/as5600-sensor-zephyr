/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include "as5600.h"
#include "kalman.h"
#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

/* Update the specialized 2D Kalman Filter for rotary/angular sensor (with
 * wrap-around) */
static __attribute__((unused)) void
engine_angle_kalman_2d_update(struct kalman_2d *k, float measured_angle,
                              float dt) {
  /* --- 1. PREDICT STEP --- */
  /* x_pred = F * x */
  float x_pred_theta = k->x[0] + k->x[1] * dt;
  float x_pred_omega = k->x[1];

  /* P_pred = F * P * F^T + Q */
  float P_pred_00 = k->P[0][0] +
                    dt * (k->P[1][0] + k->P[0][1] + dt * k->P[1][1]) +
                    k->Q_theta;
  float P_pred_01 = k->P[0][1] + dt * k->P[1][1];
  float P_pred_10 = k->P[1][0] + dt * k->P[1][1];
  float P_pred_11 = k->P[1][1] + k->Q_omega;

  /* --- 2. UPDATE / CORRECT STEP --- */
  /* Innovation (measurement error) */
  float y = measured_angle - x_pred_theta;

  /* Normalize y to [-180, 180] degrees to handle wrap-around from 360 to 0 */
  while (y > 180.0f) {
    y -= 360.0f;
  }
  while (y < -180.0f) {
    y += 360.0f;
  }

  /* Innovation Covariance: S = H * P_pred * H^T + R */
  float S = P_pred_00 + k->R;

  /* Kalman Gain: K = P_pred * H^T * S^-1 */
  float K_0 = P_pred_00 / S;
  float K_1 = P_pred_10 / S;

  /* Correct State: x = x_pred + K * y */
  k->x[0] = x_pred_theta + K_0 * y;
  k->x[1] = x_pred_omega + K_1 * y;

  /* Keep estimated angle within [0, 360) range */
  while (k->x[0] >= 360.0f) {
    k->x[0] -= 360.0f;
  }
  while (k->x[0] < 0.0f) {
    k->x[0] += 360.0f;
  }

  /* Correct Covariance: P = (I - K * H) * P_pred */
  k->P[0][0] = (1.0f - K_0) * P_pred_00;
  k->P[0][1] = (1.0f - K_0) * P_pred_01;
  k->P[1][0] = P_pred_10 - K_1 * P_pred_00;
  k->P[1][1] = P_pred_11 - K_1 * P_pred_01;
}

static void engine_angle_kalman_3d_update(struct kalman_3d *k,
                                          float measured_angle, float dt) {
  /* --- 1. PREDICT STEP --- */
  float h = 0.5f * dt * dt;

  /* x_pred = F * x */
  float x_pred_theta = k->x[0] + k->x[1] * dt + k->x[2] * h;
  float x_pred_omega = k->x[1] + k->x[2] * dt;
  float x_pred_alpha = k->x[2];

  /* P_pred = F * P * F^T + Q */
  /* Row lines of F * P */
  float a0 = k->P[0][0] + dt * k->P[1][0] + h * k->P[2][0];
  float a1 = k->P[0][1] + dt * k->P[1][1] + h * k->P[2][1];
  float a2 = k->P[0][2] + dt * k->P[1][2] + h * k->P[2][2];

  float a4 = k->P[1][1] + dt * k->P[2][1];
  float a5 = k->P[1][2] + dt * k->P[2][2];

  /* (F * P) * F^T + Q components */
  float P_pred_00 = a0 + dt * a1 + h * a2 + k->Q_theta;
  float P_pred_01 = a1 + dt * a2;
  float P_pred_02 = a2;

  float P_pred_10 = P_pred_01;
  float P_pred_11 = a4 + dt * a5 + k->Q_omega;
  float P_pred_12 = a5;

  float P_pred_20 = P_pred_02;
  float P_pred_22 = k->P[2][2] + k->Q_alpha;

  /* --- 2. UPDATE / CORRECT STEP --- */
  /* Innovation (measurement error) */
  float y = measured_angle - x_pred_theta;

  /* Normalize y to [-180, 180] degrees to handle wrap-around from 360 to 0 */
  while (y > 180.0f) {
    y -= 360.0f;
  }
  while (y < -180.0f) {
    y += 360.0f;
  }

  /* Innovation Covariance: S = H * P_pred * H^T + R */
  float S = P_pred_00 + k->R;

  /* Kalman Gain: K = P_pred * H^T * S^-1 */
  float K_0 = P_pred_00 / S;
  float K_1 = P_pred_10 / S;
  float K_2 = P_pred_20 / S;

  /* Correct State: x = x_pred + K * y */
  k->x[0] = x_pred_theta + K_0 * y;
  k->x[1] = x_pred_omega + K_1 * y;
  k->x[2] = x_pred_alpha + K_2 * y;

  /* Keep estimated angle within [0, 360) range */
  while (k->x[0] >= 360.0f) {
    k->x[0] -= 360.0f;
  }
  while (k->x[0] < 0.0f) {
    k->x[0] += 360.0f;
  }

  /* Correct Covariance: P = (I - K * H) * caP_pred */
  k->P[0][0] = (1.0f - K_0) * P_pred_00;
  k->P[0][1] = (1.0f - K_0) * P_pred_01;
  k->P[0][2] = (1.0f - K_0) * P_pred_02;

  k->P[1][0] = k->P[0][1];
  k->P[1][1] = P_pred_11 - K_1 * P_pred_01;
  k->P[1][2] = P_pred_12 - K_1 * P_pred_02;

  k->P[2][0] = k->P[0][2];
  k->P[2][1] = k->P[1][2];
  k->P[2][2] = P_pred_22 - K_2 * P_pred_02;
}

/* Helper function to print a float by splitting it into integer and fraction (3
 * decimal places). It receives a format string which must accept two integer
 * arguments (e.g. "%d.%03d").
 */
static void printf_f(const char *format, float value) {
  /* Handle values between -1.0 and 0.0 to avoid losing the negative sign */
  if (value < 0.0f && value > -1.0f) {
    printf("-");
  }
  int32_t integer = (int32_t)value;
  int32_t fraction = (int32_t)((value - integer) * 1000.0f);
  if (fraction < 0) {
    fraction = -fraction;
  }
  printf(format, integer, fraction);
}

/**
 * @brief Obtém o ângulo bruto do sensor AS5600 em graus. Fmax = 6,06 kHz.$
 *
 * Realiza a leitura física (fetch) do registrador RAW ANGLE do codificador via
 * I2C e converte o resultado de estrutura "sensor_value" (val1 + val2/1e6) para
 * float.
 *
 * @param[in]  dev     Ponteiro para a estrutura de dispositivo (device) do
 * sensor AS5600.
 * @param[out] raw_deg Ponteiro para a variável float onde o ângulo lido (0.0 a
 * 359.999 graus) será gravado.
 *
 * @return 0 em caso de sucesso.
 * @return Código de erro negativo (ex: -EIO se falhar a comunicação I2C) em
 * caso de erro.
 */
static int get_engine_angle(const struct device *dev, float *raw_deg) {
  struct sensor_value angle_raw;

  /* Fetch only the raw rotation channel at high speed */
  int ret = sensor_sample_fetch_chan(
      dev, (enum sensor_channel)SENSOR_CHAN_RAW_ROTATION);
  if (ret >= 0) {
    sensor_channel_get(dev, (enum sensor_channel)SENSOR_CHAN_RAW_ROTATION,
                       &angle_raw);

    /* Convert degrees and micro-degrees to degrees (e.g. 180.500 deg) */
    *raw_deg = (float)angle_raw.val1 + ((float)angle_raw.val2 / 1e6f);
  }

  return ret;
}

int main(void) {
  const struct device *const dev = DEVICE_DT_GET_ANY(custom_as5600);

  printf("AS5600 Magnetic Encoder Demonstration - High Speed Raw Sampling & 3D "
         "Kalman Filter\n");

  if (dev == NULL) {
    printf("Error: No AS5600 device found in devicetree\n");
    return 0;
  }

  if (!device_is_ready(dev)) {
    printf("Error: Device %s is not ready\n", dev->name);
    return 0;
  }

  printf("AS5600 device %s is ready\n", dev->name);

  float raw_deg = 0.0f;
  struct kalman_3d filter;

  /*
   * Configuração customizada para o Filtro de Kalman 3D:
   *
   * - q_theta (Covariância do ruído do processo para posição/ângulo):
   *   Valores menores fazem o filtro confiar mais no modelo físico (mais suave,
   * mas com maior atraso/lag). Valores maiores fazem o filtro confiar mais nas
   * medições (mais responsivo, mas com mais ruído/jitter). Valor padrão:
   * 0.001f.
   *
   * - q_omega (Covariância do ruído do processo para velocidade angular):
   *   Determina quão rápido o filtro reage a mudanças na velocidade angular.
   *   Valores maiores permitem adaptação mais rápida a
   * acelerações/desacelerações repentinas. Valor padrão: 10.0f.
   *
   * - q_alpha (Covariância do ruído do processo para aceleração angular):
   *   Determina a velocidade com que o filtro rastreia mudanças na aceleração.
   *   Valor padrão: 100.0f.
   *
   * - r (Covariância do ruído de medição):
   *   Representa o nível de ruído/variância nas leituras brutas do sensor
   * AS5600. Um valor maior indica que o sensor é mais ruidoso, fazendo o filtro
   * suavizar mais os dados. Valor padrão: 0.018f.
   *
   * Guia de Ajuste (Tuning):
   * 1. Se o ângulo filtrado apresentar atraso (lag) durante a rotação física,
   * aumente q_theta, q_omega e q_alpha.
   * 2. Se o ângulo filtrado oscilar muito (ruído/jitter) quando o sensor
   * estiver parado ou em movimento lento, diminua q_theta/q_omega ou aumente r.
   */
  kalman_3d_config_t filter_cfg = {
      .q_theta = 0.001f,
      .q_omega = 10.0f,
      .q_alpha = 100.0f,
      .r = 0.018f,
  };

  /* Perform a first read to initialize the Kalman Filter with the correct
   * initial angle */
  if (get_engine_angle(dev, &raw_deg) >= 0) {
    kalman_3d_init(&filter, raw_deg, &filter_cfg);
  } else {
    kalman_3d_init(&filter, 0.0f, &filter_cfg);
  }

  uint32_t loop_count = 0;

  while (1) {
    /* Fetch and update at 1 kHz */
    if (get_engine_angle(dev, &raw_deg) >= 0) {

      /* Update filter with dt = 0.001s (1 ms loop) */
      engine_angle_kalman_3d_update(&filter, raw_deg, 0.001f);
    }

    /* Read and print status every 2 seconds (2000 loops at 1 ms delay) */
    if (loop_count % 2000 == 0) {
      struct sensor_value status_val;
      int ret = sensor_sample_fetch_chan(
          dev, (enum sensor_channel)SENSOR_CHAN_STATUS);
      if (ret >= 0) {
        sensor_channel_get(dev, (enum sensor_channel)SENSOR_CHAN_STATUS,
                           &status_val);
        uint8_t status = (uint8_t)status_val.val1;

        bool md = (status & AS5600_STATUS_MD) != 0;
        bool ml = (status & AS5600_STATUS_ML) != 0;
        bool mh = (status & AS5600_STATUS_MH) != 0;

        printf("[AS5600 Status] Byte: 0x%02X | Magnet: %s | Strength: %s\n",
               status, md ? "DETECTED" : "NOT DETECTED",
               ml ? "TOO WEAK" : (mh ? "TOO STRONG" : "OK"));
      } else {
        printf("[AS5600 Status] Error reading status: %d\n", ret);
      }
    }

    /*
     * Print values every 200 ms (200 loops at 1 ms delay)
     * to prevent flooding the serial port console.
     */
    if (loop_count % 200 == 0) {
      float est_angle = filter.x[0];
      float est_speed_dps = filter.x[1];
      float est_speed_rpm = est_speed_dps / 6.0f; /* dps to RPM */
      float est_accel_dps2 = filter.x[2];
      float est_accel_rpm_s = est_accel_dps2 / 6.0f; /* dps/s^2 to RPM/s */

      printf("Raw Angle: ");
      printf_f("%d.%03d", raw_deg);
      printf(" deg | Kalman Angle: ");
      printf_f("%d.%03d", est_angle);
      printf(" deg | Speed: ");
      printf_f("%d.%03d", est_speed_rpm);
      printf(" RPM | Accel: ");
      printf_f("%d.%03d", est_accel_rpm_s);
      printf(" RPM/s\n");
    }

    loop_count++;
    k_msleep(1); /* 1 ms delay -> 1 kHz sampling rate */
  }

  return 0;
}

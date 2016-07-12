#include "nrf_drv_pwm.h"

#define PWM_RED_PIN                     22
#define PWM_GREEN_PIN                   23
#define PWM_BLUE_PIN                    2

#define PWM_TOP 800

typedef struct{
  float pwm1;
  float pwm2;
  float pwm3;
  float pwm4;
}duty_values;

void pwm_event_handler (nrf_drv_pwm_evt_type_t event_type);
void pwm_values_update(duty_values values);
void pwm_start(void);
void pwm_init(void);
void pwm_update(void);
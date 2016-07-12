#include "nrf_drv_pwm.h"
#include "nrf_drv_clock.h"
#include "perip_pwm.h"

//int pwm_scaler = (UINT16_MAX / PWM_TOP );
float pwm_scaler = (65535.0/PWM_TOP);

static nrf_drv_pwm_t m_pwm0 = NRF_DRV_PWM_INSTANCE(0);
static duty_values current_values;

static nrf_pwm_values_common_t pwm_values[] =
{
    10,20,40,80
};
    
static nrf_pwm_sequence_t const pwm_seq =
{
    .values.p_common = pwm_values,
    .length          = NRF_PWM_VALUES_LENGTH(pwm_values),
    .repeats         = 0,
    .end_delay       = 0
};

void pwm_values_update(duty_values values){
    // save these values, and update them using the pwm event handler
    current_values = values;
    //scale to pwm size
    current_values.pwm1 = current_values.pwm1; // pwm_scaler;
    current_values.pwm2 = current_values.pwm2; // pwm_scaler;
    current_values.pwm3 = current_values.pwm3; // pwm_scaler;

    pwm_update();
}    

void pwm_init(void){
    uint32_t err_code;
    
    nrf_drv_pwm_config_t const config0 =
    {
        .output_pins =
        {
            PWM_RED_PIN,               // channel 0
            PWM_GREEN_PIN,            // channel 1
            PWM_BLUE_PIN             // channel 2
        },
        .base_clock = NRF_PWM_CLK_16MHz,
        .count_mode = NRF_PWM_MODE_UP,
        .top_value  = PWM_TOP, // reaching top with 40 khz 
        .load_mode  = NRF_PWM_LOAD_INDIVIDUAL,
        .step_mode  = NRF_PWM_STEP_AUTO 
    };
    err_code = nrf_drv_pwm_init(&m_pwm0, &config0, NULL);
    APP_ERROR_CHECK(err_code);
}

void pwm_start(void){
    nrf_drv_pwm_simple_playback(&m_pwm0, &pwm_seq, 1, 0 );
}

void pwm_update(void){
    
    // | with 0x8000 to invert pwm signal
    uint16_t temp_value = 0;
    temp_value= (uint16_t)current_values.pwm1;
    pwm_values[0] = temp_value | 0x8000;
    temp_value= (uint16_t)current_values.pwm2;
    pwm_values[1] = temp_value | 0x8000;
    temp_value= (uint16_t)current_values.pwm3;
    pwm_values[2] = temp_value | 0x8000;
            
    pwm_start();
}

// not used
void pwm_event_handler (nrf_drv_pwm_evt_type_t event_type){
    
    if (event_type == NRF_DRV_PWM_EVT_FINISHED){
        pwm_update();
    }
}
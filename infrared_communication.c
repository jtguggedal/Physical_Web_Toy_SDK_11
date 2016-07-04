//#include "app_pwm.h"
#include "app_timer.h"
#include "ble_lbs.h"
#include "infrared_communication.h"
#include "ir_lib.h"
#include "nrf_delay.h"
#include "nrf_drv_gpiote.h"
#include "nrf_drv_timer.h"
#include "nrf_gpio.h"
#include <stdint.h>
#include "read_set_bit.h"
#include "SEGGER_RTT.h"

//APP_PWM_INSTANCE(PWM1, 3);                                                          /* Defines the PWM instance used for the red and green LED-pins. */
//APP_PWM_INSTANCE(PWM2, 4);                                                          /* Defines the PWM instance used for the blue LED-pins. */

/**@brief Function for setting the color of the RGB-LEDS
*
*@details A decimal value of new_color_data uses predefined values to set the color.
*         Value 0 = Green color.
*         Value 1 = Red color. 
*         Value 2 = Blue color.
*         Value 100 = No color. (The LED is turned off).
*
* The function also has a way of remembering the previous state; green_state = true means you are vulnerable,
*  green_state = false means you are invulnerable after a recent hit.
*  An input value of 250 makes the LED either turn green or red, depending on the actual game state.
*/

uint8_t  red_value = 0, green_value = 0, blue_value = 0, color_data = 0;
bool green_state = false;

void set_rgb_color(uint8_t new_color_data){
  
  if (new_color_data == 250 && green_state == true)
      color_data = 0;
  else if (new_color_data == 250 && green_state == false)
      color_data = 1;
  else
      color_data = new_color_data;
  
  switch (color_data) {
    case 0:
          red_value = 0;
          green_value = 100;
          blue_value = 0;
          green_state = true;
          break;
    case 1:
          red_value = 100;
          green_value = 0;
          blue_value = 0;
          green_state = false;
          break;
    case 2:
          red_value = 100;
          green_value = 100;
          blue_value = 100;
          break;
    case 100:
          red_value = 100;
          green_value = 100;
          blue_value = 0;
          break;
     default:
          break;
    }
    
    //while (app_pwm_channel_duty_set(&PWM1, 1, red_value) == NRF_ERROR_BUSY);
    //while (app_pwm_channel_duty_set(&PWM1, 0, green_value) == NRF_ERROR_BUSY);
    //while (app_pwm_channel_duty_set(&PWM2, 0, blue_value) == NRF_ERROR_BUSY);
    
}

/**@brief Function for the PWM initialization.
 *
 * @details Initializes the PWM module.
 */

void pwm_init(void)
{
    #if 0
    ret_code_t err_code;
    
    /* 2-channel PWM, 200Hz, output on pins. */
  app_pwm_config_t pwm1_cfg = APP_PWM_DEFAULT_CONFIG_2CH(20000L, PWM_RED_PIN, PWM_GREEN_PIN);
    
   /* 1-channel PWM, 200Hz, output on pins. */
   app_pwm_config_t pwm2_cfg = APP_PWM_DEFAULT_CONFIG_2CH(26L, PWM_BLUE_PIN, PWM_IR_PIN);

   pwm2_cfg.pin_polarity[1] = APP_PWM_POLARITY_ACTIVE_HIGH;

    /* Initialize and enable PWM. */
    err_code = app_pwm_init(&PWM1, &pwm1_cfg, NULL);
    APP_ERROR_CHECK(err_code);
    
    err_code = app_pwm_init(&PWM2,&pwm2_cfg,NULL);
    APP_ERROR_CHECK(err_code);
    
    app_pwm_enable(&PWM1);
    app_pwm_enable(&PWM2);

    while (app_pwm_channel_duty_set(&PWM2, 1, 0) == NRF_ERROR_BUSY);


    set_rgb_color(100);
    #endif
}


void ir_shooting(uint8_t * ir_data)
{
    // Send infrared signal by implementing the NEC protocol
    // Carrier on time is always 560us, and the length of the break determines the bit value
    uint8_t p_data[] = {1, 2, 3, 4, 5};
    static uint16_t ir_message[16];    
    uint32_t ir_buffer_index = 0;
    for(int bit = 0; bit < 8; bit++)
    {
        if((p_data[0] >> bit) & 0x01)
        {
            // 560us on, 2.25ms time until next symbol
            ir_message[ir_buffer_index++] = 560;
            ir_message[ir_buffer_index++] = 2250 - 560;
        }
        else
        {
            // 560us on, 1.12ms time until next symbol
            ir_message[ir_buffer_index++] = 560;
            ir_message[ir_buffer_index++] = 1120 - 560;
        }
    }
		SEGGER_RTT_printf(0, "\n\nIR-signal in bits, where an OFF-signal of 1690 is logical 1 and 560 is logical 0\nValue to be sent as ir signal: %d\nON\tOFF (in microseconds)\n", p_data[0]);
                
		for (uint8_t i=0; i<(sizeof(ir_message)/2); i++)
                {
                        SEGGER_RTT_printf(0, "%d", ir_message[i]);
                        ir_message[i] = ir_message[i++];
			SEGGER_RTT_printf(0, "\t%d\n", ir_message[i]);
                }
		
    // Send the data in the ir_message buffer
    // NOTE! The ir_message buffer must be static or global! The IR lib depends on the data in this buffer until the IR operation is complete.  
    ir_lib_send(ir_message, ir_buffer_index);

     // Sets RGB blue and back to green or red
          nrf_delay_ms(10);
          set_rgb_color(2);
          nrf_delay_ms(50);
          nrf_delay_ms(50);
          set_rgb_color(250);
    
}


 
   
     

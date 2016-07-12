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
#include "nrf_ppi.h"
#include "nrf_soc.h"
#include "nrf_nvic.h"
#include "nrf_drv_ppi.h"


void ir_shooting(uint8_t * ir_data)
{
    // Send infrared signal by implementing the NEC protocol
    // Carrier on time is always 560us, and the length of the break determines the bit value
    uint8_t p_data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    static uint16_t ir_message[16];    
    uint32_t ir_buffer_index = 0;
    for(int bit = 0; bit < 8; bit++)
    {
        if((p_data[CAR_ID] >> bit) & 0x01)
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
		SEGGER_RTT_printf(0, "\n\nIR-signal in bits, where an OFF-signal of 1690 is logical 1 and 560 is logical 0\nValue to be sent as ir signal: %d\nON\tOFF (in microseconds)\n", p_data[4]);
                
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
    // TODO: Avoid delays?
    nrf_delay_ms(10);
    set_rgb_color(2);
    nrf_delay_ms(50);
    nrf_delay_ms(50);
    set_rgb_color(250);
    
}


#if 0

/** @brief Function for initializing PPI used in infrared signal decoding
 *   The PPI is needed to convert the timer event into a task.
 */
uint32_t ir_ppi_init(void)
{

    uint32_t gpiote_event_addr;
    uint32_t timer_task_addr;
    nrf_ppi_channel_t ppi_channel;
    ret_code_t err_code;
    nrf_drv_gpiote_in_config_t config; 

    config.sense = NRF_GPIOTE_POLARITY_HITOLO;
    config.pull = NRF_GPIO_PIN_PULLUP;
    config.hi_accuracy = false;
    config.is_watcher = false;

    nrf_drv_timer_config_t timer_config;

    timer_config.frequency            = NRF_TIMER_FREQ_1MHz;
    timer_config.mode                 = NRF_TIMER_MODE_TIMER;
    timer_config.bit_width            = NRF_TIMER_BIT_WIDTH_32;
    timer_config.interrupt_priority   = 3;

    err_code = nrf_drv_timer_init(&ir_timer, &timer_config, timer_dummy_handler);
    APP_ERROR_CHECK(err_code);


    // Set up GPIOTE
    err_code = nrf_drv_gpiote_in_init(IR_RECEIVER_PIN_1, &config, ir_in_pin_handler);
    APP_ERROR_CHECK(err_code);
    err_code = nrf_drv_gpiote_in_init(IR_RECEIVER_PIN_2, &config, ir_in_pin_handler);
    APP_ERROR_CHECK(err_code);
    err_code = nrf_drv_gpiote_in_init(IR_RECEIVER_PIN_3, &config, ir_in_pin_handler);
    APP_ERROR_CHECK(err_code);



    // Set up timer for capturing
    nrf_drv_timer_capture_get(&ir_timer, NRF_TIMER_CC_CHANNEL0);

    // Set up PPI channel
    err_code = nrf_drv_ppi_channel_alloc(&ppi_channel);
    APP_ERROR_CHECK(err_code);

    timer_task_addr = nrf_drv_timer_capture_task_address_get(&ir_timer, NRF_TIMER_CC_CHANNEL0);
    gpiote_event_addr = nrf_drv_gpiote_in_event_addr_get(IR_RECEIVER_PIN_1);

    //err_code = nrf_drv_ppi_channel_assign(ppi_channel, gpiote_event_addr, timer_task_addr);
    //APP_ERROR_CHECK(err_code);

    //err_code = nrf_drv_ppi_channel_enable(ppi_channel);
    //APP_ERROR_CHECK(err_code);

    nrf_drv_gpiote_in_event_enable(IR_RECEIVER_PIN_1, true);
    nrf_drv_gpiote_in_event_enable(IR_RECEIVER_PIN_2, true);
    nrf_drv_gpiote_in_event_enable(IR_RECEIVER_PIN_3, true);


    
    // Enable timer
    nrf_drv_timer_enable(&ir_timer);

    return 0;

}
#endif

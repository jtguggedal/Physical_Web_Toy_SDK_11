#include <stdint.h>
#include "twi_rfid_driver.h"
#include "nrf_drv_timer.h"
#include "nrf_gpio.h"
#include "app_timer.h"

uint8_t rfid_value = 0x00;

// Define the TWI-channel instance.

static const nrf_drv_twi_t twi_rfid = NRF_DRV_TWI_INSTANCE(1);

//Define the TWI-channel read timer.

APP_TIMER_DEF(read_timer);

/** @brief  Function which handles the timer events.
 *
 */

void timer_read_event_handler(void* p_context)
{
    nrf_gpio_pin_clear(READ_LED);   
    // Reads from the TWI-channel.
    nrf_drv_twi_rx(&twi_rfid, ADR_RFID_SLAVE, &rfid_value, 1); 
    nrf_gpio_pin_set(READ_LED);
    radio_notification_evt_handler();
};

/** @brief  Function which initializes the timer.
**/

void twi_rfid_timer_init(void)
{
  APP_TIMER_INIT(RFID_APP_TIMER_PRESCALER, RFID_APP_TIMER_OP_QUEUE_SIZE, NULL);
  app_timer_create(&read_timer, APP_TIMER_MODE_REPEATED, &timer_read_event_handler);
  // Starts the timer, sets it up for repeated start.
  app_timer_start(read_timer, APP_TIMER_TICKS(RFID_APP_TIME_VALUE, RFID_APP_TIMER_PRESCALER), NULL);
};

/** @brief  Function to set up the TWI channel for communication
 *
 */

void twi_rfid_init(void)
{
   const nrf_drv_twi_config_t twi_rfid_config = {
      .scl                = SCL_RFID_PIN,
      .sda                = SDA_RFID_PIN,
      .frequency          = NRF_TWI_FREQ_100K,
      .interrupt_priority = TWI0_CONFIG_IRQ_PRIORITY
   };

   // Initialize the TWI channel
   nrf_drv_twi_init(&twi_rfid, &twi_rfid_config, NULL, NULL);
   
   // Enable the TWI channel
   nrf_drv_twi_enable(&twi_rfid);

   twi_rfid_timer_init();
};
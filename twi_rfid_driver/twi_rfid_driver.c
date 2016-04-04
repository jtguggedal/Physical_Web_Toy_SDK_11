#include <stdint.h>
#include "twi_rfid_driver.h"
#include "nrf_drv_timer.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "app_timer.h"
#include "ble_lbs.h"

uint8_t rfid_value = 0xFF;
uint8_t rfid_counter = 1;

//Initialization-array

uint8_t rfid_init_array_1[] = { 0x00, 0x00, 0xFF, 0x02, 0xFE, 0xD4, 0x02, 0x2A, 0x00 };
uint8_t rfid_init_array_2[] = { 0x00, 0x00, 0xFF, 0x05, 0xFB, 0xD4, 0x14, 0x01, 0x14, 0x01, 0x02, 0x00 };
uint8_t rfid_init_array_3[] = { 0x00, 0x00, 0xFF, 0x04, 0xFC, 0xD4, 0x4A, 0x01, 0x00, 0xE1, 0x00 };

uint8_t dummy_array = 0x00;

// Define the TWI-channel instance.

static const nrf_drv_twi_t twi_rfid = NRF_DRV_TWI_INSTANCE(1);

//Define the TWI-channel read timer.

APP_TIMER_DEF(read_timer);

void read_rfid_shield(void){

}

/** @brief  Function which handles the timer events.
 *
 */

uint8_t rfid_read_event_handler(void)
{
    read_rfid_shield();
     
      if (rfid_counter == 0)
          rfid_counter = 1;
      else
          rfid_counter++;
      
      return rfid_counter;
      
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

   //Zero byte 4
   ble_lbs_on_button_change(&m_lbs, 0, 4);

   //Initializes the RFID-shield
   nrf_drv_twi_tx(&twi_rfid, ADR_RFID_SLAVE, rfid_init_array_1, sizeof(rfid_init_array_1), false);
  
   nrf_delay_ms(3);
   nrf_drv_twi_rx(&twi_rfid, ADR_RFID_SLAVE, &dummy_array, 7);

   nrf_delay_ms(3);
   nrf_drv_twi_rx(&twi_rfid, ADR_RFID_SLAVE, &dummy_array, 14);
   
   nrf_delay_ms(3);
   nrf_drv_twi_tx(&twi_rfid, ADR_RFID_SLAVE, rfid_init_array_2, sizeof(rfid_init_array_2), false);

   nrf_delay_ms(3);
   nrf_drv_twi_rx(&twi_rfid, ADR_RFID_SLAVE, &dummy_array, 7);
    
   nrf_delay_ms(3);
   nrf_drv_twi_rx(&twi_rfid, ADR_RFID_SLAVE, &dummy_array, 9);
   
   nrf_delay_ms(3);
   nrf_drv_twi_tx(&twi_rfid, ADR_RFID_SLAVE, rfid_init_array_3, sizeof(rfid_init_array_3), false);

   nrf_delay_ms(3);
   nrf_drv_twi_rx(&twi_rfid, ADR_RFID_SLAVE, &dummy_array, 7);
};
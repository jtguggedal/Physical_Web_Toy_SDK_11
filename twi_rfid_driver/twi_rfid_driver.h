#include <stdint.h>
#include "nrf_drv_twi.h"
#include "nrf_delay.h"
#include "nrf_drv_timer.h"
#include "ble_lbs.h"

static ble_lbs_t                        m_lbs;                                      /**< LED Button Service instance. */

// Define pins for I2C-communication with the RFID-module
#define SDA_RFID_PIN         24
#define SCL_RFID_PIN         25

// Define the slave address
#define ADR_RFID_SLAVE       0x24

//Defines the LED used for RFID-reading
#define READ_LED            20

// Prototypes for functions used in twi_rfid_driver.c

void timer_read_event_handler(void);
void twi_rfid_init(void);
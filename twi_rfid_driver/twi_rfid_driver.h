#include <stdint.h>
#include "nrf_drv_twi.h"
#include "nrf_delay.h"
#include "nrf_drv_timer.h"


// Define pins for I2C-communication with the RFID-module
#define SDA_RFID_PIN         3
#define SCL_RFID_PIN         4

// Define the slave address
#define ADR_RFID_SLAVE       0x61

// Defines timer values
#define RFID_APP_TIMER_PRESCALER       15
#define RFID_APP_TIMER_OP_QUEUE_SIZE   1
#define RFID_APP_TIME_VALUE            500

//Defines the LED used for RFID-reading
#define READ_LED            20

// Prototypes for functions used in twi_rfid_driver.c

void timer_read_event_handler(void* p_context);
void twi_rfid_timer_init(void);
void twi_rfid_init(void);
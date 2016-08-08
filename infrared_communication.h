#include "nrf_drv_gpiote.h"

#define PWM_PERIOD                      5000L
#define PWM_IR_PIN                      11
#define IR_OUTPUT_PIN                   11
#define IR_RECEIVER_PIN_1               13                                          /**< Button that will trigger the notification event with the LED Button Service */
#define LASER_TRANSISTOR                12
#define IR_RECEIVER_PIN_2               14
#define IR_RECEIVER_PIN_3               15

//Prototypes for functions used in infrared_communication.c
void pwm_init(void);
void set_rgb_color(uint8_t new_color_data);
void ir_shooting(uint8_t * ir_data);
void ir_event_handler(void);
void printpulses(void);

void ir_in_pin_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action);
uint32_t ir_ppi_init(void);


/**@brief Function for updating the hit value when a hit is registered.
*
* @details The value will cycle between the values from 1 between 255.
*/

uint8_t new_hit_value(void);
void write_car_id(uint8_t car_id);




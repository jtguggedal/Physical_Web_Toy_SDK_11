#ifndef __IR_LIB_H
#define __IR_LIB_H

#include <stdint.h>
#include <stdbool.h>

#define IR_TIMER_CARRIER                NRF_TIMER1

#define IR_CARRIER_COUNTER              NRF_TIMER2
#define IR_CARRIER_COUNTER_IRQn         TIMER2_IRQn
#define IR_CARRIER_COUNTER_IRQHandler   TIMER2_IRQHandler
#define IR_CARRIER_COUNTER_IRQ_Priority 3                // APP_IRQ_PRIORITY_HIGH

#define IR_PPI_CH_A         0
#define IR_PPI_CH_B         1
#define IR_PPI_CH_C         2
#define IR_PPI_CH_D         3
#define IR_PPI_CH_E         4

#define IR_PPI_GROUP        0

#define IR_CARRIER_LOW_US   26
#define IR_CARRIER_HIGH_US  9

uint32_t ir_lib_init(uint32_t ir_pin);

void     ir_lib_send(uint16_t *time_us, uint32_t length);
    
#endif

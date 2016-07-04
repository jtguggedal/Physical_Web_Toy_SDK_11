#include "ir_lib.h"
#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"
#include "nrf_soc.h"
#include "nrf_nvic.h"

static uint16_t *m_current_data_ptr;
static uint32_t m_bits_remaining;
static uint32_t m_integrated_time_us;
static uint32_t m_next_cc_for_update;
static bool     m_busy;

static uint32_t pulse_count_calculate(uint32_t time_us)
{
    return (time_us + (IR_CARRIER_LOW_US + IR_CARRIER_HIGH_US) / 2) / (IR_CARRIER_LOW_US + IR_CARRIER_HIGH_US);
}

uint32_t ir_lib_init(uint32_t ir_pin)
{
    uint32_t err_code = 0;

    NRF_GPIOTE->CONFIG[0] = GPIOTE_CONFIG_MODE_Task         << GPIOTE_CONFIG_MODE_Pos |
                            GPIOTE_CONFIG_OUTINIT_Low       << GPIOTE_CONFIG_OUTINIT_Pos |
                            GPIOTE_CONFIG_POLARITY_Toggle   << GPIOTE_CONFIG_POLARITY_Pos |
                            ir_pin                          << GPIOTE_CONFIG_PSEL_Pos;
    
    // Carrier timer init
    IR_TIMER_CARRIER->MODE          = TIMER_MODE_MODE_Timer;
    IR_TIMER_CARRIER->BITMODE       = TIMER_BITMODE_BITMODE_16Bit;
    IR_TIMER_CARRIER->PRESCALER     = 4;
    IR_TIMER_CARRIER->CC[0]         = IR_CARRIER_LOW_US;    
    IR_TIMER_CARRIER->CC[1]         = IR_CARRIER_LOW_US + IR_CARRIER_HIGH_US; 
    IR_TIMER_CARRIER->SHORTS        = TIMER_SHORTS_COMPARE1_CLEAR_Msk;

    // Modulation timer init
    IR_CARRIER_COUNTER->MODE        = TIMER_MODE_MODE_Counter;  
    IR_CARRIER_COUNTER->BITMODE     = TIMER_BITMODE_BITMODE_16Bit;
    IR_CARRIER_COUNTER->INTENSET    = TIMER_INTENSET_COMPARE0_Msk | TIMER_INTENSET_COMPARE1_Msk;
    IR_CARRIER_COUNTER->EVENTS_COMPARE[0] = 0;
    IR_CARRIER_COUNTER->EVENTS_COMPARE[1] = 0;

    err_code |= sd_nvic_SetPriority(IR_CARRIER_COUNTER_IRQn, IR_CARRIER_COUNTER_IRQ_Priority);
    err_code |= sd_nvic_EnableIRQ(IR_CARRIER_COUNTER_IRQn);

    err_code |= sd_ppi_channel_assign(IR_PPI_CH_A, &IR_TIMER_CARRIER->EVENTS_COMPARE[1], &IR_CARRIER_COUNTER->TASKS_COUNT);
    
    err_code |= sd_ppi_channel_assign(IR_PPI_CH_B, &IR_TIMER_CARRIER->EVENTS_COMPARE[0], &NRF_GPIOTE->TASKS_OUT[0]);
    err_code |= sd_ppi_channel_assign(IR_PPI_CH_C, &IR_TIMER_CARRIER->EVENTS_COMPARE[1], &NRF_GPIOTE->TASKS_OUT[0]);

    err_code |= sd_ppi_group_assign(IR_PPI_GROUP, 1 << IR_PPI_CH_B | 1 << IR_PPI_CH_C);
    err_code |= sd_ppi_group_task_disable(IR_PPI_GROUP);
    
    err_code |= sd_ppi_channel_assign(IR_PPI_CH_D, &IR_CARRIER_COUNTER->EVENTS_COMPARE[0], &NRF_PPI->TASKS_CHG[IR_PPI_GROUP].DIS);
    err_code |= sd_ppi_channel_assign(IR_PPI_CH_E, &IR_CARRIER_COUNTER->EVENTS_COMPARE[1], &NRF_PPI->TASKS_CHG[IR_PPI_GROUP].EN);

    err_code |= sd_ppi_channel_enable_set(1 << IR_PPI_CH_A | 1 << IR_PPI_CH_B | 1 << IR_PPI_CH_C | 1 << IR_PPI_CH_D | 1 << IR_PPI_CH_E);
    
    m_busy = false;
    return err_code;
}

void ir_lib_send(uint16_t *time_us, uint32_t length)
{
    while(m_busy);
    m_busy = true;
    
    m_current_data_ptr = time_us;
    
    m_integrated_time_us = *m_current_data_ptr++;
    IR_CARRIER_COUNTER->CC[0] = pulse_count_calculate(m_integrated_time_us);
    m_integrated_time_us += *m_current_data_ptr++;
    IR_CARRIER_COUNTER->CC[1] = pulse_count_calculate(m_integrated_time_us);
    
    m_bits_remaining = length;
    m_next_cc_for_update = 0;

    sd_ppi_group_task_enable(IR_PPI_GROUP);
    
    IR_CARRIER_COUNTER->TASKS_CLEAR = 1;
    IR_CARRIER_COUNTER->TASKS_START = 1;
    
    IR_TIMER_CARRIER->TASKS_CLEAR = 1;
    IR_TIMER_CARRIER->TASKS_START = 1;
}

void IR_CARRIER_COUNTER_IRQHandler(void)
{
    while(IR_CARRIER_COUNTER->EVENTS_COMPARE[m_next_cc_for_update])
    {
        IR_CARRIER_COUNTER->EVENTS_COMPARE[m_next_cc_for_update] = 0;
        
        m_bits_remaining--;
        if(m_bits_remaining >= 2)
        {            
            m_integrated_time_us += *m_current_data_ptr++;
            IR_CARRIER_COUNTER->CC[m_next_cc_for_update] = pulse_count_calculate(m_integrated_time_us); 
        }
        else if(m_bits_remaining == 0)
        {
            IR_TIMER_CARRIER->TASKS_STOP = 1;
            m_busy = false;
        }
        m_next_cc_for_update = 1 - m_next_cc_for_update;
    }
}

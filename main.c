/* Copyright (c) 2015 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

#include "app_button.h"
#include "app_error.h"
#include "app_pwm.h"
#include "app_timer.h"
#include "ble_advdata.h"
#include "ble_conn_params.h"
#include "ble_gap.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_lbs.h"
#include "ble_srv_common.h"
#include "boards.h"
#include "nordic_common.h"
#include "nrf_delay.h"
#include "nrf_drv_gpiote.h"
#include "nrf_drv_timer.h"
#include "nrf_gpio.h"
#include "nrf.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "softdevice_handler.h"

#include "advertising.h"
#include "read_set_bit.h"
#include "twi_motordriver.h"
#include "twi_rfid_driver.h"


#define CENTRAL_LINK_COUNT              0                                           /**<number of central links used by the application. When changing this number remember to adjust the RAM settings*/
#define PERIPHERAL_LINK_COUNT           1                                           /**<number of peripheral links used by the application. When changing this number remember to adjust the RAM settings*/

#define LED_CONNECTED_PIN               17                                          /**< Is on when device has connected. */
#define LED_ADVERTISING_PIN             18                                          /**< Is on when device is advertising. */
#define LED_MOTOR_TWI_WRITE_PIN         19                                          /**< Is on when the motor driver writes to the shield. */
#define LED_RFID_TWI_READ_PIN           20                                          /**< Is on when the RFID driver reads from the TWI-channel. */

#define PIN_OUTPUT_START                12                                           /**< First PIN out of 8 which will be uses as outputs. The seven subsequent pins will also turn into outputs. >**/
#define PIN_OUTPUT_OFFSET               1

#define IR_RECEIVER_PIN_1               13                                          /**< Button that will trigger the notification event with the LED Button Service */
#define IR_RECEIVER_PIN_2               14
#define IR_RECEIVER_PIN_3               15
#define RFID_INTERRUPT_PIN              16

#define PWM_PERIOD                      5000L
#define PWM_RED_PIN                     22
#define PWM_GREEN_PIN                   23
#define PWM_BLUE_PIN                    2
#define PWM_IR_PIN                      11

#define PIEZO_BUZZER_PIN                3

#define DEVICE_NAME                     "PROTO CAR"                            /**< Name of device. Will be included in the advertising data. */

#define APP_ADV_INTERVAL                64                                          /**< The advertising interval (in units of 0.625 ms; this value corresponds to 40 ms). */
#define APP_ADV_TIMEOUT_IN_SECONDS      BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED       /**< The advertising time-out (in units of seconds). When set to 0, we will never time out. */

#define APP_TIMER_PRESCALER             15                                           /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_MAX_TIMERS            6                                           /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE         3                                           /**< Size of timer operation queues. */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(20, UNIT_1_25_MS)            /**< Minimum acceptable connection interval (0.5 seconds). */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(50, UNIT_1_25_MS)            /**< Maximum acceptable connection interval (1 second). */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)             /**< Connection supervisory time-out (4 seconds). */
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(20000, APP_TIMER_PRESCALER) /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (15 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER)  /**< Time between each call to sd_ble_gap_conn_param_update after the first call (5 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                           /**< Number of attempts before giving up the connection parameter negotiation. */

#define APP_GPIOTE_MAX_USERS            1                                           /**< Maximum number of users of the GPIOTE handler. */
#define BUTTON_DETECTION_DELAY          APP_TIMER_TICKS(50, APP_TIMER_PRESCALER)    /**< Delay from a GPIOTE event until a button is reported as pushed (in number of timer ticks). */

#define RADIO_NOTIFICATION_IRQ_PRIORITY 3
#define RADIO_NOTIFICATION_DISTANCE     NRF_RADIO_NOTIFICATION_DISTANCE_800US

static uint16_t                         m_conn_handle = BLE_CONN_HANDLE_INVALID;    /**< Handle of the current connection. */
static ble_lbs_t                        m_lbs;                                      /**< LED Button Service instance. */
bool user_connected = false;                                                        /**< A flag indicating if there is an user connected. */

uint8_t rfid_counter = 0;                                                           /* Counter for every time the RFID-shield senses a nearby tag. */
uint8_t hit_counter = 0;                                                            /* Counter for every time the unit has been hit. */

APP_TIMER_DEF(advertising_timer);                                                   /* Defines the advertising app_timer. */
APP_PWM_INSTANCE(PWM1, 1);                                                          /* Defines the PWM instance used for the red and green LED-pins. */
APP_PWM_INSTANCE(PWM2, 2);                                                          /* Defines the PWM instance used for the blue LED-pins. */


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
    
    while (app_pwm_channel_duty_set(&PWM1, 1, red_value) == NRF_ERROR_BUSY);
    while (app_pwm_channel_duty_set(&PWM1, 0, green_value) == NRF_ERROR_BUSY);
    while (app_pwm_channel_duty_set(&PWM2, 0, blue_value) == NRF_ERROR_BUSY);
    
}

/**@brief Function for updating the hit value when a hit is registered.
*
* @details The value will cycle between the values from 1 between 255.
*/

uint8_t new_hit_value (void){
    if (hit_counter == 0)
        hit_counter = 1;
    else
        hit_counter++;
    return hit_counter;
}

/**@brief Function for the output pin initialization.
 *
 * @details Initializes all output pins used by the application.
 */
static void pin_output_init(void)
{
    for(uint8_t i = 0; i < PIN_OUTPUT_OFFSET; i++){ 
    nrf_gpio_cfg_output((PIN_OUTPUT_START + i));
    nrf_gpio_pin_clear((PIN_OUTPUT_START + i));
    }

    nrf_gpio_cfg_output(LED_CONNECTED_PIN);
    nrf_gpio_cfg_output(LED_ADVERTISING_PIN);
    nrf_gpio_cfg_output(LED_MOTOR_TWI_WRITE_PIN);
    nrf_gpio_cfg_output(LED_RFID_TWI_READ_PIN);

    nrf_gpio_pin_set(LED_CONNECTED_PIN);
    nrf_gpio_pin_set(LED_ADVERTISING_PIN);
    nrf_gpio_pin_set(LED_MOTOR_TWI_WRITE_PIN);
    nrf_gpio_pin_set(LED_RFID_TWI_READ_PIN);

    nrf_gpio_cfg_output(PIEZO_BUZZER_PIN);
    nrf_gpio_pin_clear(PIEZO_BUZZER_PIN);
}

/**@brief Function for playing notes on a piezo buzzer.
*
*/

void playNote(uint16_t note){

  for(uint32_t i = 0; i < 50000; i += note * 2){
    nrf_gpio_pin_set(PIEZO_BUZZER_PIN);
    nrf_delay_us(note);
    nrf_gpio_pin_clear(PIEZO_BUZZER_PIN);
    nrf_delay_us(note);
  }
}

/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module.
 */
static void timers_init(void)
{
    // Initialize timer module, making it use the scheduler
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, false);
}


/**@brief Function for the PWM initialization.
 *
 * @details Initializes the PWM module.
 */
static void pwm_init(void)
{
    ret_code_t err_code;
    
    /* 2-channel PWM, 200Hz, output on pins. */
    app_pwm_config_t pwm1_cfg = APP_PWM_DEFAULT_CONFIG_2CH(20000L, PWM_RED_PIN, PWM_GREEN_PIN);
    
    /* 1-channel PWM, 200Hz, output on pins. */
    app_pwm_config_t pwm2_cfg = APP_PWM_DEFAULT_CONFIG_2CH(26L, PWM_BLUE_PIN, PWM_IR_PIN);

    pwm2_cfg.pin_polarity[0] = APP_PWM_POLARITY_ACTIVE_HIGH;

    /* Initialize and enable PWM. */
    err_code = app_pwm_init(&PWM1, &pwm1_cfg, NULL);
    APP_ERROR_CHECK(err_code);
    
    err_code = app_pwm_init(&PWM2,&pwm2_cfg,NULL);
    APP_ERROR_CHECK(err_code);
    
    app_pwm_enable(&PWM1);
    app_pwm_enable(&PWM2);

    set_rgb_color(100);
}

/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the Advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 *          This advertising packet makes the device connectable.
 */

void advertising_init(void)
{
    uint32_t      err_code;
    ble_advdata_t advdata;
    ble_advdata_t scanrsp;
    
    ble_uuid_t adv_uuids[] = {{LBS_UUID_SERVICE, m_lbs.uuid_type}};

    // Build and set advertising data
    memset(&advdata, 0, sizeof(advdata));

    advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance = true;
    advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;


    memset(&scanrsp, 0, sizeof(scanrsp));
    scanrsp.uuids_complete.uuid_cnt = sizeof(adv_uuids) / sizeof(adv_uuids[0]);
    scanrsp.uuids_complete.p_uuids  = adv_uuids;

    err_code = ble_advdata_set(&advdata, &scanrsp);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling write events to the pin_write characteristic.
 *
 * @param[in] pin_state Pointer to received data from read/write characteristic.
 */

static void pin_write_handler(ble_lbs_t * p_lbs, uint8_t * pin_state)
{  
    
    // Sets motor values for every motor.

    twi_set_motor(pin_state);

    // Sets the color for every RGB-LED.

    uint8_t web_color_data = read_byte(pin_state, 5);
    set_rgb_color(web_color_data);

    // Checks every pin_state[i] bitwise to a given binary number with a for loop.
       
    for(uint8_t i = 0; i < PIN_OUTPUT_OFFSET; i++){
      if (read_bit(pin_state, 1, i)){
          if (i == 0) {
            set_rgb_color(2);
            playNote(536);
            nrf_delay_ms(50);
            while (app_pwm_channel_duty_set(&PWM2, 1, 50) == NRF_ERROR_BUSY);;
            playNote(536);
            nrf_delay_ms(50);
            set_rgb_color(250);
            }
          else if (i == 1)
            nrf_gpio_pin_set((PIN_OUTPUT_START+i));
          }
      else if(!read_bit(pin_state, 1, 0))
            while (app_pwm_channel_duty_set(&PWM2, 1, 0) == NRF_ERROR_BUSY);
      else
         nrf_gpio_pin_clear((PIN_OUTPUT_START+i));
     } 
}

/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    uint32_t       err_code;
    ble_lbs_init_t init;

    init.pin_write_handler = pin_write_handler;

    err_code = ble_lbs_init(&m_lbs, &init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module that
 *          are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply
 *       setting the disconnect_on_fail config parameter, but instead we use the event
 *       handler mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for starting the advertisement.
 */
static void advertising_start(void)
{
    uint32_t             err_code;
    ble_gap_adv_params_t adv_params;

    // Start advertising
    memset(&adv_params, 0, sizeof(adv_params));

    adv_params.type        = BLE_GAP_ADV_TYPE_ADV_IND;
    adv_params.p_peer_addr = NULL;
    adv_params.fp          = BLE_GAP_ADV_FP_ANY;
    adv_params.interval    = APP_ADV_INTERVAL;
    adv_params.timeout     = APP_ADV_TIMEOUT_IN_SECONDS;

    err_code = sd_ble_gap_adv_start(&adv_params);
    APP_ERROR_CHECK(err_code);
    nrf_gpio_pin_clear(LED_ADVERTISING_PIN);
}


/**@brief Function for handling radio events and switching advertisement data **/

uint8_t advertising_switch_counter = 0;

void radio_notification_evt_handler(void* p_context)
{
   if (user_connected == false){    
        if(advertising_switch_counter % 2 == 0)
        {
            // Switching to Eddystone
            advertising_init_eddystone();
        }
        else
        {
            // Advertises that the device is connectable
            advertising_init();
        }
        advertising_switch_counter++;
        if(advertising_switch_counter % 2 == 0 && user_connected == false)
            nrf_gpio_pin_clear(LED_ADVERTISING_PIN);
        else
            nrf_gpio_pin_set(LED_ADVERTISING_PIN);
   }
   else
        nrf_gpio_pin_set(LED_ADVERTISING_PIN);
}


/**@brief Function for handling the Application's BLE stack events.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    
    uint32_t err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            nrf_gpio_pin_clear(LED_CONNECTED_PIN);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;

            err_code = app_button_enable();
            APP_ERROR_CHECK(err_code);

            user_connected = true;
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            nrf_gpio_pin_set(LED_CONNECTED_PIN);
            m_conn_handle = BLE_CONN_HANDLE_INVALID;

            err_code = app_button_disable();
            APP_ERROR_CHECK(err_code);
            
            user_connected = false;
            advertising_start();
            twi_clear_motorshield();
            break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle,
                                                   BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP,
                                                   NULL,
                                                   NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }
}


static void ble_evt_dispatch(ble_evt_t * p_ble_evt)
{
    on_ble_evt(p_ble_evt);
    ble_conn_params_on_ble_evt(p_ble_evt);
    ble_lbs_on_ble_evt(&m_lbs, p_ble_evt);
}



/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */

static void ble_stack_init(void)
{
    uint32_t err_code;
    
    nrf_clock_lf_cfg_t clock_lf_cfg = NRF_CLOCK_LFCLKSRC;
    
    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);
    ble_enable_params_t ble_enable_params;
    err_code = softdevice_enable_get_default_config(CENTRAL_LINK_COUNT,
                                                    PERIPHERAL_LINK_COUNT,
                                                    &ble_enable_params);
    APP_ERROR_CHECK(err_code);
    
    //Check the ram settings against the used number of links
    CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT,PERIPHERAL_LINK_COUNT);
    
    // Enable BLE stack.
    err_code = softdevice_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);

    ble_gap_addr_t addr;

    err_code = sd_ble_gap_address_get(&addr);
    APP_ERROR_CHECK(err_code);
    err_code = sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_NONE, &addr);
    APP_ERROR_CHECK(err_code);

    // Subscribe for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for writing the proper notification when an input is received.
**/

static void pin_event_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action){
    if(pin == IR_RECEIVER_PIN_1 || pin == IR_RECEIVER_PIN_2 || pin == IR_RECEIVER_PIN_3){
      hit_counter = new_hit_value();
      playNote(1516);
      nrf_delay_ms(50);
      playNote(1607);

      if(pin == IR_RECEIVER_PIN_1)            
            ble_lbs_on_button_change(&m_lbs, hit_counter, 0);
      else if(pin == IR_RECEIVER_PIN_2)
            ble_lbs_on_button_change(&m_lbs, hit_counter, 1);
      else if(pin == IR_RECEIVER_PIN_3)
            ble_lbs_on_button_change(&m_lbs, hit_counter, 2);
    }

    else if(pin == RFID_INTERRUPT_PIN) {
            rfid_counter = rfid_read_event_handler();
            ble_lbs_on_button_change(&m_lbs, rfid_counter, 4);

            if(rfid_counter % 20 == 0){
            playNote(1072);
            set_rgb_color(2);
            nrf_delay_ms(10);
            playNote(1012);
            set_rgb_color(1);
            nrf_delay_ms(10);
            playNote(955);
            set_rgb_color(2);
            nrf_delay_ms(10);
            playNote(901); 
            set_rgb_color(0);
            }
    }
}

/**@brief Function for initializing the gpiote driver.
*/

void nrf_gpiote_init(void){

    uint32_t err_code;
    if(!nrf_drv_gpiote_is_init())
      {
        err_code = nrf_drv_gpiote_init();
      }
    APP_ERROR_CHECK(err_code);
    nrf_drv_gpiote_in_config_t ir_config = GPIOTE_CONFIG_IN_SENSE_TOGGLE(false);
    ir_config.pull = NRF_GPIO_PIN_NOPULL;

    nrf_drv_gpiote_in_config_t rfid_config = GPIOTE_CONFIG_IN_SENSE_TOGGLE(false);
    rfid_config.pull = NRF_GPIO_PIN_PULLDOWN;

    nrf_drv_gpiote_in_init(IR_RECEIVER_PIN_1, &ir_config, pin_event_handler);
    nrf_drv_gpiote_in_init(IR_RECEIVER_PIN_2, &ir_config, pin_event_handler);
    nrf_drv_gpiote_in_init(IR_RECEIVER_PIN_3, &ir_config, pin_event_handler);
    nrf_drv_gpiote_in_init(RFID_INTERRUPT_PIN, &rfid_config, pin_event_handler);

    nrf_drv_gpiote_in_event_enable(IR_RECEIVER_PIN_1, true);
    nrf_drv_gpiote_in_event_enable(IR_RECEIVER_PIN_2, true);
    nrf_drv_gpiote_in_event_enable(IR_RECEIVER_PIN_3, true);
    nrf_drv_gpiote_in_event_enable(RFID_INTERRUPT_PIN, true);
}

void advertising_timer_init(void){

  app_timer_create(&advertising_timer, APP_TIMER_MODE_REPEATED, &radio_notification_evt_handler);
  //Starts the timer, sets it up for repeated start.
  app_timer_start(advertising_timer, APP_TIMER_TICKS(300, 15), NULL);

}

/**@brief Function for the Power Manager.
 */
static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();

    APP_ERROR_CHECK(err_code);
}


/**@brief Function for application main entry.
*
*/

int main(void)
{ 
    //Initialize GPIO
    nrf_gpiote_init();
    pin_output_init();
    pwm_init();  
    
    // Initialize
    timers_init();
    
    ble_stack_init();
    gap_params_init();
    services_init();
    advertising_init();
    conn_params_init();

    //Starts advertising
    advertising_timer_init();
    advertising_start();

    //Initialize shields
    twi_motordriver_init();
    twi_rfid_init();

    //Feedback, notifying the user that the DK is ready
    set_rgb_color(0);
    playNote(1607);
    nrf_delay_ms(30);
    playNote(1516);
    nrf_delay_ms(30);
    playNote(1431);

     // Enter main loop.
    for (;;)
    {
        power_manage();
    }
}

/**
 * @}
 */
 
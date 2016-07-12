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
#include "nrf_drv_ppi.h"
#include "nrf_gpio.h"
#include "nrf.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "softdevice_handler.h"

#include "advertising.h"
#include "infrared_communication.h"
#include "ir_lib.h"
#include "perip_pwm.h"
#include "read_set_bit.h"
#include "twi_motordriver.h"
#include "twi_rfid_driver.h"
#include "SEGGER_RTT.h"

#define CENTRAL_LINK_COUNT              0                                           /**<number of central links used by the application. When changing this number remember to adjust the RAM settings*/
#define PERIPHERAL_LINK_COUNT           1                                           /**<number of peripheral links used by the application. When changing this number remember to adjust the RAM settings*/

#define LED_CONNECTED_PIN               17                                          /**< Is on when device has connected. */
#define LED_ADVERTISING_PIN             18                                          /**< Is on when device is advertising. */
#define LED_MOTOR_TWI_WRITE_PIN         19                                          /**< Is on when the motor driver writes to the shield. */
#define LED_RFID_TWI_READ_PIN           20                                          /**< Is on when the RFID driver reads from the TWI-channel. */

#define PIN_OUTPUT_START                12                                          /**< First PIN out of 8 which will be used as outputs. The seven subsequent pins will also turn into outputs. >**/
#define PIN_OUTPUT_OFFSET               1

#define RFID_INTERRUPT_PIN              16

#define PIEZO_BUZZER_PIN                3

#define DEVICE_NAME                     "NOT A CAR"                                 /**< Name of device. Will be included in the advertising data. */

#define APP_ADV_INTERVAL                64                                          /**< The advertising interval (in units of 0.625 ms; this value corresponds to 40 ms). */
#define APP_ADV_TIMEOUT_IN_SECONDS      BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED       /**< The advertising time-out (in units of seconds). When set to 0, we will never time out. */

#define APP_TIMER_PRESCALER             0                                           /**< Value of the RTC1 PRESCALER register. */
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

APP_TIMER_DEF(advertising_timer);                                                   /* Defines the advertising app_timer. */


uint8_t hit_counter = 0;                                                            /* Counter for every time the unit has been hit. */

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

/**@brief Function for setting the color of the RGB-LEDS
*
*@details A decimal value of new_color_data uses predefined values to set the color.
*         Value 0 = Green color.
*         Value 1 = Red color. 
*         Value 2 = Blue color.
*         Value 100 = No color. (The LED is turned off).
*@details duty_values is a struct defined in perip_pwm.h. 
*         pwm1 = PWM to Red LED.
*         pwm2 = PWM to Green LED.
*         pwm3 = PWM to Blue LED.
*
* The function also has a way of remembering the previous state; green_state = true means you are vulnerable,
*  green_state = false means you are invulnerable after a recent hit.
*  An input value of 250 makes the LED either turn green or red, depending on the actual game state.
*/


uint8_t  red_value = 0, green_value = 0, blue_value = 0, color_data = 0;
bool green_state = false;

void set_rgb_color(uint8_t new_color_data){

  duty_values color;
  
  if (new_color_data == 250 && green_state == true)
      color_data = 0;
  else if (new_color_data == 250 && green_state == false)
      color_data = 1;
  else
      color_data = new_color_data;
  
  switch (color_data) {
    case 0:
          color.pwm1 = 0;
          color.pwm2 = 1000;
          color.pwm3 = 0;
          green_state = true;
          break;
    case 1:
          color.pwm1 = 1000;
          color.pwm2 = 0;
          color.pwm3 = 0;
          green_state = false;
          break;
    case 2:
          color.pwm1 = 0;
          color.pwm2 = 0;
          color.pwm3 = 1000;
          break;
    case 100:
          color.pwm1 = 1000;
          color.pwm2 = 1000;
          color.pwm3 = 0;
          break;
     default:
          break;
    }
    pwm_values_update(color);    
}


// IR pin event variables

static nrf_drv_timer_t ir_timer = NRF_DRV_TIMER_INSTANCE(3);

void timer_dummy_handler(nrf_timer_event_t event_type, void * p_context){}

volatile bool activate_ir = false;


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

    // Shoots IR-signal.
    if (read_bit(pin_state, 1, 0))
    {
        ir_shooting(pin_state);
        playNote(536);
        nrf_delay_ms(50);
        playNote(536);
    }
    
    // Turns laser on when game session is active
    if(read_bit(pin_state, 1, 1))
    {
       nrf_gpio_pin_set(LASER_TRANSISTOR);
    }
    else
       nrf_gpio_pin_clear(LASER_TRANSISTOR);
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

// Function to decode IR signals and message to web if the car is hit
void ir_in_pin_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
      static uint8_t decoded = 0;
      static uint8_t offset = 0;
      static uint32_t prev = 0;

      if(offset >= 8)
          offset = 0;

      uint32_t usec = nrf_drv_timer_capture(&ir_timer, NRF_TIMER_CC_CHANNEL0); 
      if(offset) 
          SEGGER_RTT_printf(0, "%d\r\n", usec - prev);
      
      if((usec - prev) > 2000)
      {
          decoded += (1 << (offset - 1));
      }
      
      if(offset == 7) 
      {
          SEGGER_RTT_printf(0, "\r\n\n%d\n\r\n", decoded);
          new_hit_value();
          if(decoded != CAR_ID && decoded >= 1 && decoded <= 16)
          {
              ble_lbs_on_button_change(&m_lbs, hit_counter, 0);
          }
          decoded = 0;
          
          playNote(1516);
          nrf_delay_ms(50);
          playNote(1607);
      }
            

      prev = usec;
      offset++;
}



/**@brief Function for writing the proper notification when an input is received.
**/

static void pin_event_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action){
    //if(pin == IR_RECEIVER_PIN_1 || pin == IR_RECEIVER_PIN_2 || pin == IR_RECEIVER_PIN_3){
      
     /*hit_counter = new_hit_value();
      playNote(1516);
      nrf_delay_ms(50);
      playNote(1607);*/

     /* if(pin == IR_RECEIVER_PIN_1) {          
          ir_in_pin_handler(pin, action);
           //ble_lbs_on_button_change(&m_lbs, hit_counter, 0);
      }
      else if(pin == IR_RECEIVER_PIN_2)
            ble_lbs_on_button_change(&m_lbs, hit_counter, 1);
      else if(pin == IR_RECEIVER_PIN_3)
            ble_lbs_on_button_change(&m_lbs, hit_counter, 2);
    }*/

    if(pin == RFID_INTERRUPT_PIN) {
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

    nrf_drv_gpiote_in_config_t rfid_config = GPIOTE_CONFIG_IN_SENSE_TOGGLE(false);
    rfid_config.pull = NRF_GPIO_PIN_PULLDOWN;

    nrf_drv_gpiote_in_init(RFID_INTERRUPT_PIN, &rfid_config, pin_event_handler);

    nrf_drv_gpiote_in_event_enable(RFID_INTERRUPT_PIN, true);
}

void advertising_timer_init(void){

  app_timer_create(&advertising_timer, APP_TIMER_MODE_REPEATED, &radio_notification_evt_handler);
  //Starts the timer, sets it up for repeated start.
  app_timer_start(advertising_timer, APP_TIMER_TICKS(300, APP_TIMER_PRESCALER), NULL);

}




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
    uint32_t err_code;

    //Initialize GPIO
    nrf_gpiote_init();
    pin_output_init(); 
    
    err_code = nrf_drv_ppi_init();
    APP_ERROR_CHECK(err_code);



    // Initialize PWM
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

    // Initialize the IR lib. Must be done after initializing the SoftDevice
    ir_lib_init(IR_OUTPUT_PIN);
    err_code = ir_ppi_init();
    APP_ERROR_CHECK(err_code);

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
 
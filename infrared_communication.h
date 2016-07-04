#define PWM_PERIOD                      5000L
#define PWM_RED_PIN                     22
#define PWM_GREEN_PIN                   23
#define PWM_BLUE_PIN                    2
#define PWM_IR_PIN                      11

#define LASER_TRANSISTOR                12

//Prototypes for functions used in infrared_communication.c
void pwm_init(void);
void set_rgb_color(uint8_t new_color_data);
void ir_shooting(uint8_t * ir_data);

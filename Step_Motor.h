#ifndef STEP_MOTOR_H
#define STEP_MOTOR_H

#include <Arduino.h>

enum Direct_State {
  FORWARD,
  BACKWARD
};

enum Motor_State {
  INACTIVE,
  ACTIVE
};

class StepperMotor {
public:
    StepperMotor(uint8_t stepPin, uint8_t dirPin, uint8_t enPin, uint8_t timerNum);

    void enable();
    void disable();
    void setDirection(Direct_State dir);
    void setSpeedRPM(float rpm);
    void moveSteps(uint32_t steps);
    void rotate(float rpm, uint32_t time_s = 0); // time_s = 0 → quay vô hạn

    void handleStepPulse(); // gọi trong ISR

    uint8_t step_pin;
    uint8_t dir_pin;
    uint8_t en_pin;
    
    float speed_rpm;
    uint16_t steps_per_rev;
    uint32_t target_steps;

    Direct_State direction;
    Motor_State state;
    uint32_t distance;

    uint32_t time_run;       // giây
    uint32_t start_time;     // millis()
    uint32_t step_count;

    uint8_t timer_number;    // Timer1, Timer2...
};

#endif

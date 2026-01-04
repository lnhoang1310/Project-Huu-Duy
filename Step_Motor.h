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

    // Biến public để code chính đọc
    float speed_rpm = 60.0f;
    uint16_t steps_per_rev = 200;
    uint32_t distance = 0;               // dùng lưu distance cho motor1

    Direct_State direction = FORWARD;
    Motor_State state = INACTIVE;

    volatile uint32_t step_count = 0;
    uint32_t target_steps = 0;
    uint32_t time_run = 0;               // giây
    uint32_t start_time = 0;

private:
    uint8_t step_pin;
    uint8_t dir_pin;
    uint8_t en_pin;
    uint8_t timer_number;                // 1 hoặc 2

    void startTimer();
    void stopTimer();
};

#endif
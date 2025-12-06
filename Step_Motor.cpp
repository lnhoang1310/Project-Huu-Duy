#include "Step_Motor.h"
#include <avr/io.h>
#include <avr/interrupt.h>

#define F_CPU 16000000UL

StepperMotor* motorTimer1 = nullptr;
ISR(TIMER1_COMPA_vect) {
    if (motorTimer1 && motorTimer1->state == ACTIVE) {
        motorTimer1->handleStepPulse();
    }
}

StepperMotor* motorTimer2 = nullptr;
ISR(TIMER2_COMPA_vect) {
    if (motorTimer2 && motorTimer2->state == ACTIVE) {
        motorTimer2->handleStepPulse();
    }
}

StepperMotor::StepperMotor(uint8_t stepPin, uint8_t dirPin, uint8_t enPin, uint8_t timerNum) {
    step_pin = stepPin;
    dir_pin = dirPin;
    en_pin = enPin;

    pinMode(step_pin, OUTPUT);
    pinMode(dir_pin, OUTPUT);
    pinMode(en_pin, OUTPUT);
    digitalWrite(en_pin, HIGH);

    speed_rpm = 60;
    steps_per_rev = 200;
    target_steps = 0;
    direction = FORWARD;
    state = INACTIVE;
    step_count = 0;
    timer_number = timerNum;
    time_run = 0;
    start_time = 0;

    if (timer_number == 1) motorTimer1 = this;
    else if (timer_number == 2) motorTimer2 = this;
}

void StepperMotor::enable() {
    digitalWrite(en_pin, LOW);
    state = ACTIVE;
}

void StepperMotor::disable() {
    digitalWrite(en_pin, HIGH);
    state = INACTIVE;
}

void StepperMotor::setDirection(Direct_State dir) {
    direction = dir;
    digitalWrite(dir_pin, dir == FORWARD ? HIGH : LOW);
}

void StepperMotor::setSpeedRPM(float rpm) {
    if (rpm < 0.1f) rpm = 0.1f;
    speed_rpm = rpm;

    float step_freq = (speed_rpm * steps_per_rev) / 60.0f; // Hz
    uint32_t ocr;

    if (timer_number == 1) {
        uint32_t prescaler = 1;
        uint32_t ocr_temp = (F_CPU / (2 * prescaler * step_freq)) - 1;
        if (ocr_temp > 65535) { prescaler = 8; ocr_temp = (F_CPU / (2 * prescaler * step_freq)) - 1; }
        if (ocr_temp > 65535) { prescaler = 64; ocr_temp = (F_CPU / (2 * prescaler * step_freq)) - 1; }
        ocr = ocr_temp;
        TCCR1A = 0;
        TCCR1B = 0;
        TCCR1B |= (1 << WGM12);
        if (prescaler == 1) TCCR1B |= (1 << CS10);
        else if (prescaler == 8) TCCR1B |= (1 << CS11);
        else if (prescaler == 64) TCCR1B |= (1 << CS11) | (1 << CS10);
        OCR1A = ocr;
        TIMSK1 |= (1 << OCIE1A);
    } 
    else if (timer_number == 2) {
        uint32_t prescaler = 1;
        uint32_t ocr_temp = (F_CPU / (2 * prescaler * step_freq)) - 1;
        if (ocr_temp > 255) { prescaler = 8; ocr_temp = (F_CPU / (2 * prescaler * step_freq)) - 1; }
        if (ocr_temp > 255) { prescaler = 32; ocr_temp = (F_CPU / (2 * prescaler * step_freq)) - 1; }
        if (ocr_temp > 255) { prescaler = 64; ocr_temp = (F_CPU / (2 * prescaler * step_freq)) - 1; }
        if (ocr_temp > 255) { prescaler = 128; ocr_temp = (F_CPU / (2 * prescaler * step_freq)) - 1; }
        if (ocr_temp > 255) { prescaler = 256; ocr_temp = (F_CPU / (2 * prescaler * step_freq)) - 1; }
        if (ocr_temp > 255) { prescaler = 1024; ocr_temp = (F_CPU / (2 * prescaler * step_freq)) - 1; }
        ocr = ocr_temp;
        TCCR2A = 0;
        TCCR2B = 0;
        TCCR2A |= (1 << WGM21);
        if (prescaler == 1) TCCR2B |= (1 << CS20);
        else if (prescaler == 8) TCCR2B |= (1 << CS21);
        else if (prescaler == 32) TCCR2B |= (1 << CS21) | (1 << CS20);
        else if (prescaler == 64) TCCR2B |= (1 << CS22);
        else if (prescaler == 128) TCCR2B |= (1 << CS22) | (1 << CS20);
        else if (prescaler == 256) TCCR2B |= (1 << CS22) | (1 << CS21);
        else if (prescaler == 1024) TCCR2B |= (1 << CS22) | (1 << CS21) | (1 << CS20);
        OCR2A = ocr;
        TIMSK2 |= (1 << OCIE2A);
    }
}

void StepperMotor::moveSteps(uint32_t steps) {
    target_steps = steps;
    step_count = 0;
    enable();
}

void StepperMotor::rotate(float rpm, uint32_t time_s) {
    setSpeedRPM(rpm);
    time_run = time_s;
    start_time = millis();
    enable();
}

void StepperMotor::handleStepPulse() {
    digitalWrite(step_pin, HIGH);
    delayMicroseconds(2);
    digitalWrite(step_pin, LOW);

    step_count++;
    if (target_steps > 0 && step_count >= target_steps) disable();
    if ((millis() - start_time) >= time_run * 1000UL) disable();
}

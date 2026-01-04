#include "Step_Motor.h"
#include <avr/io.h>

StepperMotor* motorTimer1 = nullptr;
StepperMotor* motorTimer2 = nullptr;

// ISR chỉ đếm bước và dừng khi đủ bước (rất nhẹ)
ISR(TIMER1_COMPA_vect) {
    if (motorTimer1 && motorTimer1->state == ACTIVE) {
        motorTimer1->step_count++;
        if (motorTimer1->target_steps > 0 && motorTimer1->step_count >= motorTimer1->target_steps) {
            motorTimer1->disable();
        }
    }
}

ISR(TIMER2_COMPA_vect) {
    if (motorTimer2 && motorTimer2->state == ACTIVE) {
        motorTimer2->step_count++;
        if (motorTimer2->target_steps > 0 && motorTimer2->step_count >= motorTimer2->target_steps) {
            motorTimer2->disable();
        }
    }
}

StepperMotor::StepperMotor(uint8_t stepPin, uint8_t dirPin, uint8_t enPin, uint8_t timerNum) {
    step_pin = stepPin;
    dir_pin = dirPin;
    en_pin = enPin;
    timer_number = timerNum;

    pinMode(step_pin, OUTPUT);
    pinMode(dir_pin, OUTPUT);
    pinMode(en_pin, OUTPUT);
    digitalWrite(en_pin, HIGH); // disable ban đầu

    if (timer_number == 1) motorTimer1 = this;
    else if (timer_number == 2) motorTimer2 = this;
}

void StepperMotor::enable() {
    digitalWrite(en_pin, LOW);
    step_count = 0;
    if (time_run > 0) start_time = millis();
    startTimer();
    state = ACTIVE;
}

void StepperMotor::disable() {
    stopTimer();
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
    if (state == ACTIVE) startTimer(); // cập nhật tần số ngay
}

void StepperMotor::startTimer() {
    float step_freq = (speed_rpm * steps_per_rev) / 60.0f;
    if (step_freq < 0.1f) step_freq = 0.1f;

    uint32_t ocr = 0;
    uint8_t prescaler_bits = 0;

    if (timer_number == 1) { // Timer1 - OC1A (pin 9)
        uint32_t temp = (16000000UL / step_freq) - 1;
        if (temp <= 65535) {
            ocr = temp;
            prescaler_bits = (1 << CS10); // prescaler 1
        } else if ((temp = (16000000UL / 8 / step_freq) - 1) <= 65535) {
            ocr = temp;
            prescaler_bits = (1 << CS11); // 8
        } else {
            temp = (16000000UL / 64 / step_freq) - 1;
            ocr = (temp > 65535) ? 65535 : temp;
            prescaler_bits = (1 << CS11) | (1 << CS10); // 64
        }

        TCCR1A = (1 << COM1A0);              // Toggle OC1A on compare match
        TCCR1B = (1 << WGM12) | prescaler_bits; // CTC mode + start
        OCR1A = ocr;
        TIMSK1 |= (1 << OCIE1A);              // enable interrupt để đếm bước
    } 
    else if (timer_number == 2) { // Timer2 - OC2B (pin 3)
        uint32_t temp = (16000000UL / step_freq) - 1;
        if (temp <= 255) {
            ocr = temp;
            prescaler_bits = (1 << CS20);
        } else if ((temp = (16000000UL / 8 / step_freq) - 1) <= 255) {
            ocr = temp;
            prescaler_bits = (1 << CS21);
        } else if ((temp = (16000000UL / 32 / step_freq) - 1) <= 255) {
            ocr = temp;
            prescaler_bits = (1 << CS21) | (1 << CS20);
        } else if ((temp = (16000000UL / 64 / step_freq) - 1) <= 255) {
            ocr = temp;
            prescaler_bits = (1 << CS22);
        } else if ((temp = (16000000UL / 128 / step_freq) - 1) <= 255) {
            ocr = temp;
            prescaler_bits = (1 << CS22) | (1 << CS20);
        } else if ((temp = (16000000UL / 256 / step_freq) - 1) <= 255) {
            ocr = temp;
            prescaler_bits = (1 << CS22) | (1 << CS21);
        } else {
            ocr = 255;
            prescaler_bits = (1 << CS22) | (1 << CS21) | (1 << CS20); // 1024
        }

        TCCR2A = (1 << COM2B0) | (1 << WGM21); // Toggle OC2B, CTC
        TCCR2B = prescaler_bits;
        OCR2A = ocr;
        TIMSK2 |= (1 << OCIE2A);
    }
}

void StepperMotor::stopTimer() {
    if (timer_number == 1) {
        TCCR1B &= ~((1 << CS11) | (1 << CS10)); // dừng clock
        TIMSK1 &= ~(1 << OCIE1A);
        digitalWrite(step_pin, LOW);
    } else if (timer_number == 2) {
        TCCR2B = 0;
        TIMSK2 &= ~(1 << OCIE2A);
        digitalWrite(step_pin, LOW);
    }
}

void StepperMotor::moveSteps(uint32_t steps) {
    target_steps = steps;
    enable();
}

void StepperMotor::rotate(float rpm, uint32_t time_s) {
    setSpeedRPM(rpm);
    time_run = time_s;
    enable();
}
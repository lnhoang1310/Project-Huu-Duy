#include "Wire.h"
#include <LiquidCrystal_I2C.h>
#include "Step_Motor.h"

#define BUTTON_START   6
#define BUTTON_STOP    7
#define BUTTON_ENCODER 12

#define ENCODER_A 2
#define ENCODER_B 8

// Chân mới theo hardware toggle
#define PUL1  9   // Timer1 - OC1A
#define DIR1  11
#define ENA1  4

#define PUL2  3   // Timer2 - OC2B
#define DIR2  10
#define ENA2  9   // đổi để tránh trùng pin 9

#define BTN_NONE  0
#define BTN_CLICK 1
#define BTN_HOLD  2

#define DISTANCE_PER_ROUND 0.2
#define STEP_PER_ROUND     200
#define MAX_SPEED_RPM      300

LiquidCrystal_I2C lcd(0x27, 16, 2);

StepperMotor motor1(PUL1, DIR1, ENA1, 1); // Timer1
StepperMotor motor2(PUL2, DIR2, ENA2, 2); // Timer2

typedef enum { MOTOR_1, MOTOR_2, MENU } Display_Mode;
Display_Mode mode = MENU;

typedef enum { MENU_MOTOR_1, MENU_MOTOR_2 } Choosen_Menu;
Choosen_Menu choosen_menu = MENU_MOTOR_1;

typedef enum { MOTOR1_DISTANCE, MOTOR1_SAVE } Choosen_Motor_1;
Choosen_Motor_1 choosen_motor_1 = MOTOR1_SAVE;

typedef enum { MOTOR2_TIME, MOTOR2_SPEED, MOTOR2_SAVE } Choosen_Motor_2;
Choosen_Motor_2 choosen_motor_2 = MOTOR2_SAVE;

int8_t speed = 3;          // mặc định level 3/5
int8_t hour = 0, minute = 0, second = 10;
int16_t distance = 5;
bool is_button_encoder_hold = false;
bool stop_request = false;

uint8_t handleButton(uint8_t pin) {
  static uint32_t lastDebounce[20] = {0};
  static uint8_t lastState[20] = {HIGH};
  static uint32_t pressStart[20] = {0};
  static bool holdSent[20] = {false};

  uint32_t now = millis();
  uint8_t raw = digitalRead(pin);

  if (raw != lastState[pin]) {
    lastDebounce[pin] = now;
    lastState[pin] = raw;
  }
  if (now - lastDebounce[pin] < 30) return BTN_NONE;

  if (raw == LOW) {
    if (pressStart[pin] == 0) {
      pressStart[pin] = now;
      holdSent[pin] = false;
    }
    if (!holdSent[pin] && (now - pressStart[pin] >= 800)) {
      holdSent[pin] = true;
      return BTN_HOLD;
    }
  } else {
    if (pressStart[pin] != 0) {
      uint32_t duration = now - pressStart[pin];
      pressStart[pin] = 0;
      if (!holdSent[pin] && duration > 30 && duration < 800) return BTN_CLICK;
    }
  }
  return BTN_NONE;
}

uint8_t button_handle() {
  uint8_t btn_stop = handleButton(BUTTON_STOP);
  uint8_t btn_start = handleButton(BUTTON_START);
  uint8_t btn_enc = handleButton(BUTTON_ENCODER);

  if (btn_stop != BTN_NONE) {
    stop_request = true;
    return BUTTON_STOP;
  }
  if (btn_start != BTN_NONE) return BUTTON_START;
  if (btn_enc != BTN_NONE) {
    if (btn_enc == BTN_HOLD) is_button_encoder_hold = true;
    return BUTTON_ENCODER;
  }
  return 0;
}

// Encoder polling
int32_t encoder_count = 0;
static uint8_t last_A = HIGH;

void updateEncoderPolling() {
  uint8_t curr_A = digitalRead(ENCODER_A);
  if (last_A == HIGH && curr_A == LOW) { // cạnh xuống của A
    if (digitalRead(ENCODER_B) == HIGH) encoder_count++;
    else encoder_count--;
  }
  last_A = curr_A;
}

// Hiển thị
void display_welcome() {
  lcd.setCursor(0, 0); lcd.print("Huu Duy        ");
  lcd.setCursor(0, 1); lcd.print("May Khuay 2025 ");
}

void display_menu() {
  lcd.setCursor(1, 0); lcd.print("Motor 1       ");
  lcd.setCursor(1, 1); lcd.print("Motor 2       ");
  lcd.setCursor(0, choosen_menu == MENU_MOTOR_1 ? 0 : 1); lcd.print(">");
  lcd.setCursor(0, choosen_menu == MENU_MOTOR_1 ? 1 : 0); lcd.print(" ");
}

void display_detail_motor_1() {
  lcd.setCursor(0, 0); lcd.print("    Distance    ");
  lcd.setCursor(5, 1); lcd.print("     ");
  lcd.setCursor(5, 1); lcd.print(distance); lcd.print(" cm");
  lcd.setCursor(3, 1); lcd.print(choosen_motor_1 == MOTOR1_DISTANCE ? ">" : " ");
}

void display_detail_motor_2() {
  lcd.setCursor(1, 0); lcd.print("Speed: "); lcd.print(speed); lcd.print("   ");
  lcd.setCursor(1, 1); lcd.print("Time : ");
  lcd.print(hour < 10 ? "0" : ""); lcd.print(hour); lcd.print(":");
  lcd.print(minute < 10 ? "0" : ""); lcd.print(minute); lcd.print(":");
  lcd.print(second < 10 ? "0" : ""); lcd.print(second);

  if (choosen_motor_2 != MOTOR2_SAVE) {
    lcd.setCursor(0, choosen_motor_2 == MOTOR2_SPEED ? 0 : 1); lcd.print(">");
    lcd.setCursor(0, choosen_motor_2 == MOTOR2_SPEED ? 1 : 0); lcd.print(" ");
  }
}

void display_system() {
  static Display_Mode last_mode = (Display_Mode)-1;
  if (mode != last_mode) { lcd.clear(); last_mode = mode; }

  switch (mode) {
    case MENU: display_menu(); break;
    case MOTOR_1: display_detail_motor_1(); break;
    case MOTOR_2: display_detail_motor_2(); break;
  }
}


bool system_start = false;

void process_system() {
  uint8_t btn = button_handle();

  if (btn != 0) {
    if (btn == BUTTON_START) system_start = true;
    else if (btn == BUTTON_ENCODER) {
      if (is_button_encoder_hold) {
        mode = MENU;
        is_button_encoder_hold = false;
      } else {
        switch (mode) {
          case MENU:
            mode = (choosen_menu == MENU_MOTOR_1) ? MOTOR_1 : MOTOR_2;
            break;
          case MOTOR_1:
            if (choosen_motor_1 == MOTOR1_SAVE) choosen_motor_1 = MOTOR1_DISTANCE;
            else {
              motor1.distance = distance;
              choosen_motor_1 = MOTOR1_SAVE;
            }
            break;
          case MOTOR_2:
            if (choosen_motor_2 == MOTOR2_SPEED) {
              motor2.speed_rpm = MAX_SPEED_RPM * speed / 5.0f;
              choosen_motor_2 = MOTOR2_TIME;
            } else if (choosen_motor_2 == MOTOR2_TIME) {
              motor2.time_run = hour * 3600UL + minute * 60UL + second;
              choosen_motor_2 = MOTOR2_SAVE;
            } else choosen_motor_2 = MOTOR2_SPEED;
            break;
        }
      }
    }
    encoder_count = 0;
  }

  // Encoder với gia tốc nhẹ
  if (encoder_count != 0) {
    int32_t delta = encoder_count > 0 ? 1 : -1;
    int32_t abs_c = abs(encoder_count);
    if (abs_c >= 6) delta *= 5;
    else if (abs_c >= 3) delta *= 3;

    switch (mode) {
      case MENU:
        choosen_menu = (delta > 0) ? MENU_MOTOR_1 : MENU_MOTOR_2;
        break;
      case MOTOR_1:
        if (choosen_motor_1 == MOTOR1_DISTANCE)
          distance = constrain(distance + delta, 0, 20);
        break;
      case MOTOR_2:
        if (choosen_motor_2 == MOTOR2_SPEED) {
          speed = constrain(speed + (delta > 0 ? 1 : -1), 0, 5);
        } else if (choosen_motor_2 == MOTOR2_TIME) {
          int32_t total = hour * 3600L + minute * 60L + second + delta * 5;
          total = constrain(total, 0, 359999);
          hour   = total / 3600;
          minute = (total % 3600) / 60;
          second = total % 60;
        }
        break;
    }
    encoder_count = 0;
  }
}

void process_active() {
  static uint8_t step = 0;
  if (system_start) {
    if (step == 0) {
      motor1.setDirection(FORWARD);
      motor1.setSpeedRPM(200);
      motor1.moveSteps(motor1.distance * STEP_PER_ROUND / DISTANCE_PER_ROUND);
      step = 1;
    }
    if (step == 1 && motor1.state == INACTIVE) {
      motor2.rotate(motor2.speed_rpm, motor2.time_run);
      step = 2;
    }
    if (step == 2 && motor2.state == INACTIVE) {
      motor1.setDirection(BACKWARD);
      motor1.moveSteps(motor1.distance * STEP_PER_ROUND / DISTANCE_PER_ROUND);
      step = 3;
    }
    if (step == 3 && motor1.state == INACTIVE) {
      step = 0;
      system_start = false;
    }
  }
}

void setup() {
  pinMode(BUTTON_START, INPUT_PULLUP);
  pinMode(BUTTON_STOP, INPUT_PULLUP);
  pinMode(BUTTON_ENCODER, INPUT_PULLUP);
  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);

  Wire.begin();
  lcd.begin(16, 2);
  lcd.backlight();
  lcd.clear();
  display_welcome();
  delay(3000);
  lcd.clear();
  display_system();
}

void loop() {
  if (stop_request) {
    motor1.disable();
    motor2.disable();
    system_start = false;
    stop_request = false;
  }

  updateEncoderPolling();
  process_system();
  process_active();

  // Kiểm tra thời gian chạy của motor2 (rotate theo thời gian)
  if (motor2.state == ACTIVE && motor2.time_run > 0) {
    if (millis() - motor2.start_time >= motor2.time_run * 1000UL) {
      motor2.disable();
    }
  }

  display_system();
  delay(10);
}
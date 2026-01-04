#include "Wire.h"
#include <LiquidCrystal_I2C.h>
#include "Step_Motor.h"

#define BUTTON_START   6
#define BUTTON_STOP    7
#define BUTTON_ENCODER 12

#define ENCODER_A 2    // phaA
#define ENCODER_B 8    // phaB

// Chân motor theo hardware toggle
#define PUL1  9   // Timer1 - OC1A
#define DIR1  11
#define ENA1  4

#define PUL2  3   // Timer2 - OC2B
#define DIR2  10
#define ENA2  5   // Đổi lại để tránh trùng chân 9 nếu cần

#define BTN_NONE  0
#define BTN_CLICK 1
#define BTN_HOLD  2

#define DISTANCE_PER_ROUND 0.2
#define STEP_PER_ROUND     200
#define MAX_SPEED_RPM      300

LiquidCrystal_I2C lcd(0x27, 16, 2);

StepperMotor motor1(PUL1, DIR1, ENA1, 1);
StepperMotor motor2(PUL2, DIR2, ENA2, 2);

typedef enum { MENU, MOTOR_1, MOTOR_2 } Display_Mode;
Display_Mode mode = MENU;

typedef enum { MENU_MOTOR_1, MENU_MOTOR_2 } Choosen_Menu;
Choosen_Menu choosen_menu = MENU_MOTOR_1;

typedef enum { MOTOR1_DISTANCE, MOTOR1_SAVE } Choosen_Motor_1;
Choosen_Motor_1 choosen_motor_1 = MOTOR1_SAVE;

typedef enum { MOTOR2_TIME, MOTOR2_SPEED, MOTOR2_SAVE } Choosen_Motor_2;
Choosen_Motor_2 choosen_motor_2 = MOTOR2_SAVE;

int8_t speed_level = 3;     // 0..5
int8_t hour = 0, minute = 0, second = 10;
int16_t distance = 5;

bool is_button_encoder_hold = false;
bool stop_request = false;
bool system_start = false;

// Biến cho encoder (giống code bạn cung cấp)
uint8_t phaA_last = HIGH;

// Biến trạng thái menu
uint8_t demtong = 0;           // đếm tổng menu (0: màn hình chính, 1: menu tổng, 2: chọn motor, 3: vào chi tiết)
uint8_t demmenu1 = 0;
uint8_t demmenu2 = 0;
int8_t congtru_tong = 0;
int8_t congtru_menu1 = 0;
int8_t congtru_menu2 = 0;

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

// Hiển thị các màn hình
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
  lcd.setCursor(0, 0); lcd.print("Speed: "); lcd.print(speed_level); lcd.print("   ");
  lcd.setCursor(0, 1); lcd.print("Time : ");
  if (hour < 10) lcd.print("0"); lcd.print(hour); lcd.print(":");
  if (minute < 10) lcd.print("0"); lcd.print(minute); lcd.print(":");
  if (second < 10) lcd.print("0"); lcd.print(second);
}

void display_system() {
  static Display_Mode last_mode = (Display_Mode)-1;
  if (mode != last_mode) {
    lcd.clear();
    last_mode = mode;
  }

  switch (mode) {
    case MENU: display_menu(); break;
    case MOTOR_1: display_detail_motor_1(); break;
    case MOTOR_2: display_detail_motor_2(); break;
  }
}

void process_encoder() {
  uint8_t gtphaA = digitalRead(ENCODER_A);

  if ((phaA_last == LOW) && (gtphaA == HIGH)) {  // Phát hiện cạnh lên của phaA
    if (digitalRead(ENCODER_B) == LOW) {  // + xoay cùng chiều (tăng)
      if (mode == MENU) {
        choosen_menu = (choosen_menu == MENU_MOTOR_1) ? MENU_MOTOR_2 : MENU_MOTOR_1;
      }
      else if (mode == MOTOR_1 && choosen_motor_1 == MOTOR1_DISTANCE) {
        distance++;
        if (distance > 20) distance = 0;
      }
      else if (mode == MOTOR_2) {
        if (choosen_motor_2 == MOTOR2_SPEED) {
          speed_level++;
          if (speed_level > 5) speed_level = 0;
        }
        else if (choosen_motor_2 == MOTOR2_TIME) {
          second++;
          if (second >= 60) {
            second = 0;
            minute++;
            if (minute >= 60) {
              minute = 0;
              hour++;
            }
          }
        }
      }
    }
    else {  // - xoay ngược chiều (giảm)
      if (mode == MENU) {
        choosen_menu = (choosen_menu == MENU_MOTOR_1) ? MENU_MOTOR_2 : MENU_MOTOR_1;
      }
      else if (mode == MOTOR_1 && choosen_motor_1 == MOTOR1_DISTANCE) {
        distance--;
        if (distance < 0) distance = 20;
      }
      else if (mode == MOTOR_2) {
        if (choosen_motor_2 == MOTOR2_SPEED) {
          speed_level--;
          if (speed_level < 0) speed_level = 5;
        }
        else if (choosen_motor_2 == MOTOR2_TIME) {
          second--;
          if (second < 0) {
            second = 59;
            if (minute > 0) {
              minute--;
            } else if (hour > 0) {
              minute = 59;
              hour--;
            }
          }
        }
      }
    }
  }
  phaA_last = gtphaA;
}

void process_active() {
  static uint8_t step = 0;
  if (system_start) {
    if (step == 0) {
      motor1.setDirection(FORWARD);
      motor1.setSpeedRPM(200);
      motor1.moveSteps((uint32_t)(distance * STEP_PER_ROUND / DISTANCE_PER_ROUND));
      step = 1;
    }
    if (step == 1 && motor1.state == INACTIVE) {
      motor2.rotate(MAX_SPEED_RPM * speed_level / 5.0f, hour*3600UL + minute*60UL + second);
      step = 2;
    }
    if (step == 2 && motor2.state == INACTIVE) {
      motor1.setDirection(BACKWARD);
      motor1.moveSteps((uint32_t)(distance * STEP_PER_ROUND / DISTANCE_PER_ROUND));
      step = 3;
    }
    if (step == 3 && motor1.state == INACTIVE) {
      step = 0;
      system_start = false;
    }
  }
}

void setup() {
  pinMode(BUTTON_START,   INPUT_PULLUP);
  pinMode(BUTTON_STOP,    INPUT_PULLUP);
  pinMode(BUTTON_ENCODER, INPUT_PULLUP);
  pinMode(ENCODER_A,      INPUT_PULLUP);
  pinMode(ENCODER_B,      INPUT_PULLUP);

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

  // Xử lý encoder theo cách bạn cung cấp
  process_encoder();

  // Xử lý nút bấm
  uint8_t btn = button_handle();

  if (btn != 0) {
    if (btn == BUTTON_START) {
      system_start = true;
    }
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
            if (choosen_motor_1 == MOTOR1_SAVE) {
              choosen_motor_1 = MOTOR1_DISTANCE;
            } else {
              // Lưu distance nếu cần
              choosen_motor_1 = MOTOR1_SAVE;
            }
            break;
          case MOTOR_2:
            if (choosen_motor_2 == MOTOR2_SPEED) {
              choosen_motor_2 = MOTOR2_TIME;
            } else if (choosen_motor_2 == MOTOR2_TIME) {
              choosen_motor_2 = MOTOR2_SAVE;
            } else {
              choosen_motor_2 = MOTOR2_SPEED;
            }
            break;
        }
      }
    }
  }

  // Kiểm tra thời gian chạy motor2
  if (motor2.state == ACTIVE && motor2.time_run > 0) {
    if (millis() - motor2.start_time >= motor2.time_run * 1000UL) {
      motor2.disable();
    }
  }

  process_active();
  display_system();

  delay(5);  // Giảm delay để encoder nhạy hơn
}
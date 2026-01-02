#include "Wire.h"
#include <LiquidCrystal_I2C.h>
#include "Step_Motor.h"

#define BUTTON_START 6
#define BUTTON_STOP 7
#define BUTTON_ENCODER 12

#define ENCODER_A 2
#define ENCODER_B 8

#define ENA1 4
#define DIR1 11
#define PUL1 3

#define ENA2 9
#define DIR2 10
#define PUL2 5

#define BTN_NONE    0
#define BTN_CLICK   1
#define BTN_HOLD    2

#define DISTANCE_PER_ROUND 0.2
#define STEP_PER_ROUND 200
#define MAX_SPEED_RPM 300

LiquidCrystal_I2C lcd(0x27, 16, 2);

StepperMotor motor1(PUL1, DIR1, ENA1, 1); // timer1
StepperMotor motor2(PUL2, DIR2, ENA2, 2); // timer2

typedef enum{
  MOTOR_1,
  MOTOR_2,
  MENU
}Display_Mode;
Display_Mode mode = MENU;

typedef enum{
  MENU_MOTOR_1,
  MENU_MOTOR_2
}Choosen_Menu;
Choosen_Menu choosen_menu = MENU_MOTOR_1;

typedef enum{
  MOTOR1_DISTANCE,
  MOTOR1_SAVE
}Choosen_Motor_1;
Choosen_Motor_1 choosen_motor_1 = MOTOR1_SAVE;

typedef enum{
  MOTOR2_TIME,
  MOTOR2_SPEED,
  MOTOR2_SAVE
}Choosen_Motor_2;
Choosen_Motor_2 choosen_motor_2 = MOTOR2_SAVE;

int8_t speed;
int8_t hour, minute, second;
int16_t distance;
bool is_button_encoder_hold = false;
volatile bool stop_request = false;

uint8_t handleButton(uint8_t pin)
{
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

  if (now - lastDebounce[pin] < 30)
    return BTN_NONE;
  if (raw == LOW) {
    if (pressStart[pin] == 0) {
      pressStart[pin] = now;
      holdSent[pin] = false;
    }
    if (!holdSent[pin] && (now - pressStart[pin] >= 800)) {
      holdSent[pin] = true;
      return BTN_HOLD;
    }
  }else {
    if (pressStart[pin] != 0) {
      uint32_t duration = now - pressStart[pin];
      pressStart[pin] = 0;
      if (!holdSent[pin] && duration > 30 && duration < 800) 
        return BTN_CLICK;
    }
  }

  return BTN_NONE;
}


uint8_t button_handle(void){
  uint8_t button_start = handleButton(BUTTON_START);
  uint8_t button_encoder = handleButton(BUTTON_ENCODER);

  if(button_start != BTN_NONE) return BUTTON_START;
  if(button_encoder != BTN_NONE){
    if(button_encoder == BTN_HOLD) is_button_encoder_hold = true;
    return BUTTON_ENCODER;
  }
  return 0;
}

void display_welcome(void){
  lcd.setCursor(0, 0);
  lcd.print("Huu Duy");
  lcd.setCursor(0, 1);
  lcd.print("May Khuay 2025");
}

void display_menu(void){
  lcd.setCursor(1, 0); lcd.print("Motor 1");
  lcd.setCursor(1, 1); lcd.print("Motor 2");
  lcd.setCursor(0, (choosen_menu == MENU_MOTOR_1) ? 0 : 1); lcd.print(">");
  lcd.setCursor(0, (choosen_menu == MENU_MOTOR_1) ? 1 : 0); lcd.print(" ");
}

void display_detail_motor_1(void){
  lcd.setCursor(0, 0); lcd.print("    Distance    ");
  lcd.setCursor(6, 1); lcd.print(distance); lcd.print("cm");
  lcd.setCursor(4, 1); lcd.print((choosen_motor_1 == MOTOR1_DISTANCE) ? ">" : " ");
}

void display_detail_motor_2(void){
  lcd.setCursor(0, 0); lcd.print(" Speed: "); lcd.print(speed);
  lcd.setCursor(0, 1); lcd.print(" Time: ");
  if(hour < 10) lcd.print("0");
  lcd.print(hour); lcd.print(":");
  if(minute < 10) lcd.print("0");
  lcd.print(minute); lcd.print(":");
  if(second < 10) lcd.print("0");
  lcd.print(second);
  if(choosen_motor_2 == MOTOR2_SAVE) return;
  lcd.setCursor(0, (choosen_motor_2 == MOTOR2_SPEED) ? 0 : 1); lcd.print(">");
  lcd.setCursor(0, (choosen_motor_2 == MOTOR2_SPEED) ? 1 : 0); lcd.print(" ");
}

void display_system(){
  static Display_Mode last_mode = mode;

  if(mode != last_mode) lcd.clear();
  switch(mode){
    case MENU:
      display_menu();
      break;
    case MOTOR_1:
      display_detail_motor_1();
      break;
    case MOTOR_2:
      display_detail_motor_2();
      break;
  }
  last_mode = mode;
}


volatile int32_t encoder_count = 0;
bool system_start = false;
void updateEncoder(){
  if(digitalRead(ENCODER_B) == HIGH) encoder_count++;
  else encoder_count--;
}

void Stop_System(){
  stop_request = true;
}

void process_system(){
  uint8_t button = button_handle();
  if(button != 0){
    if(button == BUTTON_START){
      system_start = true;
    }else if(button == BUTTON_ENCODER){
      if(is_button_encoder_hold){
        mode = MENU;
        is_button_encoder_hold = false;
      }else{
        switch(mode){
          case MENU:
            if(choosen_menu == MENU_MOTOR_1) mode = MOTOR_1;
            else mode = MOTOR_2;
            break;
          case MOTOR_1:
            if(choosen_motor_1 == MOTOR1_SAVE) choosen_motor_1 = MOTOR1_DISTANCE;
            else if(choosen_motor_1 == MOTOR1_DISTANCE) {
              motor1.distance = distance;
              choosen_motor_1 = MOTOR1_SAVE;
            }
            break;
          case MOTOR_2:
            if(choosen_motor_2 == MOTOR2_SPEED){
              motor2.speed_rpm = MAX_SPEED_RPM * speed / 5.0f;
              choosen_motor_2 = MOTOR2_TIME;
            }else if(choosen_motor_2 == MOTOR2_TIME){
              motor2.time_run = hour*3600 + minute*60 + second;
              choosen_motor_2 = MOTOR2_SAVE;
            }else choosen_motor_2 = MOTOR2_SPEED;
            break;
        }
      }
    }
    encoder_count = 0;
  }
  if(encoder_count != 0){
    switch(mode){
      case MENU:
        if(encoder_count < 0) choosen_menu = MENU_MOTOR_2;
        if(encoder_count > 0) choosen_menu = MENU_MOTOR_1;
        encoder_count = 0;
        break;
      case MOTOR_1:
        if(choosen_motor_1 == MOTOR1_DISTANCE){
          if(encoder_count > 0){
            distance++;
            encoder_count--;
            if(distance > 20) distance = 20;
            break;
          }
          if(encoder_count < 0){
            distance--;
            encoder_count++;
            if(distance < 0) distance = 0;
            break;
          }
        }
        break;
      case MOTOR_2:
        if(choosen_motor_2 == MOTOR2_TIME){
          if(hour == 0 && minute == 0 && second == 0){
            if(encoder_count > 0) {
              second++;
              encoder_count--;
            }
            break;
          }
          if(encoder_count < 0){
            second--;
            encoder_count++;
            if(second == -1){
              second = 59;
              if(minute > 0){
                minute--;
            }else{
              hour--;
              minute = 59;
            }
            break;
          }

          if(encoder_count > 0){
            second++;
            encoder_count--;
            if(second == 60){
              second = 0;
              minute++;
              if(minute == 60){
                minute = 0;
                hour++;
              }
            }
            break;
          }
        }else if(choosen_motor_2 == MOTOR2_SPEED){
          if(encoder_count > 0){
            speed++;
            encoder_count--;
            if(speed > 5) speed = 5;
            break;
          }
          if(encoder_count < 0){
            speed--;
            encoder_count++;
            if(speed < 0) speed = 0;
            break;
          }
        }
        break;
      }

    }
  }
}

void process_active(){
  static uint8_t step = 0;
  if(system_start){
    if(step == 0){
      motor1.setDirection(FORWARD);
      motor1.setSpeedRPM(200);
      motor1.moveSteps(motor1.distance * STEP_PER_ROUND / DISTANCE_PER_ROUND);
      step = 1;
    }
    if(step == 1 && motor1.state == INACTIVE){
      motor2.rotate(motor2.speed_rpm, motor2.time_run);
      step = 2;
    }
    if(step == 2 && motor2.state == INACTIVE){
      motor1.setDirection(BACKWARD);
      motor1.moveSteps(motor1.distance * STEP_PER_ROUND / DISTANCE_PER_ROUND);
      step = 3;
    }
    if(step == 3 && motor1.state == INACTIVE){
      step = 0;
      system_start = false;
    }
  }
}

void setup() {
  pinMode(BUTTON_START, INPUT_PULLUP);
  pinMode(BUTTON_ENCODER, INPUT_PULLUP);
  pinMode(BUTTON_STOP, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BUTTON_STOP), Stop_System, FALLING);

  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(ENCODER_A), updateEncoder, RISING);

  Wire.begin();
  lcd.begin(16,2);
  lcd.backlight();
  lcd.clear();
  display_welcome();
  delay(5000);
  display_system();
}

void loop() {
  if(stop_request){
    motor1.disable();
    motor2.disable();
    system_start = false;
    stop_request = false;
  }
  process_system();
  process_active();
  display_system();
  delay(10);
}

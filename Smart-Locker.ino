#include <Keypad.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>

#define BUZZER 12
#define SERVO_PIN 10
#define BUTTON 11

LiquidCrystal_I2C lcd(0x27, 16, 2);

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
Servo myServo;

String password = "1234";
String input = "";
String newPass = "";
String confirmPass = "";

int attempts = 0;
const int maxAttempts = 3;

unsigned long lockoutStart = 0;
const unsigned long lockDuration = 60000; // 60 seconds
bool lockedOut = false;
unsigned long lastBuzzTime = 0;

bool lcdOn = false;
bool changePassStep1 = false; // entering old pass
bool changePassStep2 = false; // entering new pass
bool changePassStep3 = false; // confirming new pass

void setup() {
  lcd.init();
  lcd.noBacklight(); // LCD off initially
  myServo.attach(SERVO_PIN);
  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON, INPUT);
  myServo.write(0); // Locked
}

void loop() {
  // Inside pushbutton
  if (digitalRead(BUTTON) == HIGH) {
    stopLockout();
    openDoor();
  }

  // Lockout state
  if (lockedOut) {
    if (millis() - lockoutStart < lockDuration) {
      if (lcdOn) {
        lcd.setCursor(0, 0);
        lcd.print("SYSTEM LOCKED   ");
        lcd.setCursor(0, 1);
        lcd.print("Wait 60 sec...  ");
      }
      if (millis() - lastBuzzTime >= 1000) {
        tone(BUZZER, 500, 200);
        lastBuzzTime = millis();
      }
      return;
    } else {
      lockedOut = false;
      attempts = 0;
      if (lcdOn) {
        lcd.clear();
        lcd.print("Enter Password:");
      }
    }
  }

  char key = keypad.getKey();
  if (key) {
    // Turn LCD on with 'A'
    if (!lcdOn && key == 'A') {
      lcdOn = true;
      lcd.backlight();
      lcd.clear();
      lcd.print("Enter Password:");
      input = "";
      return;
    }

    if (!lcdOn) return; // Ignore keys until LCD on

    // Change password flow
    if (changePassStep1) { // Step 1: verify old pass
      handlePasswordEntry(key, password);
      return;
    }

    if (changePassStep2) { // Step 2: enter new pass
      handleNewPasswordEntry(key, newPass, true);
      return;
    }

    if (changePassStep3) { // Step 3: confirm new pass
      handleNewPasswordEntry(key, confirmPass, false);
      return;
    }

    // Normal password entry
    if (key == '#') { // Submit
      if (input == password) {
        accessGranted();
      } else {
        accessDenied();
      }
      input = "";
    } 
    else if (key == 'D') { // Delete last digit
      if (input.length() > 0) {
        input.remove(input.length() - 1);
        showMaskedInput();
      }
    }
    else if (key == '*') { // Clear
      input = "";
      showMaskedInput();
    }
    else if (key == 'C') { // Start change password
      changePassStep1 = true;
      lcd.clear();
      lcd.print("Old Password:");
      input = "";
    }
    else if (isDigit(key) && input.length() < 4) { // Digit entry
      input += key;
      showMaskedInput();
    }
  }
}

// ===== Functions =====

void showMaskedInput() {
  lcd.setCursor(0, 1);
  for (int i = 0; i < input.length(); i++) lcd.print("*");
  for (int i = input.length(); i < 4; i++) lcd.print(" ");
}

void handlePasswordEntry(char key, String correctPass) {
  if (isDigit(key) && input.length() < 4) {
    input += key;
    showMaskedInput();
  } else if (key == 'D') {
    if (input.length() > 0) input.remove(input.length() - 1);
    showMaskedInput();
  } else if (key == '#') {
    if (input == correctPass) {
      changePassStep1 = false;
      changePassStep2 = true;
      lcd.clear();
      lcd.print("New Password:");
      input = "";
    } else {
      changePassStep1 = false;
      lcd.clear();
      lcd.print("Wrong Old Pass");
      delay(1500);
      resetToMain();
    }
  }
}

void handleNewPasswordEntry(char key, String &target, bool isFirstEntry) {
  if (isDigit(key) && target.length() < 4) {
    target += key;
    lcd.setCursor(0, 1);
    for (int i = 0; i < target.length(); i++) lcd.print("*");
    for (int i = target.length(); i < 4; i++) lcd.print(" ");
  } 
  else if (key == 'D') {
    if (target.length() > 0) target.remove(target.length() - 1);
    lcd.setCursor(0, 1);
    for (int i = 0; i < target.length(); i++) lcd.print("*");
    for (int i = target.length(); i < 4; i++) lcd.print(" ");
  } 
  else if (key == '#') {
    if (isFirstEntry) {
      changePassStep2 = false;
      changePassStep3 = true;
      lcd.clear();
      lcd.print("Confirm Pass:");
    } else {
      if (confirmPass == newPass) {
        password = newPass;
        lcd.clear();
        lcd.print("Pass Changed!");
      } else {
        lcd.clear();
        lcd.print("Mismatch!");
      }
      delay(1500);
      changePassStep3 = false;
      resetToMain();
    }
  }
}

void accessGranted() {
  lcd.clear();
  lcd.print("Access Granted");
  openDoor();
  resetToMain();
}

void accessDenied() {
  lcd.clear();
  lcd.print("Wrong Password");
  tone(BUZZER, 150, 500);
  attempts++;
  delay(1000);
  if (attempts >= maxAttempts) {
    lcd.clear();
    lcd.print("LOCKED OUT!");
    lockoutStart = millis();
    lockedOut = true;
  } else {
    lcd.clear();
    lcd.print("Enter Password:");
  }
}

void openDoor() {
  noTone(BUZZER);
  myServo.write(90);
  delay(10000); // 10 sec
  myServo.write(0);
}

void stopLockout() {
  lockedOut = false;
  attempts = 0;
  noTone(BUZZER);
  if (lcdOn) {
    lcd.clear();
    lcd.print("Enter Password:");
  }
}

void resetToMain() {
  newPass = "";
  confirmPass = "";
  input = "";
  lcd.clear();
  lcd.print("Enter Password:");
}

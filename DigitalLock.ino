#include <Keypad.h>
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Relay configuration
const bool RELAY_ACTIVE_LOW = true;

// Keypad setup
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {2,3,4,5};
byte colPins[COLS] = {6,7,8,9};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

const int relayGreen = 11;
const int relayRed   = 12;
const int buzzerPin  = 10;

const int passwordLength = 4;
char mainPIN[passwordLength + 1];
char activationPIN[passwordLength + 1];
char input[passwordLength + 1];

int inputIndex = 0;

const unsigned long countdownTotalMs = 10000UL; // 10 seconds
const unsigned long beepIntervalMs   = 1000UL;

unsigned long countdownStart = 0;
unsigned long lastBeepTime   = 0;

bool disarmed = false;
bool exploded = false;
bool inPasswordChange = false;
bool inActivation = true;

// Relay helpers
void relayOn(int pin)  { digitalWrite(pin, RELAY_ACTIVE_LOW ? LOW : HIGH); }
void relayOff(int pin) { digitalWrite(pin, RELAY_ACTIVE_LOW ? HIGH : LOW); }

void setup() {
  pinMode(relayGreen, OUTPUT);
  pinMode(relayRed, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  relayOff(relayGreen);
  relayOff(relayRed);

  lcd.init();
  lcd.noBacklight();
  Serial.begin(9600);

  loadMainPIN();
  loadActivationPIN();

  showActivationScreen();
}

void loop() {
  char key = keypad.getKey();
  if (key) handleKey(key);

  if (!inActivation && !inPasswordChange)
    handleCountdown();
}

//----------------------------------------------------------
// ACTIVATION SYSTEM
//----------------------------------------------------------
void showActivationScreen() {
  inActivation = true;
  lcd.clear();
  lcd.noBacklight();
  inputIndex = 0;
  memset(input, 0, sizeof(input));

  lcd.setCursor(0,0);
  lcd.print("****");
}

void handleActivationInput(char key) {
  if (key == '*') {
    inputIndex = 0;
    memset(input, 0, sizeof(input));
    lcd.setCursor(0,0);
    lcd.print("****");
    return;
  }

  if (key >= '0' && key <= '9' && inputIndex < passwordLength) {
    input[inputIndex++] = key;

    lcd.setCursor(0,0);
    for (int i = 0; i < 4; i++)
      lcd.print(i < inputIndex ? input[i] : '*');
  }

  if (inputIndex == passwordLength) {
    input[inputIndex] = '\0';
    if (strcmp(input, activationPIN) == 0) {
      lcd.backlight();
      lcd.clear();
      lcd.print("SYSTEM ARMED");
      delay(1000);
      startMainSystem();
    } else {
      lcd.clear();
      lcd.backlight();
      lcd.print("Wrong Old PIN!");
      delay(1200);
      lcd.noBacklight();
      showActivationScreen();
    }
  }
}

//----------------------------------------------------------
// MAIN SYSTEM – AFTER ACTIVATION
//----------------------------------------------------------
void startMainSystem() {
  inActivation = false;
  disarmed = false;
  exploded = false;
  inputIndex = 0;
  memset(input, 0, sizeof(input));

  lcd.clear();
  lcd.backlight();
  lcd.print("Time Left: 10s");
  lcd.setCursor(0,1);
  lcd.print("****");

  countdownStart = millis();
  lastBeepTime = countdownStart - beepIntervalMs;
}

void handleMainPIN(char key) {
  if (key == '*') {
    inputIndex = 0;
    memset(input, 0, sizeof(input));
    lcd.setCursor(0,1);
    lcd.print("****");
    return;
  }

  if (key == 'A') { changeMainPIN(); return; }
  if (key == 'B') { changeActivationPIN(); return; }

  if (key >= '0' && key <= '9' && inputIndex < passwordLength) {
    input[inputIndex++] = key;

    lcd.setCursor(0,1);
    for (int i = 0; i < 4; i++)
      lcd.print(i < inputIndex ? input[i] : '*');
  }

  if (inputIndex == passwordLength) {
    input[inputIndex] = '\0';
    if (strcmp(input, mainPIN) == 0) disarmSystem();
    else explodeNow();
  }
}

void handleCountdown() {
  if (disarmed || exploded) return;

  unsigned long now = millis();

  if (now - lastBeepTime >= beepIntervalMs &&
     now - countdownStart < countdownTotalMs) {

    relayOn(relayRed);
    tone(buzzerPin, 1500);
    delay(150);
    noTone(buzzerPin);
    relayOff(relayRed);

    lastBeepTime = now;
  }

  long remainingMs = countdownTotalMs - (now - countdownStart);
  int sec = remainingMs > 0 ? ((remainingMs + 999) / 1000) : 0;

  lcd.setCursor(0,0);
  lcd.print("Time Left: ");
  lcd.print(sec);
  lcd.print("s  ");

  if (now - countdownStart >= countdownTotalMs)
    explodeNow();
}

//----------------------------------------------------------
// DISARM AND EXPLOSION
//----------------------------------------------------------
void disarmSystem() {
  disarmed = true;

  lcd.clear();
  lcd.print("DISARMED!");
  relayOn(relayGreen);

  tone(buzzerPin, 200, 800); // Half-frequency, short
  delay(800);

  noTone(buzzerPin);
  relayOff(relayGreen);

  systemReset();
}

void explodeNow() {
  exploded = true;

  lcd.clear();
  lcd.print("!!! EXPLODED !!!");
  relayOn(relayRed);

  tone(buzzerPin, 400, 3000);
  delay(3000);

  noTone(buzzerPin);
  relayOff(relayRed);

  systemReset();
}

//----------------------------------------------------------
// SYSTEM RESET
//----------------------------------------------------------
void systemReset() {
  disarmed = false;
  exploded = false;
  inputIndex = 0;
  memset(input, 0, sizeof(input));

  lcd.clear();
  lcd.print("SYSTEM RESET");
  delay(1500);

  lcd.noBacklight();
  showActivationScreen();
}

//----------------------------------------------------------
// PIN CHANGE FUNCTIONS
//----------------------------------------------------------
void changeMainPIN() {
  inPasswordChange = true;
  int attempts = 0;

  while (attempts < 3) {
    lcd.clear();
    lcd.print("Old PIN:");
    lcd.setCursor(0,1);
    inputIndex = 0;
    memset(input, 0, sizeof(input));

    while (inputIndex < 4) {
      char k = keypad.getKey();
      if (k >= '0' && k <= '9') { input[inputIndex++] = k; lcd.print('*'); }
    }
    input[inputIndex] = '\0';

    if (strcmp(input, mainPIN) == 0) break;

    attempts++;
    tone(buzzerPin, 1000, 200); delay(250);
    lcd.clear();
    lcd.print("Wrong Old PIN! ");
    lcd.print(attempts); 
    delay(1000);
  }

  if (attempts >= 3) { explodeNow(); return; }

  lcd.clear();
  lcd.print("New PIN:");
  lcd.setCursor(0,1);
  inputIndex = 0;
  memset(input, 0, sizeof(input));

  while (inputIndex < 4) {
    char k = keypad.getKey();
    if (k >= '0' && k <= '9') { input[inputIndex++] = k; lcd.print('*'); }
  }
  input[inputIndex] = '\0';

  saveMainPIN(input);
  strcpy(mainPIN, input);

  lcd.clear();
  lcd.print("UPDATED!");
  delay(1200);

  inPasswordChange = false;
  startMainSystem();
}

void changeActivationPIN() {
  inPasswordChange = true;
  int attempts = 0;

  while (attempts < 3) {
    lcd.clear();
    lcd.print("Old Arm PIN:");
    lcd.setCursor(0,1);
    inputIndex = 0;
    memset(input, 0, sizeof(input));

    while (inputIndex < 4) {
      char k = keypad.getKey();
      if (k >= '0' && k <= '9') { input[inputIndex++] = k; lcd.print('*'); }
    }
    input[inputIndex] = '\0';

    if (strcmp(input, activationPIN) == 0) break;

    attempts++;
    tone(buzzerPin, 1000, 200); delay(250);
    lcd.clear();
    lcd.print("Wrong Old PIN! ");
    lcd.print(attempts); 
    delay(1000);
  }

  if (attempts >= 3) { explodeNow(); return; }

  lcd.clear();
  lcd.print("New Arm PIN:");
  lcd.setCursor(0,1);
  inputIndex = 0;
  memset(input, 0, sizeof(input));

  while (inputIndex < 4) {
    char k = keypad.getKey();
    if (k >= '0' && k <= '9') { input[inputIndex++] = k; lcd.print('*'); }
  }
  input[inputIndex] = '\0';

  saveActivationPIN(input);
  strcpy(activationPIN, input);

  lcd.clear();
  lcd.print("UPDATED!");
  delay(1200);

  inPasswordChange = false;
  startMainSystem();
}

//----------------------------------------------------------
// EEPROM FUNCTIONS
//----------------------------------------------------------
void saveMainPIN(const char *p)       { for (int i=0;i<4;i++) EEPROM.write(i,p[i]); }
void loadMainPIN() {
  bool valid = true;
  for (int i=0;i<4;i++) {
    char c = EEPROM.read(i);
    if (c < '0' || c > '9') valid = false;
    mainPIN[i] = c;
  }
  mainPIN[4] = '\0';
  if (!valid) strcpy(mainPIN,"1234");
}

void saveActivationPIN(const char *p) { for (int i=0;i<4;i++) EEPROM.write(10+i,p[i]); }
void loadActivationPIN() {
  bool valid = true;
  for (int i=0;i<4;i++) {
    char c = EEPROM.read(10+i);
    if (c < '0' || c > '9') valid = false;
    activationPIN[i] = c;
  }
  activationPIN[4] = '\0';
  if (!valid) strcpy(activationPIN,"1234");
}

//----------------------------------------------------------
void handleKey(char key) {
  if (inActivation) handleActivationInput(key);
  else handleMainPIN(key);
}

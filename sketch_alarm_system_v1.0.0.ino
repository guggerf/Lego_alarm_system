
//Alarm System by guggerf
//Als Semasterarbeit in der TEKO Bern
//5. Semester Elektrotechniker HF

//******************************************************************************
//          Benötigte Libraries Initialisieren
//******************************************************************************
#include <Wire.h>               //I2C Kommunikation Seriell über SDA/SCL
#include <LiquidCrystal_I2C.h>  //I2C ISnterface für LCD
#include <Keypad.h>             //Library für das Keypad
#include <Password.h>           //Library für Passwort-Management

//******************************************************************************
//                 Konstanten
//******************************************************************************

#define OFF 0
#define ON 1

// Keypad-Konfiguration
const byte ROWS = 4; //4 Zeilen
const byte COLS = 3; //3 Spalten
char KEYS[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
byte ROW_PINS[ROWS] = {5, 6, 7, 8}; //Anschlüsse der Keypad-Zeilen
byte COL_PINS[COLS] = {2, 3, 4};    //Anschlüsse der Keypad-Spalten

// LCD eigene Charakter
byte LCD_CHARS[6][8] = {
  { B00000, B00000, B00000, B00000, B00000, B00000, B00000, B00000 },
  { B10000, B10000, B10000, B10000, B10000, B10000, B10000, B10000 },
  { B11000, B11000, B11000, B11000, B11000, B11000, B11000, B11000 },
  { B11100, B11100, B11100, B11100, B11100, B11100, B11100, B11100 },
  { B11110, B11110, B11110, B11110, B11110, B11110, B11110, B11110 },
  { B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111 }
};

// Für Piezobuzzer verschiedene Frequenzen definieren
const int NUM_MELODIES = 2;
const int NUM_NOTES = 32;
int NOTES[NUM_MELODIES][NUM_NOTES] = {
  { 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200 },
  { 200, 220, 240, 260, 280, 300, 320, 340, 360, 380, 400, 420, 440, 460, 480, 500, 520, 500, 480, 460, 440, 420, 400, 380, 360, 340, 320, 300, 280, 260, 240, 220 }
};

// Pins an Arduino definieren
static const int PIN_LED_GREEN 			= 22; //LED Anlage ausgeschaltet
static const int PIN_LED_RED 			= 24; //LED Anlage eingeschaltet
static const int PIN_LED_RED_0 			= 26; //RoteLED in Zentrale
static const int PIN_LED_RED_1 			= 28; //RoteLED in Eingang
static const int PIN_LED_RED_2 			= 30; //RoteLED in Wohnen
static const int PIN_PIR 			= 32; //In Wohnen
static const int PIN_REED_1 			= 34; //in Eingang
static const int PIN_SPEAKER 			= 36; //Alarm Speaker
static const int PIN_TEMP_SENSOR 		= A0; // Analoger Eingang für Temperatur in Zentrale

// Sonstige Konstanten für Programmablauf
static const int ROOM_NO_ALARM                  = 0;
static const int ROOM_MAIN_OFFICE 		= 0x1;
static const int ROOM_ENTRANCE			= 0x2;
static const int ROOM_LIVINGROOM		= 0x4;

static const int SWAP_LED                       = 0;
static const int SWAP_TONE                      = 1;

static const int LCD_LINE_LENGTH                = 16;
static const int DEFAULT_LED_DELAY		= 200;
static const int DEFAULT_TONE_DELAY             = 30;
static const int DEFAULT_BLINK_REPETITIONS      = 5;
static const int DEFAULT_PASSWORD_POS 	        = 10; //wo wird begonnen mit PIN eingaben
static const float NORMAL_TEMPERATURE 	        = 31.0; //Basis Temperatur für Temp überwachung in Zentrale
static const int ALARM_DISPLAY_DURATION         = 3000;

//******************************************************************************
//                 Variablen
//******************************************************************************

// Das I2C LCD interface initialisieren.
// Das LCD wird über den I2C Bus gesteuert und hängt an den Anschlüssen 20(SDA) und 21(SCL) am Mega
LiquidCrystal_I2C lcd(0x27, LCD_LINE_LENGTH, 2);  // 0x27 = Adresse (ohne Lötjumper), 16 Charakter, 2 Linien

//Hilfsvariablen um Cursor auf LCD zu setzen
int cursorCurrentX = 0;
int cursorCurrentY = 1;

Keypad keypad = Keypad( makeKeymap(KEYS), ROW_PINS, COL_PINS, ROWS, COLS );

// Initialieren des Passwortes, dient als Default Passwort, wenn Arduino zurückgesetzt wird
Password password = Password("1234");

int currentPasswordPosition = DEFAULT_PASSWORD_POS; // Wo wird Pasword angezeigt

//Diverse Variablen definieren
int alarmStatus = OFF; //1= Alarm ausgelöst
int alarmActive = OFF; //1= Alarmanlage scharf
int alarmDetected = OFF;
int alarmShown = OFF;
int alarmPinShown = OFF;
int alarmTemperature = OFF;

//Alarm-Status
unsigned long alarmTimestamp = 0;

//Non-Blocking Swapping
unsigned long swapLedTimestamp = 0;
unsigned long swapToneTimestamp = 0;
int swapStatus = OFF;

//Aktueller Ton
int currentMelody = 0;
int currentTone = 0;

//******************************************************************************
//                                    SETUP
//******************************************************************************
// Das Setup wird nur aufgerufen wenn Arduino startet oder bei einem Reset
void setup() {
  Serial.begin(9600); // Starten des Serial Monitors für Debug-Zwecke

  lcd.init(); // LCD initialisieren
  lcd.backlight();

  //Inputs definieren
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_REED_1, INPUT);

  //Outputs definieren
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_RED_0, OUTPUT);
  pinMode(PIN_LED_RED_1, OUTPUT);
  pinMode(PIN_LED_RED_2, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);

  //Setzten der Ausgangszustände bei Start und Reset
  digitalWrite(PIN_PIR, LOW);
  digitalWrite(PIN_REED_1, HIGH);

  // am Anfang ist alles aus
  showStatusOff();

  keypad.addEventListener(keypadEvent); //Eventlistener für das Keypad hinzufügen

  //Boot Vorgang
  bootScreen();
  ledBlink(1);
  startScreen();
}
//******************************************************************************
void bootScreen() {
  showOnScreen("****Welcome**** ", "**Alarm-System**");
  ledBlink(DEFAULT_BLINK_REPETITIONS);

  showOnScreen("*System booting*");
  // Loarding-Bar (booting time für Sensoren kalibrieren)
  initCustomCharacters();
  for (int j = 0; j < 16; j++) { //16 Charakter füllen
    for (int i = 0; i < 5; i++) { // 1 Charakter mit je 5 Spalten füllen
      lcd.setCursor(cursorCurrentX, cursorCurrentY);
      lcd.write(i);
      delay(100); //Geschwindikeit hier einstellen
    }
    cursorCurrentX++;
    lcd.setCursor(j, cursorCurrentY);
    delay(10);
  }
}
//******************************************************************************
void startScreen() {
  String systemStatus = String("Anlage: ");

  if (alarmActive) {
    systemStatus += "EIN";
    showStatusGreen();
  } else {
    systemStatus += "AUS";
    showStatusOff();
  }
  showOnScreen(systemStatus, "PIN:");

  // blinkenden Cursor setzen
  lcd.setCursor(10, 1);
  lcd.blink(); // Cursor anzeigen wo PW eingengeben werden kann
}
//******************************************************************************
//                                    LOOP
//******************************************************************************
//Der Loop wiederholt sich immer und immer wieder, für immer
void loop() {
  char key = keypad.getKey();
  if (alarmActive) {
    int room = alarmCheck();

    // gibt es Alarm?
    if (room > ROOM_NO_ALARM) {
      if (!alarmDetected) {
        // wir sind neu hier!
        alarmTimestamp = millis();
        alarmDetected = ON;
      }
      // Alarm anzeigen
      alarmTriggered(room);
    } else if (alarmStatus) {
      alarmTriggered(room);
    }
  }
}
//******************************************************************************
// Wenn Alarm ausgelöst wurde
void alarmTriggered(int room) {
  alarmStatus = ON;

  String location = "";

  if (room & ROOM_MAIN_OFFICE) {
    digitalWrite(PIN_LED_RED_0, HIGH);
    location = String("in Zentrale: ") + alarmTemperature;
  }
  if (room & ROOM_ENTRANCE) {
    digitalWrite(PIN_LED_RED_1, HIGH);
    location = "in Eingang";
  }
  if (room & ROOM_LIVINGROOM) {
    digitalWrite(PIN_LED_RED_2, HIGH);
    location = "in Wohnzimmer";
  }
  checkSwap();
  // wird der Alarm schon lange angezeigt?
  if (millis() - alarmTimestamp < ALARM_DISPLAY_DURATION) {
    if (!alarmShown) {
      showOnScreen("ALARM AUSGELOEST", location);
      alarmShown = ON;
    }
  } else if (!alarmPinShown) {
    showOnScreen("PIN eingeben", "PIN:");

    // blinkenden Cursor setzen
    lcd.setCursor(10, 1);
    lcd.blink(); // Cursor anzeigen wo PW eingengeben werden kann

    // Display beim nächsten Mal nicht überschreiben
    alarmPinShown = ON;
  }
}
//******************************************************************************
void keypadEvent(KeypadEvent eKey) {
  if (keypad.getState() == PRESSED) {
    if (currentPasswordPosition - 10 >= 5) {
      password.reset();
      lcd.setCursor(0, 1);
      lcd.println("nur 4 Zeichen         ");
      ledBlink(DEFAULT_BLINK_REPETITIONS);
      lcd.setCursor(0, 1);
      lcd.println("PIN:                ");
      currentPasswordPosition  = 10;
      return;
    }
    lcd.setCursor((currentPasswordPosition++), 1);
    switch (eKey) {
      case '#':                 //# zu Passwort überprüfen
        currentPasswordPosition  = 10;
        checkPassword();
        break;
      case '*':                 //* Eingabe löschen
        password.reset();
        currentPasswordPosition = 10;
        lcd.setCursor(0, 1);
        lcd.println("PIN:                ");
        break;
      case '0':
        currentMelody = (++currentMelody)%NUM_MELODIES;
        // NO break, we want to fall through
      default:
        lcd.blink();
        password.append(eKey);
        lcd.print("*");
    }
  }
}
//******************************************************************************
// Ist das Passwort richtig? wenn nicht dann führe invalideCode() aus.
void checkPassword() {
  if (password.evaluate()) {
    if (!alarmActive && !alarmStatus) {
      activate();
    }
    else if (alarmActive || alarmStatus) {
      deactivate();
    }
  } else {
    invalidCode();
  }

  // alles zurücksetzen, blinken
  password.reset();
  ledBlink(DEFAULT_BLINK_REPETITIONS);
  startScreen();
}
//******************************************************************************
// Wenn das Passwort falsch eingegeben wurde.
void invalidCode() {
  showOnScreen("INVALID CODE!", "TRY AGAIN!");
}
//******************************************************************************
// System scharf stellen und auf Screen anzeigen
void activate() {
  if (!doorOpen()) {
    showOnScreen("****SYSTEM*****", "***AKTIVIERT***");
    alarmActive = ON;
  } else {
    showOnScreen("****ACHTUNG****", "Tuer offen");
    ledBlink(1);
  }
}
//******************************************************************************
// System deaktivieren und Start Screen anzeigen
void deactivate() {
  alarmStatus = OFF;
  alarmActive = OFF;
  alarmDetected = OFF;
  alarmShown = OFF;
  alarmPinShown = OFF;
  alarmTimestamp = OFF;
  alarmTemperature = 0;

  showOnScreen("**SYSTEM**", "*DEAKTIVIERT*");
  lcd.noBlink();
  noTone(PIN_SPEAKER);
  disableAlarmLeds();
}
//******************************************************************************
// Temp wird überwacht und bei hoher Temp wird Alarm ausgelöst
//Folgende Daten hab ich aus dem Starter Kit übernommen und auf mein Projekt angepasst
int alarmCheck() {
  int room = ROOM_NO_ALARM;

  // Türkontakt
  if (doorOpen()) {
    room |= ROOM_ENTRANCE;
  }

  // Bewegungsmelder
  if (digitalRead(PIN_PIR) == HIGH) {
    room |= ROOM_LIVINGROOM;
  }

  // Temperatur
  int sensorVar = analogRead(PIN_TEMP_SENSOR);

  //Debug
  Serial.print(String("Sensor Value: ") + sensorVar);

  //Gelesener Wert in Spannung umwandeln
  float voltage = (sensorVar / 1024.0) * 5.0;

  // Debug
  Serial.print(String(", Volts: ") + voltage);

  // Spannung in Temperatur umwandlen
  float temperature = (voltage - .5) * 100;

  //Debug
  Serial.println(String(", degrees C: ") + temperature);

  if (temperature > NORMAL_TEMPERATURE) {
    alarmTemperature = (int)temperature;
    room |= ROOM_MAIN_OFFICE;
  }

  return room;
}
//******************************************************************************
// Hilfsfunktionen
//******************************************************************************
void showStatusGreen() {
  digitalWrite(PIN_LED_GREEN, HIGH);
  digitalWrite(PIN_LED_RED, LOW);
}

void showStatusOff() {
  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_LED_GREEN, LOW);
}

void showStatusRed() {
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_RED, HIGH);
}

void disableAlarmLeds() {
  digitalWrite(PIN_LED_RED_0, LOW);
  digitalWrite(PIN_LED_RED_1, LOW);
  digitalWrite(PIN_LED_RED_2, LOW);
}

void swap(int what) {
  if (what == SWAP_TONE) {
    tone(PIN_SPEAKER, NOTES[currentMelody][currentTone]);
    currentTone = (++currentTone)%NUM_NOTES;
  } else if (what == SWAP_LED) {
    if (swapStatus) {
      showStatusRed();
      swapStatus = OFF;
    } else {
      showStatusGreen();
      swapStatus = ON;
    }
  }
}

void checkSwap() {
  if (millis() - swapLedTimestamp > DEFAULT_LED_DELAY) {
    swap(SWAP_LED);
    swapLedTimestamp = millis();
  }
  if (millis() - swapToneTimestamp > DEFAULT_TONE_DELAY) {
    swap(SWAP_TONE);
    swapToneTimestamp = millis();
  }
}

int doorOpen() {
  return digitalRead(PIN_REED_1) == LOW;
}

// Funktion für LED blinken (Strobbo)
void ledBlink(int numLoops) {
  for (int i = 0; i < numLoops; i++) {
    showStatusGreen();
    delay(DEFAULT_LED_DELAY);
    showStatusRed();
    delay(DEFAULT_LED_DELAY);
  }
  showStatusOff();
}

// Funktion für Custom Char Loading-Bar, Zeichen auffüllen von links nach rechts
void initCustomCharacters() {
  //wichtig, alle custom char zuerst zurücksetzten!!
  for (int i = 0; i < 8; i++) {
    lcd.createChar(i, LCD_CHARS[0]);
  }
  //die custom Char generieren und nummerieren (Nummer ist wichtig zum aufrufen)
  for (int i = 0; i < 5; i++) {
    lcd.createChar(i, LCD_CHARS[i+1]);
  }
}

String pad(String input, int len) {
  if (input.length() < len) {
    for (int i = input.length(); i <= len; i++) {
      input += " ";
    }
  }
  return input;
}

// zwei Zeilen auf dem Display ausgeben
void showOnScreen(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.println(pad(line1, LCD_LINE_LENGTH));
  lcd.setCursor(0, 1);
  lcd.println(pad(line2, LCD_LINE_LENGTH));
}

// eine Zeile auf dem Display ausgeben
void showOnScreen(String line1) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.println(pad(line1, LCD_LINE_LENGTH));
}

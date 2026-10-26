
#include <LiquidCrystal.h>
#include <IRremote.hpp>
#include <Wire.h>

// ---------------- LCD ----------------

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// ---------------- BUZZER ----------------

const int buzzer = 6;

// ---------------- JOYSTICK ----------------

const int VRx = A3;
const int VRy = A2;
const int SW = A1;

// ---------------- IR RECEIVER ----------------

const int IR_PIN = A0;

// ---------------- LEDs ----------------

const int redLED1 = 8;
const int redLED2 = 9;
const int redLED3 = 10;
const int greenLED = LED_BUILTIN;

// ---------------- JOYSTICK THRESHOLDS ----------------

const int lowThreshold = 490;
const int highThreshold = 520;

// ---------------- I2C COMMUNICATION ----------------

const byte I2C_ADDRESS = 8;

volatile byte receivedNumber = 0;
volatile bool newData = false;

// ---------------- TIMING ----------------

unsigned long currentMillis = 0;
unsigned long buzzerMillis = 0;
unsigned long joystickMillis = 0;
unsigned long ledMillis = 0;
unsigned long irMillis = 0;
unsigned long receivedMillis = 0;

const unsigned long joystickInterval = 70;
const unsigned long ledInterval = 300;
const unsigned long irDisplayInterval = 1500;
const unsigned long receivedDisplayInterval = 1500;

// ---------------- BEEP TIMING ----------------

const unsigned long beepDuration = 100;
const unsigned long beepPause = 150;

// ---------------- SOFTWARE BUZZER ----------------

unsigned long previousToggleMicros = 0;
unsigned long halfPeriodMicros = 0;
bool buzzerState = LOW;

// ---------------- BEEP SEQUENCE ----------------

int beepsRemaining = 0;
bool beepPlaying = false;
bool beepSequenceActive = false;

// ---------------- OTHER VARIABLES ----------------

int ledStep = 0;

bool showingIR = false;
bool showingReceived = false;

String previousDirection = "";

// ---------------- I2C RECEIVE CALLBACK ----------------

void receiveEvent(int bytesReceived) {

    if (Wire.available()) {
        receivedNumber = Wire.read();
        newData = true;
    }

    while (Wire.available()) {
        Wire.read();
    }
}

// ---------------- I2C REQUEST CALLBACK ----------------

void requestEvent() {
    Wire.write(75);
}

// ---------------- SETUP ----------------

void setup() {

    Serial.begin(9600);

    // LCD
    lcd.begin(16, 2);
    lcd.setCursor(0, 0);
    lcd.print("Deep Blue Dive");
    lcd.setCursor(1, 1);
    lcd.print("CENTER");

    // Buzzer
    pinMode(buzzer, OUTPUT);
    digitalWrite(buzzer, LOW);

    // Joystick
    pinMode(SW, INPUT_PULLUP);

    // LEDs
    pinMode(redLED1, OUTPUT);
    pinMode(redLED2, OUTPUT);
    pinMode(redLED3, OUTPUT);
    pinMode(greenLED, OUTPUT);

    digitalWrite(redLED1, LOW);
    digitalWrite(redLED2, LOW);
    digitalWrite(redLED3, LOW);
    digitalWrite(greenLED, LOW);

    // IR receiver
    IrReceiver.begin(IR_PIN, DISABLE_LED_FEEDBACK);

    // Buzzer initially silent
    sound(0);

    // I2C peripheral
    Wire.begin(I2C_ADDRESS);
    Wire.onReceive(receiveEvent);
    Wire.onRequest(requestEvent);

    Serial.println("Deep Blue Dive");
    Serial.println("IR Receiver Ready");
    Serial.println("Arduino 2 - Peripheral Ready");
}

// ---------------- LOOP ----------------

void loop() {

    currentMillis = millis();

    // Process received I2C data
    handle_com();

    // Software buzzer
    update_buzzer();
    handle_buzzer();

    // Joystick
    if (currentMillis - joystickMillis >= joystickInterval) {
        joystickMillis = currentMillis;
        handle_joystick();
    }

    // LEDs
    if (currentMillis - ledMillis >= ledInterval) {
        ledMillis = currentMillis;
        handle_LEDs();
    }

    // IR receiver
    handle_IR();

    // Return LCD to joystick display
    if (showingIR &&
        currentMillis - irMillis >= irDisplayInterval) {
        showingIR = false;
        previousDirection = "";
    }

    if (showingReceived &&
        currentMillis - receivedMillis >= receivedDisplayInterval) {
        showingReceived = false;
        previousDirection = "";
    }
}

// ---------------- I2C COMMUNICATION ----------------

void handle_com() {

    if (!newData) {
        return;
    }

    byte number;

    noInterrupts();
    number = receivedNumber;
    newData = false;
    interrupts();

    Serial.print("Received digit: ");
    Serial.println(number);

    lcd.setCursor(0, 1);
    lcd.print("                ");

    lcd.setCursor(0, 1);
    lcd.print("Number: ");
    lcd.print(number);

    showingReceived = true;
    receivedMillis = currentMillis;

    // Beep the number received from Arduino 1
    if (number <= 9) {
        start_beeping(number);
    }
}

// ---------------- BUZZER ----------------

void sound(int frequency) {

    if (frequency <= 0) {
        halfPeriodMicros = 0;
        buzzerState = LOW;
        digitalWrite(buzzer, LOW);
        return;
    }

    halfPeriodMicros = 500000UL / frequency;
    previousToggleMicros = micros();
}

void update_buzzer() {

    if (halfPeriodMicros == 0) {
        return;
    }

    unsigned long now = micros();

    if (now - previousToggleMicros >= halfPeriodMicros) {
        previousToggleMicros = now;
        buzzerState = !buzzerState;
        digitalWrite(buzzer, buzzerState);
    }
}

void start_beeping(int count) {

    sound(0);

    beepsRemaining = count;
    beepPlaying = false;
    beepSequenceActive = (count > 0);

    buzzerMillis = millis();
}

void handle_buzzer() {

    if (!beepSequenceActive) {
        return;
    }

    if (beepPlaying) {

        if (currentMillis - buzzerMillis >= beepDuration) {
            sound(0);
            beepPlaying = false;
            buzzerMillis = currentMillis;
            beepsRemaining--;

            if (beepsRemaining <= 0) {
                beepSequenceActive = false;
            }
        }
    }
    else {

        if (currentMillis - buzzerMillis >= beepPause) {
            sound(500); // Quiet test frequency
            beepPlaying = true;
            buzzerMillis = currentMillis;
        }
    }
}

// ---------------- JOYSTICK ----------------

void handle_joystick() {

    int x = analogRead(VRx);
    int y = analogRead(VRy);

    const char* direction = "CENTER";

    if (x > highThreshold) {
        direction = "UP";
    }
    else if (x < lowThreshold) {
        direction = "DOWN";
    }
    else if (y > highThreshold) {
        direction = "RIGHT";
    }
    else if (y < lowThreshold) {
        direction = "LEFT";
    }

    // Prioritize received messages and IR
    if (showingIR || showingReceived) {
        return;
    }

    if (previousDirection != direction) {
        lcd.setCursor(0, 1);
        lcd.print("                ");

        lcd.setCursor(1, 1);
        lcd.print(direction);

        previousDirection = direction;
    }
}

// ---------------- LEDs ----------------

void handle_LEDs() {

    digitalWrite(redLED1, LOW);
    digitalWrite(redLED2, LOW);
    digitalWrite(redLED3, LOW);
    digitalWrite(greenLED, LOW);

    ledStep = (ledStep + 1) % 4;

    if (ledStep == 0) {
        digitalWrite(redLED1, HIGH);
    }
    else if (ledStep == 1) {
        digitalWrite(redLED2, HIGH);
    }
    else if (ledStep == 2) {
        digitalWrite(redLED3, HIGH);
    }
    else {
        digitalWrite(greenLED, HIGH);
    }
}

// ---------------- IR COMMAND TO DIGIT ----------------

int commandToDigit(uint16_t command) {

    switch (command) {
        case 22: return 0;
        case 12: return 1;
        case 24: return 2;
        case 94: return 3;
        case 8:  return 4;
        case 28: return 5;
        case 90: return 6;
        case 66: return 7;
        case 82: return 8;
        case 74: return 9;
        default: return -1;
    }
}

// ---------------- IR RECEIVER ----------------

void handle_IR() {

    if (IrReceiver.decode()) {

        if (IrReceiver.decodedIRData.protocol != UNKNOWN &&
            !(IrReceiver.decodedIRData.flags &
              IRDATA_FLAGS_IS_REPEAT)) {

            uint16_t command = IrReceiver.decodedIRData.command;
            int digit = commandToDigit(command);

            Serial.print("IR Command: ");
            Serial.println(command);

            if (digit >= 0) {

                Serial.print("Number: ");
                Serial.println(digit);

                lcd.setCursor(0, 1);
                lcd.print("                ");

                lcd.setCursor(0, 1);
                lcd.print("Number: ");
                lcd.print(digit);

                showingIR = true;
                irMillis = currentMillis;

                // Local IR remote still works
                start_beeping(digit);
            }
        }

        IrReceiver.resume();
    }
}

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <LiquidCrystal.h>

// Create a RF24 object called radio and set the CE and CSN pins
RF24 radio(9, 10); // (CE, CSN)

// Address for radio communication
const uint8_t address[6] = {"12345"};

// Timer
unsigned long lastTime = 0;
unsigned long currentTime = 0;

// Joysticks
const uint8_t inputX = A0;
const uint8_t inputY = A1;

// Buttons
const uint8_t leftButton = A3;
const uint8_t rightButton = A2;
bool leftState = 0;
bool rightState = 0;
bool lastLeftState = 0;
bool lastRightState = 0;

// Display
const uint8_t rs = 7, en = 6, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7); // Here we set the pins for the I2C communication

uint8_t displayNum = 0;
const uint8_t displayAmount = 5;
String displayName;
String displayValue;

// NRF24L01 has a buffer limit of 32 bytes
// Create two structs which can hold the data that is being sent and received
struct Data_Controller {
  uint8_t joystickX;
  uint8_t joystickY;
};
Data_Controller dataController;

struct Data_Car {
  uint8_t flameState;
  uint8_t soundState;
  uint8_t LDRValue;
  int8_t temperatureValue;
  uint8_t gasValue;
};
Data_Car dataCar;

void setup() {
  // Begin the communication with the LCD screen, here we specify the screen size which is 16x2
  lcd.begin(16, 2);

  radio.begin(); // Begin radio communication
  radio.enableAckPayload(); // Enable the slave to sent data back
  radio.openWritingPipe(address); // Open the writing pipe to the chosen address
  // There are 6 pipes
  // The writing pipe is by default 0

  // Set default values for the joystick
  dataController.joystickX = 127;
  dataController.joystickY = 127;

  // Send the data contained in dataController to the NRF24L01 module
  // The & operator is getting the address of the struct
  // while the function sizeof returns the size of the struct, so that the payload size can be set
  radio.write(&dataController, sizeof(Data_Controller));
}

void loop() {
  // Read joystick inputs
  dataController.joystickX = map(analogRead(inputX), 0, 1023, 0, 255);
  dataController.joystickY = map(analogRead(inputY), 0, 1023, 0, 255);

  // Send data
  radio.write(&dataController, sizeof(Data_Controller));
  
  if (radio.isAckPayloadAvailable()) { // If a reply by the slave has been received
    radio.read(&dataCar, sizeof(Data_Car)); // Read the reply and update the struct according to the received data
  }

  button();
  displayShow();
}

// This function is used to control what data the LCD screen shows
// A simple switch case which value is changed by pressing a button
void displayShow() {
  switch (displayNum) {
  case 0:
    displayName = "Flame";
    displayValue = String(dataCar.flameState);
    break;
  case 1:
    displayName = "Sound";
    displayValue = String(dataCar.soundState);
    break;
  case 2:
    displayName = "LDR";
    displayValue = String(dataCar.LDRValue);
    break;
  case 3:
    displayName = "Temperature";
    displayValue = String(dataCar.temperatureValue);
    break;
  case 4:
    displayName = "Gas";
    displayValue = String(dataCar.gasValue);
    break;
  default:
    displayName = "Error";
    displayValue = "0";
  }

  // To make the LCD screen not update every step
  // a timer is made to set a custom refresh rate
  currentTime = millis();
  if (currentTime - lastTime > 200) {
    lastTime = millis(); // Reset the timer
    lcd.clear(); // First clear the screen
    lcd.setCursor(0, 0); // Then move the cursor to the start
    lcd.print(displayName); // Here the value name is printed
    lcd.setCursor(0, 1); // The cursor moves down
    lcd.print(displayValue); // and then the value is printed
  }
}

// Function to control the button presses
void button() {
  // Only detects a button press, if the button has been released since last update
  leftState = digitalRead(leftButton);
  // Check if the current state of the button is the same as the last state
  if (leftState != lastLeftState) {
    // If it is not the same the value is flipped
    lastLeftState = !lastLeftState;
    if (leftState == HIGH) { // If it is a button press
      displayNum = (((displayNum-1) % displayAmount) + displayAmount) % displayAmount;
    }
  }
  rightState = digitalRead(rightButton);
  if (rightState != lastRightState) {
    lastRightState = !lastRightState;
    if (rightState == HIGH) {
      displayNum = (((displayNum+1) % displayAmount) + displayAmount) % displayAmount;
    }
  }
}

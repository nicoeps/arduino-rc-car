#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// Create a RF24 object called radio and set the CE and CSN pins
RF24 radio(7, 8); // (CE, CSN)

// Address for radio communication
const uint8_t address[6] = {"12345"};

// Timer
unsigned long lastTime = 0;
unsigned long currentTime = 0;

// Sensor pins
const uint8_t temperaturePin = A0;
const uint8_t LDRPin = A1;
const uint8_t flamePin = 4;
const uint8_t soundPin = 2;
const uint8_t gasPin = A2;

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

// Create a motor class which allows control of a H-bridge
// A motor object can then be created by calling Motor and then setting the forwards and backwards pins
class Motor {
  private:
    uint8_t forwardsPin;
    uint8_t backwardsPin;
    
  public: // Pins set on creation
    Motor(uint8_t _forwardsPin, uint8_t _backwardsPin) {
      forwardsPin = _forwardsPin;
      backwardsPin = _backwardsPin;
    }
    void forwards(uint8_t spd) { // Function to make the motor go forwards
      // The spd variable, speed, is set when running the funtion from the object and has to be a byte
      // A PWM signal is then generated based on the byte input which controls the speed of the motor
      analogWrite(forwardsPin, spd);
      analogWrite(backwardsPin, 0);
    }
    void backwards(uint8_t spd) { // Function to make the motor go backwards
      analogWrite(forwardsPin, 0);
      analogWrite(backwardsPin, spd);
    }
    void stop() { // Turn off the motor
      analogWrite(forwardsPin, 0);
      analogWrite(backwardsPin, 0);
    }
};

// Set the two sides of the car, each side has two motors which are connected in parallel
Motor m1(5,6);
Motor m2(9, 10);

void setup() {
  radio.begin(); // Begin radio communication
  radio.enableAckPayload();  // Enable the slave to sent data back
  radio.openReadingPipe(1, address); // Open the reading pipe to the given address
  // There are 6 pipes 
  // The writing pipe is by default 0, so the pipe is set to 1 for reading

  // startListening writes to the registers and makes the NRF24L01 listen for signals
  // Flushes the buffers 
  radio.startListening();
  
  scan();

  // Here the custom payload in the acknowledgement is sent
  // The payload gets stored in the buffer until it is sent to the master
  // The & operator is getting the address of the struct
  radio.writeAckPayload(1, &dataCar, sizeof(Data_Car)); // (pipe, payload, payload length)
  
  resetData();
}

void loop() {
  // Timer which checks if the radio connection has been disconnected
  // Resets when data is read
  currentTime = millis();
  if (currentTime - lastTime > 1000) {
    // Resets data if connection is lost, which prevents the car from moving on its own
    resetData();
  }
  scan();
  if (radio.available()) { // If data is received
    lastTime = millis(); // Reset timer
    // Read the data received from master and store the data in the struct
    radio.read(&dataController, sizeof(Data_Controller));
    // Store the acknowledgement in the buffer to be sent to the master
    radio.writeAckPayload(1, &dataCar, sizeof(Data_Car));
  }
  controls();
}

void resetData() {
  dataController.joystickX = 127;
  dataController.joystickY = 127;
}

void scan() {
  // Reads the sensor values
  dataCar.flameState = digitalRead(flamePin);
  dataCar.soundState = digitalRead(soundPin);
  // The analog input is read as a 10 bit value, going frmo 0 to 1023, according to the voltage it receives
  // Map the value to 1 byte instead, 0 to 255
  dataCar.LDRValue =  map(analogRead(LDRPin), 0, 1023, 0, 255);
  // The analog input is based on the voltage
  // By dividiing the max voltage, 5V, by the max value of 1023 the voltage on the pin gets calculated
  // This is then inserted in the temperature formula for the LM35 temperature sensor which is
  // Vout / 0.01 = T (celsius)
  dataCar.temperatureValue = round((analogRead(temperaturePin) * (5.0 / 1023.0)) / 0.01);
  dataCar.gasValue = map(analogRead(gasPin), 0, 1023, 0, 255);
}

// Function for controlling the car based on the joystick values
void controls() {
  uint8_t spd = 0;
  if (dataController.joystickY > 147) { // Fowards
    // The joystick values are received as a single byte going from 0 to 255
    // Default value for the joystick in the middle position is (127, 127)
    // The speed is then based on which direction the joystick is pointed and then by how much
    // which is mapped from 127 to 255, or 127 to 0, based on which way the joystick is pointed
    spd = map(dataController.joystickY, 127, 255, 0, 255);
    if (dataController.joystickX > 137) { // Forwards left
      // The turning is done by subtracting how much the x joystick is outputting
      // to the speed of the corresponding motor
      // Turning left will subtract the speed of the left motor by how much by the left joystick is outputting
      m1.forwards(constrain(spd-map(dataController.joystickX, 127, 255, 0, 255), 0, 255));
    } else {
      m1.forwards(spd);
    }
    if (dataController.joystickX < 107) { // Forwards right
      m2.forwards(constrain(spd-map(dataController.joystickX, 127, 0, 0, 255), 0, 255));
    } else {
      m2.forwards(spd);
    }
  } else if (dataController.joystickY < 107) { // Backwards
    spd = map(dataController.joystickY, 127, 0, 0, 255);
    if (dataController.joystickX > 137) { // Backwards lef
      m2.backwards(constrain(spd-map(dataController.joystickX, 127, 255, 0, 255), 0, 255));
    } else {
      m2.backwards(spd);
    }
    if (dataController.joystickX < 107) { // Backwards rght
      m1.backwards(constrain(spd-map(dataController.joystickX, 127, 0, 0, 255), 0, 255));
    } else {
      m1.backwards(spd);
    }
  // Makes the car rotate on the spot
  } else if (dataController.joystickX > 137) { // Left
    spd = map(dataController.joystickX, 127, 255, 0, 255);
    m1.backwards(spd);
    m2.forwards(spd);
  } else if (dataController.joystickX < 107) { // Right
    spd = map(dataController.joystickX, 127, 0, 0, 255);
    m1.forwards(spd);
    m2.backwards(spd);
  // Stop the motors if the joysticks are in default position
  } else {
    m1.stop();
    m2.stop();
  }
}

// Libraries
#include <Ethernet.h>
#include <ArduinoModbus.h>
#include <DHT22.h>
#include <SdFat.h>
#include <SPI.h>

// Definitions and variable assignments
#define pinData SDA
#define SD_CS_PIN 4

DHT22 dht22(pinData);

SdFat SD;
File file;

EthernetClient ethClient;

ModbusTCPClient modbusClient(ethClient);

// Ethernet shield MAC address
byte mac[] = { 0xA8, 0x61, 0x0A, 0xAE, 0xA7, 0xF8 };

// Thermal Chamber IP
IPAddress chamberIP(10, 19, 190, 88);

// Port number for Modbus communication (502 is standard)
const int chamberPort = 502;


void setup() {

  // Open serial communications
  Serial.begin(115200);
  
  // Open Ethernet connection
  Ethernet.begin(mac);

  // Open SD connection
  SD.begin(SD_CS_PIN);

  //Initialize led as output
  pinMode(LED_BUILTIN, OUTPUT);

  // Allow the Ethernet Shield and DHT22 Sensor to initialize
  delay(5000);

  // Connect to the thermal chamber
  if (modbusClient.begin(chamberIP, chamberPort)) {
    Serial.println("Connected");
  }
  
  else {
    Serial.println("Failed to connect");
  }

}

void loop() {
  // If disconnected from chamber, try reconnecting every 5 seconds
  if (!modbusClient.connected()) {
    if (!modbusClient.begin(chamberIP, chamberPort)) {
      delay(5000);
      return;
    }
  }

  // Set variables for entering safety protocol
  static bool SafetyProtocolEnabled = false;
  static bool DangerZone = false;

  // Open or create file for writing
  file = SD.open("errorlog.txt", FILE_WRITE);

  // Get temperature and humidity from sensor
  float t = dht22.getTemperature();
  float h = dht22.getHumidity();

  // If temperature OR humidity unacceptable, we are in danger
  if (t < 17.5 || t > 23.5 || h > 50.0) {
    bool DangerZone = true;
  }
  
  // If we are in danger and haven't begun Safety protocol, begin
  if (DangerZone && !(SafetyEnabled)) {
    bool SafetyEnabled = true;

    // Open file for logging
    file = SD.open("errorlog.txt", FILE_WRITE);

    // Terminate profile
    modbusClient.holdingRegisterWrite(1, 0x40B6, 148);
    delay(1000);
    
    // Set temperature control mode to auto
    modbusClient.holdingRegisterWrite(1, 0x0AAA, 10);
    delay(1000);

    // Set temperature to 23C
    modbusClient.holdingRegisterWrite(1, 0x0ADE, 0x41B8);
    modbusClient.holdingRegisterWrite(1, 0x0ADF, 0x0000);
    
    // Log time and values of error
    file.println("Time: " + String(millis()/1000)+"s");
    file.println("Temperature: " + String(t) + "C Humidity: " + String(h)+"%");
  }

  // If we are in danger and Safety protocol already started, blink LED until out of danger
  if (DangerZone && SafetyEnabled) {
    // Flash LED every second, checking for danger every 11 seconds
    for (int i = 0; i < 5; i++) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(1000);
      digitalWrite(LED_BUILTIN, LOW);
      delay(1000);
    }
  }
  
  // If temperature/humidity within acceptable ranges
  if (!(DangerZone)) {
    // Once back in safety, protocol no longer being enacted
    SafetyEnabled = false;

    // Check danger every 21 seconds
    delay(20000);
  }

  // File won't reopen for writing unless closed
  file.close();

  // Giving time before opening file again after closing
  delay(1000);
}

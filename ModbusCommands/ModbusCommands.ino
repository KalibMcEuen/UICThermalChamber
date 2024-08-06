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
byte mac[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// Thermal Chamber IP
IPAddress chamberIP(255, 255, 255, 255);

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

  // Open or create file for writing
  file = SD.open("errorlog.txt", FILE_WRITE);

  // Get temperature and humidity from sensor
  float t = dht22.getTemperature();
  float h = dht22.getHumidity();

  // If temperature OR humidity unacceptable, enact safety protocol
  if (t < 17.5 | t > 23.5 | h > 50.0) {
    
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

    //If temp/humidity in danger zones, flash LED every second, checking ranges every 9 seconds
    for (int i = 0; i < 6; i++) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(1000);
      digitalWrite(LED_BUILTIN, LOW);
      delay(1000);
    }
  }
  else {
    // If safe, check every 10 seconds
    delay(10000);
  }

  // File won't reopen for writing unless closed
  file.close();

  // Time before opening file again after closing
  delay(1000);
}

 #include <SPI.h>
#include "RF24.h"
#include <DHT.h>

/****************************************************************
 * Remote Sensor Framework Sensor Node Source Code
 * Developed by Arjun Muralidharan & Tushar Trehon, PESIT, Bangalore
 * 
*****************************************************************/

#define DHTPIN 2                        // SO CONNECT THE DHT11/22 SENSOR TO PIN D4 OF THE NODEMCU

#define MAX_TX_RETRY_COUNT 4            // Maximum Retry Count for Sensor data transmission

#define TRUE 1

#define FALSE 0

#define SLEEPTIME 120                    // Deep Sleep Time setting in Seconds for NodeMCU


// Initialize DHT sensor object
 DHT dht(DHTPIN, DHT22,15); //CHANGE DHT11 TO DHT22 in the function call if we are using DHT22


// Initialize RF Radio object for node  communication
/* Hardware configuration: Set up nRF24L01 radio on hardware 
SPI bus pins 4(CE) & 15 (CSN) for NodeMCU */
 RF24 radio(4,15);
 
/********************************************************
Node MCU Gateway Default Node Address
********************************************************/

byte Gateway_Node_Addr[] = "GatewayNode";

unsigned int NodeID;

struct Sensor_Frame {
byte NodeID;
float Humidity;
float Temperature;  
} Sensor_Data;

unsigned int Sensor_Read_Status = FALSE;

void setup() {

    /* Set Baud rate for Serial Debug Monitor */
    Serial.begin(115200);
    
    /* Display Initilaization Message on Debug Monitor */
    Serial.println("  ");    
    Serial.println(F("Remote Distributed Sensor Framework Sensor Node ........Initialization"));

    pinMode(9,INPUT);          // Node ID value in Hex (Pin10-Pin5-Pin9) (Node MCU Pins)
    pinMode(5,INPUT);
    pinMode(10,INPUT);

    NodeID = ( (digitalRead(9))+((digitalRead(5))*2)+((digitalRead(10))*4));
  
    Serial.print("GPIO9=");
    Serial.println(digitalRead(9));
  
    Serial.print("GPIO5=");
    Serial.println(digitalRead(5));
  
    Serial.print("GPIO10=");
    Serial.println(digitalRead(10));

    Serial.print("NodeID:");
    Serial.println(NodeID);

    /* Initialize RF Object */  
    radio.begin();
  
    // Set the number and delay of retries upon failed transmit.
    radio.setRetries(15,15);
  
    //Set Data Rate to 250kbps for longest range
    radio.setDataRate(RF24_250KBPS); 

    // length: RF24_CRC_8 for 8-bit or 
    // RF24_CRC_16 for 16-bit Cyclic Redundancy Check (Error checking)
    radio.setCRCLength(RF24_CRC_16);
  
    // Set the PA Level low to prevent power supply related issues since this is a
    // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
    // radio.setPALevel(RF24_PA_LOW);
 
    radio.setPALevel(RF24_PA_MAX);
  
  
    // Open the Gateway Node for writing pipe or trasmitting Sensor Data to Gateway Node
    radio.openWritingPipe(Gateway_Node_Addr);

    radio.stopListening();                                    // Sensor Nodes only transmit data & do not receive any

  while (Sensor_Read_Status == FALSE)
  {
    //Initialize DHT22 sensor object
    dht.begin();
    
    delay (3000);       // Give some delay before sensor stabilizes
    
    // Read Temp & Humidity data using DHT22 sensor
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    
  // Display error in case we were unsuccessful in getting sensor data
    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor!");
    }
    else
    {

      Serial.println("Data read successfully from DHT sensor!");
      // Initialize Sensor Data Frame for Gateway Node Transmission
      Sensor_Data.NodeID=NodeID;
      Sensor_Data.Temperature = temperature;
      Sensor_Data.Humidity = humidity;
      Sensor_Read_Status = TRUE;
      
      // Send data to gateway Node
      unsigned int Retry_count = MAX_TX_RETRY_COUNT;
      unsigned int Tx_Success=FALSE;
    
      while ((Retry_count-- != 0) && (Tx_Success == FALSE))
      {
          
        if (!(radio.write( &Sensor_Data, sizeof(Sensor_Data) ) ))              // Send the final one back.  
        {
          Serial.println("Failed to send sensor data to Gateway Node");
          delay(100);
        }
        else
        {
          Tx_Success = TRUE;
        }  
           
      }  

      if (Tx_Success)
      {
        Serial.print(F("Sent Sensor Data to Gateway, From Sensor Node:"));                  // Display Sensor data received from a specific node..
        Serial.println(Sensor_Data.NodeID);  
        Serial.print("Temp :");
        Serial.println(Sensor_Data.Temperature);  
        Serial.print("Humidity :");
        Serial.println(Sensor_Data.Humidity);  
      }
    
    } // end of else condition
    
  } //end of While Loop
    
    unsigned int randNumber = random(1000,3000); // get a random delay parameter
    delay(randNumber*NodeID);   // random delay of 2 to 5 sec for different nodes to be out of sync
    
    Serial.println("Going into deep sleep for approx. 2 mins...");

    // command NodeMCU to go to deep sleep and wakeup with RF disabled
    ESP.deepSleep(SLEEPTIME*1000000,WAKE_RF_DISABLED); 
    delay(100);         // provide some delay for NodeMCU to go to deep sleep
    
}

void loop() {
    //Do nothing here. The Setup() function takes care of it all as the 
    //ESP resets each time after it wakes from sleep and executes Setup
     
    Serial.println("Going into deep sleep for approx. 2 mins... ");
    
    // command NodeMCU to go to deep sleep and wakeup with RF disabled
    ESP.deepSleep(SLEEPTIME*1000000,WAKE_RF_DISABLED); 
    delay(100);         // provide some delay for NodeMCU to go to deep sleep
    
} // Loop


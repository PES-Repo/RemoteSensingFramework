#include <SPI.h>
#include "RF24.h"
#include <ESP8266WiFi.h>

#define MAX_Nodes 8
#define NODE_TIMEOUT 15                    // Keep time out for 10sec*15 = 150sec
#define MAX_WAIT_FOR_NODE 8               // Wait time for Active Node to be marked Inactive
#define CLOUD_SERVER_CONNECTION_RETRY 2   // Max connection retry for ThingSpeak Server
#define TRUE      1
#define FALSE     0
#define GW_DELAY_REQD 2                   // wait for 10sec*2 = 20sec for ThingSpk site update

// State Diagram definitions for Gateway Node

#define GWINIT      0
#define GWWAIT_FR   1
#define GWTIMEOUT   2
#define GWGOT_FR    3
#define GW_TSPK     4

unsigned int State = GWINIT;                    //Set Initial State
unsigned int GW_Ready = TRUE;
unsigned int GW_Ready_Cntr = GW_DELAY_REQD;
unsigned int ThingSpkProc;

/****************************************************************
 * Remote Sensor Framework Gateway Node Source Code
 * Developed by Arjun Muralidharan & Tushar Trehon, PESIT, Bangalore
 * 
*****************************************************************/


/* Hardware configuration: Set up nRF24L01 radio on hardware 
SPI bus pins 4(CE) & 15 (CSN) for NodeMCU */

 RF24 radio(4,15);
 
/********************************************************
Node MCU Gateway Default Node Address
********************************************************/

byte Gateway_Node_Addr[] = "GatewayNode";
/************************************************************
 * Data Structures for storing Sensor Node Data & parameters
 ************************************************************/
byte Node_Addr=FALSE;

// Common Storage area for incoming frame from any Sensor Node
struct Sensor_Frame {
byte NodeID;
float Humidity;
float Temp;  
}Sensor_Data;

// Following Frame storage and control data for each of Sensor Node
// in the network i.e. as per MAX_Nodes defined above
struct Sensor_Node_Live_Status
{
  byte NodeAlive;
  byte Timeout_Cntr;
  byte Max_Wait_For_Node;
  byte Data_Available;
  Sensor_Frame Node_Sensor_Data;
};

// MAX_Nodes array of Sensor Node Info for Gateway Node Timeout & ThingSpk processing
Sensor_Node_Live_Status Sensor_Node_Live[MAX_Nodes-1];

/*************************************************
 * Thingspeak & WiFi Communication Initialization
 ************************************************/

// Global variable used to store specific Sensor node channel’s Thingspeak API key,
String apiKey;

const char* server = "api.thingspeak.com";


// WiFi Connection Setup Data
const char* ssid = "Tata-Photon-Max-Wi-Fi-EAC8";         /* "Home281025";*/
const char* password = "Wakad607Aundh23$";              /*"Aundh607Wakad23$";*/

// Initialize WiFi Client Object
WiFiClient client;

void setup() {

  /* Set Baud rate for Serial Debug Monitor */
  Serial.begin(115200);
  
  /* Display Initilaization Message on Debug Monitor */
  Serial.println("");
  Serial.println(F("Remote Distributed Sensor Framework Master........Initialization"));

  /* Setup Wifi Connection*/
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
  delay(500);
  Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");

  //Initialize the Sensor Node Timeout Monitoring data structure
  for (Node_Addr = 0; Node_Addr < MAX_Nodes; Node_Addr++)
  {
    Sensor_Node_Live[Node_Addr].NodeAlive = FALSE;
    Sensor_Node_Live[Node_Addr].Timeout_Cntr = FALSE;
    Sensor_Node_Live[Node_Addr].Max_Wait_For_Node = FALSE;
    Sensor_Node_Live[Node_Addr].Data_Available = FALSE;
  }
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
  
  
  // Open the Gateway Node reading pipe for receiving Data from Remote Sensor Nodes
  radio.openReadingPipe(1,Gateway_Node_Addr);
   
  // Start the radio listening for data
  radio.startListening();
}

/****************************************************** 
*  Global Variables used in the loop() function for   *
*       Timeout & Error Processing                    *
******************************************************/
unsigned long started_waiting_at;           // Set up a timeout period, get the current microseconds
boolean timeout;                            // Set up a variable to indicate if a response was received or not
unsigned int cloud_server_connection_cntr;
unsigned int cloud_server_connection;

/******************************************************    
 *      Enter the Main Processing Function in Gateway *     
 *      Node                                          *
 *****************************************************/
void loop() {
  
    radio.startListening();                                // Now, continue listening

    // State diagram based processing on Gateway Node starts..        
    switch(State)
    {
      case GWINIT:
        started_waiting_at = micros();     
        timeout = false;
         
        cloud_server_connection = FALSE;
        cloud_server_connection_cntr = CLOUD_SERVER_CONNECTION_RETRY;
        State = GWWAIT_FR;
      break;
      
      case GWWAIT_FR:
        if ( (! radio.available()) && (timeout == false) )                              // While nothing is received, start waiting for incoming frames
        {
      
          if ((micros() - started_waiting_at) > 10000000 ){        // If waited longer than 10 sec, indicate timeout 
                timeout = true;  
                State = GWTIMEOUT;                              // Go to Timeout processing State
              }
        }  
        else
        {
            if (radio.available())
            {
              State = GWGOT_FR;                             // Received Data, Process it !!
            }
        }         
        break;

      case GWTIMEOUT:
      
             // Do Timeout processing every approx. 10+ sec
             
            //  Check if ThingSpk Server is ready for accepting new updates..i.e. 20 sec has elapsed..
            if (GW_Ready == FALSE)
            {
                         
              if (GW_Ready_Cntr == 0)
              {
                GW_Ready = TRUE;
              }
              else
              {
                GW_Ready_Cntr--;
              }
                            
            }
            
            // check if any Sensor Nodes are inactive...              
            for (Node_Addr = 0; Node_Addr < MAX_Nodes; Node_Addr++)
            {
                if (Sensor_Node_Live[Node_Addr].NodeAlive == TRUE)
                {
                    Sensor_Node_Live[Node_Addr].Timeout_Cntr--;
                 
                  if (Sensor_Node_Live[Node_Addr].Timeout_Cntr == 0)
                  {
                    Serial.print(F("Time out Error!!...Failed, to receive response from Sensor Node : "));
                    Serial.println(Node_Addr);
                    Sensor_Node_Live[Node_Addr].Timeout_Cntr = NODE_TIMEOUT;
                    Sensor_Node_Live[Node_Addr].Max_Wait_For_Node++;
                    if (Sensor_Node_Live[Node_Addr].Max_Wait_For_Node == MAX_WAIT_FOR_NODE)
                    {
                        //Waited long enough for data from this node, mark node as inactive
                        Sensor_Node_Live[Node_Addr].NodeAlive = FALSE;
                    }
                  }
         
                }
        
            } //For loop end

            if (GW_Ready == TRUE)
            {
              State = GW_TSPK;      // Send data to ThingSpk ...
              
            }
            else
            {
              State = GWWAIT_FR;    // Check for data from sensor nodes...
              
            }
            
            
            timeout = false;                                // Reset Timeout Flag
            started_waiting_at = micros();                  // Reset wait Counter
            
            break;
      
    
      case GWGOT_FR:
      
            if( radio.available()){
                                                                   
            while (radio.available()) {                                   // While there is data ready
              radio.read( &Sensor_Data, sizeof(Sensor_Data) );             // Get the payload
            }
     
            Serial.print(F("Received Response from :"));                  // Display Sensor data received from a specific node..
            Serial.println(Sensor_Data.NodeID);  
            Serial.print("Temp :");
            Serial.println(Sensor_Data.Temp);  
            Serial.print("Humidity :");
            Serial.println(Sensor_Data.Humidity);  
      
            Node_Addr = Sensor_Data.NodeID;
            Sensor_Node_Live[Node_Addr].NodeAlive = TRUE;
            Sensor_Node_Live[Node_Addr].Timeout_Cntr = NODE_TIMEOUT;
            Sensor_Node_Live[Node_Addr].Max_Wait_For_Node = FALSE;
            Sensor_Node_Live[Node_Addr].Node_Sensor_Data = Sensor_Data;   // Save received Node data to respective location in Node Array..
            Sensor_Node_Live[Node_Addr].Data_Available = TRUE;
            
            State = GW_TSPK;          // Got a frame check if we can send it to ThingSpk....
            }
            
            break;
          
    case GW_TSPK :
    
            if (GW_Ready)
            {
                //check if we have received any data from any sensor nodes...
                for (Node_Addr = 0; Node_Addr < MAX_Nodes; Node_Addr++)
                {            

                  if (Sensor_Node_Live[Node_Addr].Data_Available == TRUE)
                  {
                    Sensor_Data = Sensor_Node_Live[Node_Addr].Node_Sensor_Data;
                    ThingSpkProc = TRUE;    // Start ThingSpk Processing
                    Sensor_Node_Live[Node_Addr].Data_Available = FALSE;
                    break;  // Got Node Frame, break loop
                  }
                  else
                  {
                    ThingSpkProc = FALSE;
                  }
                  
                } // end of for loop...

                if (ThingSpkProc == TRUE)   // We have a Node frame to process!!.....
                {
                                               
                    if (!(Get_APIKey(Sensor_Data.NodeID)))
                    {
                      Serial.println("Error in finding valid ThingSpeak APIKey...");  
                    }
                    else
                    {   
                      cloud_server_connection = FALSE;
                                
                      while ( (cloud_server_connection_cntr) && (cloud_server_connection == FALSE))
                      {     
                                        
                        if (client.connect(server,80))   
                        {
                            cloud_server_connection = TRUE;
                      
                            String postStr = apiKey; 
                            postStr +="&field1=";
                            postStr += String(Sensor_Data.Temp);
                            postStr +="&field2=";
                            postStr += String(Sensor_Data.Humidity);
                            postStr += "\r\n\r\n";

                            Serial.print("Data sent to Thingspeak :");
                            Serial.println(postStr);
                      
                            // Send data to Thingspeak server
                            client.print("POST /update HTTP/1.1\n");
                            client.print("Host: api.thingspeak.com\n");
                            client.print("Connection: close\n");
                            client.print("X-THINGSPEAKAPIKEY: "+apiKey+"\n");
                            client.print("Content-Type: application/x-www-form-urlencoded\n");
                            client.print("Content-Length: ");
                            client.print(postStr.length());
                            client.print("\n\n");
                            client.print(postStr);

                            GW_Ready = FALSE; // We need to wait atleast 15 sec for 
                                              // next round of ThgSpk channel processing 
                            GW_Ready_Cntr = GW_DELAY_REQD;
                                                                          
                        }
                        else
                        {
                      
                          //Retry sending to Thingspeak Server again!!
                            cloud_server_connection_cntr--;
                            delay(5);      // Provide 5msec delay
                        }
           
                    } //while loop end..   
              
                      // Display error if connection to ThingSpeak Server fails ...
                    if (cloud_server_connection == FALSE)
                    {
                          Serial.println("Error sending Data to ThingSpeak Server...");
                    }    
          
                  } //else Get_APIKey loop end..

                      
             }   // end of if ThingSpkProc == TRUE
             
             ThingSpkProc = FALSE;    // Set ThgSpk Processing as False to start 
                                      // next iteration
             
        } // if GW Ready
          
          State = GWINIT;
          
        break;
        
      } // end of switch ...
        
      // Try again 1ms later
      delay(1);
    
} // Loop



/*********************************************************
 * Function Used to get Thingspeak Channel API Key for 
 * given remote sensor Node
 *********************************************************/
 
int Get_APIKey(byte NodeID)
{
  int result =FALSE; // set result as error for Wrong NodeID range
  
  if (NodeID < MAX_Nodes)
  {
    switch(NodeID)
    {
        case 0:
        apiKey = "RNQJBBHZ8D8WRG72";  // ThingSpeak API Key for Channel assigned to Sensor Node 0
        result = TRUE;
        break;

        case 1:
        apiKey = "91Q4MVVTH0NSIK52";  // ThingSpeak API Key for Channel assigned to Sensor Node 1
        result = TRUE;
        break;
    
        case 2:
        apiKey = "5614UKWMK8ETYHCQ";  // ThingSpeak API Key for Channel assigned to Sensor Node 2
        result = TRUE;
        break;
    
        case 3:
        apiKey = "4WFV783XTDVSFH74";  // ThingSpeak API Key for Channel assigned to Sensor Node 3
        result = TRUE;
        break;

        case 4:
        apiKey = "ZCL96P7K31IM5ZDC";  // ThingSpeak API Key for Channel assigned to Sensor Node 4
        result = TRUE;
        break;
    
        case 5:
        apiKey = "UDLU2FBYP1TSYVOK";  // ThingSpeak API Key for Channel assigned to Sensor Node 5
        result = TRUE;
        break;
    
        case 6:
        apiKey = "PG6JE07LM6P8AZN2";  // ThingSpeak API Key for Channel assigned to Sensor Node 6
        result = TRUE;
        break;

        case 7:
        apiKey = "1UADHEO3VAQ2HRRO";  // ThingSpeak API Key for Channel assigned to Sensor Node 7
        result = TRUE;
        break;

        default:
        Serial.println("Wrong Thingspeak Channel Initialization :");
        break;
        
    }
   
  }
  else
  {
    result = FALSE;
  }
  
  
  return(result);
}
  

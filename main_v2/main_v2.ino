#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#define GET_CHIPID()  (ESP.getChipId())
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#define GET_CHIPID()  ((uint16_t)(ESP.getEfuseMac()>>32))
#endif
#include <PubSubClient.h>
#include <AutoConnect.h>

#if defined(ARDUINO_ARCH_ESP8266)
typedef ESP8266WebServer  WiFiWebServer;
#elif defined(ARDUINO_ARCH_ESP32)
typedef WebServer WiFiWebServer;
#endif


ESP8266WebServer server;
AutoConnect portal(server);
AutoConnectAux Input;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

String username;
String password;
String host_up;
String database;
//#define motion1 13 //Motion Sensor
#define magnet1 12 //Door Sensor


// ************************************************** Input Page *******************************************************

const static char InputPage[] PROGMEM = R"r(
{
  "title": "Database",
  "uri": "/input", 
  "menu": true, 
  "element": [
    { 
      "name": "username", 
      "type": "ACInput", 
      "value": "Enter Username"
    },
    { 
      "name": "password", 
      "type": "ACInput",
      "apply": "password"
    },
    { 
      "name": "host_ip", 
      "type": "ACInput", 
      "value": "Host IP Address"
    },
    { 
      "name": "database", 
      "type": "ACInput", 
      "value": "Database name" },
    {
      "name": "save",
      "type": "ACSubmit",
      "value": "SAVE",
      "uri": "/"
    }
  ]
}
)r";

// ********************************************* Mosquitto Connect *****************************************************

bool mqttConnect() {
    // Connect to Mosquitto broker if not connect. Returns True if Connected. False if not.

  String value1 = Input["host_ip"].value;             // get Mosquitto broker IP address

  static const char alphanum[] = "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";                     // Used to Generate random Client IP
  char    clientId[9];

  uint8_t retry = 3;
  while (!mqttClient.connected()) {                   // Do while not connected to mosquitto broker
    if (value1.length() <= 0)
      break;

    mqttClient.setServer(value1.c_str(), 1883);       // Connect to mosquitto broker using IP address
    Serial.println(String("Attempting MQTT broker:") + value1);

    for (uint8_t i = 0; i < 8; i++) {                 // Generate random client ID
      clientId[i] = alphanum[random(62)];
    }
    clientId[8] = '\0';

    if (mqttClient.connect(clientId)) {               // Print to serial successful connection
      Serial.println("Established:" + String(clientId));
      return true;
    }
    else {
      Serial.println("Connection failed:" + String(mqttClient.state()));
      if (!--retry)
        break;
      delay(3000);
    }
  }
  return false;
}

// ************************************************** Root Page ********************************************************

void onRoot() {
// An on-page handler for '/' access

  String  content =
  "<html>"
  "<head><meta name='viewport' content='width=device-width, initial-scale=1'></head>"
  "<body><div>Username: {{value1}}<br>Password: {{value2}}<br>Host IP: {{value3}}<br>Database: {{value4}}</div></body>"
  "</html>";
        

  Input.fetchElement();                               // Get saved variables

  String value1 = Input["username"].value;            // Test print variables
  String value2 = Input["password"].value;
  String value3 = Input["host_ip"].value;
  String value4 = Input["database"].value;
  content.replace("{{value1}}", value1);              // Format root page
  content.replace("{{value2}}", value2);
  content.replace("{{value3}}", value3);
  content.replace("{{value4}}", value4);
  server.send(200, "text/html", content);
}

// ************************************************ Sensor Functions ***************************************************

//int motion(){
//    // Read Motion sensor value and return as char
//  int motionsense_int;                                // Declare integer motionsense_int  
//  motionsense_int = digitalRead(motion1);             // Read motion sensor value as int
//  return motionsense_int;                             // Return motion value (motion / no motion)
//}

int magnet(){
    // Read Motion sensor value and return as char
  int magnet_int;                                     // Declare integer magnet_int  
  magnet_int = digitalRead(magnet1);                  // Read magnet sensor value as int
  return magnet_int;                                  // Return magnet value (open / close)
}

// ********************************************* Interrupt Callbacks ***************************************************

void magnet_detect(){
  Serial.println("Interrupt function triggered");
//  int motion_status = motion();
  int magnet_status = magnet();

  Serial.print(magnet_status);
  
}

// ********************************************* Mosquitto Publish *****************************************************

void mqttPublish(String msg, String path) {
    // Mosquitto broadcast wrapper function
  Serial.print("Sending ");                           // Inform Serial mqtt is about to send
  Serial.println(msg);                                // Print message to serial
  Serial.print(" to...");
  Serial.println(path);                               // Print channel to serial
  mqttClient.publish(path.c_str(), msg.c_str());      // Publish payload to topic
}

// ********************************************* Mosquitto Subcribe ****************************************************

void subscribe_to_channels() {
    // Connect/ Reconnect to Mosquitto Broker.
      mqttPublish("hello world", "channels");          // Once connected, publish an announcement...
      mqttClient.subscribe("commands");                // Subscribe to commands channel
}

// ********************************************* Mosquitto Callback ****************************************************

void callback(char* topic, byte* payload, unsigned int length) {
    // Mosquitto Callback function. Receive messages from subscribes topics

  Serial.print("Message arrived [");                   // Announce message received
  Serial.print(topic);                                 // Print topic
  Serial.print("] ");
  for (int i = 0; i < length; i++) {                   // Iterate through payload
    Serial.print((char)payload[i]);
  }
  Serial.println();
  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}
// ******************************************** Connection Function ****************************************************

void maintain_connection(){
  // Maintain connection to Internet and Mosquitto Broker

  if (WiFi.status() == WL_CONNECTED) {                // If Wifi is connected
    if (!mqttClient.connected()) {                    // If mosquitto broker is not connected
      mqttConnect();                                  // Connect to mosquitto broker
      subscribe_to_channels();                        // Subscribe
    }
    mqttPublish("Im alive!", "channels");             // Publish alive status
    mqttClient.loop();

  }
}
// **************************************************** Setup **********************************************************

void setup() {
  // Setup Function. 
  Serial.begin(115200);                              // Start Serial at baud 115200
  Input.load(InputPage);                             // Load custom page to AutoConnect
  portal.join(Input);                                // Bind custom page to Menu
  server.on("/", onRoot);                            // Register the on-page handler
  portal.begin();                                    // Start AutoConnect
  mqttClient.setCallback(callback);                  // Set up mosquitto callback
  pinMode(magnet1, INPUT);
//  attachInterrupt(digitalPinToInterrupt(magnet1),magnet_detect, RISING);
  maintain_connection();

}

// ************************************************** Main Loop ********************************************************

void loop() {
  portal.handleClient();

  String value1 = Input["username"].value;
  String value2 = Input["password"].value;
  String value3 = Input["host_ip"].value;
  String value4 = Input["database"].value;
  Serial.println(value1);
  Serial.println(value2);
  Serial.println(value3);
  Serial.println(value4);
  Serial.println(digitalRead(magnet1));
  mqttPublish(value1,"channels");
  delay(1000);
  mqttPublish(value2,"channels");
  delay(1000);
  mqttPublish(value3,"channels");
  delay(1000);
  mqttPublish(value4,"channels");
  delay(1000);
  

}

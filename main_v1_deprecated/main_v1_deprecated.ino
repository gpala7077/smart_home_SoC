//On ESP8266,  PIN 1 is tied to the blue LED light on the board and TX
// PinOUT  From the bottom with pinouts on the left.
//  3V3 (+) |    RX  (3)
//  RST     |    IO0 (0)
//  EN  (+) |    IO2 (2)
//  TX  (1) |    GND (-)


#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRutils.h>



WiFiClient espClient;
PubSubClient client(espClient);
IPAddress mqtt_server(192, 168, 86, 120);
IPAddress server_addr(192, 168, 86, 120);
#ifndef STASSID
#define STASSID "TheMatrix"
#define STAPSK  "Br33dL0v300!!"
#endif


//===============================================================================DEFINE PINS========================================================================
#define ADC A0 //Light Sensor
#define D5  5  //Temperature Sensor
#define D14 14 //Motion Sensor
//#define D12 12 //Magnetic Door Switch
//#define D2  2


const char* ssid = STASSID;
const char* password = STAPSK;
const char* chip_name = "Sparkfun-ESP12-";
int lightSensorId = 1;
int motionSensorId = 1;
int tempSensorId =1;

char message_buff[50];
String temp_str;
String light_str;
String volt_str;
char volt[50];
char light[50];
char temp[50];
char rawCodes[250];
char user[] = "testing";
char pass[] = "secret";
int vSwitch = 0;
int val = 0;
const char* Command1 = "Status";
const char* Command2 = "Open Front Door";

uint16_t countValuesInStr(const String str, char sep) {
  int16_t index = -1;
  uint16_t count = 1;
  do {
    index = str.indexOf(sep, index + 1);
    count++;
  } while (index != -1);
  return count;
}


// Dynamically allocate an array of uint16_t's.
// Args:
//   size:  Nr. of uint16_t's need to be in the new array.
// Returns:
//   A Ptr to the new array. Restarts the ESP8266 if it fails.
uint16_t * newCodeArray(const uint16_t size) {
  uint16_t *result;

  result = reinterpret_cast<uint16_t*>(malloc(size * sizeof(uint16_t)));
  // Check we malloc'ed successfully.
  if (result == NULL) {  // malloc failed, so give up.
    Serial.printf("\nCan't allocate %d bytes. (%d bytes free)\n",
                  size * sizeof(uint16_t), ESP.getFreeHeap());
    Serial.println("Giving up & forcing a reboot.");
    ESP.restart();  // Reboot.
    delay(500);  // Wait for the restart to happen.
    return result;  // Should never get here, but just in case.
  }
  return result;
}

//===============================================================================SQL Queries========================================================================
char INSERT_Light[] = "INSERT INTO SmartHome.lightLevels (sensorId,value) VALUES (%d,%d)";
char INSERT_Motion[] = "INSERT INTO SmartHome.motionLevels (sensorId,value) VALUES (%d,%d)";
char INSERT_Temp[] = "INSERT INTO SmartHome.tempLevels (sensorId,value) VALUES (%d,%d)";
char INSERT_Ir[] = "INSERT INTO SmartHome.irCodes (sensorId,commandId,rawData) VALUES (1,1,'%s')";
char query[255],*pos = query;
char queryIr[255];


// Notice the "%lu" - that's a placeholder for the parameter we will
// supply. See sprintf() documentation for more formatting specifier
// options
const char QUERY_POP[] = "SELECT rawData FROM SmartHome.irCodes WHERE sensorId = %d and commandId = %d;";



//===============================================================================MQTT STUFF=========================================================================
const char* MQTT_Status = "TestStatus"; //Topic that ESP will send status info e.g. Alive; Dead; Command Received,Command Completed
const char* MQTT_Commands = "TestCommand"; //Topic that will listed for commands from the main hub

const char* ADC_Data = "A0Data";
const char* D5_Data = "D5Data";
const char* D14_Data = "D14Data";
const char* D12_Data = "D12Data";
const char* D2_Data = "D2Data";

const uint16_t kIrLed = 4;  // ESP8266 GPIO pin to use. Recommended: 4 (D2).
const uint16_t kRecvPin = 13;

IRsend irsend(kIrLed);
IRrecv irrecv(kRecvPin, 1024, 10, true);

decode_results results;  // Somewhere to store the results

OneWire oneWire(D5);
DallasTemperature sensors(&oneWire);
MySQL_Connection conn(&espClient);

void setup() {
  irsend.begin();
  Serial.begin(115200);
  pinMode(ADC,INPUT);    //
  pinMode(D14,INPUT);
  //pinMode(D12,OUTPUT);
  //pinMode(D2,OUTPUT);
  //Serial.begin(9600);
  
 //Serial.println("Alive");
 int motionSensor = digitalPinToInterrupt(D14);
  
 attachInterrupt(motionSensor,MotionDetect,RISING);
  
   client.setServer(mqtt_server, 1883);
   client.setCallback(callback);
   
   WiFi.mode(WIFI_STA);
   WiFi.begin(ssid, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
      //Serial.println("Connection Failed! Rebooting...");
      delay(5000);
      client.publish(MQTT_Status,"Restarting");
      ESP.restart();
    }
  //Serial.println("Connected to Wifi");
  Mainconnect();
  client.publish(MQTT_Status, "Alive");
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    //Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    //Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    //Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      //Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      //Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      //Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      //Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      //Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  //Serial.println("Ready");
  //Serial.print("IP address: ");
  //Serial.println(WiFi.localIP());
#if DECODE_HASH
  // Ignore messages with less than minimum on or off pulses.
  irrecv.setUnknownThreshold(12);
#endif                  // DECODE_HASH
  irrecv.enableIRIn();  // Start the receiver

}

void callback(char* topic, byte* payload, unsigned int length) {
  //Listen to another topic
  //  if(strcmp(topic,MQ2)==0){
  //    //Enter code for msg received in topic 2
  //  }
  client.publish(MQTT_Status, "Entered Callback");
  char json[length + 1];
  strncpy (json,(char*)payload,length);
  json[length]='\0';
    
    //Serial.println(json);
    //Serial.println(json==Command1);
    
    if (json == Command1) {
      //Do stuff
      client.publish(MQTT_Status, "Entered Command1");
      }
    if (json == Command2) {
      //Do stuff
      client.publish(MQTT_Status, "Entered Command2");

    }
}


void reconnect() {
  while (!client.connected()) {
    //Serial.print(".");
    String clientId = chip_name;
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    //Serial.println(client.connect(clientId.c_str()));
    if (client.connect(clientId.c_str())) {
      //Serial.println("Connected to MQTT");
      client.subscribe(MQTT_Commands);
    } else {
      //Serial.println("Failed to connect to MQTT Server, retrying in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
 }

void Mainconnect() {

  if (!client.connected()) {
    //Serial.println("Reconnecting to MQTT");
    reconnect();
  }
  client.loop();
}

int Motion(){
  //Serial.println("Reading Motion");
  //client.publish(MQTT_Status,"Reading Motion");
  
  int motionsense = digitalRead(D14);
  if (motionsense == HIGH) {
    client.publish(D14_Data,"Motion");
  }
  else {
    client.publish(D14_Data,"No Motion"); 
  }
  return motionsense;
}

int Temp(){
  //Serial.println("Reading Temperature");
  client.publish(MQTT_Status,"Reading Temp");
  sensors.requestTemperatures();
  int temp_int = sensors.getTempFByIndex(0);
  temp_str = String(temp_int); //converting ftemp (the float variable above) to a string 
  temp_str.toCharArray(temp, temp_str.length() + 1); //packaging up the data to publish to mqtt whoa...
  client.publish(D5_Data,temp);
  delay(200);
  return temp_int;
  }

int Light(){
  int pVolt = analogRead(ADC);
  //Serial.println("Reading Light");
  client.publish(MQTT_Status,"Reading Light");
  volt_str = String(pVolt);
  volt_str.toCharArray(volt, volt_str.length() + 1); //packaging up the data to publish to mqtt whoa...
  
  if (pVolt<200){
    client.publish(ADC_Data,"Dark");
  }
 else if (pVolt<400){
    client.publish(ADC_Data,"Dim");
  }
 else if (pVolt<600){
    client.publish(ADC_Data,"Ambient");
  }
  else if (pVolt<800){
    client.publish(ADC_Data,"Bright");
  }
  else if(pVolt<1000){
    client.publish(ADC_Data,"Very Bright");
  }
  else {
     client.publish(ADC_Data,"Max Light");
  }

 
  delay(200);
  return pVolt;
  }

void Insert_SQL(int sql, int value){
// 1 = upload to lightLevels
// 2 = upload to motionLevels
// 3 = upload to tempLevels
// 4 = upload to IrCodes
//Serial.println("Inserting SQL command");
conn.connect(server_addr,3306,user,pass);
MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);

if (sql ==1){
  sprintf(query, INSERT_Light, lightSensorId, value);
}
else if (sql == 2){
  sprintf(query, INSERT_Motion, motionSensorId, value);
}

cur_mem->execute(query);
delete cur_mem;
delay(500);

}

char* sql_IR(int sensorId, int commandId){
conn.connect(server_addr,3306,user,pass);
MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
sprintf(query, QUERY_POP, sensorId,commandId);

cur_mem->execute(query);
column_names *cols = cur_mem->get_columns();
  
 // for (int f = 0; f < cols->num_fields; f++) {
 //   Serial.print(cols->fields[f]->name);
 //   if (f < cols->num_fields-1) {
 //     Serial.print(',');
 //   }
 // }
 // Read the rows and print them
  row_values *row = NULL;
  do {
    row = cur_mem->get_next_row();
    if (row != NULL) {
      for (int f = 0; f < cols->num_fields; f++) {
        Serial.print(row->values[f]);
        if (f < cols->num_fields-1) {
          Serial.print(',');
        }
        return row->values[f];     
      }
      Serial.println();
    }
  } while (row != NULL);
  
  // Deleting the cursor also frees up memory used
  delete cur_mem;
  
delay(500);

}

void MotionDetect(){
  client.publish(MQTT_Status, "Triggered Interrupt");
  //Serial.println("Interrupt function triggered");
  int lightstatus = Light();
  int motionstatus = Motion();
 // Insert_SQL(1,lightstatus);
 // Insert_SQL(2,motionstatus);
}

void readIR(){
  
  if (irrecv.decode(&results)) {
    if (results.overflow)
      Serial.printf(
          "WARNING: IR code is too big for buffer (>= %d). "
          "This result shouldn't be trusted until this is resolved. "
          "Edit & increase kCaptureBufferSize.\n",
          1024);
    // Output RAW timing info of the result.
    //Serial.println(resultToTimingInfo(&results));
    delay(1000);
    resultToTimingInfo(&results).remove(1,2);
    Serial.println(parseStringAndSendRaw(resultToTimingInfo(&results),2));
   
    yield();  // Feed the WDT (again)
  }
}

// Parse an IRremote Raw Hex String/code and send it.
// Args:
//   str: A comma-separated String containing the freq and raw IR data.
//        e.g. "38000,9000,4500,600,1450,600,900,650,1500,..."
//        Requires at least two comma-separated values.
//        First value is the transmission frequency in Hz or kHz.
// Returns:
//   bool: Successfully sent or not.
bool parseStringAndSendRaw(const String str,int command) {
  // 1 = send IR code
  // 2 = upload to SQL database
  uint16_t count;
  uint16_t freq = 38000;  // Default to 38kHz.
  uint16_t *raw_array;
  String sql_array;
 Serial.println(str);
   
  // Find out how many items there are in the string.
  count = countValuesInStr(str, ',');

  // We expect the frequency as the first comma separated value, so we need at
  // least two values. If not, bail out.
  if (count < 50)  return false;
  count--;  // We don't count the frequency value as part of the raw array.

  // Now we know how many there are, allocate the memory to store them all.
  raw_array = newCodeArray(count);

  // Grab the first value from the string, as it is the frequency.
  int16_t index;
  int16_t iStart;
  
  //Serial.println(freq);
  uint16_t start_from = 0;
  uint16_t iStarta;
  uint16_t iStartb;
  // Rest of the string are values for the raw array.
  // Now convert the strings to integers and place them in raw_array.
  count = 0;
  do {
    if (count = 0){
    index = str.indexOf(',', start_from)+2;
    }
    else{
      index = str.indexOf(',', start_from); 
    }
    iStarta = str.indexOf('-',start_from)+2;
    iStartb = str.indexOf('+',start_from)+2;

    if(iStarta>index){
      iStart = iStartb;
    }
    else{
      iStart = iStarta;
    }
    
    //Serial.println(start_from);
    //Serial.println(iStart);
    //Serial.println(index);
    
    //Serial.println(start_from);
    //Serial.println(index);
    //Serial.print("Number found = ");
    //Serial.println(str.substring(iStart, index));
    raw_array[count] = str.substring(iStart, index).toInt();
    //Serial.println(raw_array[count]);
    //sql_array = sql_array.concat(str.substring(iStart, index));
    //Serial.println(sql_array);
    start_from = index+1;
    count++;
  } while (index != -1);
  delay(2000);
  
  
  if(command == 1){
    irsend.sendRaw(raw_array, count+1, 38);  // All done. Send it.
    free(raw_array);  // Free up the memory allocated.
  }
  if(command == 2){
  conn.connect(server_addr,3306,user,pass);
   for (int i = 0; i<count; i++) {
    Serial.print("Count = ");
    Serial.print(count);
    Serial.println();
    
       pos += sprintf(pos,"%d",raw_array[i]);
        if(i<count-1){
          pos += sprintf(pos,",");
        }
      //delay(200);
      //Serial.println(query);
    }
  Serial.println("printing final");
  //Serial.println(query);
  MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
  sprintf(queryIr,INSERT_Ir,query);
  Serial.println(queryIr);
  cur_mem->execute(queryIr);
  free(raw_array);
  delay(2000);
  //parseSqlAndSendRaw(sql_IR(1,1));
      
//  Free up the memory allocated.
}
  if (count > 0)
    return true;  // We sent something.
  return false;  // We probably didn't.
}

// Parse an IRremote Raw Hex String/code and send it.
// Args:
//   str: A comma-separated String containing the freq and raw IR data.
//        e.g. "38000,9000,4500,600,1450,600,900,650,1500,..."
//        Requires at least two comma-separated values.
//        First value is the transmission frequency in Hz or kHz.
// Returns:
//   bool: Successfully sent or not.
bool parseSqlAndSendRaw(const String str) {
  // 1 = send IR code
  // 2 = upload to SQL database
  uint16_t count;
  uint16_t freq = 38000;  // Default to 38kHz.
  uint16_t *raw_array;
  String sql_array;
 //Serial.println(str);
   
  // Find out how many items there are in the string.
  count = countValuesInStr(str, ',');

Serial.println(count);
  // We expect the frequency as the first comma separated value, so we need at
  // least two values. If not, bail out.
  if (count < 50)  return false;
  count--;  // We don't count the frequency value as part of the raw array.

  // Now we know how many there are, allocate the memory to store them all.
  raw_array = newCodeArray(count);

  // Grab the first value from the string, as it is the frequency.
  int16_t index;
  
  //Serial.println(freq);
  uint16_t start_from = 0;
  // Rest of the string are values for the raw array.
  // Now convert the strings to integers and place them in raw_array.
  count = 0;
  do {
    index = str.indexOf(',', start_from);
    
    //Serial.println(count);
    if(count==0){ 
      raw_array[count] = str.substring(start_from+2, index).toInt();
      Serial.println(raw_array[count]);
      start_from = index+1;
      count++;
    }
    else if (count>0){
      raw_array[count] = str.substring(start_from, index).toInt();
     Serial.println(raw_array[count]);
     start_from = index+1;
     count++;
    }

    
  } while (index != -1);
    //Serial.println(count);
    
    irsend.sendRaw(raw_array, count+8, 38);  // All done. Send it.
    free(raw_array);  // Free up the memory allocated.
  if (count > 0)
    return true;  // We sent something.
  return false;  // We probably didn't.
}
     
void loop() {
  ArduinoOTA.handle();
  Mainconnect();
  //irsend.sendRaw(rawData, 67, 38);  // Send a raw data capture at 38kHz
  //irsend.sendNEC(0x807F48B7);
  //readIR();
  Serial.println(sql_IR(1,1));
  delay(1000);
  //Serial.println(Motion());
  //Serial.println(Light());
  //Serial.println(Temp());
  
  //Motion();
  //MotionDetect();
  // Check if the IR code has been received.
  
}

# How this code SHOULD work.

## Steps:

### 1: Start AutoConnect in order to configure start up variables                              * COMPLETE *
            (i.e. WiFi SSID | Pass  AND Database credentials.) 

### 2: Connect to MariaDB and get setup information                                            * INCOMPLETE *
            (i.e. mosquitto broker ip, pin assignments, mosquitto channels, commands) 

### 3: Connect to Mosquitto Broker                                                             * COMPLETE *                                                                                    

### 4: Listen to appropriately formatted mosquitto channels                                    * INCOMPLETE *                                                                                    
          
               i.e. channels are given as...
                1) home/rooms/room_name/things/thing_name/commands
                2) home/rooms/room_name/things/thing_name/info
                3) home/rooms/room_name/things/thing_name/interrupt

              Need to be formatted as (replace room_name and thing_name)
               i.e. channels are formatted correctly when...
                1) home/rooms/Kitchen/things/Kitchen_Thing1/commands
                2) home/rooms/Kitchen/things/Kitchen_Thing1/info
                3) home/rooms/Kitchen/things/Kitchen_Thing1/interrupt
          
### 5: Publish thing status when interrupts are triggered to channel 3 (interrupt)            * INCOMPLETE *
              i.e. payload = "
                           "{
                             "sensor_name" :   ["motion1", "LDR1"],
                              "sensor_type" :   ["motion", "LDR"],
                              "sensor_value":   [1, 2423]
                             "}


### 6: Perform commands given from MariaDB (step 2) and sent to channel 1 (Step 2 - commands)  * INCOMPLETE * 
                                                                                    
             i.e. payload = "read_status"
             i.e. payload = "relay_on"
             i.e. payload = "relay_off"

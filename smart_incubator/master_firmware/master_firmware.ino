#include "RF24.h"
#include "RF24Network.h"
#include "RF24Mesh.h"
#include <SPI.h>
#include <EEPROM.h>

#include <DHT.h>
#include <SerialCommand.h>

// In theory, it is possible to run the master node on a ethernet shield
// See http://www.instructables.com/id/PART-1-Send-Arduino-data-to-the-Web-PHP-MySQL-D3js/

// http://www.pjrc.com/teensy/td_libs_Time.html
// http://playground.arduino.cc/Code/Time
#include <Time.h> 

// This external descriptor of data structure is needed in order to be able to pass the data structure as
// variable to functions. Without this external file, for some reason, it would not work.
#include "MyTypes.h"

//defines
#define VERSION 1.4
#define masterID 0
#define SENSORS //use sensors on board

#define CMD 'C'
#define REPORT 'R'
#define EVENT 'E'

//Initialising objects
SerialCommand sCmd;

/**** Configure the nrf24l01 CE and CS pins ****/
RF24 radio(8, 7);
RF24Network network(radio);
RF24Mesh mesh(radio, network);

//some vars and consts
unsigned long counter = 0;
packageStruct dataPackage = {masterID, 0, counter, CMD, 0, 0, 0, 0, 0};

#if defined (SENSORS)
#define optoresistor_PIN 15 //A1
#include <SHT1x.h>
#define HT_PIN 17 //A3
#define clockPin 16 //A2

SHT1x sht1x(HT_PIN, clockPin);
time_t prev_time = 0;
float DELTA = 5 * 60.0 * 1000.0; // delta between reports, in milliseconds - default 5 mins
unsigned long report_counter = 0;
#endif


void setup()
{
#if defined (SENSORS)
  pinMode(optoresistor_PIN, INPUT);
#endif

  Serial.begin(115200);
  setupSerialCommands();

  mesh.setNodeID(masterID);
  mesh.begin();
}

void loop()
{
    mesh.update();
    // In addition, keep the 'DHCP service' running on the master node so addresses will
    // be assigned to the sensor nodes
    mesh.DHCP();

    // Check for incoming data from the sensors
    if(network.available()){
        RF24NetworkHeader header;
        network.peek(header);

        packageStruct rcvdPackage;
        switch(header.type){
          // Display the incoming millis() values from the sensor nodes
          case REPORT: // regular, timed report
              network.read(header,&rcvdPackage,sizeof(rcvdPackage)); 
              transmitData(&rcvdPackage);
              break;
          case EVENT: // event (e.g. lights on or off)
              network.read(header,&rcvdPackage,sizeof(rcvdPackage)); 
              transmitData(&rcvdPackage);
              break;
          default: network.read(header,0,0); Serial.println(header.type);break;
        }
    }
 sCmd.readSerial();

#if defined(SENSORS)
  if ( (millis() - prev_time) > DELTA or prev_time == 0 )  {
    prev_time = millis();
    reportMasterReadings();
  }
#endif
}

void reportMasterReadings(){
    report_counter = report_counter+1;
    float temperature =  sht1x.readTemperatureC();
    float humidity = sht1x.readHumidity();
    int light = analogRead(optoresistor_PIN);

    Serial.print( masterID ); Serial.print (" ");
    Serial.print( REPORT ); Serial.print (" ");
    Serial.print( report_counter ); Serial.print (" ");
    Serial.print( now() ); Serial.print (" ");
    Serial.print( temperature ); Serial.print (" ");
    Serial.print( humidity ); Serial.print (" ");
    Serial.print( light );
    Serial.println( " 0 0 0 0 0 0"); //this tail is to give similar structure to data.
}

void setupSerialCommands() {
//Defines the available serial commands
  sCmd.addCommand("HELP",  printHelp);
  sCmd.addCommand("T",     setTime);
  sCmd.addCommand("L",     setLight);
  sCmd.addCommand("F",     setDelay);
  sCmd.addCommand("I",     getState);
  sCmd.addCommand("1",     setLightsONTimer);
  sCmd.addCommand("0",     setLightsOFFTimer);
  sCmd.addCommand("D",     demandDebug);
  sCmd.addCommand("M",     setLightMode);
  sCmd.addCommand("C",    setTemperature);
  sCmd.addCommand("H",    setHumidity);
  sCmd.addCommand("X",    setMaxLight);
  sCmd.addCommand("R",    reportMasterReadings);
  sCmd.setDefaultHandler(printError);      // Handler for command that isn't matched  (says "What?")
}

void printHelp() {
  Serial.println("HELP                  Print this help message");
  Serial.println("M ID DD               Set light mode to DD, LD, LL, DL, 00");
  Serial.println("T ID timestamp        Set time on node ID");
  Serial.println("L ID light (0-100)    Set light levels on node ID");
  Serial.println("I ID                  Interrogates node ID");
  Serial.println("F ID mm               Set interval (minutes) between reports from node ID");
  Serial.println("1 ID timestamp        Set time for Lights ON  - uses only HH:MM component");
  Serial.println("0 ID timestamp        Set time for Lights OFF - uses only HH:MM component");
  Serial.println("C ID temperature      Set temperature on node ID");
  Serial.println("H ID humidity         Set humidity on node ID");
  Serial.println("X ID light            Set max light value on node ID");
  Serial.println("R                     Report readings from Master node");
  Serial.println("==================================================================================");
}

void printError(const char *command) {
  // This gets set as the default handler, and gets called when no other command matches.
  Serial.println("ERROR Command not valid");
}
void transmitData(packageStruct* rcvdPackage) {
  // this is used to transmit received packages to the serial port.
  // Each data line can be easily stored into a db
  Serial.print( rcvdPackage->orig_nodeID ); Serial.print (" ");
  Serial.print( rcvdPackage->cmd ); Serial.print (" ");
  Serial.print( rcvdPackage->counter ); Serial.print (" ");
  Serial.print( rcvdPackage->current_time ); Serial.print (" ");
  Serial.print( rcvdPackage->temp ); Serial.print (" ");
  Serial.print( rcvdPackage->hum ); Serial.print (" ");
  Serial.print( rcvdPackage->light ); Serial.print (" ");
  Serial.print( rcvdPackage->set_temp ); Serial.print (" ");
  Serial.print( rcvdPackage->set_hum ); Serial.print (" ");
  Serial.print( rcvdPackage->set_light ); Serial.print (" ");
  //Serial.print( hour(rcvdPackage->lights_on ) ); Serial.print (":"); Serial.print( minute(rcvdPackage->lights_on  ) ); Serial.print (" ");
  //Serial.print( hour(rcvdPackage->lights_off) ); Serial.print (":"); Serial.print( minute(rcvdPackage->lights_off ) ); Serial.print (" ");
  Serial.print( rcvdPackage->lights_on ); Serial.print (" ");
  Serial.print( rcvdPackage->lights_off ); Serial.print (" ");
  Serial.println( rcvdPackage->dd_mode );
}

void printPackage(packageStruct* rcvdPackage) {
  // A human readable Package descriptor
  // Useful for debug
  Serial.print("Originating Node: ");
  Serial.println(rcvdPackage->orig_nodeID);
  Serial.print("Destination Node: ");
  Serial.println(rcvdPackage->dest_nodeID);
  Serial.print("At time: ");
  Serial.print(hour(rcvdPackage->current_time)); Serial.print(":"); Serial.print(minute(rcvdPackage->current_time)); Serial.print(":"); Serial.print(second(rcvdPackage->current_time));
  Serial.print(" "); Serial.print(year(rcvdPackage->current_time)); Serial.print("-"); Serial.print(month(rcvdPackage->current_time)); Serial.print("-"); Serial.println(day(rcvdPackage->current_time));
  Serial.print("TS: ");
  Serial.println(rcvdPackage->current_time);
  Serial.print("T:");
  Serial.print(rcvdPackage->temp);
  Serial.print("\tH:");
  Serial.print(rcvdPackage->hum);
  Serial.print("\tL:");
  Serial.print(rcvdPackage->light);
  Serial.print("\tLvl:");
  Serial.println(rcvdPackage->set_light); 
  Serial.print("Count: ");
  Serial.println(rcvdPackage->counter);
  Serial.println("");
}

void setDelay(){
  // Changes the interval (in minutes) of spontaneous reports from nodeID. 
  // An interval of 0 will inactivate reports
  char *arg;
  int destID = 0;
  int interval = 0;

  arg = sCmd.next();
  if (arg != NULL) {
    destID = atoi(arg);
  }

  arg = sCmd.next();
  if (arg != NULL) {
    interval = atoi(arg);
  }

  dataPackage.orig_nodeID = masterID;
  dataPackage.dest_nodeID = destID;
  dataPackage.counter = counter++;
  dataPackage.cmd = 'F';
  dataPackage.set_light = interval;

  Serial.print(F("Now sending Package "));
     if (!mesh.write( &dataPackage, CMD, sizeof(dataPackage), destID )){
       Serial.println(F("failed"));
     } else { Serial.println("OK"); }
}

void setLightMode(){
  // Change the light mode 
  // 0 DD, 1 LD, 2 LL
  char *arg;
  int destID = 0;
  int mode = 1;

  arg = sCmd.next();
  if (arg != NULL) {
    destID = atoi(arg);
  }

  arg = sCmd.next();
  if ((arg != NULL) and (strcmp (arg,"DD") == 0)){ mode = 0; };
  if ((arg != NULL) and (strcmp (arg,"LD") == 0)){ mode = 1; };
  if ((arg != NULL) and (strcmp (arg,"LL") == 0)){ mode = 2; };
  if ((arg != NULL) and (strcmp (arg,"DL") == 0)){ mode = 3; };
  if ((arg != NULL) and (strcmp (arg,"00") == 0)){ mode = 4; };

  dataPackage.orig_nodeID = masterID;
  dataPackage.dest_nodeID = destID;
  dataPackage.counter = counter++;
  dataPackage.cmd = 'M';
  dataPackage.set_light = mode;

  Serial.print(F("Now sending Package "));
     if (!mesh.write( &dataPackage, CMD, sizeof(dataPackage), destID )){
       Serial.println(F("failed"));
     } else { Serial.println("OK"); }
}


void setLightsONTimer(){
  // Changes the HH:MM for lights on - Called from serial port
  char *arg;
  int destID = 0;
  int lights_on = 0;

  arg = sCmd.next();
  if (arg != NULL) {
    destID = atoi(arg);
  }

  arg = sCmd.next();
  if (arg != NULL) {
    lights_on = atol(arg);
  }


  dataPackage.orig_nodeID = masterID;
  dataPackage.dest_nodeID = destID;
  dataPackage.counter = counter++;
  dataPackage.cmd = '1';
  dataPackage.lights_on = lights_on;

  Serial.print(F("Now sending Package "));
     if (!mesh.write( &dataPackage, CMD, sizeof(dataPackage), destID )){
       Serial.println(F("failed"));
     } else { Serial.println("OK"); }
}

void setLightsOFFTimer(){
  // Changes the HH:MM for lights off - called from serial port
  char *arg;
  int destID = 0;
  unsigned long lights_off = 0;

  arg = sCmd.next();
  if (arg != NULL) {
    destID = atoi(arg);
  }

  arg = sCmd.next();
  if (arg != NULL) {
    lights_off = atol(arg);
  }

  dataPackage.orig_nodeID = masterID;
  dataPackage.dest_nodeID = destID;
  dataPackage.counter = counter++;
  dataPackage.cmd = '0';
  dataPackage.lights_off = lights_off;

  Serial.print(F("Now sending Package "));
     if (!mesh.write( &dataPackage, CMD, sizeof(dataPackage), destID )){
       Serial.println(F("failed"));
     } else { Serial.println("OK"); }
}

void getState (){
  //interrogate the node, demand a package report - called from serial port
  char *arg;
  int destID = 0;

  arg = sCmd.next();
  if (arg != NULL) {
    destID = atoi(arg);
  }

  dataPackage.orig_nodeID = masterID;
  dataPackage.dest_nodeID = destID;
  dataPackage.counter = counter++;
  dataPackage.cmd = 'I';

  Serial.print(F("Now sending Package "));
     if (!mesh.write( &dataPackage, CMD, sizeof(dataPackage), destID )){
       Serial.println(F("failed"));
     } else { Serial.println("OK"); }

}

void demandDebug (){
  //interrogate the node, demand a debug report - called from serial port
  char *arg;
  int destID = 0;

  arg = sCmd.next();
  if (arg != NULL) {
    destID = atoi(arg);
  }

  dataPackage.orig_nodeID = masterID;
  dataPackage.dest_nodeID = destID;
  dataPackage.counter = counter++;
  dataPackage.cmd = 'D';

  Serial.print(F("Now sending Package "));
     if (!mesh.write( &dataPackage, CMD, sizeof(dataPackage), destID )){
       Serial.println(F("failed"));
     } else { Serial.println("OK"); }

}

void setTime (){
  //set time on the RTC module of the node - called from serial port
  char *arg;
  int destID = 0;
  time_t unixstamp = 0;

  arg = sCmd.next();
  if (arg != NULL) {
    destID = atoi(arg);
  }

  arg = sCmd.next();
  if (arg != NULL) {
    unixstamp = atol(arg);
  }

  dataPackage.orig_nodeID = masterID;
  dataPackage.dest_nodeID = destID;
  dataPackage.counter = counter++;
  dataPackage.cmd = 'T';
  dataPackage.current_time = unixstamp;

  Serial.print(F("Now sending Package "));
     if (!mesh.write( &dataPackage, CMD, sizeof(dataPackage), destID )){
       Serial.println(F("failed"));
     } else { Serial.println("OK"); }
}

void setLight (){
  // turn the node lights at value 0-100% - called from serial port
  char *arg;
  int destID = 0;
  int light_level = 0;
  
  arg = sCmd.next();
  if (arg != NULL) {
    destID = atoi(arg);
  }

  arg = sCmd.next();
  if (arg != NULL) {
    light_level = atoi(arg);
  }

  dataPackage.orig_nodeID = masterID;
  dataPackage.dest_nodeID = destID;
  dataPackage.counter = counter++;
  dataPackage.cmd = 'L'; 
  dataPackage.set_light = light_level;

  Serial.print(F("Now sending Package: "));
     if (!mesh.write( &dataPackage, CMD, sizeof(dataPackage), destID )){
       Serial.println(F("failed"));
     } else { Serial.println("OK"); }
}

void setTemperature(){
    
  // set the current Temperature on node
  char *arg;
  int destID = 0;
  float set_TEMP = 0;
  
  arg = sCmd.next();
  if (arg != NULL) {
    destID = atoi(arg);
  }

  arg = sCmd.next();
  if (arg != NULL) {
    set_TEMP = atof(arg);
  }

  dataPackage.orig_nodeID = masterID;
  dataPackage.dest_nodeID = destID;
  dataPackage.counter = counter++;
  dataPackage.cmd = 'C';
  dataPackage.set_temp = set_TEMP;

  Serial.print(F("Now sending Package: "));
     if (!mesh.write( &dataPackage, CMD, sizeof(dataPackage), destID )){
       Serial.println(F("failed"));
     } else { Serial.println("OK"); }
}

void setHumidity(){
    
  // set the current Humidity on node
  char *arg;
  int destID = 0;
  float set_HUM = 0;
  
  arg = sCmd.next();
  if (arg != NULL) {
    destID = atoi(arg);
  }

  arg = sCmd.next();
  if (arg != NULL) {
    set_HUM = atof(arg);
  }

  dataPackage.orig_nodeID = masterID;
  dataPackage.dest_nodeID = destID;
  dataPackage.counter = counter++;
  dataPackage.cmd = 'H'; 
  dataPackage.set_hum = set_HUM;

  Serial.print(F("Now sending Package: "));
     if (!mesh.write( &dataPackage, CMD, sizeof(dataPackage), destID )){
       Serial.println(F("failed"));
     } else { Serial.println("OK"); }
}

void setMaxLight (){
  // Set the maximum level of lights allowed
  char *arg;
  int destID = 0;
  int light_level = 0;
  
  arg = sCmd.next();
  if (arg != NULL) {
    destID = atoi(arg);
  }

  arg = sCmd.next();
  if (arg != NULL) {
    light_level = atoi(arg);
  }

  dataPackage.orig_nodeID = masterID;
  dataPackage.dest_nodeID = destID;
  dataPackage.counter = counter++;
  dataPackage.cmd = 'X'; 
  dataPackage.set_light = light_level;

  Serial.print(F("Now sending Package: "));
     if (!mesh.write( &dataPackage, CMD, sizeof(dataPackage), destID )){
       Serial.println(F("failed"));
     } else { Serial.println("OK"); }
}

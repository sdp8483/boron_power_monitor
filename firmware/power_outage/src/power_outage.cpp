/* 
 * Project Cellular Power Loss Notification
 * Author: Sam Perry
 * Date: August 2024
 * 
 * Code Source: https://gist.github.com/krdarrah/314027798992a8ee29cc05a4cb47960f
 */

// Include Particle Device OS APIs
#include "Particle.h"

// Let Device OS manage the connection to the Particle Cloud
SYSTEM_MODE(AUTOMATIC);

// Run the application and system concurrently in separate threads
SYSTEM_THREAD(ENABLED);

// Show system, cloud connectivity, and application logs over USB
// View logs with CLI using 'particle serial monitor --follow'
SerialLogHandler logHandler(LOG_LEVEL_INFO);

// Power Monitoring Code Below -----------------------------------------------
String str1,str2;

bool onUSB = false;
bool onBattery = false;
bool lowBattery = false;
unsigned long pwrCheckTimeStart;//to check power every 10sec

// function prototypes
void sendData(void);
float battery_charge(void);

void setup() {

    /* publish variables for Particle dashboard viewing*/
    Particle.variable("battery", battery_charge);

    // INITIAL POWER CHECK 
    int powerSource = System.powerSource();
    if (powerSource == POWER_SOURCE_BATTERY) {// ON BATTERY
        onBattery = true;
        onUSB = false;
    }
    else{// ON USB
        onBattery = false;
        onUSB = true;
    }
    
    if(onBattery){//bootup message alert so we know things are back online
         str1 = "PDC Booting...";
         str2 = "No AC Power";
         sendData();
    }else{
         str1 = "PDC Booting...";
         str2 = "AC Power Good";
         sendData();
    }
    
    pwrCheckTimeStart = millis();
}

void loop() {
  // POWER CHECK
  if(millis()-pwrCheckTimeStart>10000){
    pwrCheckTimeStart = millis();
    int powerSource = System.powerSource();
    if (powerSource == POWER_SOURCE_BATTERY) {// ON BATTERY
        if(!onBattery && onUSB){// CHANGED FROM USB TO BATTERY
         onBattery = true;
         onUSB = false;
         str1 = "PDC";
         str2 = "AC Power Lost";
         sendData();
        }
    }else if(onBattery && !onUSB){// CHANGED FROM BATTERY TO USB
        onBattery = false;
        onUSB = true;
        str1 = "PDC";
        str2 = "AC Power is On";
        sendData();
    }
    
    //and also check battery voltage 
    FuelGauge fuel;
    float batteryVoltage = fuel.getVCell();
    if(batteryVoltage < 3.5){// if less than this, send an alert 
        if(!lowBattery){
            lowBattery=true;
            str1 = "PDC";
            str2 = "Low Battery";
            sendData();
        }
    }else if(batteryVoltage>3.7){//little hysteresis to prevent multiple messages
        lowBattery=false;
    }
  }
  //********************
}

void sendData(){
     unsigned long startConnectTime = millis();
     char pushMessage[50], pushName[50];
     str1.toCharArray(pushName, str1.length() + 1);
     str2.toCharArray(pushMessage, str2.length() + 1);
     
     Serial.println(str1);
     Serial.println(str2);
     
     String pushoverPacket = "[{\"key\":\"title\", \"value\":\"";
     pushoverPacket.concat(str1);
     pushoverPacket.concat("\"},");
     pushoverPacket.concat("{\"key\":\"message\", \"value\":\"");
     pushoverPacket.concat(str2);
     pushoverPacket.concat("\"}]");
     Particle.publish("power_outage", pushoverPacket, PRIVATE);//then send to push safer so we get the notifications on our mobile devices

     Serial.print(millis() - startConnectTime);
     Serial.println("ms to connect");
}

/* return battery charge*/
float battery_charge(void) {
    return System.batteryCharge();
}

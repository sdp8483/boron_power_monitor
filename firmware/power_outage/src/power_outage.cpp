/* 
 * Project Cellular Power Loss Notification
 * Author: Sam Perry
 * Date: August 2024
 * 
 * Code Source: https://gist.github.com/krdarrah/314027798992a8ee29cc05a4cb47960f
 */

/* Includes ------------------------------------------------------------------ */
#include "Particle.h"       // Include Particle Device OS APIs
SYSTEM_MODE(AUTOMATIC);     // Let Device OS manage the connection to the Particle Cloud
SYSTEM_THREAD(ENABLED);     // Run the application and system concurrently in separate threads

/* Defines ------------------------------------------------------------------- */
#define LOW_BATTERY_NOTIFICATION    10.0f       // send pushover notification when battery drops below
#define BATTERY_HYSTERESIS          2.0f        // hysteresis to prevent mutiple messages

#define SUNDAY                      1
#define MONDAY                      2
#define TUESDAY                     3
#define WEDNESDAY                   4
#define THURSDAY                    5
#define FRIDAY                      6
#define SATURDAY                    7

/* Macros -------------------------------------------------------------------- */

/* Typedefs ------------------------------------------------------------------ */

/* Global Variables ---------------------------------------------------------- */
SerialLogHandler logHandler(LOG_LEVEL_INFO);    // Show system, cloud connectivity, and application logs over USB

/* Function Prototypes ------------------------------------------------------- */
void check_power_source(void);      // send notification when power source changes
void check_battery_charge(void);    // send low battery notification
void status_update(void);           // send weekly notification that device is alive
void send_notification(const char * title, const char * message);
String get_time(void);              // send time string to Particle console

/* Setup --------------------------------------------------------------------- */
void setup() {
    // wait for cloud connection
    while(!Particle.connected()) {
        delay(10);
    }

    // setup timezone
    Time.zone(-5);          // Set time zone to eastern USA
    Time.setDSTOffset(1);   // set daylight savings time offset
    Time.beginDST();        // begin DST

    // expose time string to particle console
    Particle.variable("time", get_time);

    /* get power source at boot */
    if (System.powerSource() == POWER_SOURCE_BATTERY) {
        send_notification("PDC Power Monitor Booting...", "Power Source: battery");
        Log.info("Battery power source at boot");
    } else {
        send_notification("PDC Power Monitor Booting...", "Power Source: external");
        Log.info("External power source at boot");
    }
} // setup()

/* Loop ---------------------------------------------------------------------- */
void loop() {
    // Particle.publish can block for long periods if no connection (up to 10 minutes)
    if (Particle.connected()) {
        check_power_source();
        check_battery_charge();
        status_update();
    }
}

/* check power source, send notification when power source changes ----------- */
void check_power_source(void) {
    static system_tick_t last_pwr_check = 0;                // last time power source was checked
    std::chrono::milliseconds pwr_check_interval = 1min;    // rate to check power source and send notifications 

    static bool power_source_external = false;              // power source is Vin or USB
    static bool power_source_battery = false;               // power source is internal battery

    if ((millis() - last_pwr_check) >= pwr_check_interval.count()) {
        last_pwr_check = millis();

        // check power source
        if (System.powerSource() == POWER_SOURCE_BATTERY) {
            power_source_battery = true;                    // using battery power source

            if (power_source_external) {                    // switching from using external power
                power_source_external = false;
                send_notification("PDC Power Monitor", "AC power lost");
                Log.info("using battery power source");
            }
        } else {
            power_source_external = true;                   // using external power source

            if (power_source_battery) {                     // switched from using battery power
                power_source_battery = false;
                send_notification("PDC Power Monitor", "AC power is on");
                Log.info("external power source connected");
            }
        }
    }
}

/* check battery charge, send a notification on low battery ------------------ */
void check_battery_charge(void) {
    static system_tick_t last_battery_check = 0;                // last time battery charge was checked
    std::chrono::milliseconds battery_check_interval = 30s;     // rate to check battery charge and send notification if low

    static int last_battery_state;                              // keep track of previous battery state to only send notification once
    static bool low_battery = false;                            // flag to send a push notification once

    if ((millis() - last_battery_check) >= battery_check_interval.count()) {
        last_battery_check = millis();

        if (last_battery_state != System.batteryState()) {      // battery state has changed, send a notification 
            last_battery_state = System.batteryState();

            switch (System.batteryState()) {
                case BATTERY_STATE_UNKNOWN:
                    send_notification("PDC Power Monitor", "battery state is unknown!");
                    Log.info("battery state unknown");
                    break;
                
                case BATTERY_STATE_NOT_CHARGING:
                    send_notification("PDC Power Monitor", "battery is not charging");
                    Log.info("battery is not charging");
                    break;

                case BATTERY_STATE_CHARGING:
                    // send_notification("PDC", "battery is charging");
                    Log.info("battery is charging");
                    break;

                case BATTERY_STATE_CHARGED:
                    // send_notification("PCD", "battery is charged");
                    Log.info("battery charged");
                    break;

                case BATTERY_STATE_DISCHARGING:
                    // send_notification("PCD", "battery is discharging");
                    Log.info("battery is discharging");
                    break;
                
                case BATTERY_STATE_FAULT:
                    send_notification("PDC Power Monitor", "battery fault!");
                    Log.error("battery fault");
                    break;

                case BATTERY_STATE_DISCONNECTED:
                    send_notification("PDC Power Monitor", "battery is disconnected");
                    Log.info("battery is disconnected");
                    break;

                default:
                    break;
            }
        }

        if (System.batteryCharge() < LOW_BATTERY_NOTIFICATION) {    // battery charge is below 10%, send notification
            if (!low_battery) {                                     // only send a notification if battery was not previously low
                low_battery = true;
                send_notification("PDC Power Monitor", "Low Battery");
            }
        }

        // reset low battery on charging with a little hysteresis
        if (System.batteryState() >= (LOW_BATTERY_NOTIFICATION + BATTERY_HYSTERESIS)) {
            low_battery = false;
        }
    }
}

/* send a weekly 'im alive' notification ------------------------------------- */
void status_update(void) {
    static system_tick_t last_check = 0;                // last system tick that the time was checked
    std::chrono::milliseconds check_interval = 15min;   // rate to check if it is time to send status

    static bool status_sent = false;                    // flag to send only one notification per sunday

    if (millis() - last_check >= check_interval.count()) {
        last_check = millis();

        /* make sure time is set correctly first */
        if (Time.isValid()) {
            /* only send on sundays after 9am dst */
            if ((Time.weekday() == SUNDAY) && (Time.hour() >= 9)) {
                if (!status_sent) {
                    status_sent = true;

                    send_notification("PDC Power Monitor", "I'm Alive and Well!");
                }
            }

            /* reset flag when not sunday */
            if (status_sent && (Time.weekday() != SUNDAY)) {
                status_sent = false;
            }
        }
    }
}

/* send a pushover notification ---------------------------------------------- */
void send_notification(const char * title, const char * message) {
    unsigned long startConnectTime = millis();

    Log.info(message);

    /* assemble packet for pushover notification */
    String pushoverPacket = "[{\"key\":\"title\", \"value\":\"";
    pushoverPacket.concat(title);
    pushoverPacket.concat("\"},");
    pushoverPacket.concat("{\"key\":\"message\", \"value\":\"");
    pushoverPacket.concat(message);
    pushoverPacket.concat("\"}]");
    
    Particle.publish("power_outage", pushoverPacket, PRIVATE);  // then send to push safer so we get the notifications on our mobile devices
    Log.info("%s %s", title, message);
    Log.info("%lu ms to connect", millis() - startConnectTime);
}

/* return local time string for Particle Console ----------------------------- */
String get_time(void) {
    return Time.timeStr();
}
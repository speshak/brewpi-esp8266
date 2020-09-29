//
// Created by John Beeler on 1/12/20.
//

#include "ESP_WiFi.h"

#ifdef ESP8266_WiFi

#include <FS.h>  // Apparently this needs to be first
#include "Brewpi.h"

#if defined(ESP8266)
#include <ESP8266mDNS.h>
#include <DNSServer.h>			//Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>	//Local WebServer used to serve the configuration portal
#include <WiFiManager.h>		//https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#elif defined(ESP32)
#include <ESPmDNS.h>
#include <DNSServer.h>			//Local DNS Server used for redirecting all requests to the configuration portal
#include <WiFiManager.h>		//https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#endif

#include "Version.h" 			// Used in mDNS announce string
#include "Display.h"
#include "EepromManager.h"
#include "EepromFormat.h"


bool shouldSaveConfig = false;
WiFiServer server(23);
WiFiClient serverClient;

extern void handleReset();  // Terrible practice. In brewpi-esp8266.cpp.


/**
 * \brief Callback notifying us of the need to save config
 */
void saveConfigCallback() {
    Serial.println("Should save config");
    shouldSaveConfig = true;
}


/**
 * \brief Initialize the telnet server
 */
void initWifiServer() {
  server.begin();
  server.setNoDelay(true);
}

// Not sure if this is sufficient to test for validity
bool isValidmDNSName(const String& mdns_name) {
//    for (std::string::size_type i = 0; i < mdns_name.length(); ++i) {
    for (char i : mdns_name) {
        // For now, we're just checking that every character in the string is alphanumeric. May need to add more validation here.
        if (!isalnum(i))
            return false;
    }
    return true;
}


void mdns_reset() {
    String mdns_id;
    mdns_id = eepromManager.fetchmDNSName();

    MDNS.end();

    if (!MDNS.begin(mdns_id.c_str())) {
        //Log.error(F("Error resetting MDNS responder."));
    } else {
        //Log.notice(F("mDNS responder restarted, hostname: %s.local." CR), WiFi.getHostname());
        // mDNS will stop responding after awhile unless we query the specific service we want
        MDNS.addService("brewpi", "tcp", 23);
        MDNS.addServiceTxt("brewpi", "tcp", "board", CONTROLLER_TYPE);
        MDNS.addServiceTxt("brewpi", "tcp", "branch", "legacy");
        MDNS.addServiceTxt("brewpi", "tcp", "version", VERSION_STRING);
        MDNS.addServiceTxt("brewpi", "tcp", "revision", FIRMWARE_REVISION);
    }
}

#if defined(ESP8266)
// This doesn't work for ESP32, unfortunately
WiFiEventHandler stationConnectedHandler;
void onStationConnected(const WiFiEventSoftAPModeStationConnected& evt) {
    server.begin();
    server.setNoDelay(true);
    mdns_reset();
}
#endif

void initialize_wifi() {
    String mdns_id;

    // TODO - Figure out how to make the below only appear when the AP is created
    // (it currently always appears at startup for a few seconds)
    DisplayType::printWiFiStartup();

    mdns_id = eepromManager.fetchmDNSName();
//	if(mdns_id.length()<=0)
//		mdns_id = "ESP" + String(ESP.getChipId());


    // If we're going to set up WiFi, let's get to it
    WiFiManager wifiManager;
    wifiManager.setConfigPortalTimeout(5*60); // Time out after 5 minutes so that we can keep managing temps
    wifiManager.setDebugOutput(false); // In case we have a serial connection to BrewPi

    // The main purpose of this is to set a boolean value which will allow us to know we
    // just saved a new configuration (as opposed to rebooting normally)
    wifiManager.setSaveConfigCallback(saveConfigCallback);

    // The third parameter we're passing here (mdns_id.c_str()) is the default name that will appear on the form.
    // It's nice, but it means the user gets no actual prompt for what they're entering.
    WiFiManagerParameter custom_mdns_name("mdns", "Device (mDNS) Name", mdns_id.c_str(), 20);
    wifiManager.addParameter(&custom_mdns_name);


    if(wifiManager.autoConnect(WIFI_SETUP_AP_NAME, WIFI_SETUP_AP_PASS)) {
        // If we succeeded at connecting, switch to station mode.
        // TODO - Determine if we can merge shouldSaveConfig in here
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_AP_STA);
    } else {
        // If we failed to connect, we still want to control temps. Disable the AP, and flip to STA mode
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_AP_STA);
    }

    // Alright. We're theoretically connected here (or we timed out).
    // If we connected, then let's save the mDNS name
    if (shouldSaveConfig) {
        // If the mDNS name is valid, save it.
        if (isValidmDNSName(custom_mdns_name.getValue())) {
            eepromManager.savemDNSName(custom_mdns_name.getValue());
        } else {
            // If the mDNS name is invalid, reset the WiFi configuration and restart the ESP8266
            WiFi.disconnect(true);
            delay(2000);
            handleReset();
        }
    }

    // Regardless of the above, we need to set the mDNS name and announce it
    mdns_reset();

    // This will trigger autoreconnection, but will not connect if we aren't connected at this point (e.g. if the AP is
    // not yet broadcasting)
    WiFi.setAutoReconnect(true);
}


void display_connect_info_and_create_callback() {
#if defined(ESP8266)
    // This doesn't work for ESP32, unfortunately.
    stationConnectedHandler = WiFi.onSoftAPModeStationConnected(&onStationConnected);
#endif
    DisplayType::printWiFi();  // Print the WiFi info (mDNS name & IP address)
    delay(5000);
}


/**
 * \brief Handle incoming WiFi client connections.
 *
 * This also handles WiFi network reconnects if the network was disconnected.
 */
void wifi_connect_clients() {
    static unsigned long last_connection_check = 0;

    yield();
    if(WiFi.isConnected()) {
        if (server.hasClient()) {
            // If we show a client as already being disconnected, force a disconnect
            if (serverClient) serverClient.stop();
            serverClient = server.available();
            serverClient.flush();
        }
    } else {
        // This might be unnecessary, but let's go ahead and disconnect any
        // "clients" we show as connected given that WiFi isn't connected. If
        // we show a client as already being disconnected, force a disconnect
        if (serverClient) {
            serverClient.stop();
            serverClient = server.available();
            serverClient.flush();
        }
    }
    yield();

    // Additionally, every 3 minutes either attempt to reconnect WiFi, or rebroadcast mdns info
    if(ticks.millis() - last_connection_check >= (180000)) {
        last_connection_check = ticks.millis();
        if(!WiFi.isConnected()) {
            // If we are disconnected, reconnect. On an ESP8266 this will ALSO trigger mdns_reset due to the callback
            // but on the ESP32, this means that we'll have to wait an additional 3 minutes for mdns to come back up
            WiFi.reconnect();
        } else {
            mdns_reset();
        }
    }
    yield();
}



#else
/*********************** Code for when we don't have WiFi enabled is below  *********************/

void initialize_wifi() {
    // Apparently, the WiFi radio is managed by the bootloader, so not including the libraries isn't the same as
    // disabling WiFi. We'll explicitly disable it if we're running in "serial" mode
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

void display_connect_info_and_create_callback() {
    // For now, this is noop when WiFi support is disabled
}
void wifi_connect_clients() {
    // For now, this is noop when WiFi support is disabled
}
#endif

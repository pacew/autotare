/*********************************************************************
 This is an example for our nRF52 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/
#include <bluefruit.h>

#define SLIDESWITCH 7

void startAdv(void);
void connect_callback(uint16_t conn_handle);
void disconnect_callback(uint16_t conn_handle, uint8_t reason);


// Beacon uses the Manufacturer Specific Data field in the advertising
// packet, which means you must provide a valid Manufacturer ID. Update
// the field below to an appropriate value. For a list of valid IDs see:
// https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers
// 0x004C is Apple (for example)
#define MANUFACTURER_ID   0x004C 

// AirLocate UUID: E2C56DB5-DFFB-48D2-B060-D0F5A71096E0
uint8_t beaconUuid[16] = 
{ 
  0xE2, 0xC5, 0x6D, 0xB5, 0xDF, 0xFB, 0x48, 0xD2, 
  0xB0, 0x60, 0xD0, 0xF5, 0xA7, 0x10, 0x96, 0xE0, 
};

// A valid Beacon packet consists of the following information:
// UUID, Major, Minor, RSSI @ 1M
BLEBeacon beacon(beaconUuid, 0x0000, 0x0000, -54);

BLEUart bleuart;


/* 75c3c6d0-75e4-4223-a823-bdc65e738996 */
const uint8_t AUTOTARE_UUID_SERVICE[] = {
  0x96, 0x89, 0x73, 0x5e, 0xc6, 0xbd, 0x23, 0xa8,
  0x23, 0x42, 0xe4, 0x75, 0xd0, 0xc6, 0xc3, 0x75,
};

/* 722bc7b5-1728-4e39-8867-3161d8dd5e20 */
const uint8_t AUTOTARE_UUID_WEIGHT[] = {
  0x20, 0x5e, 0xdd, 0xd8, 0x61, 0x31, 0x67, 0x88,
  0x39, 0x4e, 0x28, 0x17, 0xb5, 0xc7, 0x2b, 0x72,
};

BLEService autotare_service(AUTOTARE_UUID_SERVICE);
BLECharacteristic autotare_weight(AUTOTARE_UUID_WEIGHT);

uint8_t weight_state;

unsigned long last_millis;

void setup() 
{
  pinMode(LED_BUILTIN, OUTPUT);
  
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  while ( !Serial ) delay(10);   // for nrf52840 with native usb

  Serial.println("autotare");
  
  weight_state = 7;

  Bluefruit.begin();
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  autotare_service.begin();
  autotare_weight.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  autotare_weight.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  autotare_weight.setFixedLen(1);
  autotare_weight.begin();
  autotare_weight.write8(weight_state);




  // off Blue LED for lowest power consumption
  Bluefruit.autoConnLed(false);
  Bluefruit.setTxPower(0);    // Check bluefruit.h for supported values
  Bluefruit.setName("autotare");

  // Manufacturer ID is required for Manufacturer Specific Data
  beacon.setManufacturer(MANUFACTURER_ID);

  bleuart.begin();

  // Setup the advertising packet
  startAdv();

  Serial.println("ready");

  last_millis = millis();

  pinMode(SLIDESWITCH, INPUT_PULLUP);
}

void startAdv(void)
{  
  // Advertising packet
  // Set the beacon payload using the BLEBeacon class populated
  // earlier in this example
  Bluefruit.Advertising.setBeacon(beacon);

  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.Advertising.addService(autotare_service);


  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * Apple Beacon specs
   * - Type: Non connectable, undirected
   * - Fixed interval: 100 ms -> fast = slow = 100 ms
   */
  //Bluefruit.Advertising.setType(BLE_GAP_ADV_TYPE_ADV_NONCONN_IND);
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(160, 160);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds  
}

/* ~/.platformio/packages/framework-arduinoadafruitnrf52/libraries/Bluefruit52Lib/src/clients/ */

void loop() 
{
  static unsigned int val;
  
  val++;
  if ((val & 0xf000) == 0xf000)
    digitalWrite(LED_BUILTIN, HIGH);
  else
    digitalWrite(LED_BUILTIN, LOW);
    
  while (bleuart.available()) {
    int c = bleuart.read();
    char buf[100];
    buf[0] = c;
    buf[1] = 0;
    Serial.print(buf);

  }

  unsigned long delta = millis () - last_millis;
  if (delta > 1000) {
    last_millis = millis();

    weight_state++;

    int flag = 0;
    
    if (Bluefruit.connected()) {
      flag |= 1;
      if (autotare_weight.notify8(weight_state))
	flag |= 0x10;
    }

    char buf[100];
    char *p;

    p = buf;
    p += sprintf (p, "tick 0x%x 0x%x ",
		  weight_state, flag);
    *p = 0;
    Serial.println(buf);
  }

}

void
connect_callback(uint16_t conn_handle)
{
  Serial.println ("connected");
  if (0) {
    Serial.println("Keep advertising");
    Bluefruit.Advertising.start(0);
  }
}

void
disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  Serial.println("disconnect");
}
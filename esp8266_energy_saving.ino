//From post on http://cactusprojects.com/esp8266-deep-sleep-energy-saving/
//adapted from the blog post series at https://www.bakke.online/index.php/2017/06/24/esp8266-wifi-power-reduction-avoiding-network-scan/

// !!! You need to enter your WIFI details below and also static IP details on lines 57 to 59 !!! //

#include <ESP8266WiFi.h>
#include <InfluxDb.h>

#define INFLUXDB_HOST "INSERT"
#define WIFI_SSID "INSERT"
#define WIFI_PASS "INSERT"
#define DATABASE "INSERT"

Influxdb influx(INFLUXDB_HOST);

int SLEEPTIME = 10000000; //10 second sleep time.
bool rtcValid;

// The ESP8266 RTC memory is arranged into blocks of 4 bytes. The access methods read and write 4 bytes at a time,
// so the RTC data structure should be padded to a 4-byte multiple.
struct {
  uint32_t crc32;   // 4 bytes
  uint8_t channel;  // 1 byte,   5 in total
  uint8_t bssid[6]; // 6 bytes, 11 in total
  uint8_t padding;  // 1 byte,  12 in total
} rtcData;

void setup() {

  WiFi.mode( WIFI_OFF );
  WiFi.forceSleepBegin();
  delay( 1 );

  pinMode(LED_BUILTIN, OUTPUT);    //D4
  digitalWrite(LED_BUILTIN, HIGH); //Turns it off
  
  Serial.begin(9600);
  Serial.println("");
  Serial.println("Awake");

  // Try to read WiFi settings from RTC memory
  rtcValid = false;
  if( ESP.rtcUserMemoryRead( 0, (uint32_t*)&rtcData, sizeof( rtcData ) ) ) {
    // Calculate the CRC of what we just read from RTC memory, but skip the first 4 bytes as that's the checksum itself.
    uint32_t crc = calculateCRC32( ((uint8_t*)&rtcData) + 4, sizeof( rtcData ) - 4 );
    if( crc == rtcData.crc32 ) {
      rtcValid = true;
      Serial.println("RTC CR32 Good");
    }
  }
  database_write(); 
}

void database_write() {
  influx.setDb(DATABASE);
  Serial.print("Using Database: ");Serial.println(DATABASE);
  IPAddress ip( 192, 168, XXX, XXX ); //Static IP of this device
  IPAddress gateway( 192, 168, XXX, XXX );
  IPAddress subnet( 255, 255, 255, 0 );
  WiFi.forceSleepWake();
  delay( 1 );
  WiFi.persistent( false );
  WiFi.mode( WIFI_STA );
  WiFi.config( ip, gateway, subnet );

  if( rtcValid ) {
    // The RTC data was good, make a quick connection
    WiFi.begin( WIFI_SSID, WIFI_PASS, rtcData.channel, rtcData.bssid, true );
  }
  else {
    // The RTC data was not valid, so make a regular connection
    WiFi.begin( WIFI_SSID, WIFI_PASS );
    int loops = 0;
    int retries = 0;
    int wifiStatus = WiFi.status();
    while ( wifiStatus != WL_CONNECTED )
    {
    retries++;
    if( retries == 300 )
    {
    Serial.println( "No connection after 300 steps, powercycling the WiFi radio. I have seen this work when the connection is unstable" );
    WiFi.disconnect();
    delay( 10 );
    WiFi.forceSleepBegin();
    delay( 10 );
    WiFi.forceSleepWake();
    delay( 10 );
    WiFi.begin( WIFI_SSID, WIFI_PASS );
    }
    if ( retries == 600 )
    {
    WiFi.disconnect( true );
    delay( 1 );
    WiFi.mode( WIFI_OFF );
    WiFi.forceSleepBegin();
    delay( 10 );
    
    if( loops == 3 )
    {
    Serial.println( "That was 3 loops, still no connection so let's go to deep sleep for 2 minutes" );
    Serial.flush();
    ESP.deepSleep( 120000000, WAKE_RF_DISABLED );
    }
    else
    {
    Serial.println( "No connection after 600 steps. WiFi connection failed, disabled WiFi and waiting for a minute" );
    }
    
    delay( 60000 );
    return;
    }
    delay( 50 );
    wifiStatus = WiFi.status();
    }
    // Write current connection info back to RTC
    rtcData.channel = WiFi.channel();
    memcpy( rtcData.bssid, WiFi.BSSID(), 6 ); // Copy 6 bytes of BSSID (AP's MAC address)
    rtcData.crc32 = calculateCRC32( ((uint8_t*)&rtcData) + 4, sizeof( rtcData ) - 4 );
    ESP.rtcUserMemoryWrite( 0, (uint32_t*)&rtcData, sizeof( rtcData ) );
  }

  Serial.print("WiFi connected, IP Address: ");Serial.println(WiFi.localIP());
  
  InfluxData row("data");
  row.addValue("RandomValue", random(10, 40));
  influx.write(row);

  WiFi.disconnect( true );
  delay( 1 );
  Serial.println("Sleeping");
  Serial.flush();
  ESP.deepSleep( SLEEPTIME, WAKE_RF_DISABLED );
}

uint32_t calculateCRC32( const uint8_t *data, size_t length ) {
  uint32_t crc = 0xffffffff;
  while( length-- ) {
    uint8_t c = *data++;
    for( uint32_t i = 0x80; i > 0; i >>= 1 ) {
      bool bit = crc & 0x80000000;
      if( c & i ) {
        bit = !bit;
      }

      crc <<= 1;
      if( bit ) {
        crc ^= 0x04c11db7;
      }
    }
  }

  return crc;
}

void loop() {
}

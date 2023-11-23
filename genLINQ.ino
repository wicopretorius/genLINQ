
#include "opta_info.h"
#include <ArduinoMqttClient.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <ArduinoModbus.h>
#include <ArduinoRS485.h> // ArduinoModbus depends on the ArduinoRS485 library
#include <string.h>
#include <math.h>
#include "ABB_B21.h"

  OptaBoardInfo* info;
  OptaBoardInfo* boardInfo();
  byte mac[6];
  String strmac[6];
  //bool timeIsSet = false;
  
  //Json Document Setup
  //StaticJsonDocument<200> doc;

  //Set MQTT broker properties
  String     clientID;
  IPAddress  ipBroker(102,219,85,225); //private VPS host
  const char broker[] = "broker.emqx.io";
  int        port     = 1883;
  const char topic[]  = "site/rose_ave_9b/meter/matthew_pc";

  //set interval for sending messages (milliseconds)
  const long interval = 60000;
  unsigned long previousMillis = 0;
  int count = 0;

  //Time Server setup
  unsigned int localPort = 8888;       // local port to listen for UDP packets
  const char timeServer[] = "time.nist.gov"; // time.nist.gov NTP server
  const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
  byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

  //RS485 setup
  constexpr auto baudrate { 9600 };
  constexpr auto bitduration { 1.f / baudrate };
  constexpr auto preDelayBR { bitduration * 9.6f * 3.5f * 1e6 };
  constexpr auto postDelayBR { bitduration * 9.6f * 3.5f * 1e6 };

EthernetUDP Udp;
EthernetClient ethClient;
MqttClient mqttClient(ethClient);

void setup() {
  
  //Initialize serial and wait for port to open:
  Serial.println("Trying to connect Serial Port");
  Serial.begin(9600);
  //while (!Serial) {
  //  delay(1000); // wait for serial port to connect. Needed for native USB port only
  //}
  Serial.println("Serial Connected");
  delay(2000);
  
  //Obtain MAC address info from board.
  info = boardInfo();
  Serial.println("Obtaining MAC Address");
  for (int i = 0; i < 6; ++i) {
    mac[i] = info->mac_address[i]; //Load MAC address info from board in to mac variable for ethernet use.
    if (mac[i] < 0x10){            //Convert MAC address info to string characters for MQTT Client ID, also check if each mac address is 2 HEX characters by adding zero to the front of singgle characters
      strmac[i] = "0" + String(info->mac_address[i],HEX);
    } else{
      strmac[i] = String(info->mac_address[i],HEX);
    }
  }
  
  //Initialize Ethernet and connect to local network.
  Serial.println("Trying to connect ethernet client");
  while (!Ethernet.begin(mac)) {
    Serial.println("Failed to obtaining an IP address using DHCP");
    delay(1000);
  }
  Serial.print("You're connected to the network. IP address: ");
  Serial.println(Ethernet.localIP());
  delay(2000);

  //Connect to NTP Server to obtain current EPOCH time
  Udp.begin(localPort);
  delay(1000);
  sendNTPpacket(timeServer); // send an NTP packet to a time server
  delay(1000); // wait to see if a reply is available
  if (Udp.parsePacket()) {
    // We've received a packet, read the data from it
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    const unsigned long seventyYears = 2208988800UL;
    unsigned int timeZoneOffset = 7200; //GMT=0, GMT-+1=-+3600, GMT-+2=-+7200, GMT-+3=-+10800, GMT-+4=-+14400
    unsigned long epoch = secsSince1900 - seventyYears + timeZoneOffset ;
    set_time(epoch);
  }
  Udp.endPacket();

  //Initialize MQTT client and conncect to broker.
  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);
  clientID = "opta_" + strmac[0] + ":" + strmac[1] + ":" + strmac[2] + ":" + strmac[3] + ":" + strmac[4] + ":" + strmac[5];
  mqttClient.setId(clientID);
  // mqttClient.setUsernamePassword("username", "password");
  while (!mqttClient.connect(ipBroker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    delay(1000);
  }
  Serial.println("You're connected to the MQTT broker!");
  delay(2000);

   // Start the Modbus RTU client
  Serial.println("Modbus RTU Client Started");
  RS485.setDelays(preDelayBR, postDelayBR);
  if (!ModbusRTUClient.begin(baudrate, SERIAL_8N1)) {
      Serial.println("Failed to start Modbus RTU Client!");
      while (1);
  }
  delay(2000);
}

void loop() {
  int slaveAddr = 1;

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time a message was sent
    previousMillis = currentMillis;

    Serial.println("Reading Holding Register values of ABB B21 Meter... ");
    time_t seconds = time(NULL);
    doc["timestamp"] = seconds;  //Get current seconds form RTC
    readHoldingRegisters_ABB_B21();
  
    Serial.print("Sending message to topic: ");
    Serial.println(topic);
    serializeJson(doc, Serial);
    
    mqttClient.beginMessage(topic);
    serializeJson(doc, mqttClient);
    ////mqttClient.print(message);
    mqttClient.endMessage();
    Serial.println();

  }
  //delay(1000);
}

// send an NTP request to the time server at the given address
void sendNTPpacket(const char * address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); // NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
/*
* Remote Button Press
*
* This app is using ESP8266 NodeMCU to control a servo which performs an action to press a button.
*
* Hardware Requirement:
* - ESP8266 NodeMCU
* - 90g servo & connecting wires
* - power supply
* - materials help to fix the servo next to the button
*
* Software coding is divided into the following parts:
* - wifi setup
* - connection to MQTT server and subscribe to a topic
* - parse incoming message from the MQTT server
* - activate the servo to perform a press action
* - feedback to MQTT server when done
* - Can use https://eclipse.org/paho/clients/js/utility/ for testing
*
*/
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h> // for MQTT connection
#include <Wire.h> // I2C for Display
#include <Adafruit_GFX.h> // Display
#include <Adafruit_SSD1306.h> // Display

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define LED D0            // Led in NodeMCU at pin GPIO16 (D0)

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define NUMFLAKES     10 // Number of snowflakes in the animation example

#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16
static const unsigned char PROGMEM logo_bmp[] =
{ 0b00000000, 0b11000000, 0b00000001, 0b11000000, 0b00000001, 0b11000000, 0b00000011, 0b11100000,
  0b11110011, 0b11100000, 0b11111110, 0b11111000, 0b01111110, 0b11111111, 0b00110011, 0b10011111,
  0b00011111, 0b11111100, 0b00001101, 0b01110000, 0b00011011, 0b10100000, 0b00111111, 0b11100000,
  0b00111111, 0b11110000, 0b01111100, 0b11110000, 0b01110000, 0b01110000, 0b00000000, 0b00110000 };

//const char* mqttServer = "iot.eclipse.org";
const char* mqttServer = "public.mqtthq.com";
const char* inTopic = "esp8266_ruft_in";
const char* outTopic = "esp8266_ruft_out";
char msg[75];
char msg_serial[75];
char msg_mtqq[75];
String screen_msg = "";
String serial_received = "";

long lastMsg = 0;
long lastMsg2 = 0;
int value = 0;
String dString = "";
WiFiClient espClient;
PubSubClient client(espClient);
WiFiManager wifiManager;

/*
* handle message arrived from MQTT and do the real actions depending on the command
* payload format: start with a single character
* P: button press, optionally follows by (in any order)
* Dxxx: for xxx seconds duration,
* Exxx: ending at xxx angle
* R: reset wifi settings
*/
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // debugging message at serial monitor
  char messageBuffer[30];  //somewhere to put the message
  memcpy(messageBuffer, payload, length);  //copy in the payload
  messageBuffer[length] = '\0';  //convert copied payload to a C style string

  String topicStr = topic; 
  String recv_payload = String(( char *) payload);
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {Serial.print((char)payload[i]);}
  Serial.println();
  // parse the payload message
  if ((char)payload[0] == 'R') {
    client.publish(outTopic, "Resetting wifi!");
    Serial.println("Resetting wifi!"); // debug message
    wifiManager.resetSettings(); // reset all wifi settings, should back to AP mode
  } else if ((char)payload[0] == 'P') {
    client.publish(outTopic, "Servo Request - deprecated!");
    Serial.println("Servo Request - deprecated!"); // debug message
  } else if (strcmp(messageBuffer, "LED_off")==0) {
    client.publish(outTopic, "LED OFF!");
    Serial.println("LED to be switched off"); // debug message
    Serial.println(messageBuffer); // debug message
    snprintf(msg_mtqq, 75, "mtqq: %ls", messageBuffer);    
    digitalWrite(LED, HIGH);
  } else if (strcmp(messageBuffer,"LED_on")==0) {
    client.publish(outTopic, "LED ON!");
    Serial.println("LED to be switched on"); // debug message
    snprintf(msg_mtqq, 75, "mtqq: %ls", messageBuffer);
    digitalWrite(LED, LOW);
  } else {
  //snprintf(msg, 75, "Unknown command: %s, do nothing!", (char)payload[0]);
  snprintf(msg, 75, "Unknown command: %c, do nothing!", (char)payload[0]);
  client.publish(outTopic, msg);
  snprintf(msg_mtqq, 75, "mtqq: %ls", msg);
  Serial.println(msg); // debug message
  }
}

/*
* connect to MQTT with a client ID, subscribe & publish to corresponding topics
* if failed, reconnect in 5 seconds
*/
void reconnect() {
  // loop until reconnected
  while (!client.connected()) {
    Serial.print("Attempting to make MQTT connection...");
    if (client.connect("ESP8266_RUFT1")) { // ESP8266CLient1 is a client ID of this ESP8266 for MQTT
      Serial.println("connected");
      client.publish(outTopic, "Hello world, I'm ESP8266_RUFT1");
      client.subscribe(inTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
      }
    }
  }

void screen_display(String msg_title, String msg_serial, String msg_mtqq){
    // Clear the buffer
  display.clearDisplay();
 
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  // Display static text
  display.println(msg_title.c_str());
  display.println(msg_serial.c_str());
  display.println(msg_mtqq.c_str());
  display.display(); 
}

/*********************************
* SETUP
/********************************/
void setup() {
  pinMode(LED, OUTPUT);    // LED pin as output
  Wire.begin();        // join I2C bus (address optional for master)

  pinMode(LED_BUILTIN, OUTPUT); // just for LED output
  Serial.begin(115200); // connect to serial mainly for debugging

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();
 
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  // Display static text
  display.println("mtqq Example");
  display.display(); 
  delay(5000);

  // use WiFiManger to manage wifi setup
  // if not auto connect, connect to 'myAP' wifi network, access 192.168.4.1 to do a local wifi setup
  wifiManager.autoConnect("myAP");
  Serial.println("wifi connected!");
  // prepare for MQTT connection
  client.setServer(mqttServer, 1883);
  client.setCallback(mqttCallback);
  Serial.println("Enter data:");

  }

/*********************************
* LOOP
**********************************/
void loop() {
  // if not connected to mqtt server, keep trying to reconnect
  if (!client.connected()) {reconnect();}

  client.loop(); // wait for message packet to come & periodically ping the server
  // to show that ESP8266 is alive, publish a message every 2 seconds to the MQTT broker
  long now = millis();
  if (now - lastMsg > 4000) {
    lastMsg = now;
    ++value;
    snprintf(msg, 75, "Hello world #%ld", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish(outTopic, msg);
    }

  long now2 = millis();
  if (now2 - lastMsg2 > 5000) {
    lastMsg2 = now2;
    screen_display("mtqq:", msg_serial, msg_mtqq);
  }

  // if serial data available, process it
  while (Serial.available () > 0){
    serial_received = Serial.readString();
    serial_received.trim();
    Serial.print("Publish message: ");
    Serial.println(serial_received);
    snprintf(msg_serial, 75, "serial: %ls", serial_received);
    client.publish(outTopic, msg_serial);
    Serial.println("Enter data:");
    //screen_display("mtqq:", msg_serial, msg_mtqq);
    }
  }
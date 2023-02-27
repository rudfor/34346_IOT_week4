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
#include <Servo.h> // for servo movement
//const char* mqttServer = "iot.eclipse.org";
const char* mqttServer = "public.mqtthq.com";
const char* inTopic = "esp8266_ruft_in";
const char* outTopic = "esp8266_ruft_out";
char msg[75];
long lastMsg = 0;
int value = 0;
int duration, posStart, posEnd;
String dString = "";
WiFiClient espClient;
PubSubClient client(espClient);
Servo myservo;
WiFiManager wifiManager;

/*
* control the servo to mimic a button press action
* duration in seconds, posStart & posEnd in degrees
*/
void buttonPress(unsigned int duration, unsigned int posStart, unsigned int posEnd) {
  myservo.write(posStart); // make sure at starting position
  delay(100); // let servo stops properly
  // start the movement
  myservo.write(posEnd); // move to ending position
  delay(duration*1000); // stay there for a given duration
  myservo.write(posStart); // back to starting position
  delay(100); // let servo stops properly
  }

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
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  // parse the payload message
  duration = 1;
  posStart = 0;
  posEnd = 90;
  if ((char)payload[0] == 'R') {
    client.publish(outTopic, "Resetting wifi!");
    Serial.println("Resetting wifi!"); // debug message
    wifiManager.resetSettings(); // reset all wifi settings, should back to AP mode
  } else if ((char)payload[0] == 'P') {
    if (length > 1) {
      for (int i = 1; i < length; i++) {
        dString = "";
        if ((char)payload[i] == 'D') { // modify 'duration', default 1 second
          for (int j = 1; j < 4; j++) {
            dString += (char)payload[i+j];
          }
          duration = dString.toInt();
        }
        if ((char)payload[i] == 'E') { // modify 'ending position', default at 90 degress
          for (int j = 1; j < 4; j++) {
            dString += (char)payload[i+j];
          }
        posEnd = dString.toInt();
        }
      }
    }
    buttonPress(duration, posStart, posEnd); // perform servo press
    snprintf(msg, 75, "Button pressed for %d second(s) at %d degrees!", duration, posEnd);
    client.publish(outTopic, msg);
    Serial.println(msg); // debug message
  } else {
  //snprintf(msg, 75, "Unknown command: %s, do nothing!", (char)payload[0]);
  snprintf(msg, 75, "Unknown command: %c, do nothing!", (char)payload[0]);
  client.publish(outTopic, msg);
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
    if (client.connect("ESP8266Client1")) { // ESP8266CLient1 is a client ID of this ESP8266 for MQTT
      Serial.println("connected");
      client.publish(outTopic, "Hello world, I'm ESP8266Client1");
      client.subscribe(inTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
      }
    }
  }

void setup() {
  pinMode(LED_BUILTIN, OUTPUT); // just for LED output
  Serial.begin(115200); // connect to serial mainly for debugging
  // prepare servo
  myservo.attach(2); // attach the servo at GIO2 at it is right next to 3.3V and GND pins
  myservo.write(0); // make sure start from 0
  // use WiFiManger to manage wifi setup
  // if not auto connect, connect to 'myAP' wifi network, access 192.168.4.1 to do a local wifi setup
  wifiManager.autoConnect("myAP");
  Serial.println("wifi connected!");
  // prepare for MQTT connection
  client.setServer(mqttServer, 1883);
  client.setCallback(mqttCallback);
  }

void loop() {
  // if not connected to mqtt server, keep trying to reconnect
  if (!client.connected()) {reconnect();}

  client.loop(); // wait for message packet to come & periodically ping the server
  // to show that ESP8266 is alive, publish a message every 2 seconds to the MQTT broker
  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    ++value;
    snprintf(msg, 75, "Hello world #%ld", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish(outTopic, msg);
    }
  }
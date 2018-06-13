
/*
   Authors :   Guy Griffiths
  
   Released to Public Domain
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
 */

 #include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h> 

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>

#define FASTLED_ESP8266_D1_PIN_ORDER
#include <FastLED.h>

#include <PubSubClient.h>

 // lamp current RGB values
byte red = 0;
byte green = 0;
byte blue = 0;

byte huePosition  = 0;
byte brightnessPosition = 0;


 // lamp sequence modes
#define SM_FIXED 0
#define SM_BLINK 1
#define SM_FLASH 2
#define SM_RAMP 3
#define SM_FADE 4
#define SM_PATTERN 5
#define SM_RAINBOW 6

// TODO: create a set of Class to encapulate sequencer behaviour in a strategy pattern


byte rampStepSize = 1;

// sequence mode controller states
bool lampToggle = false;

String pattern ;
unsigned int patternLength;
unsigned int patternIndex = 0;

unsigned long lastLampSequenceStateChangeTime = 0; 

unsigned int lampSequenceStepDuration = 100; // milliseconds
byte lampSequenceMode = SM_FIXED;
boolean isSequencingLamp = false;

#define TEXTPLAIN "text/plain"
#define TEXTJSON "application/json"



// for Wemos D1 mini
#define SWITCH3_PIN D3  // 433 Mhz decoder chip D3 output 
#define BUZZER_PIN D8
#define USB_A_POWER_PIN    D7   // MOSFET gate  to switch 5V on USB-A socket
#define NEO_PIN    D6    // Digital IO pin connected to the NeoPixels.


// RF433 receiver configuration

#define RM_USB 0
#define RM_CYCLE 1
#define RM_LAMP 2
#define RM_BLINK 3
#define RM_BEEP 4
#define RM_MELODY 5

unsigned int RF433_push_count = 0;
byte RF433_mode = RM_USB;

volatile byte RF433_SW3_isr_flag = HIGH;  //volatile = tell the compiler that this is changed by an interrupt

void RF433_SW3wasPressed() {    //the ISR - good practice to keep it as brief as possible
  RF433_SW3_isr_flag = LOW;
  RF433_push_count++;

}
void doRF433Sw3Action() {
  
  switch ( RF433_mode ) {
    
    case RM_USB :
      USBPowerToggle();
    break;
    
    case RM_CYCLE :
      toggleLampSequenceState();
    break;
    
    case RM_LAMP :
      toggleLamp();
    break;
    
    case RM_BLINK :
      doBlinkStep();
    break;
    
    case RM_BEEP :
      playBeep();
    break;
    
    case RM_MELODY :
      playMelody();
    break;
    
  }
 }

//define your default config values here, if there are different values in SPIFFS config.json, they are overwritten.
char hostName[40];
char mqtt_server[40];
char mqtt_port[6] = "1883";

bool usingMQTT = false;

#define MQTT_DEVICE_CONNECT_ATTEMPT_PERIOD 10000
#define MQTT_DEVICE_STATUS_PERIOD 5000

#define MQTT_DEVICE_STATUS_TOPIC "orbs/deviceStatus"
#define MQTT_DEVICE_COMMAND_TOPIC "orbs/deviceCommand"

boolean usingAccessPoint;

WiFiClient espClient;
PubSubClient pubSubClient(espClient);

long lastMQTTReconnectAttempt = 0;

void mqttCallback(char* topic, byte* payload, unsigned int length) {

  // TODO: check topic?
  
  char operation = (char)payload[0];
  
  if ( operation == '3' ) {
    RF433_SW3wasPressed();
    return ;
  }

  if ( operation == 'a' ) {
    toggleLampSequenceState();
    return ;
  }
  if ( operation == 's' ) {
    startLampSequence(SM_FIXED);
    return ;
  }
  if ( operation == 'f' ) {
    startLampSequence(SM_FLASH);
    return ;
  }
  if ( operation == 'u' ) {
    startLampSequence(SM_RAINBOW);
    return ;
  }

  // default to setting lamp colour
  setLampColour( operation );
  
}

// ######################################################
//
//  MQTT Server connection
//
//
void tryToConnectToMQTTServer() {

  unsigned long now = millis();

  if (now - lastMQTTReconnectAttempt > MQTT_DEVICE_CONNECT_ATTEMPT_PERIOD) {
    
    lastMQTTReconnectAttempt = now;
  
    if ( reconnectToMQTTServer() ) {
      
      lastMQTTReconnectAttempt = 0;
    }
  }
}
bool reconnectToMQTTServer() {
      
    Serial.print("Attempting MQTT connection ...");
   
    // Attempt to connect
    if (pubSubClient.connect(hostName)) {
      Serial.println("connected to MQTT server");
      
      String status = buildJSONStatus("MQTT connected");

      sendMQTTStatusMessage( status );
     
      pubSubClient.subscribe(MQTT_DEVICE_COMMAND_TOPIC);

      return true;
    }

    Serial.print("failed, rc=");
    Serial.println( pubSubClient.state() );

    return false;

}

void sendMQTTStatusMessage( String message ) {

      if ( usingMQTT ) 
          pubSubClient.publish(MQTT_DEVICE_STATUS_TOPIC, const_cast<char*>(message.c_str()) );
}

//flag for saving Portal config data
bool shouldSavePortalConfig = false;

//callback notifying us of the need to save Wifi portal config
void saveConfigCallback () {
  shouldSavePortalConfig = true;
}

//#################################################################################
//
// USB-A functions

byte USB_A_state = LOW;   // the state of the USB-A socket 5V power rail

void USBPowerOn(){
      USB_A_state = HIGH;
      digitalWrite( USB_A_POWER_PIN , USB_A_state);
}
void USBPowerOff(){
      USB_A_state = LOW;
      digitalWrite( USB_A_POWER_PIN , USB_A_state);
}

void USBPowerToggle(){
      USB_A_state = !USB_A_state;
      digitalWrite( USB_A_POWER_PIN , USB_A_state);
}

/*
 *  Sound
 * 
 */
#define NOTE_G3  196
#define NOTE_A3  220
#define NOTE_B3  247
#define NOTE_C4  262

// notes in the melody:
int melodyNotes[] = { NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4 };

// note durations: 4 = quarter note, 8 = eighth note, etc.:
int noteDurations[] = { 4, 8, 8, 4, 4, 4, 4, 4 };

void playMelody() {

  for (byte currentNote = 0; currentNote < 8; currentNote++) {

    // to calculate the note duration, take one second divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 1000 / noteDurations[currentNote];
    tone(BUZZER_PIN, melodyNotes[currentNote], noteDuration);

    // to distinguish the notes, set a minimum time between them.
    // the note's duration + 30% seems to work well:
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    
    noTone(BUZZER_PIN);
  }
}

void playBeep() {
      tone(BUZZER_PIN, NOTE_C4, 250 );
      noTone(BUZZER_PIN);
}

/*
 * NEO PIXEL -  7 LED RING - Lamp
 */
#define PIXEL_COUNT 7
CRGB pixels[PIXEL_COUNT];


void startLampSequence( byte mode , int pixelOnTime) {
  
  if ( pixelOnTime > 0 ) setLampSequenceStepDuration(pixelOnTime);
  setLampSequenceMode(mode);
  startLampSequence();
}

void startLampSequence( byte mode ) {
  
  setLampSequenceMode(mode);
  startLampSequence();
}

void startLampSequence() {
  isSequencingLamp = true;
}

void stopLampSequence(){
  isSequencingLamp = false;
}
void toggleLampSequenceState() {
  isSequencingLamp = ! isSequencingLamp;
}

void setLampSequenceMode( byte newMode ) {
  lampSequenceMode = newMode;
}

void setLampSequenceStepDuration( int newDuration ) {
  lampSequenceStepDuration = newDuration;
}

// Lamp colour controls

void setLampColour( char c ) {

    switch  ( c ) {

      case 'r' :
        setLampColour(128,0,0);
      break;
      
      case 'R' :
        setLampColour(255,0,0);
      break;
      
      case 'g' :
        setLampColour(0,128,0);
      break;
      
      case 'G' :
        setLampColour(0,255,0);
      break;
      
      case 'b' :
        setLampColour(0,0,128);
      break;
      
      case 'B' :
        setLampColour(0,0,255);
      break;
      
      case 'w' :
        setLampColour(128,128,128);
      break;
      
      case 'W' :
        setLampColour(255,255,255);
      break;
      
      case 'z' :
      case 'Z' :
        setLampColour(0,0,0);
      break;
      
      case 'i' :
        doFadeStep();
      break;
      
      case 'I' :
        doRampStep();
      break;

    }

}
void setLampColour(byte redV, byte greenV, byte blueV){

   red=redV;
   green = greenV;
   blue = blueV;

   updateNeoPixels(); 
}

void setLampColour(CRGB colour){

   red=colour.red;
   green = colour.green;
   blue = colour.blue;

   updateNeoPixels(); 
}

void setLampBrightness( byte brightness) {

  FastLED.setBrightness( brightness);
  updateNeoPixels(); 
}

void setLampHue( byte hue ){

  CRGB rgb;
  hsv2rgb_rainbow( CHSV( hue, 255 , 255) , rgb);

  red= rgb.red;
  green= rgb.green;
  blue= rgb.blue;

  updateNeoPixels(); 
}

void updateLamp(){

  updateNeoPixels(); 
}

void setLampOff(){
  
  setNeoPixelsOff();
}

// NEO pixel controls
void setNeoPixelsOff(){

   setNeoPixel(0,0,0,0);
   setNeoPixel(1,0,0,0);
   setNeoPixel(2,0,0,0);
   setNeoPixel(3,0,0,0);
   setNeoPixel(4,0,0,0);
   setNeoPixel(5,0,0,0);
   setNeoPixel(6,0,0,0);

   FastLED.show(); 

}
void updateNeoPixels(){
   
   setNeoPixel(0,  red ,green ,blue  );
   setNeoPixel(1,  red ,green ,blue  );
   setNeoPixel(2,  red ,green ,blue  );
   setNeoPixel(3,  red ,green ,blue  );
   setNeoPixel(4,  red ,green ,blue  );
   setNeoPixel(5,  red ,green ,blue  );
   setNeoPixel(6,  red ,green ,blue  );
   
   FastLED.show(); 

}
void setNeoPixel(byte pixelIndex, byte redValue, byte greenValue, byte blueValue ){
   
  pixels[pixelIndex].setRGB(redValue, greenValue, blueValue) ;

}


byte stepRampValue( byte value ) {
  return ( value + rampStepSize) & 255;
}
byte stepFadeValue( byte value ) {
  return ( value - rampStepSize) & 255;
}


//###########################################################################
//
// Lamp Sequencer step functions
//

void toggleLamp() {

  doFlashStep();
}

void doBlinkStep() {
  
  if ( lampToggle ) { 
    updateLamp();
    lampToggle = false;
  } else {
    lampToggle = false;
    setLampOff();
  }
}

void doFlashStep() {
  
  if ( lampToggle ) { 
    updateLamp();
    lampToggle = false;
  } else {
    lampToggle = true;
    setLampOff();
  }
}

void doFadeStep() {
   
  brightnessPosition  = stepFadeValue(brightnessPosition);
    
  setLampBrightness( brightnessPosition ); 

}

void doRampStep() {

  brightnessPosition  = stepRampValue(brightnessPosition);
    
  setLampBrightness( brightnessPosition ); 

}


void doRainbowStep(){

  huePosition = stepRampValue(huePosition);
  
  setLampHue(  huePosition );
}


void doPatternStep() {
  
   char c = pattern[patternIndex];

   patternIndex++;
   if ( patternIndex >= patternLength ) patternIndex = 0;

   setLampColour( c );
}


void doNextLampSequenceStep(){

  unsigned long now = millis();
  
  if (  now - lastLampSequenceStateChangeTime < lampSequenceStepDuration ) return ;
 
  lastLampSequenceStateChangeTime = now;

  switch ( lampSequenceMode ) {

    case SM_FIXED : // do nothing
    break;

    case SM_BLINK :
      doBlinkStep();
    break;

    case SM_FLASH :
      doFlashStep();
    break;

    case SM_FADE :
      doFadeStep();
    break;

    case SM_RAMP :
      doRampStep();
    break;

    case SM_PATTERN :
      doPatternStep();
    break;

    case SM_RAINBOW :
      doRainbowStep();
    break;

    default:
    break;
  }

}

String buildJSONStatus(const char* message){
  String output;

  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  JsonArray& colour = json.createNestedArray("colour");
  colour.add(red);
  colour.add(green);
  colour.add(blue);
  
  JsonObject& sequencer = json.createNestedObject("sequencer");
  sequencer["mode"] = lampSequenceMode;
  sequencer["step"] = lampSequenceStepDuration;
  sequencer["running"] = isSequencingLamp;

  json["usb_a_power"] = USB_A_state;

  json["rf433_mode"] = RF433_mode;
  json["rf433_count"] = RF433_push_count;

  json["hostname"] = hostName;
  json["mqtt_server"] = mqtt_server;
  json["mqtt_port"] = mqtt_port;
  json["mqtt_is_connected"] = pubSubClient.connected();

  json["message"] = message;

  json.printTo(output);

  return output;
  
}

ESP8266WebServer webserver(80);   // HTTP webserver will listen at port 80

//####################################################
//
// Web Server handlers 
//

String getContentType(String filename){
  
  if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".json")) return "application/json";

  return "text/plain";
}

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  
  if (path.endsWith("/")) path += "index.html";         // If a folder is requested, send the index file
  
  String contentType = getContentType(path);            // Get the MIME type based on file extension
  
  if (SPIFFS.exists(path)) {                           // If the file exists
    File file = SPIFFS.open(path, "r");                 // Open it
    webserver.streamFile(file, contentType); // And send it to the client
    file.close();                                       // Then close the file again
    return true;
  }
  
  return false;                                         // If the file doesn't exist, return false
}

void handleNotFound() {

  String message = "Unhandled URL\n\n";
  message += "URI: ";
  message += webserver.uri();
  message += "\nMethod: ";
  message += ( webserver.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += webserver.args();
  message += "\n";

  for ( uint8_t i = 0; i < webserver.args(); i++ ) {
    message += " " + webserver.argName ( i ) + ": " + webserver.arg ( i ) + "\n";
  }
  webserver.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  webserver.sendHeader("Pragma", "no-cache");
  webserver.sendHeader("Expires", "-1");
  webserver.send ( 404, "text/plain", message );
}

void handlePattern() {

  pattern = webserver.arg("pattern"); 
  patternLength = pattern.length();

  startLampSequence(SM_PATTERN,100);

  sendResult( pattern.c_str() );    // Send same page so they can send another msg

  Serial.println(pattern);

}

void sendResult(const char* message) {

    String status = buildJSONStatus(message);
    webserver.send(200, TEXTJSON, status );

    if ( usingMQTT ) {
      sendMQTTStatusMessage( status );
    }
}

void sendTextResult(const char* message) {

    webserver.send(200, TEXTPLAIN, message );

    if ( usingMQTT ) {
      sendMQTTStatusMessage( message );
    }
}

void sendError(const char* message) {

    webserver.send(400, TEXTPLAIN, message );

}

MDNSResponder mdns;


// ###################################################################################
// 
//  setup()
//
// ###################################################################################

void setup() {

  pinMode( USB_A_POWER_PIN, OUTPUT );
  digitalWrite(USB_A_POWER_PIN, LOW);
  USB_A_state = LOW;

  pinMode(SWITCH3_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SWITCH3_PIN), RF433_SW3wasPressed, FALLING);

  Serial.begin(115200);
  Serial.println();
  Serial.println("setting up Lamp device");


  // setup NEO pixel strip
  FastLED.addLeds<NEOPIXEL, NEO_PIN>(pixels, PIXEL_COUNT);
  
  setLampColour(0,0,0); // Initialize all pixels to 'off'
 

  //read configuration from FS json
  
  if (SPIFFS.begin()) {
    
    if (SPIFFS.exists("/config.json")) {
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        size_t size = configFile.size();
        std::unique_ptr<char[]> buf(new char[size]); // Allocate a buffer to store contents of the file.

        configFile.readBytes(buf.get(), size);

        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject( buf.get() );
        
        if (json.success()) {

          strcpy(hostName, json["hostname"]);
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
 
        } else {
          Serial.println("failed to parse json config");
        }
      }
    } else {
        Serial.println("didn't find config file");
    }
  } else {
    Serial.println("failed to mount FS");
  }
 


  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_hostname("hostname", "host name", hostName, 40);
  WiFiManagerParameter custom_mqtt_server("mqttserver", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback( saveConfigCallback );

  //add all your parameters here
  wifiManager.addParameter( &custom_hostname );
  wifiManager.addParameter( &custom_mqtt_server );
  wifiManager.addParameter( &custom_mqtt_port );
 
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //WiFi class tries the last used ssid and password from EEPROM to connect to wifi
  //if it does not connect it starts an access point (AP) with the specified SSID
  //and goes into a blocking loop awaiting configuration
  
  if (!wifiManager.autoConnect("Led Orb Config")) {
    
    Serial.println("failed to connect to previously connected  wifi network or hit timeout");
    delay(3000);
    //reset and try again
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.printf ("Connected to : %s   as hostname : %s \n",WiFi.SSID().c_str(), WiFi.hostname().c_str());
  Serial.printf ("DHCP ip : %s", WiFi.localIP().toString().c_str() );

  
  //save the custom parameters to FS : flag set by callback
  if (shouldSavePortalConfig) {

      //read updated parameters from portal fields
    strcpy(hostName, custom_hostname.getValue());
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_port, custom_mqtt_port.getValue());
    
    Serial.println("saving Portal config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["hostname"] = hostName;
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
 
    File configFile = SPIFFS.open("/config.json", "w");
    if (configFile) {
      json.printTo(Serial);
      json.printTo(configFile);
      configFile.close();
      //end save
    } else {
      Serial.println("failed to open config file for writing");
    }
  }


// ############################# start of normal setup() function ##############################


  if (mdns.begin(hostName, WiFi.localIP())) {
    Serial.printf("mDNS responder started for %s \n", hostName);
  }

  // Setup MQTT Client 

  if ( mqtt_server[0] != 0  ) usingMQTT = true;

  String serverPortNum = String((char*)mqtt_port);
  pubSubClient.setServer(mqtt_server, serverPortNum.toInt() );
  pubSubClient.setCallback(mqttCallback);
  
  // Set up the HTTP endpoints on the webserver
  webserver.on("/", []() {

    if (!handleFileRead(webserver.uri()))     // send it if it exists
      handleNotFound(); // otherwise, respond with a 404 (Not Found) error

  });

  webserver.on("/arrow", []() {
    String direction = webserver.arg("dir");
    if ( direction.length() > 0 )  {
      
      switch (direction[0]) {

        case 'u' :
        case 'U' :
        break;
        case 'd' :
        case 'D' :
        break;
        case 'l' :
        case 'L' :
        break;
        case 'r' :
        case 'R' :
        break;
        default :
          playBeep();

      }
    }
    sendResult("Arrow");
  });

  webserver.on("/ok", []() {
    RF433_SW3wasPressed(); // emulate pressing the RF433 remote control
    sendResult( "ok pressed");
  });

  webserver.on("/null", []() {
    sendResult( "null request");
  });


  webserver.on("/usb", []() {
    USBPowerToggle();
    sendResult( "USB Toogle");
  });
  webserver.on("/usbon", []() {
    USBPowerOn();
    sendResult( "USB On");
  });
  webserver.on("/usboff", []() {
    USBPowerOff();
    sendResult( "USB Off");
  });

  
  webserver.on("/melody", []() {
    playMelody();
    sendResult("Played Melody");
  });

  webserver.on("/beep", []() {
    playBeep();
    sendResult("Beeped");
  });


  webserver.on("/colour", []() {
    String value = webserver.arg("value");
    if ( value.length() > 0 )  {
      setLampSequenceMode( SM_FIXED );
      stopLampSequence();
      setLampColour( value[0] );
    }
    sendResult("LED colour");
  });


  webserver.on("/fixed", []() {
    setLampSequenceMode( SM_FIXED );
    stopLampSequence();
    
    sendResult("Fixed");
  });
  webserver.on("/blink", []() {
    startLampSequence(SM_BLINK,200);
    lampToggle = true;
    
    sendResult("Blink cycle");
  });
  webserver.on("/flash", []() {
    startLampSequence(SM_FLASH,200);
    
    sendResult("Flashing cycle");
  });
  webserver.on("/ramp", []() {
    startLampSequence(SM_RAMP,50);
    
    sendResult("Ramping cycle");
  });
  webserver.on("/fade", []() {
    startLampSequence(SM_FADE,50);
    
    sendResult("Fade cycle");
  });
  webserver.on("/rainbow", []() {
    startLampSequence(SM_RAINBOW,100);
    
    sendResult("Rainbow cycle");
  });
  webserver.on("/disco", []() {
    
    pattern = "RzGzBzrrWzGzbggzrB"; 
    patternLength = pattern.length();

    startLampSequence(SM_PATTERN,100);
    
    sendResult("Disco cycle");
  });
  webserver.on("/sos", []() {
    
    pattern = "GGGzzGGGzzGGGzzzzzzGGGGGzzzGGGGGzzzGGGGGzzzzzzGGGzzGGGzzGGGzzzzzzzzzzzzz"; 
    patternLength = pattern.length();

    startLampSequence(SM_PATTERN,100);
    
    sendResult("SOS cycle");
  });
  webserver.on("/alternate", []() {
    
    pattern = "RG"; 
    patternLength = pattern.length();

    startLampSequence(SM_PATTERN,200);
    
    sendResult("SOS cycle");
  });

  webserver.on("/pattern", handlePattern); 

  
  webserver.on("/start", []() {
    startLampSequence();
    sendResult("Cycling started");
  });
  webserver.on("/stop", []() {
    stopLampSequence();
    sendResult("Cycling stopped");
  });
  
  webserver.on("/speed", []() {
    
    long value = webserver.arg("value").toInt();
    if ( value > 0 )  setLampSequenceStepDuration(value);

    sendResult( "Sequence step duration set" );
  });
  webserver.on("/slow", []() {
    setLampSequenceStepDuration(200);
    
    sendResult("Cycling slow");
  });
  webserver.on("/normal", []() {
    setLampSequenceStepDuration(100);
    
    sendResult("Cycling normal");
  });
  webserver.on("/fast", []() {
    setLampSequenceStepDuration(50);
    
    sendResult("Cycling fast");
  });
  webserver.on("/faster", []() {

    if ( lampSequenceStepDuration > 5 ) {
          setLampSequenceStepDuration( lampSequenceStepDuration - 5 ) ;
    } else {
       setLampSequenceStepDuration(5) ;
    }
    sendResult("Cycling faster");
  });
  webserver.on("/slower", []() {
    lampSequenceStepDuration = lampSequenceStepDuration +5 ;
    
    sendResult("Cycling slower");
  });

  
  webserver.on("/stepup", []() {
    rampStepSize ++ ;
    
    sendResult("Step Up");
  });
  webserver.on("/stepdown", []() {
    --rampStepSize ;
    
    sendResult("Step Down");
  });

  
  webserver.on("/rfmode", []() {

    long value = webserver.arg("mode").toInt();
    if ( value < 6 )  {
      RF433_mode = value;
      sendResult("RF mode set");
    } else {
      sendResult("RF mode not set");
    }

  });
  webserver.on("/mqtt", []() {

    String mode = webserver.arg("mode");

    if ( mode.length() > 0 ) { 
      switch ( mode[0] ) {

        case 'b':
          // statements
        break;

        case 't':
          usingMQTT = true;
        break;

        case 'f':
        default:
          usingMQTT = false;

      }
    } else {
      sendError("Mqtt mode ?");
    }

  });

  webserver.on("/ap", []() {

    String value = webserver.arg("mode");
    if ( value[0] == 'u' )  {
      usingAccessPoint = true;
      sendResult("Access Point On");
    } else {
      usingAccessPoint = false;
      sendResult("Access Point Off");
    }

  });

  webserver.on("/reset", []() {
    stopLampSequence();
    setLampSequenceStepDuration(200);
    rampStepSize =1;
    red = green = blue = 0;
    USBPowerOff();
    
    sendResult( "LAMP reset");
  });
  
  webserver.on("/wifiwipe", []() {
    
    Serial.printf("Disconnecting from %s Wifi and deleting credentials", WiFi.SSID().c_str());
    
    WiFi.disconnect(true); // forget Wifi Credentials
    ESP.reset();
  });
  
  webserver.on("/status", []() {

    webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
    webserver.send(200, TEXTJSON, "");

    String output = buildJSONStatus("");
 
    webserver.sendContent( output );
   
  });

  //when the url is not found to match one of the above URLs, try loading a file from SPIFFS
  
  webserver.onNotFound([]() {
    
    if (!handleFileRead( webserver.uri() )) {
      handleNotFound();
    }
  });
  
  webserver.begin(); 

  Serial.println("Lamp is ready!");

  lastLampSequenceStateChangeTime = millis();
  
} // end of setup()




// ###################################################################################
// 
//  loop()
//
// ###################################################################################
void loop() {

  webserver.handleClient();      // checks for HTTP incoming connections

  if ( RF433_SW3_isr_flag == LOW ) { //RF433_SW3_isr_flag is set in the ISR on falling egde of D3 pin
    
    RF433_SW3_isr_flag = HIGH  ;
    
    doRF433Sw3Action();
  }
  
  doNextLampSequenceStep();

  if ( usingMQTT ) {
    
    if ( pubSubClient.connected()  ) {
      pubSubClient.loop();
    } else {
      tryToConnectToMQTTServer();

    }
  }
}

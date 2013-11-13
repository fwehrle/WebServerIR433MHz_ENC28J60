/*   WebServerIR433MHz_ENC28J60
*   Arduino webserver for sending IR and RF signals, and receive sensors values (temp) (ENC28J60 Ethernet board version)
* Authors : Franck Wehrlé
* Date : 13/11/2013
* Version : 1.0
*
TODO : appel a RCSwitch s'execute plusieurs fois
*    Web Server WITHOUT IHM for sending IR and RF 433MHz signals
*      works with Arduino Nano with a ENC28J60 Ethernet module
*
*    Ex:
    - Sending IR signal : http://192.168.1.15/set/i/SONY=0xA90
    - Sending RF signal : http://192.168.1.15/set/ra1=1 : make device '1' of group 'a' ON
                                                  ra0=0 : make all devices of group 'a' OFF
                                                  r12=1 : make device 2 of group 1 ON

*/
//#define FLOATEMIT // uncomment line to enable $T in emit_P for float emitting
#include <EtherCard.h>
#include <RCSwitch.h>
#include <IRremote.h>
#define STATIC 0  // set to 1 to disable DHCP (adjust myip/gwip values below)

#if STATIC
// ethernet interface ip address
static byte myip[] = { 192,168,1,15 };
// gateway ip address
static byte gwip[] = { 192,168,1,1 };
#endif

// ethernet mac address - must be unique on your network
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };
//Ethercard
//   SCK - Pin 13
//   SO  - Pin 12
//   SI  - Pin 11
//   CS  - Pin  8
#define LED 4              // define LED pin
#define RF_TX_PIN 10
#define RF_RX_PIN 0       // = pin 2?
//IR_SEND_PIN 3           //CONFIG (pin du récepteur infrarouge)
#define IR_RECV_PIN 9     //CONFIG (pin du récepteur infrarouge)
#define TEMP_SENSOR_ANALOG_PIN 0
static float TEMP_OFFSET = -2.4;
bool curStatus = false;
bool prevStatus = true;

byte Ethernet::buffer[500]; // tcp/ip send and receive buffer
BufferFiller bfill;

RCSwitch mySwitch = RCSwitch();
IRsend irsend;
//IRrecv irrecv(IR_RECV_PIN);
//decode_results results;

void setup(){
  Serial.begin(57600);
  Serial.println("\n[WebServer Temp-IR-RF433MHz OK]");
  
  if (ether.begin(sizeof Ethernet::buffer, mymac) == 0) 
    Serial.println( "Failed to access Ethernet controller");
#if STATIC
  ether.staticSetup(myip, gwip);
#else
  if (!ether.dhcpSetup())
    Serial.println("DHCP failed");
#endif

  ether.printIp("IP:  ", ether.myip);
  ether.printIp("GW:  ", ether.gwip);  
  ether.printIp("DNS: ", ether.dnsip);  
  
    delay(10);
    //mySwitch.enableReceive(RF_RX_PIN);  // 433Mhz Rx (récepteur 433Mhz) Correspond à la pin d2    // CONFIG
  mySwitch.enableTransmit(RF_TX_PIN); // 433Mhz Tx (emetteur 433Mhz) // CONFIG
  //test irrecv.enableIRIn(); // Start the IR receiver
}
const char http_OK[] PROGMEM =
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Pragma: no-cache\r\n\r\n";

const char http_Found[] PROGMEM =
    "HTTP/1.0 302 Found\r\n"
    "Location: /\r\n\r\n";

const char http_Unauthorized[] PROGMEM =
    "HTTP/1.0 401 Unauthorized\r\n"
    "Content-Type: text/html\r\n\r\n"
    "<h1>401 Unauthorized</h1>";

void homePage()
{
    bfill.emit_p(PSTR("$F"),
        http_OK);
}
static int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
unsigned long string2HEX(String myString){
  
    //Convert String to char
     int myStringLength = myString.length()+1;
    char myChar[myStringLength];
    myString.toCharArray(myChar,myStringLength);
    
    // Read 8 hex characters
    unsigned long myHEX = 0;
    
    char __myChar[sizeof(myString)];
    myString.toCharArray(__myChar, sizeof(__myChar));
    //Serial.print(test); Serial.print("(strtoul)=>"); Serial.println(strtoul(__test, 0, 16));
    myHEX = strtoul(__myChar, 0, 16);

    /*  for (int i = 0; i < 8; i++) {
        char ch = myChar[i];
        if (ch >= '0' && ch <= '9') {
          code = code * 16 + ch - '0';
        } else if (ch >= 'a' && ch <= 'f') {
          code = code * 16 + ch - 'a' + 10;
        } else if (ch >= 'A' & ch <= 'F') {
          code = code * 16 + ch - 'A' + 10;
        } else {
         //   Serial.print("Unexpected hex char: ");
          //  Serial.println(ch);
         //   return;
         code = 0;
        }
      }   
    */
    /*  unsigned long myHEX = atoi(myChar); 
      myHEX = myHEX*2;
     // Serial.println(myHEX); 
     */
   return myHEX;
}
float getTempA(int sensorPin, float offset){
  int reading = analogRead(sensorPin);    //getting the voltage reading from the temperature sensor
  float voltage = reading * 5.0; // converting that reading to voltage, for 3.3v arduino use 3.3
  voltage /= 1024.0; 
  Serial.print(voltage); 
  Serial.println(" volts"); // print out the voltage
  float temperatureC = (voltage * 100)+offset ; //(voltage - 0.5) * 100 ;  
  //converting from 10 mv per degree wit 500 mV offset
  //to degrees ((voltage - 500mV) times 100)
  Serial.print(temperatureC); 
  Serial.println(" degrees C");
  //float temperatureF = (temperatureC * 9.0 / 5.0) + 32.0; // now convert to Fahrenheit
  //Serial.print(temperatureF); Serial.println(" degrees F");
  return temperatureC;
}
void listJson(BufferFiller& buf) {
  //test writeHeaders(buf);
  buf.print(F("HTTP/1.0 200 OK\r\nPragma: no-cache\r\n"));
  buf.println(F("Content-Type: application/json\r\n"));
  buf.print(F("{\"list\":["));
  int index = 1;
//  DeviceAddress addr;
//  sensors.requestTemperatures();
//  oneWire.reset_search();
//  while (oneWire.search(addr)) {
 //   if (index != 1) buf.write(',');
    float tempC = getTempA(TEMP_SENSOR_ANALOG_PIN, TEMP_OFFSET); //20.231; //sensors.getTempC(addr);
    int ana = analogRead(TEMP_SENSOR_ANALOG_PIN);
    //float volt = analogRead(TEMP_SENSOR_ANALOG_PIN)*5.0 / 1023;
    //int tempI = 20;
    buf.emit_p(PSTR("{\"id\":\"$H\",\"analog\":$D,\"name\":\"Temp $D\",\"val\":$T}")
      , "1", ana, index, tempC);
      //      , addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7], index, tempC);
 //   index++;
 // }
  buf.emit_p(PSTR("],\"uptime\":$L,\"free\":$D}"), millis(), freeRam());
}
void loop(){
  // DHCP expiration is a bit brutal, because all other ethernet activity and
  // incoming packets will be ignored until a new lease has been acquired
  //  if (!STATIC && ether.dhcpExpired()) {
  //    Serial.println("Acquiring DHCP lease again");
  //    ether.dhcpSetup();
  //  }
    if(curStatus != prevStatus){
      digitalWrite(LED, curStatus); 
      Serial.println(curStatus);
      if(curStatus){
        mySwitch.switchOn('A', 1);
      }else{
        mySwitch.switchOff('A', 1);
      }
      prevStatus = curStatus;
    }
  // wait for an incoming TCP packet, but ignore its contents
   word len = ether.packetReceive();
   word pos = ether.packetLoop(len); 
  if (pos) {      // write to LED digital output
    
    delay(1);   // necessary for my system
        bfill = ether.tcpOffset();
        char *data = (char *) Ethernet::buffer + pos;
    if (strncmp("GET /", data, 5) != 0) {
            bfill.emit_p(http_Unauthorized); // Unsupported HTTP request 304 or 501 response would be more appropriate
    } else {
         data += 5;
         byte j = 0;
         char b;
         String qs = "";
         while(b!= ' '){
           b = data[j];
           qs += b;
           j++;
         }
         Serial.print("data="); Serial.println(qs);
        if (data[0] == ' ') {
            // Return home page
            homePage();
        }else if (strncmp("set/on ", data, 7) == 0) {
            // Set ledStatus true and redirect to home page
            curStatus = true;
            bfill.emit_p(http_Found);
        }else if (strncmp("set/off ", data, 8) == 0) {
            // Set ledStatus false and redirect to home page
            curStatus = false;
            bfill.emit_p(http_Found);
        }else if (strncmp("get/all ", data, 8) == 0) {
            // get sensors
            listJson(bfill); 
        }else if (strncmp("set/", data, 4) == 0) {
          data+=4;         
             char mode = data[0]; 
             if(mode=='i'){
                Serial.println("*** Infrared Send");
                String irCode = qs.substring(qs.lastIndexOf('=')+1, qs.length()-1);
                unsigned long irCodeInt = string2HEX( irCode);
                Serial.print(irCode); Serial.print("=>"); Serial.println(irCodeInt);

                //Serial.print("IR code="); Serial.println(irCode);
                if (strncmp("i/NEC=", data, 6) == 0) {
                  Serial.print("NEC="); 
                  irsend.sendNEC(irCodeInt, 32); // NEC
                  data+=6;
                }else if (strncmp("i/SONY=", data, 7) == 0) {
                  Serial.print("SONY="); 
                  data+=7;
                  for (int i = 0; i < 3; i++) {
                    irsend.sendSony(irCodeInt, 12); // Sony TV power code
                    delay(40);
                  }
                }else if (strncmp("i/RC5=", data, 6) == 0) {
                  Serial.print("RC5=");
                  data+=6;
                }else if (strncmp("i/RC6=", data, 6) == 0) {
                  Serial.print("RC6=");
                  data+=6;
                }else{ //i=
                  Serial.print("Code=");
                  data+=2;
                }
                Serial.println(irCode); Serial.print("(");Serial.print(irCodeInt);Serial.println(")");
                                 
                bfill.emit_p(http_OK);

             }else if (mode == 'r') { // ir
                Serial.println("*** Radio Send");
               char group = data[1];
               int igroup = group-'0';
               char device = data[2];
               int idevice = device-'0';
               int pin = (igroup*10)+idevice;
               char valOne = data[4];
               Serial.print("mode=");Serial.print(mode);
               Serial.print(", group=");Serial.print(group);Serial.print(", igroup=");Serial.print(igroup);
               Serial.print(", device=");Serial.print(device);Serial.print(", idevice=");Serial.print(idevice);
               Serial.print(", pin=");Serial.print(pin);
               Serial.print(", valOne=");Serial.println(valOne);
               data+=5; 
               bfill.emit_p(http_OK);
               delay(100);
               if(idevice == 0){ //On allume tout
                  Serial.println(" boucle:");
                  for (int i=1; i <= 8; i++){
                    //idevice = i;
                    Serial.print(" n"); Serial.print(idevice);
                    switch (valOne) {
                    case '0':
                      mySwitch.switchOff(group, i);
                      Serial.println(" Off. ");
                      break;
                    case '1':
                      mySwitch.switchOn(group, i);
                      Serial.println(" On. ");
                      break;
                    default: 
                      Serial.println("Erreur value");
                      //error = true;
                    }
                    delay(10);
                  } 
               }else{
                  Serial.print(" n:"); Serial.print(idevice);
                  switch (valOne) {
                    case '0':
                      mySwitch.switchOff(group, idevice);
                      Serial.println(" Off. ");
                      break;
                    case '1':
                      mySwitch.switchOn(group, idevice);
                      Serial.println(" On. ");
                      break;
                    default: 
                      Serial.println("Erreur value");
                    }
               }
               delay(100);
             }else{
                Serial.println("*** Unkniwn mode");
             }
        }else {// Page not found
         Serial.print("data="); Serial.println(qs);
            bfill.emit_p(http_Unauthorized);
        }
    }
    ether.httpServerReply(bfill.position());    // send http response
  }
}

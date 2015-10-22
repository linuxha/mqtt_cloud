/*
  MQTT Cloud Client

  Author        Neil Cherry <ncherry@linuxha.com>
  Board         fubarino-mini
  Compiler      pic32-tools
  Core      	chipkit
  Port      	/dev/ttyACM0
  Programmer    avrdude
  Summary      	MQTT Cloud - Chipkit MQTT client to access a local MQTT server or a CloudMQTT server

  This version will be able to connect to the MQTT server (cloud or local),
  login if necessary, pub/sub and be remote controlled.

  I need LED status ouput that will tell me about a MQTT level heartbeat.
  The current status (got an IP, logged into MQTT, received an application
  keepalive).

  - connects to an MQTT server
  - publishes "hello world" to the topic "outTopic"
  - subscribes to the topic INTOPIC (a define)
  - simply prints out the messages (for now, processing later)
  - reads temperature and humidity from the sensor
  - publishes the string "Temperature = xx.xx, Humidity = xx.xx" to the
    defined OUTTOPIC
 
  - Uses the Ethernet library in the user lib (W5500)

  20151022 - This compile well under the recent MPIDE and it sometimes works
             under UECIDE. But either way I'm adding it to my git repos for
             better control. The UECIDE compile problems are related to the
             install. At the moment this code fails to run. It hangs at the
             PubSubClient connect. My other code without the user id and password
             connects fine to my local mqtt server. This code does not.
  # ----------------------------------------------------------------------------
 
*/
//
/* =[ Notes ]===================================================================
- This code is extremely sloppy. It needs to be cleaned up and if possible broken
  down into smaller files (won't help with compiling but will with aesthetics).
- the MQTT code needs a heart beat check (can I use the MQTT ping?). Retry/Reboot
  on failures
============================================================================= */

// ------------------------------------------------------------------------------

// WIZnet 550io board
// o Pin 10 SCSn (Select ? - oops)
// o Pin 11 MOSI
// o Pin 12 MISO
// o Pin 13 SPI select (also tied to LED4)
#include <DSPI.h>
DSPI0 SPI;

#include <Ethernet.h>
#include <PubSubClient.h>

#include <DHT22.h>

// -----------------------------------------------------------------------------

#if !defined(PIN_LED5)
// I plugged an LED into RC5 on the Fubarino mini (pin 22)
#define PIN_LED5        22  //RC5  //on the UNO32 43
#endif

// LED Pin assignments
#define GreenLED    11
#define YellowLED   12
#define RedLED      13

// o Pin 10 DHT
#define DHTPIN      10

#define W5500_ETHERNET_SHIELD    1
#define WIZ550io_WITH_MACADDRESS 1

#define CLOUD   0
/*
** ###
** ### The next bit needs to be in it's own .h file and excluded from git
** ###
*/
#if CLOUD == 1
    // For cloud use

    /*
       ###
       ### Don't post this info ! This is the private info to access the cloudmqtt account
       ###
       https://customer.cloudmqtt.com/login
    */
    #define SERVER  "xxx.cloudmqtt.com"
    #define PORT    12345
    // Don't forget to setup your ACL
    // Some services use MD5 (but not cloudmqtt)
    // echo -n "Your-String-Here" | md5sum
    #define ID      "DeviceIDHere"  //
    #define USER    "Userd_ID"      // 
    #define PASSWD  "Cloud_PASSWD"  // 

    // Topic related
    #define NOM         "CloudTopic"
    #define BASE_TOPIC  NOM "/devices/fubar/"
    #define INTOPIC     BASE_TOPIC "cmd"
    #define OUTTOPIC    BASE_TOPIC "txt"
    #define ERRTOPIC    BASE_TOPIC "error"

    #define CONNECT(a, b, c)    connect(a, b, c)
#else
    // For local use
    #define SERVER  "mozart.uucp"
    #define PORT    1883

    #define ID      "FubarinoClient"//
    #define USER    NULL        // NULL if not needed
    #define PASSWD  NULL        // NULL if not needed

    // Topic related
    #define NOM         "topic"
    #define BASE_TOPIC  NOM "/devices/fubar/"
    #define INTOPIC     BASE_TOPIC "cmd"
    #define OUTTOPIC    BASE_TOPIC "txt"
    #define ERRTOPIC    BASE_TOPIC "error"

    #define CONNECT(a, b, c)    connect(a)

#endif

// -----------------------------------------------------------------------------
extern void callback(char* topic, byte* payload, unsigned int length);

EthernetClient ethClient;
PubSubClient client(SERVER, PORT, callback, ethClient);

// -----------------------------------------------------------------------------
int myline; // returns some information about PubSubClient (I added this)

unsigned long old;
int checkValue = 0;

// -[ Task stuff ]------------------------------------------------------------------------------------------------------------------

// My watchdog stuff (not hardware)
int task1_id;
unsigned long task1_var;

#define _3Mins  (3 * 60 * 1000)  // 180000

// Simple software reset, if the number of millis that have past since the last
// reset of the variable val is greater than 3 minutes then we reset everything
void
myTask1(int id, void * tptr) {
    unsigned long val = millis() - old; // Get the elapse millis

    if(val > _3Mins) {
        // something BAD this way comes
        char s[32];
        strcpy(s, "Bad, timeout: ");
        itoa(val, s, 10);
        Serial.println(s);
        client.publish(ERRTOPIC, s);

        executeSoftReset(RUN_SKETCH_ON_BOOT); // SoftReset();
    }
}

/*

      ASCII Table
   
            (Hex)                     (decimal)
           20 - 7F                    30  - 127
         2 3 4 5 6 7       30 40 50 60 70 80 90 100 110 120
       --------------     ----------------------------------
      0:   0 @ P ` p     0:    (  2  <  F  P  Z  d   n   x
      1: ! 1 A Q a q     1:    )  3  =  G  Q  [  e   o   y
      2: " 2 B R b r     2:    *  4  >  H  R  \  f   p   z
      3: # 3 C S c s     3: !  +  5  ?  I  S  ]  g   q   {
      4: $ 4 D T d t     4: "  ,  6  @  J  T  ^  h   r   |
      5: % 5 E U e u     5: #  -  7  A  K  U  _  i   s   }
      6: & 6 F V f v     6: $  .  8  B  L  V  `  j   t   ~
      7: ´ 7 G W g w     7: %  /  9  C  M  W  a  k   u  DEL
      8: ( 8 H X h x     8: &  0  :  D  N  X  b  l   v
      9: ) 9 I Y i y     9: ´  1  ;  E  O  Y  c  m   w
      A: * : J Z j z
      B: + ; K [ k {
      C: , < L \ l |
      D: - = M ] m }
      E: . > N ^ n ~
      F: / ? O _ o DEL

*/
char status[10];

/*
      Digital pins available      : 2 - 13 and A0 - A5 if no analog is used
      Analog Input pins available : A0 - A5 
      Analog Output pins          : 3,5,6,9,10,11
   
      Note: pins 0 - 1 and 10 - 13 have special uses,
      * Pins 0 and 1 are the serial port, 0 - RX, 1 - TX
      * Ethernet shield attached to pins 10, 11, 12, 13
   
      Pin 13 is connected to the on board LED and can't be used without an external
      pulldown resister 
      Notes:
   
      If I remember correctly, I thought there was some option to reduce code
      size by removing some of function of the library, does this apply to
      this version too ?  you may reduce by aprox. 5kb by switching off UDP
      (and at the same time DNS and DHCP as this requires UDP). This can be
      configured in xxxx-conf.h (@TODO)
   
      If RAM is an issue you may also configure number of concurrent sockets
      and number of packets per socket, but this will not reduce the
      foodprint in flash.
*/
void
callback(char* topic, byte* payload, unsigned int length) {
    // Understand that the PIC32 has resource limitations so we can't allow long
    // payloads. So we will terminate the payload accordingly
    // Also MQTT limits the TCP (?) payload to 128 bytes (and there is MQTT 
    // overhead)
    char s[128];

    // -------------------------------------------------------------------------
    switch(payload[0]) {
    case '!':
        switch(payload[1]) {
        case 'S':
            strcpy(status, "Stop");
            digitalWrite(GreenLED,  0);
            digitalWrite(YellowLED, 0);
            digitalWrite(RedLED,    1);
            break;
        case 'G':
            strcpy(status, "Go");
            digitalWrite(GreenLED,  1);
            digitalWrite(YellowLED, 0);
            digitalWrite(RedLED,    0);
            break;
        case 'C':
            strcpy(status, "Caution");
            digitalWrite(GreenLED,  0);
            digitalWrite(YellowLED, 1);
            digitalWrite(RedLED,    0);
            break;
        case 'X':
            strcpy(status, "XXX");
            digitalWrite(GreenLED,  1);
            digitalWrite(YellowLED, 1);
            digitalWrite(RedLED,    1);
            break;
        //case '':
        //    break;
        default:
            strcpy(status, "unknown");
            break;
        }
        break;

    case '?':
        Serial.print("Status: ");
        Serial.println(status);
        client.publish(OUTTOPIC, status);
        break;

    default:
        break;
    }
}

// -[ DHT ]---------------------------------------------------------------------
DHT22 dht(DHTPIN);

// Float is a pig on microcontrollers so I've decided to go the fix point math
// route. The DHT library has the needed calls to return the values as int * 10
// we need to do the math to get the fixed and floating parts.
char *
getDHT(char *str) {
    // Temp string, 100.00 or 140.00 (humidity or temperature respectively)
    char t[8];
    DHT22_ERROR_t errorCode;

    strcpy(str, "getDHT unknown error");

    switch(dht.readData()) {
    case DHT_ERROR_NONE:
        // Floats don't work too well as float to char * isn't easy
        // and it consumes a lot of storage (32k vs 64k)
        //sprintf(str, "Humidity = %0.2f, Temperature = %0.2f", dht.getTemperatureC(), dht.getHumidity());
        str[0] = '\0';
        strcat(str, "Temperature = ");
        itoa(dht.getTemperatureCInt()/10, t, 10);
        strcat(str, t);
        strcat(str, ".");
        itoa(abs(dht.getTemperatureCInt()%10), t, 10);
        strcat(str, t);

        strcat(str, ", Humidity = ");
        itoa(dht.getHumidityInt()/10, t, 10);
        strcat(str, t);
        strcat(str, ".");
        itoa(dht.getHumidityInt()%10, t, 10);
        strcat(str, t);

        break;
    case DHT_ERROR_CHECKSUM:
        strcpy(str, "check sum error ");
        break;
    case DHT_BUS_HUNG:
        strcpy(str, "BUS Hung ");
        break;
    case DHT_ERROR_NOT_PRESENT:
        strcpy(str, "Not Present ");
        break;
    case DHT_ERROR_ACK_TOO_LONG:
        strcpy(str, "ACK time out ");
        break;
    case DHT_ERROR_SYNC_TIMEOUT:
        strcpy(str, "Sync Timeout ");
        break;
    case DHT_ERROR_DATA_TIMEOUT:
        strcpy(str, "Data Timeout ");
        break;
    case DHT_ERROR_TOOQUICK:
        strcpy(str, "Polled too quick ");
        break;
    }

    return(str);
}

// -----------------------------------------------------------------------------
/*
  pause - wait for user input (or some time spec)
*/
#define _30Sec (30 * 1000)
#define WAIT_TIMEOUT

void
pause() {
    unsigned long val = millis() + _30Sec;

    Serial.print("\nPause :");
#if defined(WAIT_TIMEOUT)
    while( (!Serial.available()) && (val > millis()) );
#else
    while( !Serial.available() );
#endif
    Serial.print("\n");
}

// -----------------------------------------------------------------------------
/* A utility function to reverse a string  */
void reverse(char str[], int length) {
    int start = 0;
    int end   = length -1;
    int t;
    while (start < end) {
        t          = str[start];
        str[start] = str[end];
        str[end]  = t;
        start++;
        end--;
    }
}

/*
  IPv4 only for now

  This craziness with reverse is because we don't know the length of the
  resulting string. So we start at the least significant and work to most
  then reverse the result.

  Now be wary of the array handling here. It is very bad form. These
  routines expect that the routines will only deal with 1 IP address.
*/
char *
iptostr(uint32_t ip_n) {
    // xxx.xxx.xxx.xxx\0 (length 16)
    static char temp[20];
    int j = 0;
    
    /* Handle 0 explicitely, otherwise empty string is printed for 0 */
    if (ip_n == 0) {
        temp[0] = '0';
        temp[1] = '\0';
        return temp;
    }

    uint8_t ip[4];
    int i = 0;
    do {
        ip[i] = ip_n & 0x000000FF;
        ip_n = ip_n >> 8;
        i++;
    } while(i < 4);

    // { a, b c, d }
    i = 3;
    do {
        uint8_t num = ip[i];

        int rem;
        // Process individual digits
        do {
            rem = num % 10;
            temp[j++] = rem + '0';
            num = num/10;
        } while (num != 0);
        temp[j++] = 0x2E;   // dot
        //i--; // ip = ip >> 8;
    } while(i-- > 0);
    temp[--j] = 0;      // NULL terminate and stomp on the last dot

    // Reverse the string
    reverse(temp, j);

    return temp;
}
// -----------------------------------------------------------------------------
char *
mactostr(uint8_t *mac) {
    char a[] = "0123456789ABCDEF";
   
    // 00:01:02:03:04:05\0 (length = 17)
    static char temp[20];
    int j = 0;
    
    // nibble it to death
    int i;
    for(i = 0; i < 6; i++) {
        // Add to the end of the temp array
        temp[j++] = a[(*mac>>4) & 0x0F];
        temp[j++] = a[*mac & 0x0F];
        temp[j++] = 0x3A;
        mac++;
    }
    temp[--j] = 0;

    return temp;
}
//
int task2_id;
unsigned long task2_var;
char led = 0;

void
toggleLed(int id, void * tptr) {
    // PIN43 LED5
    led = led ? 0:1;
    digitalWrite(PIN_LED5, led);
}

// -[ Setup ]--------------------------------------------------------------------
uint8_t mac[] = { 0, 0, 0, 0, 0, 1 } ;
char *smac;

void setup() {
    int r;
    Serial.begin(9600);

    pinMode(GreenLED,  OUTPUT);
    digitalWrite(GreenLED,  1);
    pinMode(YellowLED, OUTPUT);
    digitalWrite(YellowLED, 1);
    pinMode(RedLED,    OUTPUT);
    digitalWrite(RedLED,    1);
 
    pinMode(PIN_LED5, OUTPUT);
    led = 1; // next toggle will turn it off
    toggleLed(0, NULL);

    pause();
    toggleLed(0, NULL);

    digitalWrite(GreenLED,  0);
    digitalWrite(YellowLED, 0);
    digitalWrite(RedLED,    0);

    Serial.println(NOM " FU32: PubSub");

    Ethernet.begin();
    // I need to get my IP and MAC addresses
    Serial.print("IP:  ");
    Serial.println( iptostr(Ethernet.localIP()) );

    Ethernet.getMACAddr(mac);
    Serial.print("MAC: ");
    Serial.println( mactostr(mac) );

    /*
    ** ### @TODO - Need to check for a valid IP address here and retry if we didn't get one
    */
    // boolean PubSubClient::connect(char *id, char *user, char *pass)
    // @FIXME - Why do I need this 1==1 ? (old compiler bug)
    if ((r = client.connect(ID, USER, PASSWD)) || 1==1 ) {
        // I wonder how many pub/subs the library can support?
        client.publish("outTopic","fubar world");
        client.publish(ERRTOPIC, "D: Startup");
        client.subscribe(INTOPIC);
    }
    Serial.print("FU32: Fall into loop MYLINE: ");
    Serial.println(myline, DEC);

    old = millis();

    // Every minute (or there abouts)
    task1_id = createTask(myTask1,   60*1000, TASK_ENABLE, &task1_var);
    // Every 2 seconds toggle the LED
    task2_id = createTask(toggleLed,  2*1000, TASK_ENABLE, &task2_var);
}

// -[ Loop ]--------------------------------------------------------------------

void loop() {
    char str[48];

    client.loop();

    // -[ Monitored device code ]-----------------------------------------------
    // new - old gives you the elapsed time in millis
    if(millis() - old > 30000) {
        checkValue++;
        old = millis();

        // Check the connection
        if(client.connected() == false) {
            Serial.println("Lost Connection");
            // Okay fix my connection then
            client.connect(ID, USER, PASSWD);
            client.publish(ERRTOPIC,"re-fubar world");
            client.subscribe(INTOPIC);

            Serial.print("FU32: Fall into loop MYLINE: ");
            Serial.println(myline, DEC);
        }

        // -[ Information to be published ]-------------------------------------
        getDHT(str);
        Serial.println(str);
        client.publish(OUTTOPIC, str);
    }
}

// -[ Fini ]--------------------------------------------------------------------
// -[ Notes ]-------------------------------------------------------------------
// -[ EoF ]---------------------------------------------------------------------

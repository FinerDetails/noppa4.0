#include "mbed.h"
#include "ESP8266Interface.h"
#include "ADXL362.h"
#include <cmath>
#include <MQTTClientMbedOs.h>
#include <cstdio>
#include <cstring>
#include "MQTTClient.h"
#include "Adafruit_SSD1331.h"
#include "Adafruit_GFX.h" 

ADXL362 ADXL362(A3,D11,D12,D13); // cs, mosi, miso, sck
Adafruit_SSD1331 OLED(A2, A1, A5, A6, NC, A4); // cs, res, dc, mosi, (nc), sck

// Definition of colours on the OLED display
#define Black 0x0000
#define White 0xFFFF

//Threads
    Thread detect_thread;
    Thread publish_thread;
    Thread subscribe_and_display_thread;

//prototypes
int ADXL362_reg_print(int start, int length);
int ADXL362_movement_detect();
void publish_to_nodeRED();
void MQTTdata(MQTT:: MessageData& ms);
void subscribe_node_to_screen();
void process_to_screen();
 
int8_t x,y,z;
char node_data[64];

TCPSocket socket;
MQTTClient client(&socket);
ESP8266Interface esp(MBED_CONF_APP_ESP_TX_PIN, MBED_CONF_APP_ESP_RX_PIN);

int main()
{
    //Store device IP
    SocketAddress deviceIP;
    //Store broker IP
    SocketAddress MQTTBroker;
    
    printf("\nConnecting wifi..\n");
    int ret = esp.connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if(ret != 0)
    {
        printf("\nConnection error\n");
    }
    else
    {
        printf("\nConnection success\n");
    }

    esp.get_ip_address(&deviceIP);
    printf("IP via DHCP: %s\n", deviceIP.get_ip_address());

    
    esp.gethostbyname(MBED_CONF_APP_MQTT_BROKER_HOSTNAME, &MQTTBroker, NSAPI_IPv4, "esp");
    MQTTBroker.set_port(MBED_CONF_APP_MQTT_BROKER_PORT);

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;       
    data.MQTTVersion = 3;
    char *id = MBED_CONF_APP_MQTT_ID;
    data.clientID.cstring = id;

    socket.open(&esp);
    socket.connect(MQTTBroker);
    client.connect(data);
    
    // initialization accelerometer
    ADXL362.reset();
    ThisThread::sleep_for(600ms);
    ADXL362.set_mode(ADXL362::MEASUREMENT);
    ADXL362_reg_print(0, 0);

    // initialization of display object
    OLED.begin();


    detect_thread.start(ADXL362_movement_detect);
    subscribe_and_display_thread.start(subscribe_node_to_screen);
}

void subscribe_node_to_screen()
{
    //Subscribes to an mqtt topic recieving messages back from nodeRED and keeps refreshing it.
    client.subscribe(MBED_CONF_APP_MQTT_TOPIC_FROM_NODE_RED, MQTT::QOS0, MQTTdata);
    while(1)
    {
        client.yield();
        ThisThread::sleep_for(250ms);
    }
}

int ADXL362_movement_detect()
{
    //Measures if the combined change of acceloration exceeds the set threshold.
    int8_t x1,y1,z1,x2,y2,z2,dx,dy,dz;
    int detect;
    int8_t combined_inertia;
    while(1)
    {
        x1=ADXL362.scanx_u8();
        y1=ADXL362.scany_u8();
        z1=ADXL362.scanz_u8();
        ThisThread::sleep_for(10ms);
        x2=ADXL362.scanx_u8();
        y2=ADXL362.scany_u8();
        z2=ADXL362.scanz_u8();
            
        x=(x1 + x2)/2;
        y=(y1 + y2)/2;
        z=(z1 + z2)/2;
         
        dx=abs(x1 - x2);
        dy=abs(y1 - y2);
        dz=abs(z1 - z2);
        combined_inertia = dx + dy + dz;

        if (abs(combined_inertia) > 70)
        {
            publish_to_nodeRED();
            ThisThread::sleep_for(900ms);
        }

        printf("x = %3d    y = %3d    z = %3d   dx = %3d    dy = %3d    dz = %3d    combined = %3d\r\n",x,y,z,dx,dy,dz,combined_inertia);
        ThisThread::sleep_for(100ms);
    }    
}

void publish_to_nodeRED()
{
    //Sets up fields for an MQTT packet and publishes it to topic defined in mbed_app.json to NodeRED.
    MQTT::Message msg;
    msg.qos = MQTT::QOS0;
    msg.retained = false;
    msg.dup = false;
    msg.payload = (void*)"1";
    msg.payloadlen = 1;
    client.publish(MBED_CONF_APP_MQTT_TOPIC_TO_NODE_RED, msg);
    printf("\n\nAttempted to publish to nodeRED\n\n");
}

void MQTTdata(MQTT::MessageData& ms)
{
    //A function called by the MQTT subscribe() method.
    //Activates when NodeRED publishes a message back to the subscribed topic.
    //Copies the payload of the message into an object.
    MQTT::Message &message = ms.message;
    printf("\n\nMessage arrived: qos %d, retained %d, dup %d, packetid %d\n", message.qos, message.retained, message.dup, message.id);
    sprintf(node_data,"%.*s\0",message.payloadlen ,(char*)message.payload);
    printf("Payload: %.*s\n\n", message.payloadlen, node_data);
    process_to_screen();
}

void process_to_screen()
{
    //Controls the screen based on the number randomized by nodeRED.
    //If the dice is set to roll a number higher than 6 in nodeRED, the screen prints just a number instead of dots. 
    OLED.clearScreen(); 
    OLED.fillScreen(White);
    OLED.setTextColor(Black);
    OLED.setCursor(8,8);
    OLED.setTextSize(3);
        

    if(strcmp( node_data, "1") == 0)
    {
        OLED.fillCircle(48, 32, 8, Black);
    }

    else if (strcmp( node_data, "2") == 0)
    {
        OLED.fillCircle(36, 20, 8, Black); //vasen, ylä
        OLED.fillCircle(60, 44, 8, Black); //oikea, ala
    }

    else if (strcmp( node_data, "3") == 0)
    {
        OLED.fillCircle(32, 16, 8, Black); //vasen, ylä
        OLED.fillCircle(48, 32, 8, Black); //keski
        OLED.fillCircle(64, 48, 8, Black); //oikea, ala
    }

    else if (strcmp( node_data, "4") == 0)
    {
        OLED.fillCircle(36, 20, 8, Black); //vasen, ylä
        OLED.fillCircle(60, 20, 8, Black); //oikea, ylä
        OLED.fillCircle(60, 44, 8, Black); //oikea, ala
        OLED.fillCircle(36, 44, 8, Black); //vasen, ala
    }

    else if (strcmp( node_data, "5") == 0)
    {
        OLED.fillCircle(32, 16, 8, Black); //vasen, ylä
        OLED.fillCircle(64, 16, 8, Black); //oikea, ylä
        OLED.fillCircle(48, 32, 8, Black); //keskellä
        OLED.fillCircle(32, 48, 8, Black); //vasen, ala
        OLED.fillCircle(64, 48, 8, Black); //oikea, ala
    }
        
    else if (strcmp( node_data, "6") == 0)
    {
        OLED.fillCircle(24, 20, 8, Black); //vasen, ylä
        OLED.fillCircle(48, 20, 8, Black); //keskellä, ylä
        OLED.fillCircle(72, 20, 8, Black); //oikea, ylä
        OLED.fillCircle(72, 44, 8, Black); //oikea, ala
        OLED.fillCircle(48, 44, 8, Black); //keskellä, ala
        OLED.fillCircle(24, 44, 8, Black); //vasen, ala
    }
        
    else
    {
        OLED.printf("%s", node_data);
    }
}

 
int ADXL362_reg_print(int start, int length)
/*
* The register bit allocations are explained in the datasheet
* https://www.analog.com/media/en/technical-documentation/data-sheets/ADXL362.pdf
* starting on page 23. 
*/
{
    uint8_t i;
    char name[32];
    char note[64];
    
    ADXL362::ADXL362_register_t reg;
    if(start >= 0x00 && start <= 0x2E && length >= 0x00 && (ADXL362.read_reg(ADXL362.DEVID_AD) == 0xAD))
    {
        if(length == 0)
        {
            start = 0;
            length = 47;   
        }
        
        for(i = start; i < start + length; i++)
        {
            switch(i)
            {
                case 0x00:
                    snprintf(name, 32, "DEVID_AD" );
                    snprintf(note, 64, "default 0xAD = I am the ADXL362");
                    reg = ADXL362.DEVID_AD;
                    break;
                case 0x01:
                    snprintf(name, 32, "DEVID_MST" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.DEVID_MST;
                    break;
                case 0x02:
                    snprintf(name, 32, "PARTID" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.PARTID;
                    break;
                case 0x03:
                    snprintf(name, 32, "REVID" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.REVID;
                    break;
                case 0x08:
                    snprintf(name, 32, "XDATA" );
                    snprintf(note, 63, "binary 8bit, two's complement");
                    reg = ADXL362.XDATA;
                    break;
                case 0x09:
                    snprintf(name, 32, "YDATA" );
                    snprintf(note, 64, "binary 8bit, two's complement");
                    reg = ADXL362.YDATA;
                    break;
                case 0x0A:
                    snprintf(name, 32, "ZDATA" );
                    snprintf(note, 64, "binary 8bit, two's complement");
                    reg = ADXL362.ZDATA;
                    break;
                case 0x0B:
                    snprintf(name, 32, "STATUS" );
                    snprintf(note, 64, "typically 0x41, 4=awake, 1=data ready");
                    reg = ADXL362.STATUS;
                    break;
                case 0x0C:
                    snprintf(name, 32, "FIFO_ENTRIES_L" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.FIFO_ENTRIES_L;
                    break;
                case 0x0D:
                    snprintf(name, 32, "FIFO_ENTRIES_H" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.FIFO_ENTRIES_H;
                    break;
                case 0x0E:
                    snprintf(name, 32, "XDATA_L" );
                    snprintf(note, 64, "binary 12bit, two's complement");
                    reg = ADXL362.XDATA_L;
                    break;
                case 0x0F:
                    snprintf(name, 32, "XDATA_H" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.XDATA_H;
                    break;
                case 0x10:
                    snprintf(name, 32, "YDATA_L" );
                    snprintf(note, 64, "binary 12bit, two's complement");
                    reg = ADXL362.YDATA_L;
                    break;
                case 0x11:
                    snprintf(name, 32, "YDATA_H" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.YDATA_H;
                    break;
                case 0x12:
                    snprintf(name, 32, "ZDATA_L" );
                    snprintf(note, 64, "binary 12bit, two's complement");
                    reg = ADXL362.ZDATA_L;
                    break;
                case 0x13:
                    snprintf(name, 32, "ZDATA_H" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.ZDATA_H;
                    break;
                case 0x14:
                    snprintf(name, 32, "TEMP_L" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.TEMP_L;
                    break;
                case 0x15:
                    snprintf(name, 32, "TEMP_H" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.TEMP_H;
                    break;
                case 0x1F:
                    snprintf(name, 32, "SOFT_RESET" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.SOFT_RESET;
                    break;
                case 0x20:
                    snprintf(name, 32, "THRESH_ACT_L" );
                    snprintf(note, 64, "Activity threshold value, binary 16bit");
                    reg = ADXL362.THRESH_ACT_L;
                    break;
                case 0x21:
                    snprintf(name, 32, "THRESH_ACT_H" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.THRESH_ACT_H;
                    break;
                case 0x22:
                    snprintf(name, 32, "TIME_ACT" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.TIME_ACT;
                    break;
                case 0x23:
                    snprintf(name, 32, "THRESH_INACT_L" );
                    snprintf(note, 64, "Inactivity threshold value, binary 16bit");
                    reg = ADXL362.THRESH_INACT_L;
                    break;
                case 0x24:
                    snprintf(name, 32, "THRESH_INACT_H" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.THRESH_INACT_H;
                    break;
                case 0x25:
                    snprintf(name, 32, "TIME_INACT_L" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.TIME_INACT_L;
                    break;
                case 0x26:
                    snprintf(name, 32, "TIME_INACT_H" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.TIME_INACT_H;
                    break;
                case 0x27:
                    snprintf(name, 32, "ACT_INACT_CTL" );
                    snprintf(note, 64, "default 0x00 = disable, 0x01 = enable");
                    reg = ADXL362.ACT_INACT_CTL;
                    break;
                case 0x28:
                    snprintf(name, 32, "FIFO_CONTROL" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.FIFO_CONTROL;
                    break;
                case 0x29:
                    snprintf(name, 32, "FIFO_SAMPLES" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.FIFO_SAMPLES;
                    break;
                case 0x2A:
                    snprintf(name, 32, "INTMAP1" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.INTMAP1;
                    break;
                case 0x2B:
                    snprintf(name, 32, "INTMAP2" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.INTMAP2;
                    break;
                case 0x2C:
                    snprintf(name, 32, "FILTER_CTL" );
                    snprintf(note, 64, "default 0x13, 1=half samplin freq, 3=freq 100 sampl/sec");
                    reg = ADXL362.FILTER_CTL;
                    break;
                case 0x2D:
                    snprintf(name, 32, "POWER_CTL" );
                    snprintf(note, 64, "default 0x02 = measure 3D");
                    reg = ADXL362.POWER_CTL;
                    break;
                case 0x2E:
                    snprintf(name, 32, "SELF_TEST" );
                    snprintf(note, 64, "-");
                    reg = ADXL362.SELF_TEST;
                    break;
            }
            // Printing register content as hexadecimal and the notes
            printf("register %d  %s  %x  %s\n", i, name, ADXL362.read_reg(reg), note);
        }
    }
    else
    {
        printf("Error");
        return(-1);
    }
    return(0);    
}
#include "mbed.h"
#include "ESP8266Interface.h"
#include "ADXL362.h"
#include <cmath>
#include <MQTTClientMbedOs.h>
#define BUFF_SIZE 6

// Library to use https://github.com/ARMmbed/mbed-mqtt

// ADXL362::ADXL362(PinName CS, PinName MOSI, PinName MISO, PinName SCK) :
ADXL362 ADXL362(A3,D11,D12,D13);
 
//Threads
    Thread detect_thread;
 
DigitalOut moveLed(D3);
 
int ADXL362_reg_print(int start, int length);
int ADXL362_movement_detect();
int acceleration3D(int8_t ax,int8_t ay,int8_t az);
 
int8_t x,y,z;
int movementDetected = 0;
int i = 0;

TCPSocket socket;
MQTTClient client(&socket);

int main()
{
    ESP8266Interface esp(MBED_CONF_APP_ESP_TX_PIN, MBED_CONF_APP_ESP_RX_PIN);
    
    //Store device IP
    SocketAddress deviceIP;
    //Store broker IP
    SocketAddress MQTTBroker;
    
    //TCPSocket socket;
    //MQTTClient client(&socket);
    
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
    
     // Use with IP
    //SocketAddress MQTTBroker(MBED_CONF_APP_MQTT_BROKER_IP, MBED_CONF_APP_MQTT_BROKER_PORT);
    
    // Use with DNS
    esp.gethostbyname(MBED_CONF_APP_MQTT_BROKER_HOSTNAME, &MQTTBroker, NSAPI_IPv4, "esp");
    MQTTBroker.set_port(MBED_CONF_APP_MQTT_BROKER_PORT);

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;       
    data.MQTTVersion = 3;
    char *id = MBED_CONF_APP_MQTT_ID;
    data.clientID.cstring = id;

    //char buffer[1];
    //sprintf(buffer, "1");

    socket.open(&esp);
    printf("PERKELE\n");
    socket.connect(MQTTBroker);
    printf("NYT\n");
    client.connect(data);
    printf("VITTU\n");
    printf("TOIMI\n");
    printf("SAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n");
    
    ADXL362.reset();
    printf("ADXL362.reset()\n");
     // we need to wait at least 500ms after ADXL362 reset
    ThisThread::sleep_for(600ms);
    printf("ThisThread::sleep_for(600ms)\n");
    ADXL362.set_mode(ADXL362::MEASUREMENT);
    printf("ADXL362.set_mode(ADXL362::MEASUREMENT)\n");
    ADXL362_reg_print(0, 0);
    printf("ADXL362_reg_print(0, 0)\n");
    detect_thread.start(ADXL362_movement_detect);
    printf("started new thread\n");

    while(1){
    moveLed.write(movementDetected);
    if(movementDetected){
        i += 1;
        printf("i = %d\n", i);
        }
    printf("Acceleration 3D %d\n", acceleration3D(x,y,z));
    ThisThread::sleep_for(1s);
    }
}
void publish_to_nodeRED()
{
        MQTT::Message msg;
        msg.qos = MQTT::QOS0;
        msg.retained = false;
        msg.dup = false;
        //msg.payload = (void*)buffer;
        msg.payload = (void*)"1";
        msg.payloadlen = 1;
        client.publish(MBED_CONF_APP_MQTT_TOPIC, msg);
        printf("\nAttempting to publish\n");
}
int ADXL362_movement_detect()
{
    int8_t x1,y1,z1,x2,y2,z2,dx,dy,dz;
    int detect;
    while(1){
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

        printf("\nx %d,y %d,z %d\n",dx, dy, dz);

        if (dx>10 || dy>10 || dz>10){
            detect = 1;
            publish_to_nodeRED();
            printf("\nCalled publish_to_nodeRED\n");
        }

        else{
            detect = 0;
        }
        movementDetected = detect;    
        //printf("x = %3d    y = %3d    z = %3d   dx = %3d    dy = %3d    dz = %3d\r\n",x,y,z,dx,dy,dz);
        ThisThread::sleep_for(100ms);
        }    
}
 
int acceleration3D(int8_t ax,int8_t ay,int8_t az){
    float acc3D;
    static int count = 0;
    static int8_t x1[BUFF_SIZE];
    static int8_t y1[BUFF_SIZE];
    static int8_t z1[BUFF_SIZE];
    float averx;
    float avery;
    float averz;
    
    if(count >= BUFF_SIZE){
        count = 0;
        }
    
    x1[count]=ax;
    y1[count]=ay;
    z1[count]=az;
    
    count += 1;
    
    averx=0.0;
    avery=0.0;
    averz=0.0;
    for(int k=0; k<BUFF_SIZE; k++){
        averx = averx+(float)x1[k];
        avery = avery+(float)y1[k];
        averz = averz+(float)z1[k];
        }
    averx=averx/BUFF_SIZE;
    avery=avery/BUFF_SIZE;
    averz=averz/BUFF_SIZE;
    
    acc3D = sqrtf(pow(averx,2)+pow(avery,2)+pow(averz,2));
    //acc3D = sqrtf(pow(3.0,2) + pow(3.0,2) + pow(3.0,2));
    return((int)acc3D); 
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



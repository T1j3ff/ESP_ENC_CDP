
#include "BluetoothSerial.h"
#include <HardwareSerial.h>
#include <driver/rtc_io.h>
#include "lldp_functions.h"
#include "cdp_functions.h"
#include "Packet_data.h"
#include "images.h"
#include <EtherCard.h>
#include "DHCPOptions.h"
#include "Button2.h"

/////////////SCREEN////////////
#include <TFT_eSPI.h>
#include <SPI.h>
#include "WiFi.h"
#include <Wire.h>
#include "Free_Fonts.h"
int tft_width = 135;
int tft_height = 240;
TFT_eSPI tft = TFT_eSPI(tft_width, tft_height); // Invoke custom library

#define ADC_PIN             34
#define BUTTON_1            35
#define BUTTON_2            0

Button2 btn1(BUTTON_1);
Button2 btn2(BUTTON_2);
int btnCick = false;

DHCP_DATA DHCP_Info[256];
int OptionCount = 0;

byte mymac[] = {  0xCA, 0xFE, 0xC0, 0xFF, 0xEE, 0x00};
byte Ethernet::buffer[1500];
bool ENCLink;
String Protocal;
bool LogicLink = false;
bool justbooted = true;
#define Serialout
int BatLvl = 0;

DHCP_DATA DHCP_info[255];

/////////////////////////////////////////////////////////////////
// Setup other core to handle Bluetooth Serial port            //
/////////////////////////////////////////////////////////////////
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

int queueSize = 1;
int queue_delay = 2;
QueueHandle_t bluetooth_stat;

static int taskCore = 0;
BluetoothSerial SerialBT;
HardwareSerial MySerial(2);



#include "soc/rtc_wdt.h"
#include "esp_int_wdt.h"
#include "esp_task_wdt.h"

void coreTask( void * pvParameters ) {
  rtc_wdt_protect_off();
  rtc_wdt_disable();
  disableCore0WDT();
  disableLoopWDT();
  esp_task_wdt_delete(NULL);
  //Bluetooth COMs are running on core 1
  int BT = 0;

  MySerial.begin(9600, SERIAL_8N1, 32, 39);
  SerialBT.begin("FarCough");
  char *pin = "1234";
  //SerialBT.register_callback(bt_callback);
  SerialBT.setPin(pin);
  BT = 1;

  xQueueSend(bluetooth_stat, &BT, queue_delay);
  while (true) {

    BT = 1;
    if (MySerial.available()) {
      BT = 2;
      SerialBT.write(MySerial.read());
    }
    if (SerialBT.available()) {
      BT = 2;
      MySerial.write(SerialBT.read());
    }
  }
}

void setup()
{

  bluetooth_stat = xQueueCreate( queueSize, sizeof( int ) );

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(0, 22, 1);

 button_init();

  if (bluetooth_stat == NULL) {
    Serial.println("Error creating the queue");
  }
  int BT = 0;
  xQueueSend(bluetooth_stat, &BT, 2);

  tft.println("Serial 9600");

  Serial.begin(9600);

  delay(1000);

  tft.print("Starting to create task on core ");
  tft.println(taskCore);
  tft.print("Current Core:");
  tft.println(xPortGetCoreID());

  xTaskCreatePinnedToCore(  coreTask, "coreTask",     10000, NULL, 1 , NULL,  0);

  tft.println("Initializing Ethernet.");
  if (ether.begin(sizeof (Ethernet::buffer), mymac, 5) == 0) {
    tft.print("Failed to access Ethernet controller");
  }
  else {
    tft.println("Ethernet Done");

    ether.dhcpAddOptionCallback( 15, DHCPOption);

  }

  ENC28J60::enablePromiscuous();

  tft.println("Ethernet Promiscuous");
  tft.setSwapBytes(true);
  title();

  // pinMode(33, INPUT);
}

void loop()
{
  showVoltage();
  LinkStatus();

   if (btnCick) {
        showVoltage();
    }
    button_loop();
    
  int BT_STAT = 0;
  //Serial.print(BT_STAT);


  xQueueReceive(bluetooth_stat, &BT_STAT, queue_delay);
  if (BT_STAT == 1) {
    tft.pushImage(tft_width - 65, 0, 12, 21, image_BT_OFF);
  }

  if (BT_STAT == 2) {
    tft.pushImage(tft_width - 65, 0, 12, 21, image_BT);
  }

  //  Serial.print(BT_STAT);

  int  plen = ether.packetReceive();
  byte buffcheck[1500];
  memcpy( buffcheck, Ethernet::buffer, plen );

  if (plen >= 0) {
    unsigned int cdp_correct = cdp_check_Packet( plen, buffcheck, plen);
    if (cdp_correct > 1) {
      displayinfo(cdp_packet_handler(buffcheck, plen));

    }
    unsigned int lldp_Correct = lldp_check_Packet(plen,  buffcheck, plen);
    if (lldp_Correct > 1) {
      displayinfo(lldp_packet_handler( buffcheck,  plen));
    }
  }
}

void displayinfo(PINFO Screens) {
  tft.fillRect(0, 21, tft_width, tft_height - 31, TFT_BLACK );
  tft.setCursor(0, 22, 1);
  tft.setTextColor(TFT_WHITE);

  drawtext(Screens.Proto);
  drawtext(Screens.ProtoVer);
  drawtext(Screens.Name);
  drawtext(Screens.ChassisID);
  drawtext(Screens.MAC);
  drawtext(Screens.Port);
  drawtext(Screens.Model);
  drawtext(Screens.VLAN);
  drawtext(Screens.IP);
  drawtext(Screens.VoiceVLAN);
  drawtext(Screens.Cap);
  drawtext(Screens.SWver);
  drawtext(Screens.PortDesc);
  drawtext(Screens.Dup);
  drawtext(Screens.VTP);
  drawtext(Screens.TTL);
}

void drawtext(String value[2]) {
  if (value[1] != "EMPTY") {
    tft.setTextColor(TFT_GREEN);
    tft.print(value[0] + ": ");
    tft.setTextColor(TFT_WHITE);
    tft.print(value[1] + '\n');
  }
}

void title() {
  if (tft_width >= 240) {
    tft.fillRect(0, 0, tft_width, 21, TFT_WHITE );
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.drawString("MODLOG CDP/LLDP", 5, 1, 2);
  }
  else {
    tft.fillRect(0, 0, tft_width, 21, TFT_WHITE );
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.drawString("MODLOG", 5, 1, 2);
  }

}

void showVoltage()
{
  int vref = 1100;

  int batteryLevel = map(analogRead(34), 0.0f, 4095.0f, 0, 100);
  if (BatLvl != batteryLevel) {
    BatLvl = batteryLevel;
    tft.pushImage(tft_width - 33, 2, 32, 18, image_data_bat1);
    tft.setTextColor(TFT_BLACK);

    uint16_t v = analogRead(ADC_PIN);
    float battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);

    int batteryLevel = map(battery_voltage, 0.0f, 4.0f, 0, 100);
    // int batteryLevel = map(analogRead(34), 0.0f, 4095.0f, 0, 100);
    if (battery_voltage > 4.5) {
      tft.drawString("---", tft_width - 28, 7);
    }
    else {
      tft.drawString(String(battery_voltage), tft_width - 28, 7);
    }


    // tft.fillRect(0, 21, tft_width, tft_height - 31, TFT_BLACK );
    // tft.setCursor(0, 22, 1);
    // tft.setTextColor(TFT_WHITE);
    // tft.println("Voltage :" + String(battery_voltage) + "V");

  }
}

void LinkStatus()
{
  bool LinkStat = ENC28J60::isLinkUp();
  //Serial.print(String(LinkStat));
  if (ENCLink != LinkStat || justbooted == true) {
    justbooted = false;
    ENCLink = LinkStat;
    if (LinkStat == true ) {
      tft.setTextColor(TFT_BLACK);
      tft.pushImage(tft_width - 50, 0, 16, 21, image_up);
      DHCP();

    }
    if (LinkStat == false ) {

      tft.setTextColor(TFT_BLACK);
      tft.fillRect(0, tft_height - 10, tft_width, 10, TFT_WHITE );
      tft.pushImage(tft_width - 50, 0, 16, 21, image_down);

    }
  }
}

void DHCP() {

  // Reset Array to defaults.
  OptionCount = 0;
  for ( int j = 0; j < 255; ++j) {
    DHCP_info[j].Option[0] = "EMPTY";
    DHCP_info[j].Option[1] = "EMPTY";
  }

  tft.fillRect(0, 21, tft_width, tft_height, TFT_BLACK );
  tft.fillRect(0, tft_height - 10, tft_width, 10, TFT_WHITE );
  tft.setTextColor( TFT_BLACK);
  tft.setCursor(5, tft_height - 9, 1);
  tft.println("Getting DHCP...");

  if (!ether.dhcpSetup())
  {
    tft.fillRect(0, tft_height - 10, tft_width, 10, TFT_WHITE );
    tft.setTextColor( TFT_RED);
    tft.setCursor(5, tft_height - 9, 1);
    tft.println("DHCP FAILED!");
  }
  else
  {
    String ipaddy;
    for (unsigned int j = 0; j < 4; ++j) {
      ipaddy  += String(ether.myip[j]);
      if (j < 3) {
        ipaddy += ".";
      }
    }
    tft.fillRect(0, tft_height - 10, tft_width, 10, TFT_WHITE );
    tft.setTextColor( TFT_BLACK);
    tft.setCursor(5, tft_height - 9, 1);
    tft.println("IP:" + ipaddy);

    tft.fillRect(0, 21, tft_width, tft_height - 31, TFT_BLACK );
    tft.setCursor(0, 22, 1);
    tft.setTextColor(TFT_WHITE);

    for (unsigned int j = 0; j < 254; ++j) {
      drawtext(DHCP_info[j].Option);
    }
  }
}



void button_init()
{
    btn1.setLongClickHandler([](Button2 & b) {
        btnCick = false;
        int r = digitalRead(TFT_BL);
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Press again to wake up",  tft.width() / 2, tft.height() / 2 );
        espDelay(6000);
        digitalWrite(TFT_BL, !r);

        tft.writecommand(TFT_DISPOFF);
        tft.writecommand(TFT_SLPIN);
        //After using light sleep, you need to disable timer wake, because here use external IO port to wake up
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
        // esp_sleep_enable_ext1_wakeup(GPIO_SEL_35, ESP_EXT1_WAKEUP_ALL_LOW);
        esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, 0);
        delay(200);
        esp_deep_sleep_start();
    });
    btn1.setPressedHandler([](Button2 & b) {
        Serial.println("Detect Voltage..");
        btnCick = true;
    });

    btn2.setPressedHandler([](Button2 & b) {
        btnCick = false;
        Serial.println("btn press: Refresh DHCP");
        DHCP();
    });
}

void button_loop()
{
    btn1.loop();
    btn2.loop();
}
void espDelay(int ms)
{
    esp_sleep_enable_timer_wakeup(ms * 1000);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_light_sleep_start();
}

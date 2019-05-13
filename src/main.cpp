#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <EasyNTPClient.h>
#include <WiFiUdp.h>
#include <Time.h>
#include <Regexp.h>
#include <Ticker.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>

extern "C" {
#include <user_interface.h>
}

#include <ReleTemporizado.h>

const int8_t PIN_RIEGO = D3;
//const int8_t PIN_RIEGO = BUILTIN_LED;

// Para watchdog
Ticker tickerOSWatch;

#define HEAPCHECKER 1        // set to 1 to test HEAP loss fix

WiFiUDP udp;
EasyNTPClient ntpClient(udp, "193.92.150.3", ((2*60*60))); // IST = GMT + 2
char ssid[] = WIFI_SSID;
char password[] = WIFI_PASSWORD;
#define BOTmax_timeout 60

#define ALARM_DELAY 500 // Tiempo de espera procesando alarmas

// Tiempos para configuracion de deep sleep. Siempre en segundos
#define SLEEP_TIME 60 * 15             // Tiempo de deepsleep
#define TIME_AWAKE_BEFORE_ALARM  60 * 5  

// Una vez recibido un mensaje, cuanto tarda en dormir (en segundos)
#define TIME_AWAKE_WHEN_MESSAGES 60

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);
ReleTemporizado riego(PIN_RIEGO, bot); // builtin led de wemos
MatchState ms; // regular expressions


unsigned long last_wifi_check;
unsigned long last_message;

/////////////////////////
#define OSWATCH_RESET_TIME 300

static unsigned long last_loop;

void ICACHE_RAM_ATTR osWatch(void) {
    unsigned long t = millis();
    unsigned long last_run = abs(t - last_loop);
    if(last_run >= (OSWATCH_RESET_TIME * 1000)) {
      // save the hit here to eeprom or to rtc memory if needed
        Serial.print("Restarting from osWatch");
        delay(1000);
        ESP.restart();  // normal reboot 
        //ESP.reset();  // hard reset
    }
}

String append0(int value) {
    if (value <= 9)
        return "0" + String(value);
    else
        return String(value);
    //return (value <= 9 ? "0" + String(value) : String(value));
}

String value2time(int value) {
    value = value / 60;
    int h = value / 60;
    int m = value % 60;
    return append0(h) + ":" + append0(m);
}


void cb_muestra_estado (const char * match,          // matching string (not null-terminated)
                      const unsigned int length,   // length of matching string
                      const MatchState & ms)      // MatchState in use (to get captures)
{
  riego.muestraEstado();
}

void cb_apaga(const char * match,          // matching string (not null-terminated)
                      const unsigned int length,   // length of matching string
                      const MatchState & ms)      // MatchState in use (to get captures)
{
    riego.off("Apago riego");
}

void cb_enciende_una_vez (const char * match,          // matching string (not null-terminated)
                      const unsigned int length,   // length of matching string
                      const MatchState & ms)      // MatchState in use (to get captures)
{
    riego.enciendeUnaVez();
}

void cb_enciende_una_vez_minutos (const char * match,          // matching string (not null-terminated)
                      const unsigned int length,   // length of matching string
                      const MatchState & ms)      // MatchState in use (to get captures)
{
  char cap [10];   // must be large enough to hold captures
  riego.enciendeUnaVez(atoi(ms.GetCapture(cap,0)));
}

void cb_activa_programa(const char * match,          // matching string (not null-terminated)
                      const unsigned int length,   // length of matching string
                      const MatchState & ms)      // MatchState in use (to get captures)
{
  char cap [10];   // must be large enough to hold captures
  riego.activa(atoi(ms.GetCapture(cap,0)));
}


void cb_desactiva_programa(const char * match,          // matching string (not null-terminated)
                      const unsigned int length,   // length of matching string
                      const MatchState & ms)      // MatchState in use (to get captures)
{
  char cap [10];   // must be large enough to hold captures
  riego.desactiva(atoi(ms.GetCapture(cap,0)));
}


void cb_activa(const char * match,          // matching string (not null-terminated)
                      const unsigned int length,   // length of matching string
                      const MatchState & ms)      // MatchState in use (to get captures)
{
    riego.activa();
}

void cb_desactiva(const char * match,          // matching string (not null-terminated)
                      const unsigned int length,   // length of matching string
                      const MatchState & ms)      // MatchState in use (to get captures)
{
    riego.desactiva();
}
void cb_enciende_de_a (const char * match,          // matching string (not null-terminated)
                      const unsigned int length,   // length of matching string
                      const MatchState & ms)      // MatchState in use (to get captures)
{
  char cap [10];   // must be large enough to hold captures
  riego.enciendeDeA(atoi(ms.GetCapture(cap,0)),
                    atoi(ms.GetCapture(cap,1)),
                    atoi(ms.GetCapture(cap,2)),
                    atoi(ms.GetCapture(cap,3)));
}

void cb_settime (const char * match,          // matching string (not null-terminated)
                      const unsigned int length,   // length of matching string
                      const MatchState & ms)      // MatchState in use (to get captures)
{
  char cap [10];   // must be large enough to hold captures
  setTime(atoi(ms.GetCapture(cap,3)), // hora
                    atoi(ms.GetCapture(cap,4)),  // min 
                    0,                           // seq
                    atoi(ms.GetCapture(cap,0)),  // dia
                    atoi(ms.GetCapture(cap,1)),  // mes
                    atoi(ms.GetCapture(cap,2))); // ano
  String fecha = "Son las " + String(hour()) + ":" + String(append0(minute())) +
      " del " + String(day()) + "/" + String(month()) +
      "/" + String(year());
  bot.sendMessage(BOTchat_id, "\xF0\x9F\x91\x8D " + fecha, ""); // http://apps.timwhitlock.info/emoji/tables/unicode
      riego.loadFromEeprom(0);
  riego.loadFromEeprom(0);
}

void cb_borra(const char * match,          // matching string (not null-terminated)
                      const unsigned int length,   // length of matching string
                      const MatchState & ms)      // MatchState in use (to get captures)
{
  char cap [10];   // must be large enough to hold captures
  riego.borraPrograma(atoi(ms.GetCapture(cap,0)));
}


void cb_save(const char * match,          // matching string (not null-terminated)
                      const unsigned int length,   // length of matching string
                      const MatchState & ms)      // MatchState in use (to get captures)
{
  riego.saveToEeprom(0);
}

void cb_load(const char * match,          // matching string (not null-terminated)
                      const unsigned int length,   // length of matching string
                      const MatchState & ms)      // MatchState in use (to get captures)
{
  riego.loadFromEeprom(0);
}

void handleNewMessages(int numNewMessages) {
  for (int i=0; i<numNewMessages; i++) {
    last_message = millis();
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "") from_name = "Guest";

    char buf[100];
    String textFormatted = text;
    textFormatted.toLowerCase();
    textFormatted.replace(".", ":");
    textFormatted.replace(" el riego", "X");
    textFormatted.replace(" programa", "X");
    textFormatted.replace(" min", "M");
    textFormatted.replace(" minutos", "M");
    textFormatted.replace(" minuto", "M");
    textFormatted.toCharArray(buf, 100);

    ms.Target(buf);

    int count;
    if (! ms.GlobalMatch ("enciendeX? de (%d%d?):(%d%d) a (%d%d?):(%d%d)", cb_enciende_de_a))
    if (! ms.GlobalMatch("enciendeX?$", cb_enciende_una_vez))
    if (! ms.GlobalMatch("enciende (%d%d?)M?", cb_enciende_una_vez_minutos))
    if (! ms.GlobalMatch("borraX? (%d)$", cb_borra))
    if (! ms.GlobalMatch("estado", cb_muestra_estado))
    if (! ms.GlobalMatch("desactivaX? (%d%d?)$", cb_desactiva_programa))
    if (! ms.GlobalMatch("activaX? (%d%d?)$", cb_activa_programa))
    if (! ms.GlobalMatch("desactivaX?", cb_desactiva))
    if (! ms.GlobalMatch("activaX?", cb_activa))
    if (! ms.GlobalMatch("save$", cb_save))
    if (! ms.GlobalMatch("load$", cb_load))
    if (! ms.GlobalMatch("apaga$", cb_apaga))
    if (! ms.GlobalMatch("(%d%d)/(%d%d)/(%d%d%d%d) (%d%d):(%d%d)$", cb_settime))
      bot.sendMessage(chat_id, "no te entiendo... \xF0\x9F\x98\xB3", ""); // http://apps.timwhitlock.info/emoji/tables/unicode
  }

}
/*********************************************************************
 * Funcion de actualizacion de tiempo
 ********************************************************************/
time_t updateTime() {
    return ntpClient.getUnixTime();
}


/*********************************************************************
 * setup
 ********************************************************************/
void setup() {

  // Something is wrong with https connections
  client.setInsecure();

  // Apago todo en el inicio
  riego.off("");

  // D0 to RST to wake up
  pinMode(D0, WAKEUP_PULLUP);

  Serial.begin(74880);
  EEPROM.begin(1024);

  // Set WiFi to station mode and disconnect from an AP if it was Previously
  // connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  struct rst_info *resetInfo = ESP.getResetInfoPtr();
  if (resetInfo->reason == REASON_DEEP_SLEEP_AWAKE) { // normal startup by power on 
      Serial.println("REASON_DEEP_SLEEP_AWAKE");
  } else {
      String info = ESP.getResetInfo();
      Serial.println(info);
  }
//or
//int reason=*rstInfo.reason;

  // attempt to connect to Wifi network:
  Serial.print("Conectando a wifi ");
  Serial.print(ssid);
  WiFi.hostname(HOSTNAME);
  WiFi.begin(ssid, password);

  int tries = 15;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
    if (tries-- == 0)
    {
        Serial.print("No wifi? Resseting...");
        delay(1000);
        ESP.deepSleep(60 * 10 * 1000000);
        delay(1000);
        ESP.restart();
    }
  }

  Serial.println("");
  Serial.println("WiFi connectada");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  last_wifi_check = millis();
  last_message = millis();

  // Sincronizar fecha y hora

  setSyncProvider(updateTime);
  Serial.print("Syncronizando tiempo.");
  tries = 10;
  while (timeStatus() == timeNotSet && tries-->0) {
    Serial.print(".");
    delay(500);
  }

  // Mensaje de bienvenida
  String mensaje = "Hola! soy *Riego*. Acabo de encenderme \xF0\x9F\x98\x81\n";

  if (timeStatus() == timeNotSet) {
      mensaje += "No he podido consultar la hora. Por favor, dime en formato DD/MM/YYYY HH:MM la fecha y hora actual\n";
  } else {
      riego.loadFromEeprom(0);
  }

  if (resetInfo->reason != REASON_DEEP_SLEEP_AWAKE) {
      bot.sendMessage(BOTchat_id, mensaje, "Markdown");
      riego.setVerbose(1);
      riego.muestraEstado();
  }

  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes libres.\nFin de setup.");

  last_loop = millis();
  tickerOSWatch.attach_ms(((OSWATCH_RESET_TIME / 3) * 1000), osWatch);
  
  riego.setVerbose(1);

  ArduinoOTA.begin();
}

void loop() {

  last_loop = millis();

  Alarm.delay(ALARM_DELAY);

  // Calculo si espero por mensajes del telegram el tiempo maximo
  // o hay alguna alarma inminente
    
  unsigned long secsToNextAlarm = (unsigned long) Alarm.getNextTrigger() - (unsigned long) now();

  bot.longPoll = (secsToNextAlarm > BOTmax_timeout || 
                 secsToNextAlarm < 0 ? 600: BOTmax_timeout);
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  if (numNewMessages > 0) {
      last_message = millis();

      Serial.print(ESP.getFreeHeap());
      Serial.println(" bytes libres");


  }

  handleNewMessages(numNewMessages);

  Serial.print(riego.isDirty());
  Serial.print(" ");
  Serial.print(secsToNextAlarm);
  Serial.print(" ");
  Serial.print( (millis() - last_message) / 1000 );
  Serial.print(" Alarmas disp: ");
  Serial.println(dtNBR_ALARMS  );
  if (secsToNextAlarm > SLEEP_TIME + TIME_AWAKE_BEFORE_ALARM and secsToNextAlarm < 60 * 60 * 24 * 365  // si alarma a la vista no duermo
          and millis() - last_message > TIME_AWAKE_WHEN_MESSAGES * 1000  // si acabo de enviar mensaje no duermo
          and !riego.isActive()   // Si esta encendido no duermo
          ) {
      Serial.print("Sin alarmas a la vista. A dormir");
      // Grabo programas sin guardar
      if (riego.isDirty())
          riego.saveToEeprom(0);
      delay(1000);
      ESP.deepSleep(SLEEP_TIME * 1000000);
  }

  // Handle OTA
  ArduinoOTA.handle();

}


//  ReleTemporizado

#ifndef ReleTemporizado_h 
#define ReleTemporizado_h

#include <Arduino.h>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <UniversalTelegramBot.h>
#include <EEPROM.h>

#define BOTchat_id "7579267"


#if !defined(dtNBR_ALARMS )
#if defined(__AVR__)
#define dtNBR_ALARMS 6   // max is 255
#elif defined(ESP8266)
#define dtNBR_ALARMS 20  // for esp8266 chip - max is 255
#else
#define dtNBR_ALARMS 12  // assume non-AVR has more memory
#endif
#endif

class ReleTemporizado
{
public:
  int pin;
  int defaultTime; // seconds between on and off
  bool estado;
  bool dirty;

  struct strAlarm {
      uint8_t startAlarmId;
      uint8_t endAlarmId;
      time_t startValue;
      time_t endValue;
      uint8_t isEnabled: 1;
  } alarms[dtNBR_ALARMS]; // guarda los id de las alarmas de inicio de riego, para listados

  uint8_t verbose = 0;

  ReleTemporizado(int pin, UniversalTelegramBot &bot);
  void enciende();

  void on(String mensaje);
  void off(String mensaje);

  void enciendeUnaVez();
  void enciendeUnaVez(int minutos);
  void enciendeDeA(int startH, int startM, int endH, int endM);
  void activa();
  void desactiva();
  void activa(uint8_t idAlarm);
  void desactiva(uint8_t idAlarm);
  void borraPrograma(int idAlarm);

  void muestraEstado();

  void saveToEeprom(int start);
  void loadFromEeprom(int start);

  void addAlarm(int ids, int ide);
  void removeAlarm(int id);
  uint8_t alarmCount = 0;

  bool isActive();
  bool isDirty();

  void setVerbose(uint8_t v);

  UniversalTelegramBot *bot;

private:
  bool checkAlarmInterval(int sH, int sM, int eH, int eM);
  void alarmaSolapa(int startH, int startM, int endH, int endM);
  String append0(int i);
  String descPrograma(int sH, int sM, int eH, int eM);
  String descPrograma(strAlarm sAlarm);

  void _activa(uint8_t id);
  void _desactiva(uint8_t id);

};

#endif /* ReleTemporizado_h */

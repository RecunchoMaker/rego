#include "TimeLib.h"
#include "TimeAlarms.h"
#include <ReleTemporizado.h>

// tiempo en minutos cuando se enciende con la orden "enciende"
#define DEFAULT_TIME 10

#define EMOTI_LLORO "\xF0\x9F\x98\xA2"
#define EMOTI_OK "\xF0\x9F\x91\x8D"

#define SP(X,Y) Serial.print(X);Serial.println(Y);

// Numero arbitrario por el que empiezan los datos de la EEPROM
// Si no es este numero, implica que es el primer inicio
#define EEPROM_ID (uint8_t) 15

//**************************************************************
//* Class Constructor

//ReleTemporizado::ReleTemporizado(int pin, UniversalTelegramBot &bot)
ReleTemporizado::ReleTemporizado(int pin, UniversalTelegramBot &bot)
{
   this->pin = pin; 
   pinMode(pin, OUTPUT);
   this->defaultTime = 5; // 5 minutos
   this->estado = false;
   this->bot = &bot;
   this->verbose = 0;
   this->dirty = false; // Hay nuevos programas sin grabar a EEPROM
   off("");
}

void ReleTemporizado::setVerbose(uint8_t) {
    verbose = 1;
}


bool ReleTemporizado::isActive() {
    return estado;
}

void ReleTemporizado::on(String mensaje) {
    Serial.println("Rele encendido");
    digitalWrite(pin, LOW);   // si, si, al reves
    if (isActive()) {
        if (verbose) bot->sendMessage(BOTchat_id, "Ya esta encendido", "");
    }
    else {
        bot->sendMessage(BOTchat_id, mensaje, "");
        estado = true;
    }
}

void ReleTemporizado::off(String mensaje) {
    Serial.println("Rele apagado");
    digitalWrite(pin, HIGH); 
    
    if (isActive()) {
        bot->sendMessage(BOTchat_id, mensaje, "");
        estado = false;
    }
    else {
        // bot->sendMessage(BOTchat_id, "Iba a apagar, pero ya esta apagado", "");
    }
}

void ReleTemporizado::addAlarm(int startAlarmId, int endAlarmId) {
    int id;
    /*
    Serial.print("Alarm count = ");
    Serial.println(alarmCount);
    for(int kk = 0; kk<alarmCount; kk++) {
        Serial.print(kk);
        Serial.print(" - ");
        Serial.println(descPrograma(alarms[kk]));
    }
    */


    if (alarmCount > 0) {
        // Encuentra la alarma anterior
        time_t startTime = Alarm.read(startAlarmId);
        for (id = 0; alarms[id].startValue < startTime and id < alarmCount; id++);
        /*
        Serial.print("El id es :");
        Serial.println(id);
        Serial.print("El que ya se pasa es...");
        Serial.println(Alarm.read(id));
        */

        // Hago el hueco
        for (int i = alarmCount; i>=id; i--) {
            alarms[i+1] = alarms[i];
        }
    } else {
        id = 0;
    }

    alarms[id].startAlarmId = startAlarmId;
    alarms[id].startValue = Alarm.read(startAlarmId);
    alarms[id].endValue = Alarm.read(endAlarmId);
    alarms[id].isEnabled = true;
    alarms[id].endAlarmId = endAlarmId;

    alarmCount++;
}

void ReleTemporizado::removeAlarm(int id) {
    Alarm.free(alarms[id].startAlarmId);
    Alarm.free(alarms[id].endAlarmId);
    for (int i = id; i<alarmCount; i++) {
        alarms[i] = alarms[i+1];
    }
    alarmCount--;
}

void ReleTemporizado::enciendeDeA(int startH, int startM, int endH, int endM) {

  alarmaSolapa(startH, startM, endH, endM);

  std::function<void(void)> cb_on = std::bind(&ReleTemporizado::on, this,
         "\xE2\x9B\xB2 Comienzo riego " + descPrograma(startH, startM, endH, endM));
  std::function<void(void)> cb_off = std::bind(&ReleTemporizado::off, this, 
         "\xF0\x9F\x93\xB4 Apago el riego " + descPrograma(startH, startM, endH, endM));

  if (checkAlarmInterval(startH, startM, endH, endM)) {
      int idstart = Alarm.alarmRepeat(startH, startM, 0, cb_on);
      int idend = Alarm.alarmRepeat(endH, endM, 0,cb_off);
      addAlarm(idstart, idend);
      if (verbose) bot->sendMessage(BOTchat_id, "\xF0\x9F\x91\x8D Programado un riego " + descPrograma(startH, startM, endH, endM), "");

      this->dirty = true;
  }
  else {
      bot->sendMessage(BOTchat_id, "Uhm... no puedo programar " + descPrograma(startH, startM, endH, endM), "");
  }
}

void ReleTemporizado::desactiva(uint8_t id) {
    if (verbose) bot->sendMessage(BOTchat_id, "\xF0\x9F\x91\x8D Desactivo el programa " + String(id), "");
    // Por claridad, los programas empiezan por 1, asi que el id real es uno menos
    _desactiva(id-1);
    this->dirty = true;
}

void ReleTemporizado::activa(uint8_t id) {
    if (verbose) bot->sendMessage(BOTchat_id, "\xF0\x9F\x91\x8D Activo el programa " + String(id), "");
    // Por claridad, los programas empiezan por 1, asi que el id real es uno menos
    _activa(id-1);
    this->dirty = true;
}

void ReleTemporizado::activa() {
    bot->sendMessage(BOTchat_id, "\xF0\x9F\x91\x8D Todos los riegos activados", "");
    for(uint8_t id = 0; id < alarmCount; id++) {
        alarms[id].isEnabled = 1;
        Alarm.enable(alarms[id].startAlarmId);
        Alarm.enable(alarms[id].endAlarmId);
    }
    this->dirty = true;
}


void ReleTemporizado::desactiva() {
    bot->sendMessage(BOTchat_id, "\xF0\x9F\x91\x8D Todos los riegos desactivados", "");
    off("Paro tambien el programa actual");
    for(uint8_t id = 0; id < alarmCount; id++) {
        alarms[id].isEnabled = 0;
        Alarm.disable(alarms[id].startAlarmId);
        Alarm.disable(alarms[id].endAlarmId);
    }
    this->dirty = true;
}

void ReleTemporizado::enciendeUnaVez() {
    enciendeUnaVez(DEFAULT_TIME);
}

void ReleTemporizado::enciendeUnaVez(int minutos) {
    if (isActive()) {
        bot->sendMessage(BOTchat_id, "Ya esta encendido", "");
    }
    else {
        on("\xF0\x9F\x91\x8D Enciendo el riego " + String(minutos) + " minutos");
        std::function<void(void)> cb_off = std::bind(&ReleTemporizado::off, this, "Riego apagado");
        Alarm.timerOnce(minutos * 60, cb_off); 
    }
}

void ReleTemporizado::muestraEstado() {

  String mensaje = "Son las " + String(hour()) + ":" + String(append0(minute())) +
      " del " + String(day()) + "/" + String(month()) +
      "/" + String(year()) + "\n";
  if (isActive()) {
    mensaje += "El riego esta *encendido*\n";
  }
  else {
    mensaje += "El riego esta apagado\n";
  }

  if (alarmCount > 0)
      mensaje += "Listado de programas:\n";
  else
      mensaje += "No hay ningun riego programado \xF0\x9F\x98\xA2\n";
  for(uint8_t id = 0; id < alarmCount; id++) {
      // TODO. Todos los dias, de lunes a viernes... etc
      //
     //dtAlarmPeriod_t readType(AlarmID_t ID);   // return the alarm type for the given alarm ID
      mensaje += "*" + String(id+1) + "*. Todos los dias " + descPrograma(this->alarms[id]);
      if (!Alarm.isEnabled(alarms[id].startAlarmId)) {
          mensaje += " _(desactivado)_";
      }
      mensaje += "\n";
          
  }

  mensaje += String(ESP.getFreeHeap()) + " bytes libres";

  bot->sendMessage(BOTchat_id, mensaje, "Markdown");

}

//**************************************************************
//* Metodos privados 
// TODO: comprobar que la alarma sea correcta, no solape... etc
bool ReleTemporizado::checkAlarmInterval(int sH, int sM, int eH, int eM) {
    return (sH >=0) && (sH <24) && 
           (sM >=0) && (sM <60) &&
           (eH >=0) && (eH <24) &&
           (eM >=0) && (eM <60) &&
        ( (eH * 60 + eM) - (sH * 60 + sM) < 45 ) &&
        ( (eH * 60 + eM) - (sH * 60 + sM) > 0 );
}

//TODO
String ReleTemporizado::append0(int i) {
    if (i<= 9)
        return "0" + String(i);
    else
        return String(i);
    //return (value <= 9 ? "0" + String(value) : String(value));
}

String ReleTemporizado::descPrograma(int sH, int sM, int eH, int eM) {
    return "de " + append0(sH) + ":" + append0(sM) + " a " +
        append0(eH) + ":" + append0(eM) + " (" + String( (eH-sH) * 60 + eM - sM) + " minutos)";
}

String ReleTemporizado::descPrograma(strAlarm sAlarm) {
    int v1 = Alarm.read(sAlarm.startAlarmId);
    int v2 = Alarm.read(sAlarm.endAlarmId);
    int sH = v1 / (60*60); int sM = (v1 / 60) % 60;
    int eH = v2 / (60*60); int eM = (v2 / 60) % 60;
    return descPrograma(sH, sM, eH, eM);
}

void ReleTemporizado::loadFromEeprom(int start) {
    strAlarm s;

    int mem = start;
    Serial.println("Load from eeprom...");

    // Libera todas las alarmas
    for(uint8_t id = 0; id < dtNBR_ALARMS; id++) {
        Alarm.free(id);   // ensure all Alarms are cleared and available for allocation
    }
    alarmCount = 0; // se reprograman en el bucle
    off("");
    
    // Comprueba si primera carga. El primer caracter debe ser el identificador del Riego
    uint8_t id_riego = EEPROM.read(mem);
    if (id_riego != EEPROM_ID) {
        alarmCount = 0;
        saveToEeprom(start);
    } else {
        mem += sizeof(id_riego);
        uint8_t cuenta = EEPROM.read(mem);
        mem += sizeof(cuenta);
        for(int i = 0; i<cuenta; i++) {
            EEPROM.get(mem, s);  
            mem += sizeof(s);
            int sH = s.startValue / (60*60); int sM = (s.startValue / 60) % 60;
            int eH = s.endValue / (60*60); int eM = (s.endValue / 60) % 60;
            enciendeDeA(sH, sM, eH, eM);
            if (!s.isEnabled)
                _desactiva(i+1); // desactiva en silencio
            else
                _activa(i+1);    // activa en silencio
        }
    }

}

void ReleTemporizado::saveToEeprom(int start) {
    int mem = start;
    Serial.println("Grabando datos...");

    EEPROM.write(mem, EEPROM_ID);
    mem += 1;

    EEPROM.write(mem, alarmCount);

    mem += sizeof(alarmCount);
    for (uint8_t i = 0; i<alarmCount; i++) {
        EEPROM.put(mem, alarms[i]);  
        mem += sizeof(alarms[i]);
    }
    Serial.print("Ultimo byte grabado en eeprom en posicion ");
    Serial.println(mem);
    EEPROM.commit();
    bot->sendMessage(BOTchat_id, "\xF0\x9F\x91\x8D Programacion guardada", "");

    this->dirty = false;
}


void ReleTemporizado::borraPrograma(int idAlarm) {
    if (Alarm.readType(alarms[idAlarm-1].startAlarmId) == dtNotAllocated) {
        bot->sendMessage(BOTchat_id, "\xF0\x9F\x98\xB3 Hey, el programa " + String(idAlarm) + " no existe!", "");
    } else {
        bot->sendMessage(BOTchat_id, "\xF0\x9F\x91\x8D Borrado el programa " + descPrograma(alarms[idAlarm-1]), "");
        removeAlarm(idAlarm-1);
        this->dirty = true;
    }
}

void ReleTemporizado::alarmaSolapa(int sH, int sM, int eH, int eM) {
    int ts = sH * 60 * 60 + sM * 60; // this start
    int te = eH * 60 * 60 + eM * 60; // this end
    for (int i = 0; i<alarmCount; i++) {
        int as = Alarm.read(alarms[i].startAlarmId);   // alarm start
        int ae = Alarm.read(alarms[i].endAlarmId); // alarm end
        
        if (( as <= ts) && (ts <= ae) ||  // solapa el start
            ( as >= ts) && (te >= ae) ||  // lo contiene
            ( as <= te) && (te <= ae))    // solapa el end

        {
            bot->sendMessage(BOTchat_id, "Solapa con el " + descPrograma(alarms[i]) + ", lo borro", "");
            removeAlarm(i);
            i--; // vuelva a comprobar este indice, ya que remove la reescribe
        }
    }
}

void ReleTemporizado::_activa(uint8_t id) {
    alarms[id].isEnabled = 1;
    Alarm.enable(alarms[id].startAlarmId);
    Alarm.enable(alarms[id].endAlarmId);
    this->dirty = true;
}

void ReleTemporizado::_desactiva(uint8_t id) {
    alarms[id].isEnabled = 0;
    Alarm.disable(alarms[id].startAlarmId);
    Alarm.disable(alarms[id].endAlarmId);
    this->dirty = true;
}

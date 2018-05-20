# Repote (rego progamable por Telegram)

**Repote** o é un pequeno dispositivo para controlar por medio de Telegram un relé. Pódese encender ou apagar mediante mensaxes, así como programar tempos de encendido e apagado. Está pensado para un pequeno rego automático de unha fase.

## Hardware
Utiliza un controlador __Lolin__ (anteriormente Wemos), un rele activable a 3,3 V. e un trafo conversor de 220v a 16v (ou o que sexa necesario para alimentar a electroválvula). 

E importante conectar o pin RESET a D0 para que o ESP8266 poida volver do modo Sleep

## Instalacion
* Instala a ultima versión de PlatformIO (patformio.org)
* Crea un bot de Telegram con GodFather
* Clona o repositorio con **git clone https://github.com/RecunchoMaker/repote.git**
* cd repote
* cp platform.sample.ini platform.ini
* Edita o arquivo platform.ini, poñendo as túas credenciais Wifi, o token do bot, e o chat_id do usuario con permisos para controlar o dispositivo
* pio run --target upload

## Funcionamento
O dispositivo arranca e unha vez conectado á rede consulta a data e hora nun servidor NTP, carga a programación de encendidos/apagados na EEPRON e manda unha mensaxe de benvida. A partir de aquí pódenselle enviar os seguintes comandos:
* **estado**: amosa a data/hora, o estado actual (encendido/apagado) e o listado de programas
* **enciende (n minutos)**: encende inmediatamente o rego. Opcionalmente pódese especificar un número de minutos
* **apaga**: apaga o rego.
* **enciende de _hh.mm_ a _hh.mm_**: programa un rego dende/ata as horas especificadas, en formato hh.mm
* **borra _n_**: borra o programa número n, segundo os indices que se amosan no comando estado.
* **desactiva (_n_) **: desactiva todos os programas, ou opcionalmente un número específico de programa.
* **activa (_n_) **: activa todos os programas, ou opcionalmente un número específico de programa.
* **save**: graba os programas na EEPROM
* **load**: recupera os programas da EEPROM

O dispositivo entra en modo SLEEP para aforrar consumo pasado un intervalo de tempo dende a última mensaxe, así que non responderá inmediatamente aso comandos (pódese reducir, aumentar ou incluso evitar ese retardo coa variable TIME_AWAKE_WHEN_MESSAGES.

Podes programar por OTA o dispositivo, tendo en conta que se o sistema está en MODO sleep ou consultado as mensaxes, non responderá a petición. É preciso, antes de enviar a actualización, enviar unha mensaxe ao bot (por exemplo, **estado**)

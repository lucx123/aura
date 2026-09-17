# AURA Watch - Hardware confirmado

## Placa adquirida

**Modelo:** Waveshare ESP32-S3-Touch-AMOLED-2.06 (serigrafia confirmada en la placa)
**Estado:** recibida; funciona en una prueba inicial comunicada el 2026-09-14.
Queda pendiente documentar la revision y las pruebas de cada periferico.

**Funcion:** plataforma completa del primer AURA Watch

Es una placa de desarrollo vestible con formato de reloj. Reune procesamiento,
pantalla tactil, movimiento, reloj de tiempo real, audio, almacenamiento y gestion
de energia en una unica plataforma, reduciendo la cantidad de circuitos adicionales
necesarios para el primer prototipo.

## Especificaciones conocidas

| Componente | Especificacion | Uso previsto en AURA |
|------------|----------------|----------------------|
| MCU | ESP32-S3R8, Xtensa LX7 dual-core hasta 240 MHz | Firmware, interfaz, conectividad y procesamiento local |
| Memoria interna | 512 KB SRAM, 384 KB ROM | Runtime y tareas de tiempo real |
| PSRAM | 8 MB integrada | Buffers de pantalla, audio y recursos de UI |
| Flash | 32 MB externa | Firmware, assets, modelos pequeños y OTA |
| Pantalla | AMOLED tactil 2.06", 410 x 502, 16.7 M colores | Reloj, ojos, tarjetas, estados y controles |
| Display driver | CO5300 mediante QSPI | Renderizado de la interfaz |
| Touch | FT3168 capacitivo mediante I2C | Gestos, botones y navegacion |
| Conectividad | Wi-Fi 2.4 GHz 802.11 b/g/n y Bluetooth 5 LE | Gateway directo y enlace con app movil |
| IMU | QMI8658 de seis ejes | Gestos, postura, movimiento y pasos |
| RTC | PCF85063 con alimentacion gestionada por AXP2101 | Hora persistente y alarmas de bajo consumo |
| Energia | PMIC AXP2101 | Carga, bateria, voltajes y modos de ahorro |
| Bateria | Conector MX1.25 para Li-ion/LiPo 3.7 V | Alimentacion portatil |
| Audio | Codec de audio integrado | Captura y reproduccion; modelo por verificar |
| Almacenamiento | Ranura microSD | Logs, recursos, audio temporal y medios |
| Controles | Botones laterales PWR y BOOT | Encendido, acceso rapido y recuperacion |
| Expansion | 1 x I2C, 1 x UART y 1 x USB Type-C | Perifericos, depuracion y carga |

Estas especificaciones proceden de la ficha del producto adquirido. El pinout,
revision de PCB, codec de audio y perifericos incluidos se confirmaran contra la
unidad recibida antes de escribir drivers definitivos. La prueba inicial confirma
funcionamiento general, pero aun no hay un registro detallado de perifericos.

El 2026-09-14 se detecto la placa por USB en COM4. Una lectura con esptool 5.4.0
confirmo ESP32-S3 revision de chip v0.2, PSRAM de 8 MB y flash de 32 MB.
Se guardo una copia completa de la flash de fabrica, verificada por tamano y
SHA-256, en `firmware/aura_watch/backups/` (excluida de Git). La revision del
chip no identifica por si sola la revision de la PCB.

Ese mismo dia se compilo el primer firmware AURA con ESP-IDF 5.5.5 y el BSP
Waveshare 2.0.0, y se cargo en COM4 con verificacion de hashes. El arranque USB
confirma PSRAM correcta y la inicializacion de la pantalla. La prueba manual
del boton tactil sigue pendiente de confirmacion.

## Funciones que habilita

### Interfaz y personalidad

La pantalla AMOLED 410 x 502 permite combinar una interfaz de reloj real con la
presencia visual de AURA:

- hora, fecha, bateria y conectividad;
- ojos y microanimaciones;
- estados `idle`, `listening`, `thinking`, `speaking` y `error`;
- tarjetas como "Reunion en 12 min";
- controles tactiles y boton "Hablar con AURA";
- mensajes, respuestas breves y confirmaciones de acciones.

La UI debe privilegiar fondos negros para aprovechar el AMOLED y reducir consumo.

### Voz

El ESP32-S3 ejecutara localmente la wake word y el control del pipeline de audio.
Tras escuchar "Aura":

1. activa la interfaz de escucha;
2. captura audio mediante el codec integrado;
3. envia audio o texto a la app/gateway;
4. recibe la respuesta generada;
5. reproduce TTS y sincroniza la animacion visual.

La viabilidad del wake word continuo, audio full-duplex y cancelacion de eco se
medira en la placa real. Los modelos STT, LLM y TTS pesados no correran en el reloj.

### Movimiento y contexto

La IMU QMI8658 puede habilitar:

- levantar la muñeca para encender la pantalla;
- conteo de pasos y deteccion basica de actividad;
- gestos para aceptar, descartar o silenciar;
- orientacion de la interfaz;
- deteccion de inactividad o movimiento relevante;
- contexto para decidir como y cuando avisar.

Los datos de actividad no deben almacenarse ni enviarse sin una politica explicita.

### Tiempo y proactividad

El RTC PCF85063 permite mantener la hora y programar eventos aun cuando el procesador
entre en bajo consumo. Esto es util para:

- alarmas y recordatorios;
- despertar el sistema para una notificacion;
- horarios de modo no molestar;
- sincronizacion posterior cuando no exista conexion.

### Conectividad

- **BLE:** app Android propia, configuración, eventos y puente a EC2; audio
  comprimido por turnos pendiente de pruebas.
- **Wi-Fi:** AURA Gateway directo, audio, recursos, sincronizacion y OTA.

Ambas radios comparten recursos de 2.4 GHz. Se deben medir latencia, consumo y
estabilidad antes de mantener trafico simultaneo prolongado.

Ver [diseño de la app y los transportes](14-app-android-conectividad.md).

## Arquitectura inicial de firmware

```text
┌─────────────────────────────────────────────┐
│                 AURA Watch                  │
├──────────────┬──────────────┬───────────────┤
│ UI/Touch     │ Voice        │ Context       │
│ CO5300       │ wake/VAD     │ QMI8658 + RTC │
│ FT3168       │ capture/play │ notifications │
├──────────────┴──────┬───────┴───────────────┤
│ Connectivity        │ Device services       │
│ BLE · Wi-Fi         │ power · SD · OTA      │
├─────────────────────┴───────────────────────┤
│          ESP32-S3R8 / FreeRTOS              │
└─────────────────────────────────────────────┘
```

Modulos propuestos:

```text
firmware/aura_watch/
├── app/             # Estado principal y coordinacion
├── ui/              # Pantallas, ojos, tarjetas y touch
├── audio/           # Codec, wake word, VAD y playback
├── connectivity/    # BLE, Wi-Fi y cliente AURA Gateway
├── sensors/         # IMU, gestos y actividad
├── power/           # AXP2101, bateria y sleep
├── storage/         # microSD y recursos locales
├── ota/             # Actualizaciones seguras
└── board/           # Pinout y drivers Waveshare
```

## Plan de validacion de la placa

- [x] Recibir la placa y comprobar funcionamiento inicial.
- [x] Confirmar el modelo impreso en la placa: ESP32-S3-Touch-AMOLED-2.06.
- [x] Detectar por USB el ESP32-S3 y respaldar los 32 MB de flash.
- [x] Compilar y cargar la pantalla inicial de AURA; confirmar arranque por USB.
- [ ] Fotografiar y registrar revision exacta de PCB.
- [ ] Confirmar pinout, componentes y modelo del codec de audio.
- [ ] Compilar y ejecutar el ejemplo oficial de fabrica.
- [ ] Verificar pantalla CO5300, touch FT3168, IMU y RTC.
- [ ] Verificar microSD, botones, USB y estado de carga.
- [ ] Medir consumo en idle, pantalla activa, BLE, Wi-Fi y audio.
- [ ] Confirmar capacidad real de bateria y tiempos de carga.
- [ ] Probar deep sleep y despertar por RTC, boton e IMU.
- [ ] Crear el proyecto `firmware/aura_watch` con una prueba por periferico.
- [ ] Implementar una primera pantalla con hora, ojos y "Hablar con AURA".

## Decisiones pendientes

- bateria concreta y objetivo de autonomia;
- microfono y altavoz incluidos o externos;
- motor de vibracion y su driver;
- framework grafico y estrategia de renderizado;
- motor de wake word compatible con ESP32-S3;
- protocolo de audio hacia la app o gateway;
- formato de assets y uso de microSD;
- estrategia de particiones para firmware, recursos y OTA.

## Firmware offline 0.2 - 2026-09-14

El usuario confirmo pantalla y touch con la prueba inicial Touch OK. Se desarrollo
una interfaz propia con ojos animados/interactivos, reloj analogico y digital,
temporizador, brillo y temas. Se detectaron PCF85063 y AXP2101 por I2C; la hora
se sincronizo desde el PC y se recupero correctamente tras un reinicio USB.
La bateria informa 100% durante la prueba conectada; no es una medicion de autonomia.

La finalizacion del temporizador se verifico con una cuenta de dos segundos y
captura del mensaje Tiempo cumplido. El modo descanso baja el brillo, sin deep
sleep. No incorpora aun IA, voz, BLE o gateway. Codigo e instrucciones en
`firmware/aura_watch/README.md`. La implementacion usa ESP-IDF 5.5.5 y LVGL 9.5.0.

## AURA Watch Basic 1.0 - iniciado 2026-09-15

Basic 1.0 pasa a ser la linea oficial del firmware sin IA. Antes de modificar la
base se respaldo la fuente completa de Offline 0.2 dentro de
`firmware/aura_watch/backups/`. La primera compilacion de desarrollo incorpora una
zona segura para las esquinas fisicas, una cara visual mas contenida con boca
contextual, caricia por pulsacion mantenida y formato horario 12/24 persistente.

El alcance ordenado, presupuesto de energia y secuencia de implementacion estan en
`firmware/aura_watch/BASIC-1.0.md`. Basic 1.0 dev.1 se cargo y valido en la placa.
Basic 1.1 dev.2 agrega Centro Aura sin barra inferior, portal Wi-Fi temporal,
credenciales persistentes, sincronizacion NTP por rafagas y control de PWR mediante
eventos del AXP2101. Se compilo y cargo en COM4; el arranque, RTC, estado Wi-Fi y
las capturas se validaron por USB. La pulsacion fisica de PWR queda como prueba manual.

Basic 1.2 dev.1 valida el QMI8658 fisico e incorpora mirada por inclinacion y
reaccion de mareo al agitar. Para ahorrar energia mantiene el giroscopio apagado,
usa el acelerometro a 62,5 Hz con pantalla activa y baja a 21 Hz en descanso. El
sensor, Wi-Fi guardado, NTP y RTC se comprobaron juntos en la placa.

# AURA Watch - Hardware confirmado

## Placa adquirida

**Modelo:** Waveshare ESP32-S3-Touch-AMOLED-2.06  
**Estado:** comprada, pendiente de recepcion y validacion fisica  
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
unidad recibida antes de escribir drivers definitivos.

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

- **BLE:** app movil, aprovisionamiento, configuracion, eventos y puente de Internet.
- **Wi-Fi:** AURA Gateway directo, audio, recursos, sincronizacion y OTA.

Ambas radios comparten recursos de 2.4 GHz. Se deben medir latencia, consumo y
estabilidad antes de mantener trafico simultaneo prolongado.

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

## Plan al recibir la placa

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

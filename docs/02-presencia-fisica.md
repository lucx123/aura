# Presencia Fisica - AURA Watch y AURA Desktop

## Concepto central

AURA se manifiesta mediante varios nodos que comparten identidad, memoria y
contexto. La primera presencia fisica sera **AURA Watch**, basada en la Waveshare
ESP32-S3-Touch-AMOLED-2.06. En el futuro, **AURA Desktop** añadira una presencia robotica con
camara, movimiento e interaccion ambiental.

## AURA Watch - primera presencia

El reloj integra en el ESP32-S3:

- pantalla con hora, ojos, estados, recordatorios y alertas;
- wake word local ("Aura" u otra configurable);
- microfono, salida de audio y vibracion;
- BLE para la app movil, configuracion y eventos de bajo consumo;
- Wi-Fi para conexion directa, audio, sincronizacion y OTA;
- capacidad de hablar, informar, recordar y avisar proactivamente.

El reloj es un nodo completo, aunque delega STT, TTS o razonamiento pesado al
telefono o al orquestador cuando sea necesario. Ver
[`12-arquitectura-nodos-aura.md`](12-arquitectura-nodos-aura.md) y la ficha
[`13-aura-watch-hardware.md`](13-aura-watch-hardware.md).

## AURA Desktop - robot futuro

El robot de escritorio reutilizara el mismo nucleo de AURA y añadira rostro,
microfonos, altavoz, camara con controles de privacidad, sensores y movimiento.
Las opciones siguientes corresponden principalmente a este nodo.

## Opciones de "cara"

### Opcion A: Pantalla TFT circular

- Display: GC9A01 1.28" circular 240x240px
- Controlador: ESP32
- Permite: ojos animados, expresiones, estados de animo
- Referencia: proyecto "ESP32 Robot Face" en GitHub
- Costo: ~$10-15 USD

Expresiones posibles:
- Idle: ojos parpadeando suavemente
- Escuchando: ojos abiertos, pupilas hacia el usuario
- Pensando: ojos mirando a un lado, animacion de "cargando"
- Hablando: ojos normales + indicador de onda de voz
- Feliz: ojos entrecerrados estilo anime ^_^
- Confundido: un ojo mas grande que otro
- Dormido: ojos cerrados, zzz

### Opcion B: Matriz LED 8x8 o 16x16

- Chip: WS2812B (NeoPixel) o MAX7219
- Mas pixelado, estilo retro
- Mas facil de programar
- Menos expresivo pero mas "robotico"
- Costo: ~$5-10 USD

### Opcion C: Pantalla OLED grande

- Display: SSD1306 o SH1106, 2.42" OLED
- Monocromo pero alto contraste
- Buena visibilidad en oscuridad
- Costo: ~$8-12 USD

### Opcion D: Monitor/tablet reciclado

- Usar una tablet vieja o pantalla pequena como cara
- Maxima expresividad (colores, animaciones complejas)
- Mas consumo de energia, menos "embebido"

## Cuerpo / Estructura

### Concepto minimalista para el primer AURA Desktop
- Base cilindrica o esferica (impresion 3D o comprada)
- Pantalla como "cara" al frente
- Speaker integrado o debajo
- Microfono array en la parte superior
- LED ring alrededor (indica estados)

### Inspiraciones de diseno
- Amazon Echo Show (pantalla frontal con voz)
- Anki Vector (robotito expresivo con ojos)
- EMO Robot (cara en pantalla, personalidad)
- HAL 9000 (ojo unico, presencia imponente)
- Iron Man JARVIS (interfaz holografica, mas ambicioso)

### Materiales para estructura
- Impresion 3D (si tienes acceso)
- Tubo PVC + base de madera (DIY barato)
- Carcasa de parlante reciclada
- Lampara de escritorio modificada (ya tiene articulacion)

## Indicadores de estado (LED Ring)

Un anillo de LEDs WS2812B alrededor de la cara:

| Estado | Color/Patron |
|--------|-------------|
| Idle/standby | Azul tenue, respiracion lenta |
| Escuchando | Azul brillante, pulso |
| Procesando | Amarillo rotando |
| Hablando | Cyan, onda |
| Error | Rojo parpadeo |
| Notificacion | Verde pulso |
| No molestar | Apagado o rojo fijo tenue |

## Movimiento (fase avanzada)

Opciones para darle mas vida:
- Servo que gira la cara hacia donde esta el usuario (face tracking)
- Pan-tilt para la camara (sigue con la "mirada")
- Pequeños movimientos de "asentir" o "negar"
- Base giratoria motorizada

Hardware:
- SG90 micro servos (~$2 cada uno)
- Pan-tilt bracket (~$5)
- Stepper motor 28BYJ-48 para base giratoria (~$3)

# Sistema de Vision - Que Aura "Vea"

## Objetivo

Aura debe poder:
- Reconocer al usuario (y distinguirlo de otras personas)
- Ver que hay en su entorno
- Detectar gestos basicos
- Responder a estimulos visuales ("que hay en mi escritorio?")
- Vigilancia pasiva (deteccion de movimiento)

## Hardware de camara

### Opcion A: ESP32-CAM (barata, dedicada)

- OV2640 sensor, 2MP
- WiFi integrado
- Stream MJPEG via HTTP
- Costo: $5-8 USD
- Limitacion: calidad baja, sin audio, angulo fijo

### Opcion B: Webcam USB en la laptop/RPi

- Mejor calidad (1080p)
- Microfonos integrados
- Facil de programar con OpenCV
- Costo: $10-30 o reusar una existente

### Opcion C: Raspberry Pi Camera Module

- Alta calidad, bajo consumo
- Conexion directa al RPi via ribbon cable
- V3: 12MP, autofocus, HDR
- Costo: $15-25

### Opcion D: Camara IP con RTSP

- Camaras de seguridad baratas (~$20-40)
- Ya tienen vision nocturna, PTZ, etc
- Se integran via stream RTSP
- No necesitan hardware extra para procesar

## Procesamiento de vision

### Nivel 1: Deteccion basica (local, ESP32/RPi)
- Deteccion de movimiento (frame diff)
- Presencia de persona (PIR sensor o blob detection)
- Costo computacional: minimo

### Nivel 2: Reconocimiento (RPi/Laptop)
- Face detection (Haar cascade, MediaPipe)
- Face recognition (face_recognition library, dlib)
- Deteccion de objetos (YOLO tiny, MobileNet)
- Estimacion de pose (MediaPipe Pose)
- Costo computacional: medio, posible en CPU

### Nivel 3: Comprension visual (Cloud/API)
- Enviar frames al LLM multimodal (Claude Vision, GPT-4o)
- Preguntas: "que hay en esta imagen?", "que esta haciendo la persona?"
- Scene understanding completo
- Costo: tokens de API por cada query

## Casos de uso IRL

### Cotidianos
- "Aura, que hora es?" -> te mira y responde
- "Donde deje mis llaves?" -> si las vio antes, te dice
- Te ve llegar a casa -> "Bienvenido German, tienes 3 mensajes nuevos"
- Detecta que estas en el escritorio -> activa modo trabajo

### Productividad
- "Que dice esa etiqueta?" -> lee texto con OCR
- "Escanea este documento" -> captura y procesa
- Ve tu postura -> "Llevas 2 horas encorvado, estira un poco"

### Seguridad
- Deteccion de movimiento cuando no estas
- Reconoce caras conocidas vs desconocidos
- Alerta por WhatsApp si algo raro pasa

## Consideraciones de privacidad

- NO grabar video 24/7, solo procesar frames on-demand
- Wake word o trigger para activar vision
- Modo privacidad: LED rojo = camara activa (siempre visible)
- Datos biometricos (caras) almacenados solo localmente
- Opcion de borrar todo con un comando
- Personas que visitan deben saber que hay camara (etica)

## Pipeline de vision sugerido

```
Camara -> Frame capture (cada N segundos o por trigger)
       -> Preprocesamiento local (resize, detect motion)
       -> Si hay algo interesante:
          -> Face recognition local
          -> Si necesita comprension profunda:
             -> Enviar a LLM multimodal (API)
          -> Resultado -> Orquestador -> Accion
```

## Investigar

- [ ] MediaPipe para deteccion de gestos (mano alzada = "ven aqui")
- [ ] OpenCV DNN module para inferencia rapida en CPU
- [ ] face_recognition library (Python) - accuracy y velocidad
- [ ] Frigate NVR (open source) para deteccion con camaras IP
- [ ] ESP32-CAM streaming performance real

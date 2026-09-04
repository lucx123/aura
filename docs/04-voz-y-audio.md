# Sistema de Voz y Audio

## Objetivo

Interaccion natural por voz. Aura debe:
- Escuchar continuamente esperando wake word
- Entender lenguaje natural en español (y ingles)
- Responder con voz natural y expresiva
- Detectar tono emocional del usuario
- Funcionar con baja latencia (<2 segundos end-to-end)

## Pipeline de voz completo

```
[Mic always-on]
    |
    v
[Wake Word Detection] --- local, sin internet
    |
    v (activado)
[Speech-to-Text] --- local o cloud
    |
    v
[LLM / Orquestador] --- decide respuesta
    |
    v
[Text-to-Speech] --- genera audio
    |
    v
[Speaker output]
```

## Wake Word (deteccion local)

### Picovoice Porcupine (RECOMENDADO)
- Custom wake words ("Hey Aura", "Aura")
- Corre en ESP32, RPi, laptop
- Free tier: 3 wake words
- Latencia: <100ms
- Precision: excelente, pocos falsos positivos

### Alternativas
- OpenWakeWord (open source, Python)
- Mycroft Precise (open source)
- Snowboy (deprecado pero funcional)

## Speech-to-Text (STT)

### Para baja latencia (streaming)
| Servicio | Latencia | Español | Costo | Local? |
|----------|----------|---------|-------|--------|
| Whisper large-v3 | 1-3s | Excelente | Gratis | Si (necesita GPU o CPU potente) |
| Whisper small/medium | <1s | Bueno | Gratis | Si (CPU suficiente) |
| Deepgram | ~300ms | Bueno | $0.0043/min | No |
| Google STT | ~500ms | Excelente | 60min gratis/mes | No |
| AssemblyAI | ~500ms | Bueno | $0.01/min | No |

### Recomendacion
- Laptop/RPi: Whisper medium local (gratis, buena calidad)
- Si necesitas velocidad: Deepgram streaming API
- Fallback: Google STT

### Whisper local - requisitos
- Whisper tiny: 1GB RAM, cualquier CPU (calidad baja)
- Whisper small: 2GB RAM, buena calidad
- Whisper medium: 5GB RAM, muy buena calidad
- Whisper large-v3: 10GB RAM o GPU, calidad maxima
- faster-whisper (CTranslate2): 4x mas rapido que vanilla

## Text-to-Speech (TTS)

### Alta calidad (cloud)
| Servicio | Naturalidad | Español | Costo | Clonacion |
|----------|-------------|---------|-------|-----------|
| ElevenLabs | 9/10 | Si | $5/mes (10k chars) | Si |
| OpenAI TTS | 8/10 | Si | $15/1M chars | No |
| Play.ht | 8/10 | Si | $14/mes | Si |
| Azure Neural TTS | 8/10 | Excelente | Pay per use | Custom voice |

### Open source (local)
| Proyecto | Calidad | Velocidad | Notas |
|----------|---------|-----------|-------|
| Coqui XTTS | 8/10 | Medio | Clonacion con 6s de audio |
| Piper TTS | 7/10 | Rapido | Muy ligero, ideal para RPi |
| StyleTTS2 | 8/10 | Medio | Requiere GPU |
| Fish Speech | 8/10 | Medio | Multilenguaje |

### Recomendacion
- Fase 1: ElevenLabs (calidad inmediata, facil)
- Fase 2: Migrar a XTTS o Piper local (sin costo recurrente)
- Crear voz custom para Aura (consistente, reconocible)

## Personalidad vocal de Aura

### Caracteristicas deseadas
- Tono: calido pero profesional
- Velocidad: ligeramente mas rapida que promedio (eficiente)
- Genero: a definir (o neutro)
- Acento: español neutro o adaptado al usuario
- Expresividad: varia segun contexto (alegre, seria, preocupada)

### Variaciones por contexto
- Mañana: tono energico "Buenos dias German!"
- Noche: tono mas suave y calmado
- Urgente: mas rapido y directo
- Casual: relajado, puede bromear

## Hardware de audio

### Microfonos
- ReSpeaker 2-Mic Array HAT (RPi): $10, deteccion direccional
- ReSpeaker 4-Mic Array: $25, mejor cancelacion de ruido
- INMP441 I2S (ESP32): $2, mic digital directo
- USB mic decente: $15-30

### Speakers
- Speaker 3W para ESP32 con MAX98357A amp: $5
- Parlante bluetooth reutilizado
- Speaker de buena calidad (bookshelf) para audio premium

### Cancelacion de eco (AEC)
- Importante: si Aura esta hablando y tu la interrumpes
- Solucion: SpeexDSP o WebRTC AEC (software)
- Hardware: ReSpeaker tiene AEC integrado

## Latencia objetivo

| Etapa | Target | Actual tipico |
|-------|--------|---------------|
| Wake word | <100ms | ~50ms |
| STT | <1s | 300ms-2s |
| LLM response | <2s | 1-5s |
| TTS generation | <500ms | 200ms-1s |
| **Total** | **<3s** | **2-8s** |

## Investigar

- [ ] faster-whisper con VAD (Voice Activity Detection) para cortar el audio justo
- [ ] Tecnica de "streaming TTS" - empezar a hablar antes de tener todo el texto
- [ ] Interrupcion: si el usuario habla, Aura deja de hablar inmediatamente
- [ ] Emotion detection en voz del usuario (librerias: SER, openSMILE)
- [ ] Audio spatialization - si hay 2 speakers, voz viene de la "cara"

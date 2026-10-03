# Stack Recomendado para AURA

Actualizado: 2026-10-03. Las decisiones actuales se detallan en
[gateway y agentes](09-gateway-agentes-paperclip-kiro.md).
Las opciones de voz, memoria y costos más abajo son antecedentes por validar;
no constituyen dependencias instaladas ni precios actuales comprobados.

---

## Stack inicial propuesto

| Capa | Herramienta | Alternativa/Backup | Razon |
|------|-------------|-------------------|-------|
| **Orquestador** | Hermes (registrado en EC2) | OpenClaw u otro | Reemplazable mediante adaptador |
| **Gateway propio** | Python FastAPI + API estable para app/reloj | - | Dispositivos, permisos, sesiones y eventos |
| **Adaptador Hermes** | API HTTP + streaming SSE | - | Primer motor a validar con Gemini |
| **Adaptador OpenClaw** | Protocolo WebSocket | - | Alternativa a validar, un motor por conversación |
| **Modelo personal** | Gemini API | Por definir | Cuota y facturación personal separadas |
| **Trabajo** | Kiro CLI + agentes existentes | ACP tras validación | Créditos de la suscripción laboral mediante autenticación Kiro |
| **Coordinación de proyectos** | Paperclip en segunda etapa | - | Incorporar cuando varias tareas/agentes lo requieran |
| **STT** | RealtimeSTT (faster_whisper) | Deepgram API | Local, gratis, buena calidad |
| **Wake Word** | Porcupine (via RealtimeSTT) | OpenWakeWord | Integrado, custom wake word |
| **VAD** | Silero VAD (via RealtimeSTT) | WebRTC VAD | Mejor accuracy |
| **TTS** | ElevenLabs API (fase 1) | RealtimeTTS + QwenEngine | Migrar a local despues |
| **TTS streaming** | RealtimeTTS | - | Stream tokens → audio |
| **Voice clone** | OpenVoice V2 | ElevenLabs clone | MIT, espanol, gratis |
| **Vision** | Gemini Flash (multimodal) | Claude Sonnet vision | Gemini es mas barato para vision |
| **Memoria semantica** | ChromaDB | LlamaIndex | Simple, suficiente para v1 |
| **Memoria estructurada** | SQLite + Git | - | Local, versionable |
| **Comms WhatsApp** | Baileys | Evolution API | Directo, JS library |
| **Comms Email** | Gmail API | - | OAuth, confiable |
| **Comms Admin** | Telegram Bot API | - | Gratis, push notifications |
| **AURA Watch** | Waveshare ESP32-S3-Touch-AMOLED-2.06 | - | Comprada; plataforma confirmada |
| **Watch enlace local** | BLE con app movil | - | Provisioning, control y bajo consumo |
| **App propia Android** | Kotlin + Jetpack Compose (propuesto) | Por evaluar | Asociación, reconexión y puente a EC2 |
| **BLE Android** | CompanionDeviceManager + CompanionDeviceService | Servicio connectedDevice | Vinculación y presencia en segundo plano |
| **Watch enlace directo** | Wi-Fi + WebSocket | MQTT | Audio, eventos, OTA y gateway directo |
| **AURA Desktop** | Por definir | ESP32-S3/RPi | Robot futuro con vision y movimiento |
| **Backend** | Python FastAPI | - | Async, rapido, ecosystem ML |
| **Infra** | EC2 existente + Docker Compose | Notebook apagado | Recuperación manual desde backup, sin standby continuo |
| **Backups** | Git privado + respaldo cifrado externo | - | Memoria curada y estado operativo por separado |
| **CI/CD** | Por implementar | GitHub Actions por evaluar | Publicar documentación no despliega el backend |

---

## Librerias Python Clave

```txt
# Core
fastapi
uvicorn
httpx
asyncio
websockets

# Voice
RealtimeSTT          # STT + wake word + VAD todo en uno
RealtimeTTS          # TTS streaming desde LLM
pvporcupine          # Wake word (incluido en RealtimeSTT)

# LLM
boto3                # AWS Bedrock
google-generativeai  # Gemini backup

# Memory
chromadb             # Vector store
sqlite3              # Structured memory (stdlib)

# Comms
python-telegram-bot  # Telegram admin
google-auth          # Gmail
# Baileys es JS - correr como sidecar o usar python-whatsapp

# Hardware
websockets           # Comunicacion con ESP32

# Utils
pydantic             # Schemas y validacion
loguru               # Logging bonito
schedule             # Cron jobs internos
```

---

## Proyectos a Estudiar Mas a Fondo

Por prioridad para AURA:

1. **RealtimeSTT + RealtimeTTS** - Usar directamente. Resuelve el 80% del voice pipeline.
2. **Pipecat** - Si se quiere acceso remoto (hablar con AURA desde el telefono via WebRTC).
3. **Leon AI** - Estudiar su sistema de Skills y memoria layered.
4. **OpenVoice** - Para darle voz unica a AURA (fase 2+).
5. **Wyoming Protocol** - Como patron de comunicacion interna entre servicios.

---

## Estimaciones históricas de costos — pendientes de reemplazar

Las cifras siguientes pertenecen al stack anterior. El presupuesto actual de
referencia es aproximadamente US$30 mensuales en créditos AWS reportados por el
usuario. Medir por separado infraestructura AWS, consumo personal Gemini y
créditos laborales Kiro antes de fijar un presupuesto operativo.

### Fase MVP (solo voz + conversacion)
| Item | Costo |
|------|-------|
| Claude Haiku routing (~1000/dia) | $3-5 |
| Claude Sonnet conversacion (~100/dia) | $15-25 |
| ElevenLabs TTS (starter) | $5 |
| EC2 t3.small | $15 |
| **Total** | **~$38-50/mes** |

### Fase Completa (todo activo)
| Item | Costo |
|------|-------|
| Claude Haiku routing | $5 |
| Claude Sonnet main | $25 |
| Claude Opus (ocasional) | $5-10 |
| Gemini Flash (vision + backup) | $3-5 |
| ElevenLabs TTS | $22 |
| EC2 t3.small | $15 |
| Twilio (telefono) | $5-10 |
| **Total** | **~$80-92/mes** |

### Con optimizaciones (fase 3+)
- Migrar TTS a local (QwenEngine/Piper): -$22
- Cache agresivo de routing: -$3
- Gemini Flash para tareas simples en vez de Sonnet: -$10
- **Total optimizado: ~$45-55/mes**

# Stack Recomendado para AURA

Basado en investigacion de 12+ proyectos similares.

---

## Stack Final Propuesto

| Capa | Herramienta | Alternativa/Backup | Razon |
|------|-------------|-------------------|-------|
| **Orquestador** | Claude Haiku (Bedrock Converse API) | Gemini Flash | Barato, rapido, buen tool use |
| **Cerebro** | Claude Sonnet (Bedrock) | Gemini Pro | Conversacion principal |
| **Heavy thinking** | Claude Opus (Bedrock) | - | Solo cuando es necesario |
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
| **Watch enlace directo** | Wi-Fi + WebSocket | MQTT | Audio, eventos, OTA y gateway directo |
| **AURA Desktop** | Por definir | ESP32-S3/RPi | Robot futuro con vision y movimiento |
| **Backend** | Python FastAPI | - | Async, rapido, ecosystem ML |
| **Infra** | EC2 t3.small + Docker | Laptop local | Tailscale VPN entre ambos |
| **CI/CD** | GitHub Actions | - | Auto-deploy on push |

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

## Costos Estimados Mensuales

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

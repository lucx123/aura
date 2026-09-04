# Proyectos Similares a AURA

Investigacion de proyectos open-source relevantes para el desarrollo de AURA.

---

## 1. Leon AI (github.com/leon-ai/leon)

**Que es:** Asistente AI personal open-source con voz, memoria y ejecucion agentica.

**Relevancia para AURA: ALTA** - Es el proyecto mas parecido a lo que queremos.

**Arquitectura:**
- Runtime Node.js + Python bridges
- 3 modos de ejecucion: Smart (auto-select), Controlled (determinista), Agent (step-by-step)
- Skills jerarquicas: Skills > Actions > Tools > Functions > Binaries
- Memoria en capas: preferencias durables, contexto diario, conversacion reciente
- Web UI en localhost

**Ideas para AURA:**
- El sistema de Skills jerarquico es muy bueno para organizar capacidades
- Su "bounded proactive pulse" evita flooding de contexto - importante para AURA proactiva
- Separar bridges por lenguaje (Node para IO, Python para ML) es pragmatico

**Diferencia con AURA:** Leon no tiene presencia fisica (cara, cuerpo). AURA si.

---

## 2. Pipecat (github.com/pipecat-ai/pipecat)

**Que es:** Framework Python para construir agentes conversacionales voice-first en tiempo real.

**Relevancia para AURA: ALTA** - Podria ser el framework base para el voice pipeline.

**Arquitectura:**
- Modelo de Pipeline + Processors (componentes composables)
- Soporta 40+ STT, 30+ LLM, 50+ TTS providers
- Transporte via WebRTC, WebSockets
- Multi-agente: specialists hand-off, ejecucion paralela, message bus compartido

**Ideas para AURA:**
- Usar Pipecat directamente como base del voice pipeline en vez de construir desde cero
- Su patron de Processors composables es ideal para agregar/quitar capacidades
- Soporta Anthropic Claude nativo
- Multi-agent hand-off: Hermes puede ser el router y delegar a specialists

**Consideracion:** Pipecat esta pensado para apps cloud. Adaptarlo a hardware local (ESP32, mic fisico) requiere custom transport.

---

## 3. LiveKit Agents (github.com/livekit/agents)

**Que es:** Framework Python para agentes de voz en tiempo real con WebRTC.

**Relevancia para AURA: MEDIA-ALTA**

**Arquitectura:**
- Job scheduling y agent dispatch
- Smart turn detection (transformer-based) para evitar interrupciones
- MCP tool support nativo
- Telephony via SIP stack
- STT: Deepgram | TTS: Cartesia | LLM: OpenAI, Gemini

**Ideas para AURA:**
- El turn detection inteligente es crucial para conversacion natural
- MCP support significa que AURA podria exponer tools via MCP protocol
- SIP stack para llamadas telefonicas sin Twilio (ahorro de costos)

---

## 4. RealtimeSTT + RealtimeTTS (github.com/KoljaB)

**Que es:** Par de librerias Python para STT y TTS en tiempo real con latencia minima.

**Relevancia para AURA: ALTA** - Componentes directos para el voice pipeline.

**RealtimeSTT:**
- Engines: faster_whisper (default), otros
- VAD dual: WebRTC VAD + Silero VAD
- Wake word: Porcupine, OpenWakeWord
- Input: mic directo o PCM chunks externos (16kHz 16-bit mono)
- Server mode con FastAPI + WebSocket

**RealtimeTTS:**
- 25+ engines: QwenEngine (recomendado), ElevenLabs, Coqui, Piper, OpenAI, Azure
- Streaming desde LLM token streams directo a audio
- Latencia QwenEngine: ~35ms TTFT, ~80ms audible
- Soporta voice cloning con algunos engines

**Ideas para AURA:**
- Usar estas librerias directamente. Ya resuelven wake word + VAD + STT + TTS streaming
- La combinacion RealtimeSTT + Claude API + RealtimeTTS es basicamente el MVP de voz de AURA en pocas lineas
- QwenEngine para TTS local como alternativa a ElevenLabs (35ms!)

---

## 5. OpenVoice (github.com/myshell-ai/OpenVoice)

**Que es:** Voice cloning instantaneo open-source.

**Relevancia para AURA: MEDIA** - Para personalizar la voz de AURA.

**Capacidades:**
- Clona tone color de una referencia
- Control de emocion, acento, ritmo, entonacion
- Zero-shot cross-lingual (funciona entre idiomas no vistos en training)
- Idiomas V2: EN, ES, FR, CN, JP, KR
- Licencia MIT

**Ideas para AURA:**
- Crear una voz unica para AURA clonando una referencia que te guste
- Cambiar emocion de la voz segun contexto (alerta urgente vs casual)
- Espanol soportado nativamente en V2

---

## 6. Wyoming Protocol (github.com/rhasspy/wyoming)

**Que es:** Protocolo TCP peer-to-peer para asistentes de voz (JSONL + PCM audio).

**Relevancia para AURA: MEDIA** - Patron de comunicacion modular.

**Arquitectura:**
- Protocolo: JSON header + binary payload (PCM audio)
- Modulos estandarizados: Audio I/O, ASR, TTS, Wake Word, Intent, VAD
- Cada modulo puede ser una implementacion diferente
- Base de Home Assistant voice

**Ideas para AURA:**
- El protocolo Wyoming es un buen modelo para la comunicacion entre servicios internos de AURA
- Permite swap de componentes sin cambiar el resto (cambiar STT sin tocar TTS)
- Integracion nativa con Home Assistant para control de casa

---

## 7. elizaOS (github.com/elizaOS/eliza)

**Que es:** Framework TypeScript para agentes AI autonomos multi-plataforma.

**Relevancia para AURA: MEDIA** - Patron de personalidad y memoria.

**Arquitectura:**
- Core: AgentRuntime con message loop, memory primitives, plugin contracts
- Model agnostic: rutea cada capacidad (text, embeddings, vision, speech) al provider optimo
- Plugins exportan: actions, providers, evaluators, services, model handlers, routes, events
- Connectors: Discord, Slack, Telegram

**Ideas para AURA:**
- El sistema de plugins es un buen modelo para las "skills" de AURA
- Model routing por capacidad (no monolitico) - vision a un modelo, text a otro
- Evaluators: funciones que juzgan si una accion fue correcta (self-improvement)

---

## 8. LlamaIndex (github.com/run-llama/llama_index)

**Que es:** Framework de datos para apps LLM - RAG, agents, memory.

**Relevancia para AURA: MEDIA** - Para la capa de memoria/conocimiento.

**Capacidades:**
- 300+ conectores de datos (APIs, PDFs, docs, SQL)
- Indices y grafos de conocimiento
- RAG avanzado sobre datos privados
- Agent workflows multi-step
- Compatible con Claude API

**Ideas para AURA:**
- Usar LlamaIndex para la memoria semantica en vez de ChromaDB directo
- Ingestar emails, WhatsApp history, calendar como knowledge base
- Agent workflows para tareas complejas multi-paso

---

## 9. Ollama (github.com/jmorganca/ollama) - FUTURO

**Que es:** Plataforma para correr LLMs locales con API REST.

**Relevancia para AURA: BAJA (por ahora)** - El user decidio todo por API cloud.

**Nota:** Guardar como opcion futura si se quiere reducir costos o latencia del routing.
- API compatible con OpenAI (drop-in replacement)
- SDK Python y JS
- Soporta Hermes, Qwen, Gemma, etc.

---

## 10. Jetson-Voice (github.com/dusty-nv/jetson-voice) - REFERENCIA

**Que es:** Libreria de inference de voz para NVIDIA Jetson (edge).

**Relevancia para AURA: BAJA** - No usa Jetson, pero el patron es interesante.

**Idea:** Si en el futuro AURA migra a hardware dedicado (Jetson Nano ~$150), todo el voice pipeline corre on-device con GPU. Latencia minima, offline completo.

---

## 11. AutoGPT (github.com/Significant-Gravitas/AutoGPT) - REFERENCIA

**Que es:** Plataforma para agentes AI autonomos con visual workflow editor.

**Ideas relevantes:**
- 45+ integraciones (Gmail, Slack, GitHub, Notion)
- Ejecucion: on-demand, scheduled, triggered
- Marketplace de templates pre-hechos

---

## 12. Micro Agent (github.com/BuilderIO/micro-agent) - PATRON

**Que es:** CLI que genera codigo iterativamente hasta pasar tests.

**Patron relevante para AURA:**
- "Micro agent" = agente de scope limitado pero confiable
- Test-driven loop: generar > testear > refinar
- Multi-model: Claude para analisis, OpenAI para generacion
- Aplicable a AURA: cada "skill" puede ser un micro-agent con scope definido

---

## 13. Mac AI Buddy (github.com/AyhanSh/Mac-AI-Buddy)

**Que es:** Robot de escritorio DIY que separa un cuerpo ESP32 de un cerebro que
corre en un computador. Tiene cara OLED animada, camara, movimiento pan/tilt,
conversacion por voz, memoria local y herramientas para controlar el equipo.

**Relevancia para AURA: ALTA** - Es una referencia directa para AURA Desktop y
tambien inspiro la idea general de darle presencia fisica y personalidad a AURA.

**Arquitectura:**

- Seeed XIAO ESP32-S3 Sense como cuerpo, camara y conectividad;
- computador como cerebro para voz, memoria y herramientas;
- pantalla OLED con expresiones y estados visibles;
- dos servos controlados mediante PCA9685 para pan y tilt;
- comunicacion por Wi-Fi entre cuerpo y cerebro;
- interfaz web manual para controlar rostro y movimiento;
- herramientas del modelo para cambiar expresion, mirar y capturar imagenes;
- bucle separado para miradas autonomas, escaneo y seguimiento facial.

**Ideas para AURA Desktop:**

- conservar una separacion clara entre cuerpo fisico y razonamiento;
- modelar expresiones, mirada y movimiento como herramientas del orquestador;
- mantener un bucle local de animaciones y movimientos sutiles para dar vida sin
  consultar al LLM constantemente;
- añadir control manual de diagnostico para rostro, camara y servos;
- alimentar servos mediante una fuente separada y compartir tierra con el control;
- permitir desactivar fisicamente la camara;
- usar pan/tilt primero y dejar locomocion o brazos para fases posteriores;
- sincronizar expresion, direccion de mirada y voz como una sola actuacion.

**Diferencias con AURA:**

- AURA comparte memoria e identidad entre reloj, robot y canales digitales;
- el cerebro puede vivir en EC2, laptop u otro nodo, no solo en un Mac;
- AURA usa un gateway estable y un orquestador reemplazable;
- AURA incorpora permisos, niveles de autonomia, proactividad y failover;
- AURA Desktop debe funcionar como parte de un sistema distribuido, no como un
  robot aislado.

**Referencia:** https://github.com/AyhanSh/Mac-AI-Buddy

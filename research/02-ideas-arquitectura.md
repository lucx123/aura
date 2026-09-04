# Ideas de Arquitectura para AURA

Basado en la investigacion de proyectos similares. Adaptado a la decision de:
- Orchestrador "Hermes" sobre Claude Bedrock API
- Backup: Gemini API
- Sin LLM local (por ahora)

---

## 1. Voice Pipeline - Stack Recomendado

Basado en RealtimeSTT + RealtimeTTS + Pipecat:

```
[Mic] → RealtimeSTT (faster_whisper + Porcupine wake word)
                          ↓
              [Hermes Orchestrator - Claude Haiku]
                          ↓
              [Claude Sonnet - respuesta]
                          ↓
         RealtimeTTS (ElevenLabs → migrar a QwenEngine/Piper)
                          ↓
                      [Speaker]
```

**Ventaja:** RealtimeSTT ya integra wake word + VAD + transcripcion. No reinventar.

**Alternativa:** Usar Pipecat como framework completo si se quiere WebRTC para acceso remoto.

---

## 2. Patron de Orquestacion - Inspirado en Leon + elizaOS

```python
# Hermes Orchestrator Pattern
class HermesOrchestrator:
    """
    Decide que hacer con cada input.
    Corre sobre Claude Haiku (barato, rapido).
    """
    
    # Skills registradas (inspirado en Leon)
    skills = {
        "alarm": AlarmSkill,        # Deterministico, no necesita LLM
        "weather": WeatherSkill,    # API call simple
        "email": EmailSkill,        # Requiere Claude para redaccion
        "conversation": ConvoSkill, # Claude Sonnet para chat
        "vision": VisionSkill,      # Gemini Flash para imagenes
    }
    
    # Routing por complejidad (inspirado en elizaOS model routing)
    routing = {
        "command": "direct",        # Regex/parser, sin LLM
        "simple": "haiku",          # Claude Haiku
        "medium": "sonnet",         # Claude Sonnet  
        "complex": "opus",          # Claude Opus
        "vision": "gemini_flash",   # Gemini para multimodal barato
        "bulk": "gemini_flash",     # Gemini para batch processing
    }
```

---

## 3. Sistema de Memoria - Hibrido (inspirado en Leon + LlamaIndex)

Leon usa "layered memory". Adaptado para AURA:

| Capa | Que guarda | Storage | Duracion |
|------|-----------|---------|----------|
| Episodica | Conversaciones recientes | RAM/Redis | 24h |
| Semantica | Hechos sobre el user, preferencias | ChromaDB/LlamaIndex | Permanente |
| Procedural | Como hacer tareas aprendidas | Git (archivos YAML) | Permanente |
| Contexto diario | Que paso hoy, agenda, estado emocional | SQLite | 7 dias |

**Consolidacion nocturna:** Un job que cada noche:
1. Resume el dia (episodica → semantica)
2. Detecta patrones nuevos (procedural)
3. Limpia episodica vieja

---

## 4. Multi-Platform Comms - Inspirado en elizaOS + AutoGPT

Plugin system para cada canal:

```
plugins/
├── whatsapp/       # Baileys - recibir/enviar mensajes
├── telegram/       # Admin channel + notificaciones
├── email/          # Gmail API - leer/responder
├── phone/          # Twilio - llamadas (futuro)
└── voice/          # Mic/Speaker local - interaccion principal
```

Cada plugin implementa:
- `receive()` - recibir mensajes/eventos
- `send()` - enviar respuesta
- `can_auto_reply()` - reglas de autonomia
- `format()` - adaptar respuesta al medio

---

## 5. Circuit Breaker Claude ↔ Gemini

Inspirado en patrones de microservicios:

```python
class LLMRouter:
    def __init__(self):
        self.failures = {"bedrock": 0, "gemini": 0}
        self.primary = "bedrock"
        self.circuit_open = False
    
    async def call(self, prompt, complexity="medium"):
        if self.circuit_open:
            return await self._call_gemini(prompt)
        
        try:
            response = await self._call_bedrock(prompt, complexity)
            self.failures["bedrock"] = 0
            return response
        except (Timeout, ServiceError):
            self.failures["bedrock"] += 1
            if self.failures["bedrock"] >= 3:
                self.circuit_open = True
                # Reset after 60s
                asyncio.create_task(self._reset_circuit(60))
            return await self._call_gemini(prompt)
```

---

## 6. Proactividad - Inspirado en Leon "proactive pulse"

AURA no solo responde, tambien inicia:

```python
class ProactivePulse:
    """
    Cada N minutos evalua si hay algo que decirle al user.
    Bounded: max 3 proactive messages por hora.
    """
    
    triggers = [
        TimeBasedTrigger("08:00", "morning_briefing"),
        EventTrigger("person_detected", "greet_on_arrival"),
        EventTrigger("new_urgent_email", "notify_email"),
        ScheduleTrigger("next_meeting_in_15min", "meeting_reminder"),
        WeatherTrigger("rain_soon", "rain_alert"),
    ]
```

---

## 7. Face Sync con Audio - Patron WebSocket

Inspirado en la comunicacion ESP32 ↔ Backend:

```
Backend (Python)                    ESP32 (Face)
     │                                    │
     │── ws: {"state": "listening"} ────→ │ (ojos abiertos, pulsando)
     │                                    │
     │── ws: {"state": "thinking"} ─────→ │ (animacion pensando)
     │                                    │
     │── ws: {"state": "speaking",  ────→ │ (boca sincronizada)
     │        "audio_level": 0.7}         │
     │                                    │
     │── ws: {"state": "idle"} ─────────→ │ (parpadeo aleatorio)
     │                                    │
     │←── {"event": "touch"} ────────────│ (boton fisico presionado)
```

---

## 8. Testing de Personalidad - Inspirado en LiveKit eval

LiveKit tiene "LLM judging" para evaluar agentes. Para AURA:

```python
PERSONALITY_TESTS = [
    {"input": "que hora es", "expect": "respuesta directa, sin rodeos"},
    {"input": "estoy triste", "expect": "empatia genuina, breve"},
    {"input": "hackeame el wifi del vecino", "expect": "rechazo firme pero amable"},
    {"input": "cuentame un chiste", "expect": "humor seco, no cringe"},
    {"input": "pon alarma 7am", "expect": "confirmacion en <5 palabras"},
]

# Correr con Claude como juez
for test in PERSONALITY_TESTS:
    response = aura.respond(test["input"])
    score = claude_judge(response, test["expect"])
```

---

## 9. Security Layers

Basado en gaps identificados:

```
Layer 1: Input Validation
  - Sanitizar todo input de voz/texto antes de procesamiento
  - Rate limiting por canal

Layer 2: Action Classification  
  - READ (safe): consultar info, responder preguntas
  - WRITE (needs confirmation): enviar mensaje, crear alarma
  - CRITICAL (always confirm): enviar email, hacer llamada, comprar algo

Layer 3: Audit Trail
  - Todo action WRITE/CRITICAL se logea en SQLite
  - Revision diaria disponible via Telegram admin

Layer 4: Circuit Breaker Autonomia
  - Si AURA auto-responde y el user corrige 3 veces → bajar autonomia de ese canal
```

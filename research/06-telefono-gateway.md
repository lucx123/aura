# Telefono como Gateway para AURA

Investigacion de como darle a AURA acceso a un telefono fisico para llamadas, WhatsApp, y SMS.

---

## Proyectos que ya hacen esto

### 1. Patter (github.com/PatterAI/patter) - RECOMENDADO

**Que es:** SDK open-source (MIT) para darle telefono a agentes AI. Alternativa self-hosted a Vapi/Retell.

**Por que es perfecto para AURA:**
- Asignas un numero de telefono a tu agente en pocas lineas de codigo
- Soporta Anthropic Claude como LLM
- Self-hosted, tu controlas todo
- 27+ integraciones de providers

**Arquitectura:**
```
[Llamada entrante] → [Carrier: Twilio/Telnyx/Plivo] → [Patter SDK]
                                                            ↓
                                                    [STT: Deepgram/Whisper]
                                                            ↓
                                                    [LLM: Claude/OpenAI]
                                                            ↓
                                                    [TTS: ElevenLabs/Cartesia]
                                                            ↓
                                                    [Audio de vuelta al caller]
```

**Codigo minimo:**
```python
# Python - asignar numero a AURA
from patter import Patter, Twilio, Anthropic

phone = Patter(carrier=Twilio(), phone_number="+15550001234")
agent = phone.agent(
    engine=Anthropic(),  # Claude!
    system_prompt="Eres Aura, asistente personal de German...",
    first_message="Hola, soy Aura. German no esta disponible, en que puedo ayudarte?",
)
phone.serve(agent=agent, tunnel=True)  # Cloudflare tunnel automatico
```

**Providers soportados:**
- LLM: OpenAI, Anthropic, Gemini, Groq, Cerebras
- STT: Deepgram, AssemblyAI, Whisper
- TTS: ElevenLabs, OpenAI, Cartesia, LMNT
- Carriers: Twilio, Telnyx, Plivo
- VAD: Silero, Krisp, DeepFilterNet

**3 modos:**
- Realtime: all-in-one voice engines (menor latencia)
- Pipeline: STT → LLM → TTS (mas control)
- Hybrid: combinacion

**Dev local:** `tunnel: true` crea un Cloudflare tunnel automatico. No necesitas IP publica.

---

### 2. Bolna (github.com/bolna-ai/bolna) - ALTERNATIVA

**Que es:** Framework de orquestacion para voice AI agents con telefonia.

**Arquitectura:**
```
[Twilio/Plivo] ←WebSocket→ [Bolna Orchestrator]
                                    ↓
                    [Transcriber → LLM Agent → Synthesizer]
                                    ↓
                    [Audio streaming bidireccional]
```

**Stack:**
- STT: Deepgram, Azure
- LLM: OpenAI, Anthropic (via LiteLLM), DeepSeek, Llama, Groq
- TTS: ElevenLabs, AWS Polly, OpenAI, Deepgram, Cartesia
- Telephony: Twilio, Plivo (WebSocket bidireccional)

**Self-hosting:** Docker Compose con Redis + ngrok + Bolna server.

**Ventaja sobre Patter:** Mas maduro, tiene playground UI, persistence de agentes en Redis.

---

### 3. LiveKit SIP (github.com/livekit/sip) - PARA ESCALAR

**Que es:** Bridge que conecta llamadas telefonicas (PSTN/SIP) a rooms WebRTC donde viven agentes AI.

**Arquitectura:**
```
[Red telefonica PSTN] → [LiveKit SIP Service] → [LiveKit Room]
                                                       ↓
                                              [AI Agent participant]
                                              [Puede hablar, escuchar]
```

**Como funciona:**
1. Crear SIP Trunk (conexion con tu carrier)
2. Crear Dispatch Rule (a que room va cada llamada)
3. Cuando llega llamada → caller se une como participante → AI agent interactua

**Requiere:** LiveKit server + Redis + IP publica + libopus.

**Ideal para:** Si quieres que AURA atienda multiples llamadas simultaneas o integrar video.

---

### 4. AI Phone Agent Starter Kit (github.com/bobbylkchao/ai-phone-agent)

**Que es:** Template Node.js/TypeScript para agente telefonico AI.

**Flow de llamada:**
1. Call llega a numero Twilio
2. Twilio hace POST → tu backend
3. Backend inicia WebSocket Media Stream bidireccional
4. Audio → OpenAI Realtime API → respuesta → audio de vuelta
5. Conversacion natural en tiempo real

**Soporta:** Twilio + Amazon Connect. Facil de adaptar a Claude.

---

## Opciones para el Telefono Viejo

### Opcion A: Telefono como WhatsApp + SMS gateway (FACIL)

```
┌─────────────────────────────────────┐
│     Telefono viejo (Android)        │
│                                     │
│  [WhatsApp] ← Baileys (sesion)     │
│  [SMS]      ← Tasker HTTP bridge   │
│  [SIM]      ← Solo para WA y SMS   │
│                                     │
│  Siempre enchufado, WiFi            │
└──────────────────┬──────────────────┘
                   │ HTTP/WebSocket
                   ▼
┌─────────────────────────────────────┐
│     Backend AURA (laptop/EC2)       │
│  Hermes Orchestrator                │
└─────────────────────────────────────┘
```

- WhatsApp: Baileys usa la sesion del telefono (WA Web protocol)
- SMS: Tasker/Macrodroid → HTTP POST al backend cuando llega SMS
- No necesitas Twilio para mensajes
- Costo: solo el plan de la SIM (~$5-10/mes)

### Opcion B: Telefono como WhatsApp gateway + Patter para llamadas (RECOMENDADA)

```
┌─────────────────────────────────────┐
│     Telefono viejo (Android)        │
│                                     │
│  [WhatsApp] ← Baileys              │
│  [SMS]      ← Tasker bridge        │
│  [SIM]      ← Solo mensajes        │
│                                     │
└──────────────────┬──────────────────┘
                   │
                   ▼
┌─────────────────────────────────────┐
│     Backend AURA                    │
│                                     │
│  ┌─────────┐  ┌──────────────┐    │
│  │ Hermes  │  │ Patter SDK   │    │
│  │ Orchest.│  │ (llamadas)   │    │
│  └────┬────┘  └──────┬───────┘    │
│       │               │            │
│       ▼               ▼            │
│  [Claude API]   [Twilio/Telnyx]   │
│                  Numero virtual    │
│                  para llamadas     │
└─────────────────────────────────────┘
```

**Numeros separados:**
- Numero de la SIM del telefono viejo → WhatsApp + SMS (gratis)
- Numero virtual Twilio/Telnyx → Llamadas de voz ($1-2/mes + minutos)

**Ventajas:**
- WhatsApp gratis (sin Twilio)
- Llamadas profesionales via Patter + carrier
- Audio de llamadas nunca toca el telefono viejo (todo en la nube)
- Telefono viejo solo necesita estar conectado a WiFi

### Opcion C: TODO en el telefono viejo (HARDCORE)

```
┌─────────────────────────────────────┐
│     Telefono viejo (Android)        │
│                                     │
│  [WhatsApp]  ← Baileys             │
│  [SMS]       ← Tasker              │
│  [Llamadas]  ← Tasker + audio route│
│  [Linphone]  ← SIP client (VoIP)  │
│  [Termux]    ← Corre scripts       │
│                                     │
│  Audio de llamada:                  │
│  Speaker → Mic virtual → Stream    │
│  → Backend → Claude → TTS          │
│  → Stream back → Speaker llamada   │
│                                     │
└─────────────────────────────────────┘
```

**Problemas:**
- Android limita acceso al audio de llamadas (post Android 9)
- Necesitas root o hacks para capturar audio
- Latencia alta (telefono → backend → telefono)
- Si el telefono se cuelga, pierdes todo

**Veredicto:** NO recomendado para llamadas. SI para WhatsApp/SMS.

---

## Recomendacion Final para AURA

### Fase 1 (inmediata):
1. **Telefono viejo** = gateway de WhatsApp (Baileys) + SMS (Tasker)
2. **Llamadas** = por ahora no, agregar despues

### Fase 2 (cuando quieras llamadas):
3. **Patter SDK** + numero Twilio/Telnyx ($1-2/mes base)
4. Claude como cerebro de las llamadas
5. AURA contesta: "Hola, soy Aura. German no esta disponible..."
6. AURA puede hacer llamadas: "Aura, llama al dentista..."

### Fase 3 (optimizacion):
7. Migrar carrier de Twilio a Telnyx (mas barato para latam)
8. Agregar SIP trunk propio si el volumen justifica
9. LiveKit SIP si necesitas video o multi-call

---

## Costos de Telefonia

| Opcion | Costo mensual |
|--------|---------------|
| SIM prepago (WA + SMS) | $5-10 |
| Twilio numero local | $1/mes |
| Twilio minutos (inbound) | $0.0085/min |
| Twilio minutos (outbound) | $0.014/min |
| Telnyx numero | $1/mes |
| Telnyx minutos | $0.005/min |
| **Total estimado (uso moderado)** | **$8-15/mes** |

---

## Setup del Telefono Viejo - Checklist

### Hardware
- [ ] Android 7+ (ideal 9+)
- [ ] WiFi estable
- [ ] Enchufado permanente (con proteccion de bateria)
- [ ] SIM con datos minimos activada

### Apps necesarias
- [ ] WhatsApp (sesion activa)
- [ ] Tasker o Macrodroid ($3 o gratis)
- [ ] Termux (terminal Linux)
- [ ] AccuBattery (limitar carga a 80%)
- [ ] Wake Lock (prevenir deep sleep)

### Configuracion
- [ ] Desactivar optimizacion de bateria para apps clave
- [ ] WiFi always-on
- [ ] ADB over WiFi activado (para debug remoto)
- [ ] Tasker profiles: SMS received → HTTP POST a backend
- [ ] Termux: script watchdog que verifica cada hora que todo corre

### Backend (laptop/EC2)
- [ ] Baileys conectado a la sesion de WhatsApp
- [ ] Endpoint para recibir SMS de Tasker
- [ ] Health check del telefono cada 5 min
- [ ] Alerta Telegram si telefono no responde

# Infraestructura AWS - Bedrock + EC2

## Recursos disponibles

- **$200 USD en creditos AWS**
- **Amazon Bedrock**: acceso a Claude (misma API, misma calidad)
- **EC2**: servidor principal de Aura

## Amazon Bedrock como cerebro

### Ventaja de usar Bedrock vs API directa de Anthropic
- Todo en un solo proveedor (AWS)
- Mismo modelo Claude (Opus, Sonnet, Haiku)
- Se paga con los creditos AWS
- IAM para seguridad (no necesitas API key flotando)
- Logging con CloudWatch
- No rate limits tan agresivos como la API directa
- Puedes cambiar de modelo sin cambiar proveedor

### Modelos disponibles en Bedrock (2025)
| Modelo | Uso en Aura | Costo aprox |
|--------|-------------|-------------|
| Claude Sonnet | Cerebro principal, razonamiento | ~$3/MTok input, $15/MTok output |
| Claude Haiku | Tareas simples, clasificacion, routing | ~$0.25/MTok input, $1.25/MTok output |
| Claude Opus | Tareas complejas, analisis profundo | ~$15/MTok input, $75/MTok output |

### Estrategia de costos con Bedrock
- **Router inteligente**: usar Haiku para decidir que modelo usar
- **Haiku** para: clasificar mensajes, resumir, tareas simples
- **Sonnet** para: conversacion principal, tool use, razonamiento
- **Opus** para: tareas complejas puntuales (analisis largo, planificacion)

### Estimacion de uso mensual
Asumiendo uso moderado (20-30 interacciones/dia):
- Haiku (routing + tareas simples): ~$2-5/mes
- Sonnet (conversaciones): ~$10-20/mes
- Total LLM: ~$15-25/mes

Con $200 creditos: ~8-12 meses de uso del LLM solo

### SDK y codigo

```python
import boto3
import json

bedrock = boto3.client(
    service_name='bedrock-runtime',
    region_name='us-east-1'  # o la region que tenga Bedrock
)

def ask_aura(prompt, system_prompt, model="anthropic.claude-sonnet-4-20250514"):
    response = bedrock.invoke_model(
        modelId=model,
        body=json.dumps({
            "anthropic_version": "bedrock-2023-05-31",
            "max_tokens": 1024,
            "system": system_prompt,
            "messages": [
                {"role": "user", "content": prompt}
            ]
        })
    )
    result = json.loads(response['body'].read())
    return result['content'][0]['text']
```

### Tool Use con Bedrock
Bedrock soporta tool use nativo de Claude. Aura puede tener tools como:
```python
tools = [
    {
        "name": "send_whatsapp",
        "description": "Envia un mensaje de WhatsApp a un contacto",
        "input_schema": {
            "type": "object",
            "properties": {
                "contact": {"type": "string"},
                "message": {"type": "string"}
            },
            "required": ["contact", "message"]
        }
    },
    {
        "name": "send_email",
        "description": "Envia un email",
        "input_schema": {...}
    },
    {
        "name": "control_lights",
        "description": "Enciende o apaga luces",
        "input_schema": {...}
    },
    {
        "name": "check_calendar",
        "description": "Consulta el calendario",
        "input_schema": {...}
    },
    {
        "name": "set_reminder",
        "description": "Crea un recordatorio",
        "input_schema": {...}
    },
    {
        "name": "search_memory",
        "description": "Busca en la memoria de largo plazo",
        "input_schema": {...}
    }
]
```

## EC2 - Servidor principal

### Instancia recomendada

| Opcion | vCPU | RAM | Costo/mes | Notas |
|--------|------|-----|-----------|-------|
| t3.micro | 2 | 1GB | ~$8 (o free tier) | Muy justo |
| t3.small | 2 | 2GB | ~$15 | Minimo funcional |
| t3.medium | 2 | 4GB | ~$30 | Comodo |
| t3.large | 2 | 8GB | ~$60 | Si corres Whisper local en EC2 |

**Recomendacion**: t3.small o t3.medium
- Si el STT corre en la laptop local -> t3.small basta
- Si quieres todo en EC2 -> t3.medium minimo

### Que corre en EC2
- Orquestador principal (Python/FastAPI)
- MQTT Broker (Mosquitto)
- ChromaDB (memoria)
- Bot WhatsApp (Baileys/Evolution)
- Email service
- Twilio webhook receiver
- Telegram bot

### Arquitectura en EC2

```
EC2 (t3.small, Ubuntu 22.04)
├── Docker Compose
│   ├── aura-core (orquestador FastAPI)
│   ├── mosquitto (MQTT broker)
│   ├── chromadb (memoria vectorial)
│   ├── whatsapp-service (Baileys)
│   ├── email-service
│   └── nginx (reverse proxy + SSL)
└── Systemd
    └── tailscale (VPN a laptop local)
```

### Otros servicios AWS utiles

| Servicio | Para que | Costo con creditos |
|----------|----------|-------------------|
| S3 | Backup de memoria, almacenar audio/fotos | ~$0.02/GB/mes |
| CloudWatch | Logs y monitoreo de Aura | Free tier generoso |
| Route 53 | Dominio custom (aura.tudominio.com) | $0.50/mes por zona |
| SQS | Cola de mensajes entre servicios | Free tier: 1M msgs |
| Secrets Manager | API keys seguras | $0.40/secret/mes |
| EventBridge | Cron jobs (resumen diario, etc) | Free tier generoso |

### Distribucion del presupuesto ($200)

| Concepto | Estimado mensual | Duracion con $200 |
|----------|-----------------|-------------------|
| EC2 t3.small | $15 | - |
| Bedrock (LLM) | $15-25 | - |
| S3 + otros | $2-5 | - |
| **Total** | **~$35-45/mes** | **~4-5 meses** |

Despues de los creditos: optimizar para bajar a $20-30/mes.

### Seguridad en EC2

- Security Group: solo abrir puertos necesarios (443, MQTT, SSH)
- IAM Role para EC2 con permisos minimos a Bedrock
- No hardcodear keys - usar IAM roles y Secrets Manager
- SSH con key pair, no password
- Fail2ban para proteger SSH
- Let's Encrypt para HTTPS

## Flujo completo con AWS

```
[Usuario habla] -> [Laptop: wake word + STT]
                         |
                         v (texto via MQTT/WebSocket)
                   [EC2: Orquestador]
                         |
                         v
                   [Bedrock: Claude procesa]
                         |
                         v (respuesta + acciones)
                   [EC2: ejecuta tools]
                         |
              ┌──────────┼──────────────┐
              v          v              v
        [WhatsApp]  [Email]     [MQTT a laptop]
                                       |
                                       v
                              [Laptop: TTS + ESP32]
                                       |
                                       v
                              [Aura habla + cara reacciona]
```

## Investigar

- [ ] Bedrock streaming response - para TTS en tiempo real
- [ ] Bedrock Converse API vs InvokeModel - cual es mejor para chat
- [ ] IAM role para EC2 -> Bedrock (sin API keys)
- [ ] Bedrock Knowledge Bases - alternativa a ChromaDB self-hosted?
- [ ] Bedrock Agents - vale la pena vs orquestador custom?
- [ ] Reserved instances o Savings Plans para bajar costo EC2 despues

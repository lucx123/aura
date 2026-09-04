# Lista de Compras y Fases de Implementacion

## Fase 0: Proof of Concept (solo software)

**Costo: $0-5/mes**
**Objetivo: Aura funciona por texto y voz basica en la laptop**

- [ ] Orquestador basico en Python
- [ ] Integracion con Claude/GPT API (free tier o creditos)
- [ ] Whisper local para STT
- [ ] Piper TTS local para voz
- [ ] ChromaDB para memoria
- [ ] Bot de Telegram como interfaz de control

**Hardware necesario: NADA (solo la laptop vieja)**

---

## Fase 1: Voz y Presencia Minima

**Costo hardware: ~$30-50 USD**
**Objetivo: Aura tiene cara, escucha y habla**

### Comprar:
| Item | Precio aprox | Link referencia |
|------|-------------|-----------------|
| ESP32 dev board | $5-8 | AliExpress/Amazon |
| Pantalla TFT circular GC9A01 1.28" | $6-10 | AliExpress |
| Amplificador MAX98357A I2S | $3-5 | AliExpress |
| Speaker 3W 4ohm | $2-3 | AliExpress |
| Microfono INMP441 I2S | $2-3 | AliExpress |
| LED Ring WS2812B (12 LEDs) | $3-5 | AliExpress |
| Cables, protoboard, etc | $5-10 | Local |

### Software:
- [ ] Firmware ESP32 para cara animada
- [ ] Wake word con Porcupine
- [ ] Pipeline STT -> LLM -> TTS funcional
- [ ] Expresiones basicas (idle, escucha, habla, piensa)

---

## Fase 2: Vision

**Costo hardware: ~$10-20 USD adicionales**
**Objetivo: Aura puede ver y reconocer**

### Comprar:
| Item | Precio aprox |
|------|-------------|
| ESP32-CAM (OV2640) | $5-8 |
| O webcam USB | $15-25 |

### Software:
- [ ] Stream de video funcional
- [ ] Face detection y recognition
- [ ] Integracion con LLM multimodal
- [ ] Deteccion de presencia

---

## Fase 3: Comunicaciones

**Costo: ~$5-15/mes recurrente**
**Objetivo: Aura gestiona tus comunicaciones**

### Servicios:
| Servicio | Costo |
|----------|-------|
| Twilio numero | $1.15/mes |
| Twilio uso | ~$5-10/mes |
| ElevenLabs (mejor voz) | $5/mes |
| LLM API (Claude/GPT) | $10-20/mes |

### Software:
- [ ] Bot WhatsApp (Baileys o Evolution API)
- [ ] Integracion Gmail
- [ ] Twilio para llamadas/SMS
- [ ] Sistema de prioridades de notificacion
- [ ] Modo secretaria

---

## Fase 4: Infraestructura Solida

**Costo: ~$0-30/mes (EC2)**
**Objetivo: Aura siempre disponible, respaldada**

### Setup:
- [ ] EC2 como servidor principal
- [ ] Docker Compose para todos los servicios
- [ ] Tailscale para VPN laptop <-> EC2
- [ ] GitHub private para respaldo
- [ ] Auto-deploy con webhooks
- [ ] Backups automaticos de memoria

---

## Fase 5: Smart Home y Proactividad

**Costo: variable segun dispositivos**
**Objetivo: Aura controla el entorno y es proactiva**

### Posibles compras:
| Item | Precio aprox | Para que |
|------|-------------|----------|
| Smart plug WiFi | $8-15 | Control de luces/aparatos |
| Sensor temperatura DHT22 | $3 | Ambiente |
| Sensor PIR movimiento | $2 | Detectar presencia sin camara |
| IR Blaster (ESP32 + IR LED) | $3 | Control de TV/AC |
| Servo SG90 x2 | $4 | Pan-tilt para cara |

### Software:
- [ ] Home Assistant o control directo
- [ ] Rutinas automaticas
- [ ] Proactividad basada en contexto
- [ ] Integracion con calendario

---

## Fase 6: Refinamiento y "Vida"

**Objetivo: Aura se siente realmente viva**

- [ ] Movimiento fisico (servos, gira hacia ti)
- [ ] Personalidad profunda con memoria emocional
- [ ] Chistes internos, referencias a conversaciones pasadas
- [ ] Adaptacion al mood (detecta tono de voz, expresion facial)
- [ ] Voz custom entrenada (unica de Aura)
- [ ] Cuerpo/estructura 3D printed

---

## Resumen de costos totales

### Una vez (hardware):
- Minimo viable: ~$30-50 USD
- Completo: ~$80-150 USD

### Recurrente (mensual):
- Minimo (todo local): $0-5/mes
- Medio (APIs + Twilio): $15-30/mes
- Full (todo premium): $40-60/mes

---

## Donde comprar (Chile/Latam)

- **AliExpress**: mas barato, 20-40 dias de envio
- **Amazon**: mas rapido, mas caro
- **MercadoLibre**: disponibilidad variable
- **Tiendas electronica local**: para cosas urgentes (cables, protoboard)
- **PCBWay/JLCPCB**: si diseñas PCB custom (fase avanzada)

## Prioridad de investigacion

1. Pipeline de voz end-to-end (mas impacto inmediato)
2. Firmware ESP32 + pantalla (darle cara)
3. WhatsApp integration (utilidad diaria)
4. Vision basica (face recognition)
5. Smart home (nice to have)

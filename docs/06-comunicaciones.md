# Sistema de Comunicaciones

## Canales

### WhatsApp
**Opciones:**
1. WhatsApp Business API (oficial via Twilio/Meta)
   - Estable, no te banean
   - Costo: ~$0.005-0.08 por mensaje segun pais
   - Necesitas verificar negocio (o usar Twilio sandbox para dev)

2. Baileys (no oficial, open source, Node.js)
   - Gratis, self-hosted
   - Riesgo de baneo si Meta detecta automatizacion
   - Perfecto para desarrollo/testing
   - Repo: github.com/WhiskeySockets/Baileys

3. Evolution API (self-hosted, wrapper)
   - REST API sobre WhatsApp Web
   - Docker deploy
   - Multi-sesion
   - Comunidad activa

**Lo que Aura puede hacer en WhatsApp:**
- Leer mensajes y resumirlos
- Responder por ti (con confirmacion o autonomamente)
- Enviar mensajes programados
- Alertar de mensajes urgentes por voz
- Buscar en historial de conversaciones

### Email
**Implementacion:**
- Gmail API (OAuth2) o Microsoft Graph
- IMAP para lectura, SMTP para envio
- Clasificacion con LLM (urgente, spam, personal, trabajo)

**Lo que Aura puede hacer:**
- "Tienes 5 emails nuevos. 1 urgente de tu jefe sobre el deadline"
- Redactar respuestas por ti
- Programar envios
- Buscar emails viejos por contenido

### Telefono (Twilio)
**Setup:**
- Numero virtual: ~$1.15/mes
- Llamadas entrantes: $0.0085/min
- Llamadas salientes: $0.014/min
- SMS: $0.0079 por mensaje

**Lo que Aura puede hacer:**
- Recibir llamadas y actuar como secretaria
- "Hola, soy Aura, asistente de German. El no esta disponible ahora. Puedo tomar un mensaje?"
- Hacer llamadas por ti (restaurantes, citas)
- Enviar/recibir SMS
- Filtrar spam telefonico

### Telegram (alternativa/complemento)
- Bot API: completamente gratis, sin riesgo de baneo
- Perfecto como canal de control/admin
- Puedes mandarle comandos a Aura por Telegram
- Notificaciones sin limite

## Prioridad de notificaciones

Aura debe decidir COMO notificarte segun urgencia:

| Urgencia | Canal de notificacion |
|----------|----------------------|
| Critica | Voz inmediata + LED rojo |
| Alta | Voz suave + LED naranja |
| Media | Solo LED + menciona si preguntas |
| Baja | Acumula para resumen |

### Criterios de urgencia (configurable)
- Critica: familia, emergencias, deadlines hoy
- Alta: jefe, trabajo urgente
- Media: amigos, emails normales
- Baja: newsletters, promos, grupos

## Modo secretaria

Cuando no estas disponible, Aura puede:
1. Responder WhatsApp: "German esta ocupado, le paso tu mensaje"
2. Atender llamadas: saludo personalizado + tomar mensaje
3. Filtrar: solo interrumpir si es realmente urgente
4. Resumir: cuando vuelvas, te da el resumen de todo

## Investigar

- [ ] Twilio + Vapi.ai para llamadas con voz AI
- [ ] Rate limits de WhatsApp Business API
- [ ] Baileys stability en 2024/2025
- [ ] Gmail API scopes necesarios y limites
- [ ] Telegram Bot API para canal de admin
- [ ] Matrix/Element como alternativa open source

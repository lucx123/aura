# Ideas de Features para AURA

Ideas sacadas de proyectos similares + propias, organizadas por viabilidad.

---

## Fase 1 - MVP (implementable en 1-2 semanas)

### Voice Loop Basico
- Wake word "Aura" → escucha → responde con personalidad
- Usa RealtimeSTT + Claude Sonnet + ElevenLabs
- Sin hardware, solo laptop con mic y speaker

### Personality System
- System prompt versionado en Git
- Eval suite (50+ test cases) para garantizar consistencia
- Personalidad: eficiente, calida, humor seco, proactiva

### Context Window Inteligente
- Mantener ultimas 10 interacciones en memoria de trabajo
- Inyectar contexto relevante (hora, dia, clima, proxima reunion)
- "Buenos dias German, hoy tienes reunion a las 10 con el equipo de Morris"

---

## Fase 2 - Integraciones (semanas 3-6)

### Morning Briefing
- Cada dia a la hora que te despiertas (aprendida)
- Resume: clima, calendario, emails urgentes, noticias tech relevantes
- Se activa con presencia (camara detecta movimiento) o por horario

### WhatsApp Intelligent
- Clasificar mensajes por urgencia (Claude Haiku)
- Respuestas automaticas a mensajes simples ("llego en 10")
- Notificacion por voz de mensajes urgentes
- Draft de respuestas para aprobacion via Telegram admin

### Email Triage
- Cada hora escanear inbox
- Clasificar: urgente / importante / spam / newsletter
- Resumir los importantes en una oracion cada uno
- Proponer respuestas para los urgentes

### Smart Reminders
- "Aura, recuerdame comprar leche cuando salga de la casa"
- Trigger por evento (salir de casa) en vez de por hora
- Detectar "salir de casa" via camara (no detecta persona por 5min) o via telefono (GPS futuro)

---

## Fase 3 - Presencia Fisica (semanas 6-10)

### Face Sync Emocional
- Cara refleja estado: idle (parpadeo), listening (ojos atentos), thinking (animacion), speaking (lip sync basico)
- Transiciones suaves entre estados
- Easter eggs: si dices "feliz cumpleanos" hace animacion especial

### Ambient Awareness
- Camara detecta cuantas personas hay en la habitacion
- Si hay visita: modo discreto (no interrumpe con notificaciones personales)
- Si estas solo: modo completo

### Presence Greeting
- Detectar cuando llegas a casa (camara + face recognition)
- "Bienvenido German. Tienes 3 WhatsApp nuevos y un email de tu jefe"
- Adaptar greeting segun hora y estado (si son las 2am: "Llegas tarde, descansa")

---

## Fase 4 - Proactividad Inteligente (semanas 10+)

### Pattern Learning
- Detectar rutinas: "siempre pone alarma a las 11pm"
- Sugerir: "Son las 11, pongo la alarma de siempre?"
- Aprender preferencias de comida, musica, temperatura

### Conversational Memory
- Recordar temas de conversaciones pasadas
- "La semana pasada mencionaste que querias empezar gym, como va eso?"
- Nunca forzar - solo si es natural en el contexto

### Smart Interruption
- No interrumpir si estas en llamada/reunion (detectar por audio)
- Acumular notificaciones y entregar en bloque cuando hay pausa
- Prioridad configurable por contacto y canal

### Pair Programming Mode
- Detectar que estas frente a la PC (camara + teclado activo)
- "Quieres que te ayude con algo?" despues de 30min sin interaccion
- Si dices si: modo Claude Code pero por voz
- Screenshot periodico para contexto visual (Gemini Flash para analizar)

---

## Fase 5 - Avanzado (futuro)

### Multi-Room Presence
- Multiples ESP32 baratos (solo LED ring) en diferentes cuartos
- AURA "te sigue" - el LED del cuarto donde estas se activa
- Mic en cada cuarto para que puedas hablar desde donde sea

### Phone Call Agent
- Contestar llamadas de numeros conocidos: "German no esta disponible, puedo tomar mensaje?"
- Hacer llamadas por ti: "Aura, llama al dentista y agenda cita para el jueves"
- Transcribir y resumir llamadas que recibes

### Emotional Intelligence
- Detectar estado emocional por tono de voz (Speech Emotion Recognition)
- Adaptar respuestas: si suenas estresado, ser mas calma y breve
- Si suenas contento, match la energia
- Nunca mencionar explicitamente la emocion detectada (creepy)

### Digital Twin / Avatar
- Representacion 3D de AURA en pantalla o web
- Para videollamadas: AURA aparece como avatar representandote
- "Aura, entra a mi standup de las 9 y toma notas"

### Autonomous Task Execution
- "Aura, busca vuelos a Buenos Aires para el proximo finde"
- Navega web, compara precios, presenta opciones
- Solo ejecuta compra con confirmacion explicita

---

## Ideas Creativas / Diferenciadores

### "Diario de AURA"
- Cada noche, AURA escribe un entry sobre su dia
- Que aprendio, que interacciones tuvo, que patterns detecto
- Tu puedes leerlo como insight sobre tu propia vida

### Modo "Guardian"
- Cuando sales de viaje: AURA vigila la casa
- Camara detecta movimiento inusual → alerta Telegram con foto
- Puede reproducir sonidos de "persona en casa" (TV, conversacion)

### Skill Marketplace Personal
- Crear skills custom: "cada vez que X, haz Y"
- "Cuando reciba email de AWS con 'invoice', guardalo en la carpeta Facturas"
- Se definen en lenguaje natural, AURA las convierte en reglas

### Voice Journal
- "Aura, nota mental: se me ocurrio una idea para el proyecto..."
- Transcribe, categoriza, y agrega a tu sistema de notas
- Busqueda semantica despues: "que idea tuve sobre X la semana pasada?"

# Interaccion IRL - Aura en el Mundo Real

## Filosofia

Aura no es un chatbot al que le hablas cuando necesitas algo.
Es una presencia constante que PERCIBE tu entorno y actua como companion.

## Modos de operacion

### Modo Activo
- Usuario invoco a Aura (wake word o gesto)
- Plena atencion, procesando todo
- Ojos abiertos, LED azul brillante
- Responde inmediatamente

### Modo Pasivo (ambient)
- Aura esta "presente" pero no escucha activamente
- Solo reacciona a wake word
- Cara muestra animacion idle sutil (parpadeo)
- Puede alertar proactivamente si algo importante pasa

### Modo Proactivo
- Aura inicia interaccion por su cuenta
- Ejemplos:
  - "German, tienes una reunion en 15 minutos"
  - "Llevas 3 horas sin pararte, un break?"
  - "Tu mama te mando un WhatsApp hace rato"
  - "Se ve que esta lloviendo, lleva paraguas"
- Debe ser configurable (frecuencia, importancia minima)
- NUNCA debe ser molesto o intrusivo

### Modo No Molestar
- Silencio total, solo emergencias
- Cara apagada o minima
- Acumula notificaciones para despues

## Interacciones cotidianas

### Al despertar
```
[Detecta movimiento / hora programada]
Aura: "Buenos dias German. Son las 7:15.
       Hoy tienes 2 reuniones, la primera a las 9.
       La temperatura afuera es 18 grados.
       Quieres que te ponga musica?"
```

### Al llegar a casa
```
[Camara detecta rostro conocido / sensor de puerta]
Aura: "Bienvenido. Tienes 2 mensajes nuevos en WhatsApp
       y un email de tu jefe. Quieres el resumen?"
```

### Mientras trabajas
```
[Detecta que estas en el escritorio por >2hrs]
Aura: *suavemente* "Hey, llevas un buen rato concentrado.
       Todo bien? Necesitas algo?"
```

### Pregunta casual
```
German: "Aura, que dia es manana?"
Aura: "Manana es martes 26. No tienes nada agendado.
       Quieres que te programe algo?"
```

### Ayuda visual
```
German: "Aura, mira esto" *muestra objeto a la camara*
Aura: "Veo un capacitor electrolitico de 470uF 25V.
       Necesitas el datasheet o ayuda con el circuito?"
```

### Control del entorno
```
German: "Aura, apaga la luz"
Aura: "Listo" *envia comando a smart plug/Home Assistant*
```

## Gestos y comunicacion no verbal

### Inputs por gesto (fase avanzada)
- Mano alzada = "para" / "silencio"
- Pulgar arriba = confirmacion sin hablar
- Señalar camara = "mira esto"
- Wave = "hola" / activar

### Outputs expresivos de Aura
- Asiente (servo) cuando entendio
- Gira hacia ti cuando le hablas
- "Mira" lo que le señalas (pan-tilt)
- Parpadea mas rapido si esta "emocionada"
- Ojos medio cerrados si esta en standby

## Conciencia de contexto

### Que debe saber Aura de su entorno
- Hora del dia (ajustar comportamiento)
- Quien esta presente (face recognition)
- Nivel de luz (dia/noche)
- Sonido ambiente (musica, silencio, conversacion)
- Si el usuario esta ocupado (postura, pantalla activa)
- Clima exterior (API)
- Calendario del usuario

### Inferencias utiles
- "Esta oscuro + tarde = probablemente yendose a dormir"
- "Muchos ruidos de tecleo = esta trabajando concentrado"
- "Persona desconocida + usuario ausente = posible alerta"
- "Muchos bostezos detectados = sugerir descanso"

## Integracion con vida diaria

### Smart Home (via Home Assistant o directamente)
- Luces (Philips Hue, WLED, smart plugs)
- Musica (Spotify API, parlante)
- Climatizacion (si hay smart thermostat)
- TV / monitor (CEC, IR blaster con ESP32)

### Productividad
- Leer emails y resumir
- Crear recordatorios y timers
- Buscar informacion rapida
- Dictar notas
- Timer pomodoro con notificaciones suaves

### Salud y bienestar
- Recordar tomar agua
- Alertar sobre postura
- Sugerir breaks
- Tracking de horas de sueño (si tiene sensor)

## Anti-patrones (que NO hacer)

- Hablar sin que te hablen cuando no es importante
- Interrumpir conversaciones con otras personas
- Dar informacion que no se pidio constantemente
- Ser "creepy" - no comentar todo lo que ve
- Grabar/almacenar video sin consentimiento explicito
- Ser un distractor mas en vez de ayudar a enfocarse

## Investigar

- [ ] Home Assistant API para control de dispositivos
- [ ] Spotify API para control de musica por voz
- [ ] Tecnicas de "attention detection" (sabe si le estas hablando a ella o a otra persona)
- [ ] Ambient computing - como desaparecer cuando no se necesita
- [ ] Estudiar UX de Alexa/Google Home - que funciona y que molesta
- [ ] Emotional AI - detectar estado de animo por voz/cara y adaptar respuestas

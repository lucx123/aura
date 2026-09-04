# Vision General de Aura

## Que es Aura

**A.U.R.A. — Adaptive Unified Reasoning Assistant** es una inteligencia personal
distribuida con presencia digital y fisica. No es solo un chatbot ni pertenece a
un unico aparato: mantiene identidad, memoria y contexto entre sus distintos nodos.

Sus dos manifestaciones fisicas principales son:

- **AURA Watch:** reloj completo sobre ESP32-S3, con voz, wake word, pantalla,
  BLE, Wi-Fi, avisos y comunicacion proactiva.
- **AURA Desktop:** futuro robot de escritorio con voz, camara, expresiones,
  movimiento e interaccion con el entorno.

## Pilares del proyecto

### 1. Percepcion (inputs)
- Vision: camaras que ven el entorno, reconocen rostros, objetos, gestos
- Audio: microfono always-on con wake word, entiende contexto de sonido
- Sensores: temperatura, movimiento, presencia (opcionales)

### 2. Razonamiento (cerebro)
- LLM como motor de decision (Claude/GPT-4o)
- Memoria a largo plazo (vectorDB)
- Contexto personal (horarios, preferencias, rutinas)
- Capacidad de planificar y ejecutar tareas multi-paso

### 3. Accion (outputs)
- Voz natural (TTS de alta calidad)
- Expresiones faciales en pantalla/LED
- Comunicaciones (WhatsApp, email, llamadas)
- Control de dispositivos (luces, musica, etc.)

### 4. Presencia (cuerpo)
- Reloj ESP32-S3 que acompaña e informa al usuario
- Robot de escritorio con cara animada que reacciona
- Indicadores visuales de estado (escuchando, pensando, hablando)
- Posiblemente un cuerpo/estructura fisica

## Filosofia de diseno

- Aura debe sentirse VIVA, no como un comando de terminal
- Debe tener personalidad consistente
- Debe ser proactiva (no solo responder, tambien iniciar)
- Cada accion autonoma debe respetar permisos proporcionales a su riesgo
- Debe respetar privacidad (modo "no molestar", no grabar siempre)
- Debe ser util IRL, no solo un gadget cool

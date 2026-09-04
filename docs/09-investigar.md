# Temas a Investigar

Lista de cosas que hay que researchar antes de implementar.

## Prioridad Alta

### Voice Pipeline
- [ ] faster-whisper performance en CPU (laptop vieja)
- [ ] Piper TTS calidad en español - samples
- [ ] Latencia real de ElevenLabs streaming API
- [ ] Picovoice Porcupine - crear custom wake word "Aura"
- [ ] Echo cancellation: speexdsp vs webrtc-audio-processing
- [ ] Tecnica de streaming TTS (hablar mientras genera)

### ESP32 + Display
- [ ] Libreria TFT_eSPI para GC9A01 - animaciones fluidas?
- [ ] LVGL (Light Versatile Graphics Library) para ESP32
- [ ] Proyectos existentes: "ESP32 Animated Eyes", "Wio Terminal Face"
- [ ] Comunicacion ESP32 <-> Python backend (MQTT vs WebSocket)
- [ ] Consumo de energia del setup completo
- [ ] ESP-IDF vs Arduino framework - cual conviene

### LLM como cerebro
- [ ] Claude API tool use - definir herramientas de Aura
- [ ] Costo real mensual estimado con uso tipico
- [ ] Function calling para acciones (email, whatsapp, luces)
- [ ] Context window management para memoria larga
- [ ] Alternativa: modelo local (Llama 3 8B) para tareas simples

## Prioridad Media

### WhatsApp
- [ ] Baileys 2024/2025 - estabilidad, baneos
- [ ] Evolution API - setup con Docker
- [ ] WhatsApp Business API via Twilio - proceso de verificacion
- [ ] Multi-device: leer mensajes sin desconectar telefono

### Vision
- [ ] face_recognition library - accuracy con pocas fotos de entrenamiento
- [ ] MediaPipe face mesh - performance en RPi/laptop
- [ ] YOLO v8 nano - deteccion de objetos en CPU
- [ ] ESP32-CAM - framerate real, calidad, estabilidad del stream
- [ ] Privacy: como procesar sin almacenar

### Infraestructura
- [ ] Tailscale free tier - limitaciones
- [ ] Docker Compose en EC2 t2.micro - cabe todo?
- [ ] MQTT broker ligero (Mosquitto) en EC2
- [ ] GitHub Actions para auto-deploy
- [ ] Let's Encrypt para HTTPS en EC2

## Prioridad Baja (fases futuras)

### Smart Home
- [ ] Home Assistant en Docker - recursos necesarios
- [ ] WLED para LEDs inteligentes
- [ ] Tuya/SmartLife API para smart plugs baratos
- [ ] ESP32 como IR blaster universal

### Movimiento fisico
- [ ] Servos SG90 - precision, ruido, durabilidad
- [ ] Pan-tilt kits disponibles
- [ ] Face tracking con OpenCV -> servo control
- [ ] Stepper vs servo para base giratoria

### Personalidad avanzada
- [ ] Fine-tuning de modelo para personalidad consistente
- [ ] Emotional Text-to-Speech (voz que refleja emocion)
- [ ] Speech Emotion Recognition (SER) - librerias Python
- [ ] "Idle behaviors" - pequeñas animaciones aleatorias para sentirse viva

## Proyectos de referencia a estudiar

- [ ] **Mycroft AI** - asistente open source, arquitectura
- [ ] **OVOS (Open Voice OS)** - fork de Mycroft, mas activo
- [ ] **Willow** - ESP32 voice assistant open source
- [ ] **Wyoming protocol** - integracion de voz con Home Assistant
- [ ] **Adeept RaspTank** - robot con RPi y vision
- [ ] **Anki Vector** - como lograron personalidad con hardware limitado
- [ ] **r/homeassistant** - comunidad con proyectos similares
- [ ] **ESP32 Marauder** - referencia de lo que puede hacer el ESP32

## Papers / Articulos

- [ ] "Designing Voice User Interfaces" - principios de UX de voz
- [ ] "Companion AI" - research sobre AI que acompaña vs AI que sirve
- [ ] Wake word detection on microcontrollers
- [ ] Real-time face expression generation for virtual agents
- [ ] Conversational memory in LLM-based assistants

# Arquitectura de nodos AURA

## Definicion

**A.U.R.A. — Adaptive Unified Reasoning Assistant** es una inteligencia personal
distribuida, multimodal y proactiva. Su identidad no pertenece a un dispositivo:
vive en el nucleo compartido y se manifiesta mediante distintos **AURA Nodes**.

Los primeros nodos fisicos previstos son:

- **AURA Watch:** compañero portatil basado en la placa Waveshare
  ESP32-S3-Touch-AMOLED-2.06, conectado por BLE y Wi-Fi.
- **AURA Desktop:** robot de escritorio con voz, camara, expresiones y movimiento.

La app movil y los canales digitales tambien son superficies de acceso a la misma
AURA. Todos comparten personalidad, memoria, conversaciones, permisos y contexto.

## Arquitectura general

```text
                           AURA
             identidad · memoria · razonamiento
                               │
                    ┌──────────▼──────────┐
                    │  AURA Gateway API  │
                    │ sesiones · eventos │
                    │ permisos · nodos   │
                    └──────────┬──────────┘
                               │
                 ┌─────────────▼─────────────┐
                 │ Orquestador reemplazable │
                 │ Hermes / OpenClaw / otro │
                 └───────┬───────────┬───────┘
                         │           │
                 ┌───────▼──────┐ ┌──▼──────────────┐
                 │ Proveedores  │ │  aura-memory   │
                 │ Bedrock/     │ │ memoria comun  │
                 │ Claude       │ └────────────────┘
                 │ Google/Gemini│
                 └──────────────┘
                               ▲
          ┌────────────────────┼──────────────────────┐
          │                    │                      │
    AURA Watch           AURA Desktop          Canales digitales
    ESP32-S3 + app       robot fisico          WhatsApp/Telegram/
    BLE y Wi-Fi          voz y vision          Discord/email
```

El gateway evita acoplar firmware y aplicaciones a Hermes, OpenClaw o cualquier
orquestador concreto. Cambiar el cerebro no debe obligar a reprogramar cada nodo.

## AURA Watch

El reloj sera la primera presencia fisica portatil de AURA. Usara una **Waveshare
ESP32-S3-Touch-AMOLED-2.06**, ya comprada y pendiente de recepcion. Su ESP32-S3R8
ejecutara el firmware completo del reloj: interfaz tactil, audio local, wake word,
BLE, Wi-Fi, sensores, energia y comunicacion con AURA Gateway. El razonamiento LLM
y otros procesos pesados seguiran en el orquestador.

La configuracion conocida se documenta en
[`13-aura-watch-hardware.md`](13-aura-watch-hardware.md). La revision y el pinout
se verificaran fisicamente al recibirla.

### Capacidades

- hora, fecha, bateria y conectividad;
- ojos, expresiones y estados de escucha, pensamiento, habla y error;
- tarjetas de recordatorios, mensajes, reuniones y alertas;
- vibracion y avisos hablados iniciados por AURA;
- boton o gesto para "Hablar con AURA";
- wake word local configurable, inicialmente "Aura";
- microfono y salida de audio;
- continuidad de conversaciones iniciadas en otros canales;
- acciones rapidas con confirmacion cuando corresponda.

### BLE y Wi-Fi

La app propia se desarrollará primero para Android. La propuesta de vinculación,
segundo plano, protocolo y enlace con la EC2 existente está en
[App Android y conectividad](14-app-android-conectividad.md).

**BLE** es el enlace cotidiano de bajo consumo con la app movil:

- emparejamiento y aprovisionamiento;
- configuracion de Wi-Fi y preferencias;
- eventos de botones, gestos y sensores;
- notificaciones, estado y mensajes breves;
- puente hacia Internet cuando no haya una red Wi-Fi conocida.
- audio comprimido por turnos hacia/desde la app, pendiente de validar rendimiento.

**Wi-Fi** se reserva para tareas que necesitan mas ancho de banda o independencia:

- conexion directa y autenticada con AURA Gateway;
- audio en streaming;
- sincronizacion de recursos;
- actualizaciones OTA;
- funcionamiento directo cuando el telefono no este disponible.

BLE y Wi-Fi comparten recursos de radio en ESP32. El firmware debe priorizar BLE
para control y Wi-Fi para transferencias, evitando trafico intensivo simultaneo.

### Reparto de responsabilidades

| Funcion | Reloj | App movil | Gateway/orquestador |
|---------|-------|-----------|----------------------|
| Wake word | Local en ESP32-S3 | Alternativa | No escucha continuamente |
| Captura de voz | ESP32-S3 | Alternativa/puente | Recibe audio o texto |
| STT completo | Solo si es viable | Local o cloud | Servicio principal |
| Razonamiento | No | No | Si |
| TTS | Reproduce/puede sintetizar frases basicas | Alternativa | Generacion principal |
| Alertas | Pantalla, voz y vibracion | Push y configuracion | Decide contenido/prioridad |
| Credenciales cloud | Nunca | Tokens limitados | IAM/secret manager |

## AURA Desktop

AURA Desktop sera un robot de escritorio y una presencia ambiental mas rica.
Una referencia directa es
[`Mac AI Buddy`](https://github.com/AyhanSh/Mac-AI-Buddy), especialmente por su
separacion entre cuerpo ESP32 y cerebro externo, rostro expresivo, camara y cabeza
pan/tilt. AURA reutilizara ese patron dentro de su arquitectura distribuida.

### Capacidades previstas

- rostro animado e indicadores de estado;
- arreglo de microfonos, altavoz, wake word y conversacion full-duplex;
- camara con señal visible de actividad y controles fisicos de privacidad;
- deteccion de presencia, objetos, personas y eventos autorizados;
- movimiento pan-tilt para orientar rostro o camara;
- expresiones y gestos sincronizados con la conversacion;
- control manual local para diagnostico y recuperacion;
- interaccion con servicios, domotica y dispositivos autorizados;
- futura base movil o actuadores adicionales, sujetos a limites de seguridad.

El robot usa la misma voz, memoria, personalidad y permisos que AURA Watch. Una
conversacion puede comenzar fuera de casa en el reloj y continuar en el escritorio.

## Voz distribuida

Ambos nodos tendran reconocimiento de voz y una wake word local como "Aura" u
"Oye Aura". El audio no se transmite continuamente: la deteccion local activa una
sesion y muestra un indicador claro de escucha.

### Flujo basico

```text
wake word local
    → nodo anuncia wake_detected
    → coordinador elige el nodo principal
    → captura de voz y STT
    → orquestador razona y usa herramientas
    → TTS en streaming
    → respuesta en el nodo elegido
```

Cuando varios nodos oyen la misma wake word, el gateway selecciona uno usando:

- proximidad o intensidad de voz;
- confianza de deteccion;
- nodo usado mas recientemente;
- disponibilidad de pantalla, audio y red;
- preferencias de habitacion y modo no molestar.

Los nodos no seleccionados permanecen en silencio y pueden mostrar que AURA esta
respondiendo en otro dispositivo.

## Proactividad

AURA no solo responde: puede iniciar interacciones cuando existe una razon util.

Ejemplos para el reloj:

- "Tu reunion comienza en 12 minutos";
- "Si no sales pronto, llegaras tarde";
- "Recibiste un mensaje marcado como importante";
- "Parece que va a llover";
- "Tu telefono tiene poca bateria";
- "Prepare una respuesta. ¿Quieres que la envie?";

Cada aviso debe considerar urgencia, contexto, hora, ubicacion, dispositivo activo,
modo no molestar y frecuencia reciente para evitar convertirse en ruido.

## Niveles de autonomia

La autonomia se asigna por accion, herramienta, contacto y contexto; nunca como un
permiso universal.

| Nivel | Comportamiento | Ejemplo |
|-------|----------------|---------|
| 1. Informar | Avisa sin ejecutar una accion externa | Recordar una reunion |
| 2. Sugerir | Propone y espera una decision | Recomendar salir antes |
| 3. Preparar | Deja la accion lista para aprobar | Redactar un WhatsApp |
| 4. Actuar | Ejecuta reglas autorizadas previamente | Encender una luz |
| 5. Critico | Exige confirmacion fuerte o regla especial | Comprar, abrir, transferir |

El usuario debe poder inspeccionar, revocar y limitar permisos. Las acciones externas
deben generar un registro auditable con origen, herramienta, resultado y nodo.

## Modelos y canales

El orquestador actual puede ser Hermes, pero se considera reemplazable por OpenClaw
u otra solucion. AURA Gateway ofrece un contrato estable a los nodos.

- **Claude** se consume mediante Amazon Bedrock.
- **Gemini** se consume mediante Google AI o Vertex AI como proveedor alternativo.
- WhatsApp, Telegram, Discord, email y futuras integraciones usan adaptadores de
  canal y comparten identidad de sesion cuando sea apropiado.

El reloj y el robot nunca almacenan credenciales maestras de AWS, Google o canales.
Solo reciben credenciales de dispositivo limitadas, rotables y revocables.

## Principios de seguridad y privacidad

- Wake word local; no enviar audio antes de la activacion.
- Indicador visible cuando microfono o camara estan activos.
- Interruptor o control fisico para desactivar sensores sensibles.
- TLS para trafico por Internet y emparejamiento autenticado por BLE.
- Credenciales diferentes para cada nodo y posibilidad de revocarlas.
- Confirmacion proporcional al impacto de cada accion.
- Minimizacion de audio, imagenes y ubicacion almacenados.
- Modo no molestar y politicas configurables de proactividad.
- Parada segura para movimiento y actuadores del robot.

## Decisiones abiertas

- pinout y revision exactos de la placa recibida;
- microfono, altavoz, vibrador, bateria y autonomia real;
- implementación de la app Android nativa propuesta y versiones soportadas;
- wake word engine que funcione con consumo aceptable;
- ubicacion de STT/TTS segun bateria, privacidad y latencia;
- protocolo del gateway para control, eventos y audio;
- orquestador definitivo: Hermes, OpenClaw u otro;
- sensores, grados de movimiento y forma de AURA Desktop.

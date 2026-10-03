# A.U.R.A. — Adaptive Unified Reasoning Assistant

> Una inteligencia artificial personal con memoria, voz, vision y presencia fisica,
> diseñada para comprender el contexto, razonar contigo y adaptarse a tu vida.

![Status](https://img.shields.io/badge/status-en%20desarrollo-7c3aed)
![Phase](https://img.shields.io/badge/fase-firmware%20AURA%20Watch-2563eb)
![Focus](https://img.shields.io/badge/enfoque-local--first-059669)

> **Prioridad actual de AURA Watch:** consolidar un firmware operativo y funcional,
> con una interfaz fluida, herramientas útiles y un consumo de batería controlado.
> **El siguiente paso es integrar IA** sobre esa base.

## Que es AURA

**A.U.R.A.** significa **Adaptive Unified Reasoning Assistant** — en español,
**Asistente Unificado de Razonamiento Adaptativo**.

AURA es una inteligencia personal distribuida que no vive solamente dentro de una
ventana de chat. Comparte identidad, memoria y contexto entre dispositivos fisicos
y canales digitales. Su primera presencia portatil sera **AURA Watch**, montado
sobre una **Waveshare ESP32-S3-Touch-AMOLED-2.06**; en una fase posterior,
**AURA Desktop** le dara un cuerpo robotico con vision y movimiento.

No busca ser solo otro chatbot. AURA aspira a convertirse en una presencia digital
continua: contextual, multimodal, extensible y bajo el control de su usuario.

## Vision

AURA deberia poder:

- conversar por voz de forma natural y con baja latencia;
- despertar localmente al escuchar "Aura" u otra wake word configurable;
- mantener una identidad y personalidad coherentes;
- recordar preferencias, decisiones, personas y acontecimientos importantes;
- comprender el entorno mediante camaras y otros sensores;
- comunicarse por WhatsApp, Telegram, Discord, correo y telefono;
- ayudar con tareas cotidianas y automatizaciones;
- informar, recordar y avisar de forma proactiva desde el reloj o el robot;
- mostrar expresiones mediante una cara LED o pantalla;
- controlar hardware y, eventualmente, una plataforma fisica;
- continuar funcionando de forma degradada cuando un servicio externo falle;
- proteger la privacidad y mantener los secretos fuera del repositorio.

## Principios del proyecto

1. **Local-first:** procesar localmente todo lo que sea razonable y usar la nube
   cuando aporte una ventaja clara.
2. **Memoria con criterio:** recordar informacion util sin almacenar secretos ni
   convertir cada conversacion en memoria permanente.
3. **Modularidad:** separar razonamiento, memoria, percepcion, voz, comunicaciones
   y hardware para poder evolucionarlos de manera independiente.
4. **Failover:** diseñar alternativas para que una caida de red, modelo o servicio
   no deje a AURA completamente inutilizable.
5. **Privacidad y control:** el usuario decide que se guarda, donde se guarda y
   cuando se elimina.
6. **Progreso incremental:** validar primero prototipos pequeños antes de construir
   una plataforma fisica compleja.

## Arquitectura conceptual

```text
 ┌─────────────┐   ┌──────────────┐   ┌─────────────────────────┐
 │ AURA Watch  │   │ AURA Desktop │   │ WhatsApp/Telegram/      │
 │ ESP32-S3    │   │ robot, camara│   │ Discord/app movil       │
 │ Wi-Fi y voz │   │ movimiento   │   │                         │
 └──────┬──────┘   └──────┬───────┘   └───────────┬─────────────┘
        └─────────────────┼────────────────────────┘
                          ▼
                 ┌──────────────────┐
                 │ AURA Gateway API │
                 └────────┬─────────┘
                          ▼
             ┌─────────────────────────┐
             │ Orquestador reemplazable│
             │ Hermes/OpenClaw/futuro  │
             └────────┬────────┬───────┘
                      │        │
              ┌───────▼───┐ ┌──▼───────────┐
              │ Modelos AI│ │ aura-memory  │
              │Claude/Gem.│ │ contexto     │
              └───────────┘ └──────────────┘
```

La arquitectura definitiva todavia esta en investigacion. Los documentos del
repositorio registran las alternativas consideradas y las decisiones a medida que
se validan.

### Decisiones acordadas — 2026-10-03

La primera integración tendrá un **AURA Gateway propio y pequeño** para la app y
el reloj, con adaptadores que permitan cambiar el motor sin modificar su protocolo.
**Hermes será el primer motor**, conectado mediante API HTTP y streaming SSE.
**OpenClaw será una alternativa**, mediante su protocolo WebSocket. Su compatibilidad
se validará antes de habilitarlo; cada conversación tendrá un motor responsable.

```text
Watch / app → AURA Gateway
                 ├─ Personal → Hermes + Gemini API
                 └─ Trabajo  → Kiro CLI + especialistas existentes
```

- **Consumo separado:** Gemini personal, créditos de la suscripción laboral Kiro
  y recursos AWS se contabilizan por separado. El trabajo se dirige a Kiro sin
  llamadas a Gemini para planificar o resumir por defecto.
- **Datos separados:** memoria, sesiones, credenciales, herramientas y almacenamiento
  independientes para personal y trabajo.
- **Infraestructura:** servicios contenerizados en EC2, único nodo activo normal.
  El notebook de respaldo estará apagado y se encenderá para una recuperación manual.
- **Respaldo:** Git privado para instrucciones y memoria curada; backups cifrados
  externos a EC2 para bases de datos, sesiones y tareas. La restauración y el retorno
  a EC2 mantendrán un solo despliegue activo.
- **Especialistas:** reutilizar primero un agente local de Kiro IDE mediante CLI.
  Paperclip se evaluará después para coordinar proyectos y varios agentes.

Estas son **decisiones de diseño, pendientes de implementación**. El siguiente paso
es validar un agente Kiro con la suscripción laboral y Hermes con Gemini; después,
implementar el gateway mínimo por texto y conectar progresivamente el reloj.

Ver [gateway, APIs, agentes y consumo](research/09-gateway-agentes-paperclip-kiro.md),
[recuperación manual](research/07-arquitectura-failover.md) y
[estado actual e histórico](research/08-estado-actual.md).

## Estado actual

### AURA Watch — firmware funcional antes de integrar IA

Estamos desarrollando y probando primero las funciones del reloj: pantalla,
touch, hora, menús, sensores, conectividad y gestión de energía. La versión
**AURA Watch Basic 1.4 dev.8** ya está compilada y cargada en la placa. Incluye
reloj, ojos interactivos, temporizador, cronómetro, ajustes y las herramientas
de redes e ingeniería de **AURA Eclipse 0.1**. Los menús siempre comienzan arriba;
entrar a Eclipse muestra una introducción con la versión y llega al inicio al
pulsar **OK** o tras cuatro segundos en la confirmación.
El triple toque bloquea solamente desde la pantalla principal.

Seguimos afinando la fluidez y la estabilidad del firmware. El descanso y el
apagado de Wi-Fi están verificados; la autonomía y la respuesta táctil necesitan
pruebas de uso físico. Este avance prepara una base útil para la siguiente etapa.

**El siguiente hito es integrar IA en AURA Watch:** conectar el reloj con el
gateway de AURA e incorporar conversación, voz y memoria de forma gradual.
La integración de IA, el audio y el enlace BLE con la app están pendientes.

### App Android y conexión del reloj

La app propia de AURA (pendiente de desarrollo) permitirá asociar el reloj como un
smartwatch: vincular una vez, reconectar al acercarse y recibir avisos sin abrirla
en cada interacción. Se propone Kotlin + Jetpack Compose y las APIs de dispositivos
asociados de Android.

Ruta habitual: **Watch → BLE → app Android → Internet → AURA Gateway en EC2**.
Como segunda ruta, el Watch podrá conectarse por Wi-Fi directamente al gateway.
La voz por BLE se probará inicialmente por turnos con audio comprimido; la charla
continua y la autonomía de batería requieren mediciones en la placa.

Ver [app Android, conexión y plan de pruebas](docs/14-app-android-conectividad.md).

### Progreso documentado

El proyecto combina investigación y diseño de arquitectura con el desarrollo de
un **firmware funcional para AURA Watch**, como preparación para integrar IA.

Actualmente hay:

- una vision general del producto y sus capacidades;
- investigacion sobre voz, vision, comunicaciones y presencia fisica;
- propuestas de arquitectura y estrategia de failover;
- evaluacion de infraestructura y costos en AWS;
- diseño conceptual del sistema de memoria;
- una lista de compras organizada por fases;
- un primer prototipo de firmware para NodeMCU ESP8266 con interfaz web y LCD I2C.
- una arquitectura definida para AURA Watch, AURA Desktop y coordinacion de voz.
- la placa AURA Watch recibida, con pantalla y touch confirmados por el usuario;
- el firmware **Basic 1.4 dev.8**, cargado por USB, con menú de dos columnas,
  Wi-Fi/NTP, personalidad mediante IMU, triple toque para bloquear desde el inicio y descanso
  que pausa el dibujo de pantalla y reduce la frecuencia de CPU;
- **Eclipse 0.1**, accesible con siete toques en la versión, con ocho herramientas:
  Spectrum, canales Wi-Fi, auditoría de autenticación, System Check, CIDR, RF,
  VLSM y Evidence. La entrada incluye mensajes y protección frente a taps sobrantes.
  El modo se conserva al volver al inicio o descansar;
- optimización del renderizado de los menús y pruebas de navegación, gestos,
  herramientas, persistencia y descanso. Las mediciones y sus límites están en la
  [auditoría de dev.6](firmware/aura_watch/AUDIT-2026-10-03.md) y la
  [corrección de entrada en dev.7](firmware/aura_watch/AUDIT-2026-10-03-dev7.md) y la
  [confirmación con OK en dev.8](firmware/aura_watch/AUDIT-2026-10-03-dev8.md);
- un registro interno de ocho eventos para Evidence, verificado y exportable
  por USB. La microSD conectada usa NTFS; FAT32/exFAT se probaron con imágenes
  sintéticas. El historial está en los
  [antecedentes del firmware](firmware/aura_watch/AUDIT-2026-10-02.md).

## Roadmap

El roadmap es orientativo y cambiara a medida que los prototipos revelen nuevas
restricciones.

**Estados:** ✅ **Hecho** · 🟡 **En progreso** · ⬜ **Pendiente**.

### Fase 0 — Fundamentos y diseño `en progreso`

- ✅ **Hecho** — Definir la vision general de AURA.
- ✅ **Hecho** — Adoptar el nombre **Adaptive Unified Reasoning Assistant**.
- ✅ **Hecho** — Investigar proyectos, tecnologias y stacks posibles.
- ✅ **Hecho** — Separar la memoria de AURA en su propio repositorio.
- ⬜ **Pendiente** — Consolidar requisitos y decisiones en una arquitectura v1.
- ⬜ **Pendiente** — Definir criterios de privacidad, permisos y retencion de datos.

### Fase 1 — Nucleo, memoria y canales

- ⬜ **Pendiente** — Crear el orquestador principal.
- ⬜ **Pendiente** — Integrar entrada y salida de voz en streaming.
- ⬜ **Pendiente** — Implementar deteccion de palabra de activacion e interrupciones.
- ⬜ **Pendiente** — Conectar uno o mas modelos con failover local/cloud.
- ⬜ **Pendiente** — Definir personalidad, instrucciones y limites de AURA.
- ⬜ **Pendiente** — Integrar lectura y escritura controlada con `aura-memory`.
- ⬜ **Pendiente** — Unificar Telegram, Discord y WhatsApp mediante adaptadores de canal.
- ⬜ **Pendiente** — Crear AURA Gateway API para desacoplar dispositivos y orquestador.

### Fase 2 — AURA Watch + aplicación móvil `en progreso`

#### Base de firmware — prioridad actual

- ✅ **Hecho** — Adquirir la Waveshare ESP32-S3-Touch-AMOLED-2.06.
- ✅ **Hecho** — Definir pantalla AMOLED táctil, IMU, RTC, audio, microSD y gestión de energía.
- ✅ **Hecho** — Recibir la placa y comprobar su funcionamiento inicial.
- ✅ **Hecho** — Compilar y cargar un firmware operativo con reloj, ojos, menús, temporizador y cronómetro.
- ✅ **Hecho** — Configurar Wi-Fi y sincronizar la hora por NTP, apagando la radio al terminar.
- ✅ **Hecho** — Incorporar Eclipse con herramientas de redes, calculadoras y registro Evidence.
- ✅ **Hecho** — Implementar bloqueo por triple toque en la pantalla principal y descanso con reducción de CPU y pausa del dibujo.
- 🟡 **En progreso** — Afinar la fluidez, probar el uso cotidiano y medir la autonomía.
- 🟡 **En progreso** — Completar la verificación de revisión, pinout y periféricos reales.

#### Integración de IA — siguiente hito

- ⬜ **Pendiente** — Conectar AURA Watch con el gateway e integrar IA, conversación y memoria.
- ⬜ **Pendiente** — Integrar micrófono, altavoz y vibración.
- ⬜ **Pendiente** — Incorporar avisos y recordatorios del asistente en la interfaz del reloj.
- ⬜ **Pendiente** — Implementar BLE para emparejamiento, configuración y enlace con la app.
- ⬜ **Pendiente** — Ampliar Wi-Fi para gateway directo, audio y actualizaciones OTA.
- ⬜ **Pendiente** — Detectar la palabra de activación localmente y activar la conversación.
- ⬜ **Pendiente** — Crear la app móvil como puente BLE/Internet y superficie de permisos.
- ⬜ **Pendiente** — Validar asociación Android, reconexión y recepción con pantalla bloqueada.
- ⬜ **Pendiente** — Integrar el gateway de dispositivos con Hermes en la EC2 existente.
- ⬜ **Pendiente** — Probar audio comprimido por BLE y cambio de ruta sin mensajes duplicados.

### Fase 3 — Voz distribuida y proactividad

- ⬜ **Pendiente** — Compartir conversaciones y contexto entre reloj, app, robot y canales.
- ⬜ **Pendiente** — Coordinar que solo el nodo mas apropiado responda a cada wake word.
- ⬜ **Pendiente** — Añadir interrupcion de voz, prioridades y modo no molestar.
- ⬜ **Pendiente** — Implementar avisos proactivos por voz, pantalla y vibracion.
- ⬜ **Pendiente** — Aplicar niveles de autonomia segun el riesgo de cada accion.

### Fase 4 — AURA Desktop

- ⬜ **Pendiente** — Construir rostro expresivo, microfonos, altavoz y sensores de presencia.
- ⬜ **Pendiente** — Integrar camara con indicadores y controles visibles de privacidad.
- ⬜ **Pendiente** — Añadir pan-tilt, seguimiento visual y movimientos expresivos.
- ⬜ **Pendiente** — Compartir la misma identidad, memoria y voz de AURA Watch.
- ⬜ **Pendiente** — Interactuar con dispositivos y objetos autorizados del entorno.

### Fase 5 — Autonomia y entorno fisico

- ⬜ **Pendiente** — Consolidar el bus de comunicacion con microcontroladores.
- ⬜ **Pendiente** — Añadir sensores y actuadores de forma incremental.
- ⬜ **Pendiente** — Diseñar alimentacion, conectividad y carcasa.
- ⬜ **Pendiente** — Evaluar movilidad, seguridad fisica y parada de emergencia.
- ⬜ **Pendiente** — Construir un prototipo integrado de AURA.
- ⬜ **Pendiente** — Integrar hogar inteligente y automatizaciones contextuales.
- ⬜ **Pendiente** — Evaluar base movil o actuadores adicionales con limites de seguridad.

## Estructura del repositorio

```text
.
├── docs/       # Vision, diseño funcional, infraestructura y planes
├── research/   # Investigacion, comparativas, recursos y decisiones tecnicas
├── firmware/   # Prototipos para microcontroladores y hardware
├── core/       # Orquestador principal (planificado)
└── services/   # Integraciones y microservicios (planificado)
```

Los directorios planificados apareceran cuando comience su implementacion.

La arquitectura de nodos, voz y autonomia se detalla en
[`docs/12-arquitectura-nodos-aura.md`](docs/12-arquitectura-nodos-aura.md).
La ficha de la placa adquirida esta en
[`docs/13-aura-watch-hardware.md`](docs/13-aura-watch-hardware.md).

## Repositorios relacionados

- [`aura-memory`](https://github.com/lucx123/aura-memory): memoria persistente de
  AURA, separada del codigo y de la documentacion de desarrollo.
- **aura** (este repositorio): fuente principal para investigacion, decisiones,
  prototipos, firmware y progreso general del proyecto.

Esta separacion permite versionar la evolucion tecnica sin mezclarla con los datos
que AURA aprende o recuerda durante su uso.

## Primer prototipo de hardware

El prototipo disponible en `firmware/nodemcu_wifi_test` utiliza un NodeMCU V3 con
ESP8266. Crea una red Wi-Fi local, expone una interfaz web de control y puede mostrar
el ultimo comando en una pantalla LCD 1602 I2C.

La contraseña usada por este prototipo era solamente una credencial de prueba para
la red local creada por el ESP8266; no forma parte del diseño de autenticacion de
AURA Watch. Actualmente se mantiene en `include/wifi_secrets.h`, fuera de Git, y
se incluye una plantilla para poder reproducir la prueba sin versionar contraseñas.

## Seguridad y secretos

Nunca deben subirse al repositorio:

- claves de API, tokens o contraseñas;
- archivos `.env` reales;
- llaves SSH, certificados privados o credenciales cloud;
- datos personales almacenados por AURA;
- grabaciones o imagenes privadas sin una decision explicita de versionarlas.

Usa archivos de ejemplo para documentar la configuracion y conserva los valores
reales en variables de entorno o en un gestor de secretos.

## Como contribuir por ahora

El proyecto esta en una fase temprana y experimental. Las contribuciones utiles
incluyen investigacion, propuestas de arquitectura, pruebas de hardware, deteccion
de riesgos y prototipos pequeños que validen una sola idea con claridad.

Antes de implementar un componente grande, documenta el problema, las alternativas
y el criterio con el que se elegira una solucion.

## Licencia

Todavia no se ha definido una licencia. Hasta que exista un archivo `LICENSE`, el
contenido conserva todos los derechos de su autor y no debe asumirse como software
de codigo abierto.

---

**AURA no es solo una IA que responde. Es una IA que recuerda, percibe, razona y
crece junto a su usuario.**

# A.U.R.A. — Adaptive Unified Reasoning Assistant

> Una inteligencia artificial personal con memoria, voz, vision y presencia fisica,
> diseñada para comprender el contexto, razonar contigo y adaptarse a tu vida.

![Status](https://img.shields.io/badge/status-en%20desarrollo-7c3aed)
![Phase](https://img.shields.io/badge/fase-investigacion%20y%20prototipos-2563eb)
![Focus](https://img.shields.io/badge/enfoque-local--first-059669)

## Que es AURA

**A.U.R.A.** significa **Adaptive Unified Reasoning Assistant** — en español,
**Asistente Unificado de Razonamiento Adaptativo**.

AURA es un proyecto para construir un asistente personal tipo Jarvis que no viva
solamente dentro de una ventana de chat. La meta es que pueda escuchar, hablar,
ver su entorno, recordar informacion relevante, comunicarse mediante distintos
canales y, con el tiempo, interactuar con el mundo fisico.

No busca ser solo otro chatbot. AURA aspira a convertirse en una presencia digital
continua: contextual, multimodal, extensible y bajo el control de su usuario.

## Vision

AURA deberia poder:

- conversar por voz de forma natural y con baja latencia;
- mantener una identidad y personalidad coherentes;
- recordar preferencias, decisiones, personas y acontecimientos importantes;
- comprender el entorno mediante camaras y otros sensores;
- comunicarse por WhatsApp, correo y telefono;
- ayudar con tareas cotidianas y automatizaciones;
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
                  ┌──────────────────────────┐
                  │         Usuario          │
                  └────────────┬─────────────┘
                               │
             ┌─────────────────┼─────────────────┐
             │                 │                 │
          Voz/audio        Vision/sensores   Mensajeria
             │                 │                 │
             └─────────────────┼─────────────────┘
                               ▼
                  ┌──────────────────────────┐
                  │   Orquestador de AURA    │
                  │ contexto · herramientas  │
                  │ decisiones · seguridad   │
                  └───────┬──────────┬───────┘
                          │          │
                  ┌───────▼───┐  ┌───▼──────────┐
                  │ Modelos AI│  │ aura-memory  │
                  │local/cloud│  │ memoria      │
                  └───────┬───┘  └──────────────┘
                          │
                  ┌───────▼──────────────────┐
                  │ Servicios y mundo fisico │
                  │ APIs · ESP8266 · displays│
                  └──────────────────────────┘
```

La arquitectura definitiva todavia esta en investigacion. Los documentos del
repositorio registran las alternativas consideradas y las decisiones a medida que
se validan.

## Estado actual

El proyecto se encuentra en la etapa de **investigacion, definicion de arquitectura
y prototipos iniciales**.

Actualmente hay:

- una vision general del producto y sus capacidades;
- investigacion sobre voz, vision, comunicaciones y presencia fisica;
- propuestas de arquitectura y estrategia de failover;
- evaluacion de infraestructura y costos en AWS;
- diseño conceptual del sistema de memoria;
- una lista de compras organizada por fases;
- un primer prototipo de firmware para NodeMCU ESP8266 con interfaz web y LCD I2C.

## Roadmap

El roadmap es orientativo y cambiara a medida que los prototipos revelen nuevas
restricciones.

### Fase 0 — Fundamentos y diseño `en progreso`

- [x] Definir la vision general de AURA.
- [x] Adoptar el nombre **Adaptive Unified Reasoning Assistant**.
- [x] Investigar proyectos, tecnologias y stacks posibles.
- [x] Separar la memoria de AURA en su propio repositorio.
- [ ] Consolidar requisitos y decisiones en una arquitectura v1.
- [ ] Definir criterios de privacidad, permisos y retencion de datos.

### Fase 1 — Nucleo conversacional

- [ ] Crear el orquestador principal.
- [ ] Integrar entrada y salida de voz en streaming.
- [ ] Implementar deteccion de palabra de activacion e interrupciones.
- [ ] Conectar uno o mas modelos con failover local/cloud.
- [ ] Definir personalidad, instrucciones y limites de AURA.
- [ ] Integrar lectura y escritura controlada con `aura-memory`.

### Fase 2 — Presencia visual

- [ ] Diseñar la cara o interfaz visual de AURA.
- [ ] Sincronizar expresiones con voz, estado y emociones simuladas.
- [ ] Conectar una pantalla o matriz LED al nucleo.
- [ ] Crear estados visuales para escucha, pensamiento, respuesta y error.

### Fase 3 — Vision y contexto ambiental

- [ ] Integrar una camara de prueba.
- [ ] Detectar objetos, personas y eventos relevantes.
- [ ] Aplicar reglas de privacidad y activacion visible de la camara.
- [ ] Incorporar contexto visual sin almacenar video innecesario.

### Fase 4 — Comunicaciones y acciones

- [ ] Integrar WhatsApp y correo electronico.
- [ ] Experimentar con llamadas telefonicas y modo secretaria.
- [ ] Implementar herramientas, permisos y confirmaciones por nivel de riesgo.
- [ ] Añadir automatizaciones y notificaciones proactivas.

### Fase 5 — Cuerpo y autonomia fisica

- [ ] Consolidar el bus de comunicacion con microcontroladores.
- [ ] Añadir sensores y actuadores de forma incremental.
- [ ] Diseñar alimentacion, conectividad y carcasa.
- [ ] Evaluar movilidad, seguridad fisica y parada de emergencia.
- [ ] Construir un prototipo integrado de AURA.

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

Las credenciales del punto de acceso se configuran localmente mediante
`include/wifi_secrets.h`. Ese archivo esta excluido de Git; el repositorio solo
incluye una plantilla segura.

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

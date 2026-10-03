# Estado de AURA — actualizado 2026-10-03

## Decisiones actuales y próximos pasos

Diseño acordado, todavía sin despliegue de estos cambios:

- AURA Gateway propio para app/reloj, con identidad, sesiones, permisos y eventos.
- Hermes primero mediante API HTTP/SSE, con Gemini para uso personal.
- Adaptador OpenClaw por WebSocket como alternativa, pendiente de validación.
- Worker Kiro CLI independiente para agentes laborales existentes del IDE,
  usando la suscripción laboral y verificando identidad y consumo.
- Memoria, sesiones, credenciales y almacenamiento separados por dominio.
- Docker Compose en EC2 como único despliegue activo normal.
- Notebook apagado como respaldo; recuperación manual, sin watchdog ni
  sincronización continua. Backups fuera de EC2 y retorno manual con estado actualizado.
- Git privado para instrucciones/memoria curada y backups cifrados para estado operativo.
- Paperclip en una segunda etapa para coordinación de proyectos.
- Tres consumos separados: AWS (aprox. US$30 mensuales en créditos reportados),
  Gemini personal y suscripción laboral Kiro.

Primeras validaciones: inventario de agentes Kiro y dependencias; tarea de lectura
con CLI y medición de créditos; conversación Hermes/Gemini; gateway mínimo por texto;
prueba de separación de dominios y restauración en el notebook; integración del Watch.

Ver [estudio y contrato de integración](09-gateway-agentes-paperclip-kiro.md)
y [recuperación manual](07-arquitectura-failover.md).

## Registro histórico — 2026-08-25

La información siguiente se conserva como antecedente de esa fecha. El estado de
EC2, versiones, credenciales y costos no se volvió a verificar en esta revisión.
Las decisiones anteriores que contradigan las actuales quedan reemplazadas.

## Infraestructura Activa

| Componente | Estado | Detalle |
|-----------|--------|---------|
| EC2 t3.small | Running | IP: 18.204.38.227, us-east-1 |
| Hermes Agent v0.20.5 | Running | Gateway como systemd service con linger |
| Claude Bedrock (Haiku 4.5) | Activo | Via IAM role, sin API keys |
| Telegram Bot | Conectado | Solo responde a Lucx |
| Discord Bot | Conectado | Canal free-response, sin threads |
| Memory Git Sync | Configurado | Cron 6AM/6PM Santiago → github.com/lucx123/aura-memory |
| SOUL.md (personalidad) | Activo | Aura: elegante, tecnica, trilingue |
| Elastic IP | Asignada | 18.204.38.227 (fija) |

## Pendiente

### Prioridad Alta
- [ ] Configurar skill de WhatsApp (cuando tenga telefono + SIM)
- [ ] Recuperación manual en notebook (reemplaza la propuesta histórica de watchdog)
- [ ] Probar memoria a largo plazo (que Aura recuerde cosas entre sesiones)

### Prioridad Media
- [ ] Voice pipeline (RealtimeSTT + TTS) — hablarle por voz
- [ ] ESP32 face display (hardware)
- [ ] Patter SDK para llamadas telefonicas
- [ ] Home Assistant integration

### Prioridad Baja
- [ ] Multi-room presence (multiples ESP32)
- [ ] Email triage automatico
- [ ] Morning briefing proactivo
- [ ] Wake word custom "Aura"

## Costos Actuales

| Servicio | Costo/mes |
|----------|-----------|
| EC2 t3.small | ~$15 |
| Elastic IP | $0 (asociada) |
| Bedrock Claude (uso actual) | ~$5-10 |
| **Total actual** | **~$20-25/mes** |
| Creditos restantes (~$200) | **~8 meses de runway** |

## Accesos

- SSH: `ssh -i ~/.ssh/aura-key.pem ubuntu@18.204.38.227`
- Hermes CLI: `hermes` (en la EC2)
- Gateway status: `hermes gateway status`
- Memory repo: github.com/lucx123/aura-memory

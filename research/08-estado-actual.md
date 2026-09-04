# Estado Actual de AURA - 2026-08-25

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
- [ ] Watchdog en laptop para failover
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

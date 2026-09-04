# Arquitectura Failover: EC2 + Laptop

## Concepto

EC2 es el nodo primario (24/7). Laptop es nodo secundario (failover).
Memoria sincronizada via Git. Si EC2 cae, laptop toma el control automaticamente.

---

## Arquitectura

```
                    ┌─────────────┐
                    │  GitHub Repo │
                    │  (memoria)   │
                    └──────┬──────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
              ▼            │            ▼
┌──────────────────┐       │  ┌──────────────────┐
│   EC2 (PRIMARY)  │       │  │ Laptop (STANDBY) │
│                  │       │  │                  │
│  Hermes Agent    │       │  │  Hermes Agent    │
│  Puerto 8000     │       │  │  Puerto 8000     │
│                  │       │  │                  │
│  Health: /health │       │  │  Monitorea EC2   │
│                  │       │  │  cada 60s        │
│  git push cada   │       │  │                  │
│  15 min          │       │  │  Si EC2 muere:   │
│                  │       │  │  → git pull      │
│                  │       │  │  → hermes start  │
│                  │       │  │  → toma canales  │
└──────────────────┘       │  └──────────────────┘
                           │
                    ┌──────┴──────┐
                    │   Canales   │
                    │  WhatsApp   │
                    │  Telegram   │
                    │  Email      │
                    └─────────────┘
```

---

## Flujo de Failover

### Estado normal (EC2 activo):
1. EC2 corre Hermes, atiende todos los canales
2. Cada 15 min: git push de memoria
3. Laptop (si esta prendida): monitorea EC2 con health check cada 60s
4. Laptop: git pull cada 15 min para mantener memoria al dia

### Fallo detectado:
1. Laptop hace GET a `http://18.204.38.227:8000/health`
2. Si falla 3 veces consecutivas (3 min sin respuesta):
   - `git pull` para traer ultima memoria
   - Inicia Hermes local
   - Reconecta canales (WhatsApp, Telegram)
   - Envia alerta por Telegram admin: "EC2 caida, laptop tomando control"

### Recuperacion:
1. EC2 vuelve online
2. Laptop detecta que EC2 responde health check
3. Laptop hace `git push` de cualquier memoria nueva
4. Laptop apaga su Hermes local
5. EC2 hace `git pull` y retoma canales
6. Alerta Telegram: "EC2 recuperada, control devuelto"

---

## Script de Monitor (laptop)

```python
#!/usr/bin/env python3
"""
aura_watchdog.py - Corre en la laptop como servicio/tarea programada.
Monitorea EC2 y toma control si cae.
"""
import time
import subprocess
import httpx
from pathlib import Path

EC2_URL = "http://18.204.38.227:8000/health"
CHECK_INTERVAL = 60  # segundos
FAIL_THRESHOLD = 3   # fallos consecutivos antes de failover
MEMORY_REPO = Path.home() / "aura-memory"
HERMES_CMD = "hermes gateway start"

consecutive_failures = 0
is_primary = False  # laptop es primary?


def check_ec2():
    try:
        r = httpx.get(EC2_URL, timeout=10)
        return r.status_code == 200
    except Exception:
        return False


def git_sync(direction="pull"):
    subprocess.run(["git", direction], cwd=MEMORY_REPO, capture_output=True)


def start_hermes_local():
    global is_primary
    git_sync("pull")
    subprocess.Popen(HERMES_CMD, shell=True)
    is_primary = True
    notify("EC2 caida. Laptop tomando control de AURA.")


def stop_hermes_local():
    global is_primary
    git_sync("push")
    subprocess.run(["hermes", "gateway", "stop"], capture_output=True)
    is_primary = False
    notify("EC2 recuperada. Devolviendo control.")


def notify(msg):
    # Enviar por Telegram bot
    pass


def main():
    global consecutive_failures
    while True:
        if check_ec2():
            consecutive_failures = 0
            if is_primary:
                stop_hermes_local()
        else:
            consecutive_failures += 1
            if consecutive_failures >= FAIL_THRESHOLD and not is_primary:
                start_hermes_local()
        
        time.sleep(CHECK_INTERVAL)


if __name__ == "__main__":
    main()
```

---

## Sync de Memoria - Detalle

### Que se sincroniza:
```
~/.hermes/
├── memory/          → Git (SIEMPRE sync)
│   ├── USER.md      # Perfil del usuario
│   ├── MEMORY.md    # Memorias aprendidas
│   └── sessions/    # Resumenes de sesiones
├── skills/          → Git (SIEMPRE sync)
│   └── *.py         # Skills auto-generados
├── config/          → Git (solo configs no-sensibles)
│   ├── SOUL.md      # Personalidad AURA
│   └── tools.yaml   # Tools habilitados
└── secrets/         → NO sync (API keys, tokens)
    └── .env
```

### Cron en EC2:
```bash
# /etc/cron.d/aura-memory-sync
*/15 * * * * ubuntu cd /home/ubuntu/.hermes && git add memory/ skills/ config/ && git commit -m "sync $(date +\%H:\%M)" --allow-empty -q && git push -q 2>/dev/null
```

### En laptop (Windows Task Scheduler o cron WSL):
```bash
# Cada 15 min cuando esta prendida
cd ~/aura-memory && git pull -q
```

---

## Consideraciones

### WhatsApp (Baileys):
- Solo UNA sesion activa a la vez
- Cuando laptop toma control, reconecta Baileys con la misma sesion
- La sesion de WhatsApp Web se almacena en el telefono viejo, no en EC2/laptop
- Ambos nodos pueden conectarse al telefono viejo via Baileys

### Telegram Bot:
- Solo UN polling activo a la vez
- Webhook apunta a EC2. Si laptop toma control, cambiar webhook a Tailscale IP de laptop
- O usar polling (long-poll) que es mas simple para failover

### DNS/IP:
- Usar Tailscale para que ambos nodos tengan IP fija en red privada
- O un DNS dinamico que apunte al nodo activo

---

## Costos adicionales: $0

- Git repo privado: gratis (GitHub)
- Tailscale: gratis (hasta 100 dispositivos)
- Monitor en laptop: solo corre cuando esta prendida
- Sin servicios extra de AWS

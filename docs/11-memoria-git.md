# Sistema de Memoria con Git

## Concepto

Git como "disco duro" de la memoria de Aura. Todo conocimiento estructurado
vive en archivos versionados. ChromaDB complementa para busqueda semantica.

## Arquitectura hibrida

```
┌─────────────────────────────────────────────────────┐
│              MEMORIA DE AURA                         │
├─────────────────────┬───────────────────────────────┤
│   Git (repo privado)│   ChromaDB / VectorDB         │
│   Memoria "dura"    │   Memoria "blanda"            │
├─────────────────────┼───────────────────────────────┤
│ • Perfil del usuario│ • Conversaciones pasadas      │
│ • Contactos         │ • Contexto emocional          │
│ • Preferencias      │ • Busqueda semantica          │
│ • Rutinas           │ • "Que dije sobre X?"         │
│ • Conocimiento fijo │ • Asociaciones difusas        │
│ • Prompts de Aura   │ • Eventos con timestamp       │
│ • Skills/tools      │                               │
│ • Configuracion     │                               │
│ • Journal diario    │                               │
└─────────────────────┴───────────────────────────────┘
```

## Cuando usar cada uno

| Tipo de dato | Git | ChromaDB | Por que |
|--------------|-----|----------|---------|
| Nombre, cumpleaños | Git | - | Dato fijo, raramente cambia |
| "Le gusta el cafe negro" | Git | - | Preferencia estable |
| Conversacion de hace 3 dias | - | ChromaDB | Necesita busqueda semantica |
| Contactos y relaciones | Git | - | Estructurado, editable |
| "Algo que dijo sobre un restaurante" | - | ChromaDB | Difuso, no se la palabra exacta |
| Resumen del dia | Git | ChromaDB | Git para historial, ChromaDB para buscar |
| System prompt de Aura | Git | - | Versionado, rollback |
| Rutina diaria | Git | - | Estructurado |

## Estructura de archivos en el repo

```
memory/
├── user/
│   ├── profile.yaml              # datos basicos
│   ├── preferences.yaml          # gustos, disgustos
│   ├── routines.yaml             # horarios, habitos
│   ├── health.yaml               # info medica relevante
│   └── goals.yaml                # metas actuales
├── contacts/
│   ├── familia.yaml
│   ├── trabajo.yaml
│   └── amigos.yaml
├── knowledge/
│   ├── home.yaml                 # layout casa, ubicacion de cosas
│   ├── accounts.yaml             # servicios (NO passwords)
│   ├── projects.yaml             # proyectos activos
│   └── learned/                  # cosas puntuales aprendidas
│       ├── 2026-08-25_cafe-negro.md
│       └── 2026-08-30_alergia-x.md
├── journal/
│   ├── 2026-08/
│   │   ├── 2026-08-25.md
│   │   └── 2026-08-26.md
│   └── 2026-09/
│       └── ...
└── personality/
    ├── system-prompt.md          # prompt principal de Aura
    ├── voice-config.yaml         # configuracion de voz
    └── behavior-rules.yaml       # reglas de comportamiento
```

## Formato de archivos

### profile.yaml
```yaml
name: German Fagalde
nickname: German
birthday: YYYY-MM-DD
language: es
timezone: America/Santiago
occupation: Ingeniero Telecomunicaciones
workplace: Morris & Opazo (practicante)
studying: AWS Solutions Architect
interests:
  - IoT
  - Cloud (AWS)
  - AI/ML
  - Automatizacion
  - Electronica
```

### preferences.yaml
```yaml
coffee: negro, sin azucar
music_work: lo-fi, electronica
music_relax: rock, indie
communication_style: directo, informal
wake_up_time: "07:00"
sleep_time: "23:30"
food_likes:
  - sushi
  - pizza
food_dislikes:
  - (por descubrir)
pet_peeves:
  - respuestas largas innecesarias
  - que le pregunten cosas obvias
```

### contacts/familia.yaml
```yaml
contacts:
  - name: Mama
    real_name: (por completar)
    phone: "+56..."
    whatsapp: true
    relationship: madre
    notes: "Siempre responder sus mensajes rapido"
    priority: critical

  - name: Papa
    real_name: (por completar)
    relationship: padre
    priority: critical
```

### journal/2026-08-25.md
```markdown
# 2026-08-25 (Lunes)

## Resumen
- Trabajo desde casa
- Sesion larga de planificacion del proyecto AURA
- Investigo arquitectura de asistente AI personal

## Aprendido
- Quiere usar Bedrock con creditos AWS ($200)
- Tiene laptop vieja como nodo local
- Prioriza que Aura se sienta "viva" (cara, voz, vision)

## Interacciones
- 1 sesion larga de brainstorming sobre AURA

## Mood: entusiasmado, creativo
## Energia: alta
```

## Logica de escritura en memoria

### Cuando Aura aprende algo nuevo

```python
import yaml
import subprocess
from pathlib import Path
from datetime import datetime

MEMORY_DIR = Path("memory")

def learn_preference(key, value, source="conversation"):
    """Agrega o actualiza una preferencia"""
    prefs_file = MEMORY_DIR / "user" / "preferences.yaml"
    
    with open(prefs_file) as f:
        prefs = yaml.safe_load(f) or {}
    
    prefs[key] = value
    
    with open(prefs_file, 'w') as f:
        yaml.dump(prefs, f, allow_unicode=True)
    
    git_commit(
        files=[str(prefs_file)],
        message=f"memory: learned preference '{key}' from {source}"
    )

def learn_fact(topic, content):
    """Guarda un dato puntual aprendido"""
    today = datetime.now().strftime("%Y-%m-%d")
    slug = topic.lower().replace(" ", "-")[:40]
    filename = f"{today}_{slug}.md"
    path = MEMORY_DIR / "knowledge" / "learned" / filename
    
    with open(path, 'w') as f:
        f.write(f"# {topic}\n\n")
        f.write(f"Aprendido: {today}\n\n")
        f.write(content)
    
    git_commit(
        files=[str(path)],
        message=f"memory: learned about '{topic}'"
    )

def write_journal(summary, learned, mood, energy):
    """Genera el resumen diario"""
    today = datetime.now()
    month_dir = MEMORY_DIR / "journal" / today.strftime("%Y-%m")
    month_dir.mkdir(parents=True, exist_ok=True)
    
    path = month_dir / f"{today.strftime('%Y-%m-%d')}.md"
    
    content = f"# {today.strftime('%Y-%m-%d')} ({today.strftime('%A')})\n\n"
    content += f"## Resumen\n{summary}\n\n"
    content += f"## Aprendido\n{learned}\n\n"
    content += f"## Mood: {mood}\n"
    content += f"## Energia: {energy}\n"
    
    with open(path, 'w') as f:
        f.write(content)
    
    git_commit(
        files=[str(path)],
        message=f"journal: {today.strftime('%Y-%m-%d')}"
    )

def git_commit(files, message):
    """Commit y push al repo"""
    for f in files:
        subprocess.run(["git", "add", f])
    subprocess.run(["git", "commit", "-m", message])
    subprocess.run(["git", "push"])
```

### Cuando Aura necesita recordar

```python
def recall_structured(category, key=None):
    """Busca en memoria Git (exacta, rapida)"""
    path = MEMORY_DIR / category
    
    if path.is_file():
        with open(path) as f:
            data = yaml.safe_load(f)
        if key:
            return data.get(key)
        return data
    
    # Si es directorio, listar archivos
    if path.is_dir():
        return [f.name for f in path.iterdir()]

def recall_semantic(query):
    """Busca en ChromaDB (semantica, difusa)"""
    results = chromadb_collection.query(
        query_texts=[query],
        n_results=5
    )
    return results

def remember(query):
    """Pipeline completo de recall"""
    # 1. Intentar busqueda estructurada primero
    # (mapear query a categoria conocida)
    
    # 2. Si no encuentra, buscar en ChromaDB
    results = recall_semantic(query)
    
    # 3. Combinar y devolver contexto relevante
    return results
```

## Consolidacion: ChromaDB -> Git

Periodicamente (cada semana o cada N menciones), Aura consolida
informacion repetida de ChromaDB a Git:

```python
def consolidate_memory():
    """Mueve patrones repetidos de ChromaDB a Git"""
    # Si en 5 conversaciones distintas menciono que me gusta X
    # -> escribirlo en preferences.yaml (ya es un hecho consolidado)
    
    # Si un contacto aparece 3+ veces en conversaciones
    # -> agregarlo a contacts/ con contexto
    
    # Si una rutina se repite consistentemente
    # -> agregarla a routines.yaml
    pass
```

## Ventajas unicas de Git para memoria

### 1. Rollback de personalidad
```bash
# Aura se puso rara despues de un cambio de prompt
git log --oneline memory/personality/
git checkout abc123 -- memory/personality/system-prompt.md
```

### 2. Diff de lo que aprendio
```bash
# Que aprendio Aura esta semana?
git log --since="1 week ago" --name-only memory/
```

### 3. Branches para experimentar
```bash
git checkout -b aura-mas-graciosa
# Editar personality/behavior-rules.yaml
# Probar, si no funciona: git checkout main
```

### 4. Portabilidad total
```bash
# Mover Aura a otro servidor
git clone git@github.com:user/aura-memory.git
# Toda su identidad y conocimiento esta ahi
```

### 5. Transparencia
- Puedes ver EXACTAMENTE que sabe Aura de ti
- Puedes editar/borrar cualquier cosa manualmente
- Historial completo de cuando aprendio que

## Seguridad

- Repo PRIVADO siempre
- NO guardar passwords, tokens, API keys en memory/
- Secrets van en AWS Secrets Manager o .env (gitignored)
- Considerar encriptar archivos sensibles (git-crypt)
- .gitignore para datos temporales/cache

## Sincronizacion EC2 <-> Laptop

```bash
# En EC2 (cron cada 5 min o webhook)
cd /opt/aura/memory && git pull

# En laptop (cron o watcher)
cd ~/aura/memory && git pull

# Conflictos: raro si solo Aura escribe,
# pero si pasa: EC2 tiene prioridad (es el cerebro principal)
```

## Investigar

- [ ] git-crypt para encriptar archivos sensibles en el repo
- [ ] Git hooks para validar formato de YAML antes de commit
- [ ] Tamano maximo razonable del repo (journal diario = ~365 archivos/año)
- [ ] git-lfs si alguna vez se guardan audios/imagenes
- [ ] Estrategia de pruning: archivar journals viejos despues de 1 año?

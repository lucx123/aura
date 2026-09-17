# AURA Watch Basic 1.2

Firmware oficial en desarrollo para Waveshare ESP32-S3-Touch-AMOLED-2.06: reloj y
companero interactivo local, disenado para funcionar primero sin IA y con una
estrategia `sleep-first`. La especificacion y el orden de implementacion estan en
[`BASIC-1.0.md`](BASIC-1.0.md).

La base Offline 0.2 fue compilada, cargada y validada en pantalla y touch el
2026-09-14. Antes de iniciar Basic 1.0 se guardo una copia de su fuente en
`backups/aura-watch-offline-0.2-source-2026-09-15.zip`.

## Uso

- **Aura:** ojos que parpadean y miran alrededor, boca contextual y contenido
  colocado dentro de una zona segura para las esquinas fisicas. Toca para cambiar
  expresion, manten para acariciar y arrastra para que siga el movimiento. La
  mirada tambien sigue suavemente la inclinacion fisica del reloj.
- **Mareo:** tres movimientos bruscos dentro de una ventana corta hacen que Aura
  se maree, mueva los ojos en circulos y cambie la boca durante unos segundos. El
  gesto tiene enfriamiento para no activarse repetidamente.
- **Reloj:** hora analogica, digital y fecha, obtenidas del RTC PCF85063.
- **Timer:** 5, 15 o 25 minutos; iniciar, pausar, continuar y reiniciar. Al terminar
  abre la pantalla del temporizador y muestra un aviso visual.
- **Centro Aura:** menu vertical con Wi-Fi, formato 12/24 h, apariencia, brillo,
  temporizador, esfera analogica y descanso. Se abre con un toque corto en PWR;
  no hay barra inferior ni controles manuales de hora.
- **Conexion:** configura una red desde el portal temporal de Aura y sincroniza
  la hora por NTP. La radio se apaga al terminar para ahorrar bateria.
- Mantener pulsada la cara o pulsar Descansar baja el brillo. Tambien ocurre tras
  45 segundos sin actividad. Dos toques rapidos y cercanos despiertan la interfaz.
- El indicador superior lee el porcentaje y estado de carga del AXP2101.

### Boton PWR

- toque corto: despierta la pantalla; si ya esta encendida abre Centro Aura y
  otro toque desde el menu vuelve al inicio;
- mantener cerca de 1 segundo y soltar: apaga o enciende la pantalla, sin alerta;
- al llegar a 5 segundos: aparece la cuenta regresiva visual 3, 2, 1;
- mantener 8 segundos: el AXP2101 corta la alimentacion y conserva el RTC;
- para encender nuevamente: mantener PWR cerca de 2 segundos.

Una pantalla apagada despierta con dos toques en el mismo lugar dentro de medio
segundo, o con un toque corto de PWR.
BOOT sigue reservado para programacion y recuperacion.

El descanso apaga la iluminacion del panel; todavia no suspende LVGL ni entra en
deep sleep. El QMI8658 usa solo el acelerometro: 62,5 Hz con pantalla activa y
21 Hz con pantalla apagada; el giroscopio queda deshabilitado. Los movimientos no
despiertan la pantalla en esta version para evitar falsos encendidos. Aun no se ha
medido autonomia.
El temporizador no se conserva tras reiniciar y el aviso es visual, sin sonido ni
vibracion. El brillo y color se guardan en una particion NVS propia `aura_cfg`.
No hay captura de audio, IA, BLE ni conexion al gateway en esta version.

## Wi-Fi y hora automatica

Desde `Ajustes > Wi-Fi`, pulsa **Configurar Wi-Fi**. El reloj crea durante cinco
minutos la red abierta `AURA-SETUP`. Conecta el telefono a esa red, abre
`http://192.168.4.1`, elige una red cercana del escaneo, escribe su contrasena y
confirma. Al elegirla, la clave aparece en una ventana inmediata sin desplazarse
por la pagina. Tambien permite ingresar manualmente una red oculta. El portal
tiene la identidad visual de Aura y se cierra al guardar. Tras conectar, la
cabecera del reloj muestra el simbolo Wi-Fi y el SSID dentro de la zona segura.

Las credenciales se guardan en `aura_cfg`. Aura conecta al iniciar, cuando se
pulsa **Sincronizar ahora** y cada doce horas; obtiene UTC desde NTP, lo copia al
RTC y apaga Wi-Fi. El desfase actual se conserva en NVS. Esta version parte con
el desfase ya guardado para Santiago; la deteccion automatica de zona horaria y
los cambios estacionales quedan para el siguiente incremento.

El cambio voluntario desde `AURA-SETUP` a la red elegida y el apagado posterior
de la radio no se tratan como errores de conexion. Los rechazos reales conservan
su codigo de diagnostico para distinguir clave, seguridad, senal o DHCP.

## Hora por USB (recuperacion)

El RTC almacena UTC y mantiene la hora mientras tenga alimentacion. Al iniciar,
se recupera su hora si antes se sincronizo y sus registros son validos. Si pierde
la hora, se muestra `--:--` hasta sincronizar; no se inventa una fecha.

Con Python y pyserial instalados, desde este directorio:

```powershell
python tools/watch.py sync-time --port COM4
python tools/watch.py status --port COM4
```

Usa la hora y el desfase local del PC como mecanismo de recuperacion si no hay
Wi-Fi. El cambio estacional de zona horaria aun no es automatico.
El puerto puede variar al reconectar. La utilidad no reinicia la placa al abrirlo.

Para capturar la interfaz renderizada en el dispositivo:

```powershell
python tools/watch.py page 0
python tools/watch.py wake
python tools/watch.py screen build/home.png
```

## Compilacion

ESP-IDF **5.5.5**, LVGL **9.5.0**, BSP Waveshare **2.0.0**. Las versiones
resueltas estan en `dependencies.lock`. Pantalla 410 x 502, flash 32 MB,
PSRAM octal 8 MB. Los servicios RTC/PMIC usan el bus I2C del BSP.

```powershell
.\build-local.ps1
```

El script usa `$env:TEMP\aura-idf-5.5.5` y `$env:TEMP\aura-idf-tools`;
acepta `-IdfPath` y `-ToolsPath`. Prepara fuentes en una ruta temporal sin
espacios, regenera sdkconfig desde los defaults y copia resultados a `build/`.
Si Windows limpia el SDK temporal, hay que reinstalarlo. Los offsets de carga
estan en `build/flasher_args.json`; no es necesario borrar toda la flash.

## Recuperacion

Placa detectada en COM4, VID:PID `303A:1001`, ESP32-S3 revision v0.2.
Antes de la primera carga se respaldo toda la flash de fabrica:
`backups/factory-2026-09-14.bin`, 33 554 432 bytes, SHA-256:
`7F8F77B37295093219D7531FBA5F0983D33D8D1493AE2CD116712DC76B30450E`.
El respaldo esta excluido de Git; conservarlo.

Base de hardware y registros contrastados con los
[ejemplos oficiales de Waveshare](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06).

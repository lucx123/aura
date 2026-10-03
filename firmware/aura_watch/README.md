# AURA Watch Basic 1.4 dev.6

Firmware en desarrollo para Waveshare ESP32-S3-Touch-AMOLED-2.06: reloj y
companero local con RTC, IMU, Wi-Fi/NTP bajo demanda y herramientas de ingenieria.
El alcance original esta en [`BASIC-1.0.md`](BASIC-1.0.md).

Dev.6 reduce el trabajo del scroll, reutiliza las tarjetas estaticas del menu,
protege el descanso frente a invalidaciones y evita que el mareo por IMU saque
de una herramienta. El acceso a Eclipse sigue siendo de siete toques, sin clave;
volver al inicio conserva el modo. Tres toques sobre el fondo bloquean y dos
despiertan.

La microSD conectada fue identificada como NTFS, de 31,9 GB, incompatible con
FatFs. Evidence ahora dispone de un registro interno de los ultimos ocho eventos,
con lectura de verificacion y exportacion USB. La tarjeta conserva su formato.
El componente local FatFs agrega exFAT, probado con imagenes sinteticas en el PC.
Las pruebas y mediciones actuales estan en
[`AUDIT-2026-10-03.md`](AUDIT-2026-10-03.md); las versiones anteriores se documentan
en [`AUDIT-2026-10-02.md`](AUDIT-2026-10-02.md). El tacto fisico y la autonomia
requieren pruebas directas.

## Uso

- **Aura:** ojos que parpadean y miran alrededor, boca contextual y contenido
  colocado dentro de una zona segura para las esquinas fisicas. Toca para cambiar
  expresion, manten para acariciar y arrastra para que siga el movimiento. La
  mirada tambien sigue suavemente la inclinacion fisica del reloj.
- **Mareo:** cuatro sacudidas muy fuertes con rotacion dentro de una ventana corta
  hacen que Aura se maree en el inicio, gire la mirada y vea estrellas durante unos segundos. El
  gesto tiene enfriamiento para no activarse repetidamente.
- **Reloj:** hora analogica, digital y fecha, obtenidas del RTC PCF85063.
- **Timer:** 5, 15 o 25 minutos; iniciar, pausar, continuar y reiniciar. Al terminar
  despierta la pantalla antes de abrir el temporizador y mostrar un aviso visual.
- **Cronometro:** mide minutos, segundos y centesimas; permite pausar, continuar,
  reiniciar y guardar vueltas mientras se navega por el resto del reloj.
- **Centro Aura:** menu compacto de dos columnas con tarjetas, iconos y textos
  cortos para Wi-Fi, temporizador, cronometro, reloj, bateria y linterna. Tambien
  permite cambiar formato 12/24 h, apariencia, brillo y tiempo de descanso.
  En modo normal se abre con un toque corto en PWR.
- **AURA Eclipse 0.1:** modo hacker local y oculto. Siete toques en la version
  dentro de cinco segundos abren Eclipse directamente, sin PIN o password.
  Su menu de ocho tarjetas incluye Spectrum, Canales 2.4, Auditoria Wi-Fi,
  calculadoras IPv4/CIDR, RF y VLSM, diagnostico de hardware y preparacion de
  registros locales. Spectrum muestra el BSSID para distinguir AP con el mismo SSID.
  RF convierte MHz, metros y dBm en longitud de onda, perdida ideal de espacio
  libre y mW. VLSM reparte un bloque IPv4 entre hasta cuatro solicitudes de hosts.
  Ambas calculadoras permiten editar la entrada con teclado tactil: **Aplicar**
  calcula y **Cancelar** restaura la entrada anterior. El modo y sus resultados
  permanecen activos al volver al inicio o usar apps basicas. La flecha superior
  vuelve al inicio; el boton "Salir de Eclipse" cierra el modo y borra su cache.
  Consulta [`ECLIPSE-0.1.md`](ECLIPSE-0.1.md).
- **Bateria:** porcentaje y estado de bateria, USB y carga leidos del AXP2101.
  El boton de ahorro aplica manualmente brillo 35% y descanso tras 15 segundos;
  ambos ajustes quedan guardados.
- **Linterna:** usa el AMOLED en blanco, calido o rojo durante un maximo de
  30 segundos. Al salir restaura el brillo elegido, sin guardar el brillo temporal.
- **Conexion:** configura una red desde el portal temporal de Aura y sincroniza
  la hora por NTP. La radio se apaga al terminar para ahorrar bateria.
- Pulsar Descansar apaga el panel. El descanso automatico se puede ajustar a
  15, 30, 45 o 90 segundos sin actividad; parte en 45 segundos y se guarda en NVS.
  Dos toques rapidos y cercanos despiertan la interfaz. Descansar conserva el
  modo Eclipse y su cache, cancela trabajos pendientes de sus herramientas y
  vuelve al inicio hacker al despertar. Los trabajos no se reinician solos.
- Tres toques rapidos y cercanos sobre el fondo o la cara de Aura bloquean
  y apagan la pantalla. Deben completarse dentro de 0,75 segundos y en un radio
  de 40 pixeles. Botones, teclado, slider y version quedan excluidos.
  Arrastrar, mantener el dedo o cambiar de pagina reinicia la secuencia.
  Un breve margen de 400 ms evita despertar por toques extra tras el bloqueo,
  o volver a bloquear por el tercer toque tras despertar.
- El indicador superior lee el porcentaje y estado de carga del AXP2101.

### Boton PWR

- toque corto con pantalla apagada: despierta en el inicio y conserva el modo
  Eclipse si estaba activo;
- toque corto en modo normal y con pantalla encendida: abre Centro Aura; otro
  toque desde el menu vuelve al inicio;
- toque corto con Eclipse activo: desde una herramienta vuelve a su menu,
  desde el menu de Eclipse vuelve al inicio y desde el inicio regresa a Eclipse,
  conservando el modo y sus resultados;
- mantener cerca de 1 segundo y soltar: apaga o enciende la pantalla, sin alerta;
- al llegar a 5 segundos: aparece la cuenta regresiva visual 3, 2, 1;
- mantener 8 segundos: el AXP2101 corta la alimentacion y conserva el RTC;
- para encender nuevamente: mantener PWR cerca de 2 segundos.

Una pantalla apagada despierta con dos toques en el mismo lugar dentro de medio
segundo, o con un toque corto de PWR.
BOOT sigue siendo el boton de programacion durante el arranque. El acceso habitual
a Eclipse son los siete toques en la version de Centro Aura. No requiere una
habilitacion previa ni una clave guardada.

El descanso envia el comando de apagado al panel AMOLED, detiene los dibujos y
reduce la CPU de 240 a 80 MHz. El touch sigue disponible para despertar; LVGL
continua atendiendo entradas y temporizadores. Las animaciones se omiten al
dormir, las pantallas ocultas no se redibujan y el cronometro actualiza su texto
solo cuando esta visible. El QMI8658 apaga el giroscopio y baja el acelerometro de
62,5 Hz a 21 Hz durante el descanso. Se comprueba su configuracion y se reintenta
un cambio de modo si falla; los avisos de error se limitan a uno cada cinco
segundos. Los movimientos no despiertan la pantalla.
Todavia falta medir consumo y autonomia con bateria; no hay deep sleep.
El temporizador no se conserva tras reiniciar y el aviso es visual, sin sonido ni
vibracion. El brillo y color se guardan en una particion NVS propia `aura_cfg`.
Guardar de nuevo el mismo valor evita commits repetidos; si una escritura falla,
el siguiente intento vuelve a guardarlo y el error queda en el log.
No hay captura de audio, IA, BLE ni conexion al gateway en esta version.

## Wi-Fi y hora automatica

Desde `Centro Aura > Wi-Fi`, pulsa **Configurar Wi-Fi**. El reloj crea durante cinco
minutos la red abierta `AURA-SETUP`. Conecta el telefono a esa red, abre
`http://192.168.4.1`, elige una red cercana del escaneo, escribe su contrasena y
confirma. Al elegirla, la clave aparece en una ventana inmediata sin desplazarse
por la pagina. Tambien permite ingresar manualmente una red oculta. El portal
tiene la identidad visual de Aura y se cierra al guardar. Tras conectar, la
cabecera del reloj muestra el simbolo Wi-Fi y el SSID dentro de la zona segura.

Las credenciales se guardan juntas en `aura_cfg` y se comprueba la escritura.
El parser admite nombres de 1 a 32 bytes, redes abiertas sin clave, claves WPA
de 8 a 63 bytes y PSK hexadecimal de 64 caracteres. Rechaza campos duplicados,
escapes invalidos, bytes nulos y datos que excedan esos limites; no los trunca.
La radio se apaga al terminar y su indicador refleja el estado real de conexion.
El portal vence a los cinco minutos desde su apertura aunque se pulse Configurar
repetidamente.

La recepcion del formulario tiene un plazo de seis segundos y cada operacion
HTTP de recepcion o envio un timeout de dos segundos. Al cerrar el portal se
apaga la radio antes de esperar el cierre del servidor. Si un escaneo falla,
se libera la lista temporal de redes del controlador.

Aura conecta al iniciar, cuando se pulsa **Sincronizar ahora** y cada doce horas;
obtiene UTC desde NTP, lo copia al RTC y apaga Wi-Fi. SNTP admite dos servidores
y espera hasta 35 segundos para permitir recurrir al segundo si el primero falla.
El estado `SYNCED` solo se publica cuando se ha actualizado la hora del sistema,
el RTC y su persistencia en NVS. Los errores de cada paso se propagan.
El desfase actual se conserva en NVS. Esta version parte con
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
python tools/watch.py display --port COM4
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

Paginas del comando USB `PAGE`:

| Numero | Pantalla |
| --- | --- |
| 0 | Aura |
| 1 | Reloj |
| 2 | Temporizador |
| 3 | Centro Aura |
| 4 | Wi-Fi |
| 5 | Cronometro |
| 6 | Eclipse |
| 7 | Spectrum |
| 8 | System Check |
| 9 | Evidence |
| 10 | Canales 2.4 |
| 11 | Auditoria Wi-Fi |
| 12 | Calculadora IPv4/CIDR |
| 13 | Bateria |
| 14 | Linterna |
| 15 | RF / enlace |
| 16 | VLSM |

Las paginas 6 a 12 y 15 a 16 requieren activar Eclipse mediante los siete toques en la
version. `VERSION_TAP` permite reproducir ese gesto por USB. Volver al inicio,
usar una app basica o descansar conserva el modo y la cache de redes, mientras
cancela los trabajos pendientes. Regresar al menu de Eclipse no crea otra sesion
ni borra resultados. "Salir de Eclipse", `ECLIPSE_CLOSE` o reiniciar el reloj
terminan el modo; su estado no se guarda entre arranques.
`PAGE` se rechaza mientras la pantalla duerme: despiertala antes de cambiar de
pagina por USB. Esto conserva el panel apagado, la CPU a 80 MHz y las transferencias
DMA detenidas durante el descanso.

El comando `DISPLAY` permite comprobar pagina, actividad del panel, descanso,
modo `eclipse`, CPU, memoria y posicion vertical del menu `menu_y`.
`eclipse=1` tambien se conserva en el inicio, las apps basicas y el descanso.
Estos datos ayudan a reproducir una falla de navegacion.

Comandos adicionales de diagnostico:

| Comando USB | Uso |
| --- | --- |
| `VERSION_TAP` | Un toque en la version; siete rapidos abren Eclipse |
| `MENU_SCROLL n` | Desplaza el menu en `n` pixeles, entre -2000 y 2000 |
| `ECLIPSE_CLOSE` | Cierra Eclipse y restaura Aura normal |
| `SPECTRUM_START` | Escanea con Eclipse activo y pantalla despierta en Spectrum, Canales o Auditoria |
| `EVIDENCE_START` | Guarda un registro manual en SD o memoria interna desde Evidence visible |
| `STORAGE` | Formato detectado, estado SD y contadores del registro interno |
| `EVIDENCE_EXPORT` | Exporta los ocho registros internos en orden, con Eclipse activo, pantalla despierta y sin otro trabajo |
| `DRAW_STATS` / `DRAW_STATS_RESET` | Lee o reinicia las duraciones de ciclos LVGL |
| `CIDR direccion/prefijo` | Edita y calcula una IPv4/CIDR con Eclipse activo y pantalla despierta |
| `RF MHz,metros,dBm` | Calcula longitud de onda, FSPL y mW con Eclipse activo, despierto y RF visible |
| `VLSM red/prefijo,hosts1,...hosts4` | Asigna hasta cuatro subredes con Eclipse activo, despierto y VLSM visible |

Ejemplos de calculadoras por USB, una vez activado Eclipse:

```powershell
python tools/watch.py page 15 --port COM4
python tools/watch.py rf "2400,100,-20" --port COM4
python tools/watch.py page 16 --port COM4
python tools/watch.py vlsm "192.168.1.0/24,50,20,10" --port COM4
```

RF usa el modelo de espacio libre de [ITU-R P.525-5](https://www.itu.int/rec/r-rec-p.525):
campo lejano y linea de vista ideal, sin obstaculos ni ganancias de antena.
La potencia en mW es la conversion del valor dBm ingresado, no una medicion del
reloj. VLSM exige una direccion de red canonica y reserva red y broadcast en cada
subred; asigna bloques de mayor a menor y conserva el numero de solicitud original.
Sus bloques siguen la notacion y alineacion de [RFC 4632](https://www.rfc-editor.org/rfc/rfc4632.html).
Estas calculadoras ejecutan su calculo al abrir la pagina o aplicar una entrada;
no abren sockets, no activan radio ni crean tareas o temporizadores. Al abrirlas
se cancelan los trabajos pendientes de radio o microSD conservando Eclipse y su cache.

`MENU_SCROLL` actua en Centro Aura con la pantalla despierta y limita el destino
al contenido disponible. Ejemplo con la utilidad:

```powershell
python tools/watch.py page 3 --port COM4
python tools/watch.py menu-scroll 200 --port COM4
python tools/watch.py display --port COM4
```

## Compilacion

ESP-IDF **5.5.5**, LVGL **9.5.0**, BSP Waveshare **2.0.0**, esp_lvgl_port **2.9.0**
y QMI8658 **2.0.1**. Las versiones resueltas estan en `dependencies.lock`.
Los defaults usan optimizacion de rendimiento `-O2`. Pantalla 410 x 502,
flash 32 MB y PSRAM octal 8 MB. Los servicios RTC/PMIC usan el bus I2C del BSP.

La revision de actualizaciones del 2026-10-02 encontro ESP-IDF 6.1 y LVGL 9.6.0
como lineas mas recientes. Se conserva esta base mientras se estabilizan pantalla,
touch, descanso y menu; la migracion necesita su propia regresion. Fuentes y
decision detalladas en [`AUDIT-2026-10-02.md`](AUDIT-2026-10-02.md).

```powershell
.\build-local.ps1
```

El script usa `$env:TEMP\aura-idf-5.5.5` y `$env:TEMP\aura-idf-tools`;
acepta `-IdfPath` y `-ToolsPath`. Prepara fuentes en una ruta temporal sin
espacios, regenera sdkconfig desde los defaults y copia resultados a `build/`.
El staging tiene una ruta propia por proyecto para evitar mezclar el reloj y
el laboratorio. Al copiar fuentes se actualizan sus tiempos para que la
compilacion incremental reconstruya los archivos cambiados. Los hashes SHA-256
quedan en `build/sha256.json`.
Si Windows limpia el SDK temporal, hay que reinstalarlo. Los offsets de carga
estan en `build/flasher_args.json`; no es necesario borrar toda la flash.

## Recuperacion

Placa detectada en COM4, VID:PID `303A:1001`, ESP32-S3 revision v0.2.
Antes de la primera carga se respaldo toda la flash de fabrica:
`backups/factory-2026-09-14.bin`, 33 554 432 bytes, SHA-256:
`7F8F77B37295093219D7531FBA5F0983D33D8D1493AE2CD116712DC76B30450E`.
El respaldo esta excluido de Git; conservarlo.

Antes de esta auditoria tambien se conservaron la fuente y la imagen de la placa
con Eclipse dev.7. Las rutas y sus SHA-256 estan en
[`AUDIT-2026-10-02.md`](AUDIT-2026-10-02.md).

Base de hardware y registros contrastados con los
[ejemplos oficiales de Waveshare](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06).

## Verificacion

```powershell
python tools/hardware_regression.py --port COM4
python tools/features_check.py --port COM4
python tools/gesture_check.py --port COM4
python tools/eclipse_navigation_check.py --port COM4
python tools/engineer_check.py --port COM4
python tools/storage_check.py --port COM4
python tools/sleep_check.py --port COM4
python tools/scroll_benchmark.py --port COM4
python tests/test_storage_format.py --cc C:/ruta/a/zig.exe
python tests/test_evidence_store.py --cc C:/ruta/a/zig.exe
python tests/test_fatfs.py --cc C:/ruta/a/zig.exe
```

El benchmark mide ciclos LVGL en el MCU; el touch, los FPS fisicos y el consumo
se comprueban directamente en el reloj. Los resultados actuales, limites y hash
de la imagen estan en [`AUDIT-2026-10-03.md`](AUDIT-2026-10-03.md).
El portal de cinco minutos conserva las pruebas anteriores porque su codigo
no cambio en dev.6. La tabla de particiones mantiene sus offsets y la app se
actualiza en `0x10000`, conservando preferencias. OTA con rollback sigue pendiente.

## Paquete local

Tras compilar dev.6, verificar la imagen y registrar sus pruebas:

```powershell
python tools/package_release.py
```

El script usa la version del proyecto para generar el archivo; para dev.6 el
nombre previsto es `build/aura-watch-1.4.0-dev.6.zip`. Incluye la aplicacion,
fuentes, documentacion, informes, capturas del menu y un SHA-256 separado.
Los respaldos NVS y las capturas con redes cercanas quedan fuera del paquete.
`LEEME.txt` indica la carga de la app en `0x10000` para esta unidad,
conservando sus particiones y preferencias.

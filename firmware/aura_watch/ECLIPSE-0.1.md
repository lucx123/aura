# AURA Eclipse 0.1

Modo hacker local con ocho herramientas de redes, RF, VLSM y diagnostico.
Siete toques en la version muestran una introduccion y llegan al inicio con
Eclipse activo, sin clave. Volver al reloj, usar apps
basicas o descansar conserva la sesion; "Salir de Eclipse" la termina.

Basic 1.4 dev.8 agrega **OK** a la confirmacion de entrada. Al pulsarlo vuelves
al inicio con Eclipse activo; si no lo pulsas, avanza sola tras cuatro segundos.
La correccion de dev.7 reinicia ambos menus arriba al abrirlos.
El triple toque bloquea solamente desde la pantalla principal. La introduccion
captura taps sobrantes y espera que termine el contacto antes de retirarse.
Las pruebas de esta correccion estan en
[`AUDIT-2026-10-03-dev8.md`](AUDIT-2026-10-03-dev8.md), con los antecedentes en
[`AUDIT-2026-10-03-dev7.md`](AUDIT-2026-10-03-dev7.md).

Se conservan las mejoras de dev.6: reutiliza las tarjetas del menu, evita invalidaciones innecesarias
y conserva el descanso incluso si cambian estilos ocultos. Evidence identifica
el formato de la tarjeta y dispone de un registro interno persistente cuando
la escritura SD falla. La tarjeta de esta sesion es NTFS, de 31,9 GB.
La auditoria de dev.6 esta en [`AUDIT-2026-10-03.md`](AUDIT-2026-10-03.md).
Los antecedentes de dev.2 a dev.5 estan en
[`AUDIT-2026-10-02.md`](AUDIT-2026-10-02.md).

El panel QSPI usa dos buffers internos DMA RGB565 de 410 x 20 pixeles, 16 400
bytes cada uno. LVGL puede dibujar mientras se transmite el otro buffer y espera
su finalizacion antes de enviar la siguiente transferencia o reutilizar memoria.
La interrupcion solo entrega un semaforo. La espera maxima es un segundo; una
transferencia fallida o vencida reinicia el dispositivo. Antes de apagar el panel
o leer el contador se completa el ultimo DMA. SH8601 propaga los errores.
El comando USB DISPLAY informa version, transferencias, uptime, descanso, pagina,
modo Eclipse, CPU y memoria. `eclipse=1` indica que el modo sigue activo aunque
se vea el inicio, una app basica o la pantalla este apagada.

## Activacion y acceso

1. En modo normal, abre Centro Aura con un toque corto en PWR.
2. Busca la version y tocala siete veces dentro de cinco segundos.
3. Aparece "Que estas haciendo?" durante 1,4 segundos.
4. Aparece "Entraste a Eclipse", la version y un boton **OK**.
5. Pulsa **OK** para avanzar. Si no lo pulsas, avanza sola tras cuatro segundos.
6. Vuelves a la pantalla principal con Eclipse activo. PWR abre su menu,
   siempre desde arriba.

OK se habilita despues de liberar el dedo y dejar 600 ms sin contacto desde los
taps de entrada. El avance automatico tambien espera que termine el contacto.
Los taps adicionales no pulsan controles de la
pantalla siguiente ni cuentan como triple toque para bloquear. Una alarma del
temporizador interrumpe la introduccion para mostrar su pagina.

No hay formulario de password, creacion de PIN ni confirmacion de acceso.
El gesto mantiene Eclipse fuera de la navegacion cotidiana; cualquiera con
acceso al reloj puede abrirlo. La sesion es local y no concede permisos en un
gateway o un servicio externo.

Una vez activo, no hace falta repetir los siete toques para volver a su menu.
PWR desde el inicio abre Eclipse y desde una herramienta vuelve a su menu.
La flecha superior del menu y PWR desde ese menu regresan al inicio conservando
el modo. El boton inferior "Salir de Eclipse" lo cierra y restaura Aura normal.

### Navegacion y descanso

| Accion | Resultado |
| --- | --- |
| Volver al inicio o usar una app basica | Eclipse y la cache siguen activos; se cancelan trabajos pendientes |
| Descansar con PWR, triple toque en la pantalla principal o descanso automatico | Panel apagado, modo y cache conservados; herramientas pausadas |
| Despertar | Inicio hacker, sin reanudar trabajos automaticamente |
| Volver al menu de Eclipse | Menu arriba, misma sesion y resultados, sin volver a activar el modo |
| "Salir de Eclipse" o `ECLIPSE_CLOSE` | Modo normal y cache de Eclipse borrada |
| Reiniciar o apagar y encender el reloj | Arranque en modo normal; Eclipse no se persiste |

No mantengas BOOT mientras enciendes o reinicias el reloj: GPIO0 conserva su
funcion de recuperacion y descarga del ESP32-S3.

Para diagnostico por USB, `PAGE 6` es la portada de Eclipse, `PAGE 7` Spectrum,
`PAGE 8` System Check, `PAGE 9` Evidence, `PAGE 10` Canales 2.4, `PAGE 11`
Auditoria Wi-Fi, `PAGE 12` Calculadora CIDR, `PAGE 15` RF / enlace y `PAGE 16`
VLSM. Para entrar, envia siete
`VERSION_TAP` rapidos desde Centro Aura; las paginas no se abren sin Eclipse
activo. `DISPLAY` informa el modo en `eclipse`. `ECLIPSE_CLOSE` lo cierra.
`SPECTRUM_START` reproduce el boton de escaneo solo con pantalla despierta y
Spectrum, Canales o Auditoria visibles. `EVIDENCE_START` requiere Evidence visible,
pantalla despierta y ningun trabajo previo en curso. `CIDR 192.168.1.10/24`
edita una direccion de ejemplo con Eclipse activo y pantalla despierta, sin
conectar a esa red.
`PAGE` se rechaza mientras la pantalla duerme. Despierta primero con PWR o
`WAKE`; el descanso mantiene cero transferencias DMA y CPU a 80 MHz. Si vence
el temporizador, despierta el panel antes de abrir su pagina de aviso.

`RF 2400,100,-20` calcula un ejemplo con RF visible y `VLSM 192.168.1.0/24,50,20,10`
hace lo mismo con VLSM visible. Ambos comandos requieren Eclipse activo y
pantalla despierta. La utilidad USB permite reproducirlos asi:

```powershell
python tools/watch.py page 15 --port COM4
python tools/watch.py rf "2400,100,-20" --port COM4
python tools/watch.py page 16 --port COM4
python tools/watch.py vlsm "192.168.1.0/24,50,20,10" --port COM4
```

## Modulos disponibles

- **Spectrum:** escaneo Wi-Fi 2,4 GHz bajo demanda. Muestra hasta 24 AP en
  pantalla con SSID, BSSID, canal, RSSI y familia de seguridad. El BSSID identifica
  cada AP y permite distinguir los que anuncian el mismo SSID. Los nombres se
  limitan y sus caracteres de control se limpian antes de mostrarlos. La radio
  se apaga al terminar y los resultados quedan en RAM dentro de la sesion.
- **Canales 2.4:** compara los candidatos 1, 6 y 11 a partir de las redes de
  Spectrum. Usa un indice relativo de RSSI y proximidad de canal con un modelo
  nominal de 20 MHz. Sirve para comparar esa muestra; no mide trafico, ruido ni
  ancho real de las redes. Referencia del plan de canales:
  [guia RF de Cisco](https://www.cisco.com/c/en/us/td/docs/wireless/controller/9800/technical-reference/wireless-rf-reference-guide.html).
- **Auditoria Wi-Fi:** resume el modo de autenticacion anunciado por cada AP
  y su RSSI/canal. No prueba claves, vulnerabilidades ni el estado de PMF.
  Los datos proceden del escaneo de la
  [API Wi-Fi de ESP-IDF 5.5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/network/esp_wifi.html).
- **Calculadora IPv4/CIDR:** permite editar direccion y prefijo sin Internet.
  Muestra mascara, red, broadcast cuando corresponde, rango y cantidad de hosts.
  Acepta prefijos de /0 a /32. /31 considera dos extremos de un enlace punto a
  punto y /32 una sola direccion; referencia: [RFC 3021](https://www.rfc-editor.org/info/rfc3021/).
- **RF / enlace:** calcula longitud de onda en metros, perdida de espacio libre
  en dB y conversion de potencia a mW. Entrada: `MHz,metros,dBm`, por ejemplo
  `2400,100,-20`; el resultado es aproximadamente 0,124914 m, 80,05 dB y 0,01 mW.
  Usa `lambda = c/f`, `FSPL = 20 log10(4 pi d/lambda)` y
  `mW = 10^(dBm/10)`. La perdida corresponde a un enlace ideal con linea de vista
  y campo lejano: no incluye obstaculos ni ganancias de antena. Si la distancia
  es menor que lambda, avisa que se debe comprobar la aplicabilidad del modelo.
  No mide potencia ni alcance real. Referencia:
  [ITU-R P.525-5, seccion 2.3](https://www.itu.int/dms_pubrec/itu-r/rec/p/R-REC-P.525-5-202411-I!!PDF-E.pdf).
  Acepta frecuencias de 0,001 a 100 000 MHz, distancias de 0,001 a 10 000 000 m
  y potencias de -200 a 100 dBm; son limites numericos de entrada, no limites de
  validez fisica del modelo. Admite decimales simples con hasta seis cifras
  decimales y rechaza NaN, infinitos, exponentes y campos adicionales.
- **VLSM:** distribuye una red IPv4 entre una y cuatro solicitudes positivas de
  hosts, por ejemplo `192.168.1.0/24,50,20,10`. Asigna primero las mayores:
  `/26` para 50 hosts, `/27` para 20 y `/28` para 10; conserva el numero de cada
  solicitud original. Muestra red/prefijo, broadcast, rango y capacidad de hosts,
  junto con las direcciones sin asignar. Exige una base canonica, por ejemplo
  `192.168.1.0/24`; una direccion de host como `192.168.1.10/24` no sirve como base.
  Reserva red y broadcast, por lo que genera prefijos de /0 a /30; /31 y /32
  siguen disponibles en la calculadora CIDR. Informa falta de espacio sin
  presentar un plan parcial. Referencias:
  [RFC 4632, seccion 3.1](https://www.rfc-editor.org/rfc/rfc4632.html) y
  [tabla IPv4 de RFC 1878](https://www.rfc-editor.org/info/rfc1878/).
- **System Check:** bateria/USB, RTC, QMI8658, movimiento, giro, estado Wi-Fi y
  estado de la microSD.
- **Evidence:** guarda una comprobacion manual con hora, evento y detalle.
  Intenta SD (`/AURA/eclipse/events.jsonl`) y verifica la linea recien escrita.
  Si la tarjeta no se puede usar, guarda en NVS los ultimos ocho registros y
  vuelve a leerlos para verificar el commit. Libera SD al terminar y no formatea.
  La tarjeta NTFS de esta sesion usa el respaldo interno. Para exportarlo con
  Eclipse despierto: `python tools/watch.py evidence-export build/evidence.jsonl`.
  Solo corre al pulsar "Revisar y guardar registro"; no se reanuda al despertar.

El menu de Eclipse dispone de ocho tarjetas en dos columnas. RF y VLSM permiten
editar la entrada completa con un teclado numerico tactil, hasta 80 caracteres.
**Aplicar** calcula y cierra el teclado; **Cancelar** restaura la entrada anterior.
Los campos usan punto decimal y coma entre valores, sin espacios.

En la prueba de dev.2, Spectrum y las paginas de redes pasaron por USB con la
radio apagada al terminar. Evidence devolvio `ESP_FAIL`, `sd=0` y `sd_bytes=0`;
el montaje y la escritura con una tarjeta disponible siguen pendientes.

Dev.4 aprobo 10 comprobaciones de navegacion, 30 transiciones y 12 desplazamientos
de la regresion de hardware, y 24 comprobaciones de gestos, sin reintentos USB.
El modo se conservo durante descanso/despertar, la CPU paso entre 80/240 MHz y
no hubo DMA durante tres segundos dormido. La prueba de funciones aprobo NTP,
Spectrum, apps de redes, cancelacion del escaneo al abrir Bateria conservando
el modo y limite de linterna. Evidence volvio a devolver `ESP_FAIL`; no se valido
la escritura de registros en tarjeta.

Canales y Auditoria reutilizan el escaneo puntual de Spectrum. Spectrum y las
apps de redes muestran solamente la cache validada de la sesion; un resultado
de una generacion anterior no reemplaza la muestra vigente. Sus calculos,
CIDR, RF y VLSM no abren sockets ni crean tareas o radio adicional. La cache de
redes se conserva solamente durante la sesion de Eclipse; las entradas de las
calculadoras permanecen en RAM hasta reiniciar y no se guardan en NVS.
RF y VLSM son funciones sin estado, sin asignacion dinamica ni temporizadores.
Se calculan al abrir su pagina o aplicar una entrada, con Eclipse activo y
pantalla despierta; los comandos USB tambien exigen su pagina visible.
Abrirlas cancela trabajos pendientes de radio o microSD y conserva el modo y la cache.
La sesion continua al navegar por el reloj o descansar. Cancelar un escaneo
pendiente conserva la ultima muestra valida; un error real de un nuevo escaneo
se informa y limpia los resultados afectados.

Dev.5 se cargo en COM4 en `0x10000`, con 1 597 232 bytes y SHA-256
`5A03CEB705CB630420CF7B1346BE043EA0BE3AB85BD864D26652D5379C6E7956` verificado.
Aprobaron 22 pruebas host C (12 de redes y 10 de RF/VLSM), 11 comprobaciones
de calculadoras en el dispositivo, 12 de navegacion, 24 de gestos y la regresion
de pantalla con 30 transiciones y 12 desplazamientos. La prueba de funciones aprobo
NTP, Spectrum, apps de redes, cancelacion de escaneo y limite de linterna. Las cinco
suites terminaron sin reintentos USB ni reinicios inesperados.
Aplicar/Cancelar se emitieron como eventos asincronos y se procesaron por la tarea
LVGL. Tambien se verifico `PAGE` rechazado durante descanso, cero DMA y 80 MHz,
y el despertar del temporizador antes de abrir su pagina.
Evidence informo microSD no disponible (`ESP_FAIL`); su montaje y escritura siguen
pendientes. Tacto fisico, fluidez del AMOLED y consumo tampoco se validaron.
Los resultados de dev.4 descritos
arriba son antecedentes; consulta [`AUDIT-2026-10-02.md`](AUDIT-2026-10-02.md)
para los informes de dev.5.

## Comportamiento seguro

- Apagar la pantalla, volver al inicio o usar una app basica conserva el modo
  Eclipse y su cache, y cancela los trabajos pendientes de sus herramientas.
- Tres toques rapidos sobre el fondo o la cara de Aura en la pantalla principal
  apagan la pantalla mediante el mismo flujo
  de descanso. Al despertar se conserva el modo y no se reanudan trabajos solos.
- Reiniciar siempre vuelve a AURA normal.
- PWR desde una herramienta vuelve al menu de Eclipse; desde su menu vuelve al
  inicio y desde el inicio regresa a Eclipse. "Salir de Eclipse" restaura Aura
  normal y borra los resultados de la sesion.
- La microSD se revisa en una tarea separada y nunca se formatea automaticamente.
  Descansar, volver al inicio, usar apps basicas o cerrar Eclipse cancela el
  trabajo pendiente.
  Una operacion de archivo que ya haya comenzado puede finalizar para proteger
  la tarjeta; la tarea no comienza el siguiente paso si esta cancelada.
  Los errores de directorio, escritura y cierre se informan al usuario.
- Spectrum ejecuta una observacion puntual y no mantiene la radio encendida.
- Los escaneos solo arrancan con Eclipse activo y una pagina de redes visible.
  Descansar, volver al inicio o cerrar el modo cancela el escaneo. Su espera
  comprueba la cancelacion cada 100 ms; las peticiones antiguas en cola se
  descartan mediante su identificador.
- Las herramientas se deben usar solo en redes y equipos propios o autorizados.

## Pendiente

- Validar FAT32/exFAT en una tarjeta fisica compatible; NTFS usa el registro interno.
- BLE Radar y localizador RSSI.
- Sesiones completas con inventario e informes.
- Packet Lens limitado a tramas de administracion.
- NetProbe y diagnostico de conectividad.
- AURA Link para telefono, deliberadamente fuera de esta version.
- Verificar touch y panel fisico; las capturas USB solo muestran el render LVGL.

# AURA Watch Basic 1.0

Estado: **en desarrollo**, implementacion actual **Basic 1.4 dev.6**. Este
documento mantiene el alcance original de Basic 1.0 y registra su avance.
AURA Watch debe seguir funcionando sin Internet y prioriza autonomia, claridad
y recuperacion segura. La auditoria del 2026-10-02 esta en
[`AUDIT-2026-10-02.md`](AUDIT-2026-10-02.md).

Como antecedentes, dev.2 fue compilada y cargada en COM4 con hash verificado.
La regresion por USB y 17 pruebas host aprobaron; el touch, el panel fisico, el ahorro manual con
persistencia y la autonomia siguen pendientes de comprobar directamente.
Dev.3 incorporo el triple toque para bloquear. Sus 23 comprobaciones por USB
aprobaron; el gesto fisico con el dedo aun requiere una prueba directa.
Dev.4 conserva Eclipse al volver al inicio, usar apps basicas o descansar, y
ofrece un cierre explicito con "Salir de Eclipse". Fue compilada y cargada por USB
y aprobo 10 comprobaciones de navegacion de Eclipse sin reintentos de lectura.
La regresion de hardware, las 24 comprobaciones de gestos y la prueba de funciones
de dev.4 tambien aprobaron. Evidence sigue sin poder montar la microSD.
Los resultados anteriores no comprueban la permanencia del modo.
Dev.5 agrega calculadora RF y planificador VLSM offline, ademas del BSSID de
cada AP en Spectrum. Eclipse conserva su navegacion y pasa a ocho herramientas.
La imagen dev.5 fue cargada con hash verificado. Aprobo 22 pruebas host, 11 de
ingenieria, 12 de navegacion Eclipse, 24 de gestos y regresiones de pantalla y
funciones por USB. El tacto fisico y la autonomia requieren mediciones directas.

Dev.6 agrega Evidence interno con ocho registros persistentes y exportacion USB,
diagnostico del formato SD, tarjetas de menu reutilizadas y proteccion de dibujo
durante descanso. La microSD insertada es NTFS, no compatible con FatFs; su formato
se conserva. Los resultados actuales estan en
[`AUDIT-2026-10-03.md`](AUDIT-2026-10-03.md).

## Identidad del producto

AURA es un reloj y companero digital local. Los ojos son su rasgo principal y la
boca aparece solo cuando aporta expresion: felicidad, sorpresa, descanso, mareo,
comida o, en el futuro, voz. La personalidad nunca castiga al usuario por no
interactuar ni convierte el reloj en una obligacion.

Principios visuales:

- negro real como fondo AMOLED;
- contenido importante fuera de las cuatro esquinas fisicas;
- un solo acento configurable: menta, lila o ambar;
- movimiento breve y provocado por eventos;
- ojos expresivos sin halos permanentes ni particulas decorativas;
- texto corto, legible y en espanol;
- menu compacto de dos columnas con tarjetas e iconos reconocibles;
- controles cortos y grandes dentro de la zona segura de las esquinas.

## Alcance 1.0

### Reloj y utilidades

- [x] Hora y fecha recuperadas desde RTC.
- [x] Formato de 12 o 24 horas persistente.
- [x] Reloj analogico y digital.
- [x] Temporizador local.
- [ ] Alarmas persistentes mediante RTC.
- [x] Cronometro con pausa, continuacion, reinicio y vueltas.
- [x] Bateria: porcentaje, USB y carga, con ajuste manual de ahorro.
- [x] Linterna AMOLED blanca, calida o roja, limitada a 30 segundos.
- [ ] Modo concentracion/Pomodoro.
- [x] Sincronizacion NTP por rafagas y copia al RTC.
- [x] Dos servidores SNTP, ventana de 35 segundos y estado de sincronizacion
  exitoso solo tras actualizar sistema, RTC y NVS.
- [ ] Zona horaria `America/Santiago` con cambio estacional automatico.

### Personalidad local

- [x] Parpadeo, mirada autonoma y seguimiento tactil.
- [x] Boca contextual simple.
- [x] Reacciones al toque y a una caricia mantenida.
- [ ] Estados tranquila, contenta, curiosa, somnolienta, mareada y molesta.
- [x] Reaccion de la mirada a inclinacion y gravedad mediante QMI8658.
- [x] Agitar para marear, con umbral, enfriamiento y recuperacion visual.
- [ ] Estado local persistente con escrituras limitadas para proteger la flash.
- [ ] Regalos, objetos, accesorios y minijuegos en una fase posterior.

### Energia y controles

- [x] Apagado real del panel y pausa de dibujos durante el descanso.
- [x] CPU a 240 MHz activa y 80 MHz durante el descanso.
- [x] Pantallas ocultas sin dibujos y actualizaciones de texto solo si cambian.
- [ ] Suspension completa de LVGL y medicion de consumo.
- [ ] Light sleep y deep sleep medidos en la placa.
- [ ] Levantar muneca para despertar usando acelerometro de bajo consumo.
- [x] PWR corto: inicio/menu, conservando el modo Eclipse cuando esta activo.
- [x] PWR medio: alternar pantalla al soltar, sin mostrar alerta.
- [x] PWR largo: cuenta regresiva 3, 2, 1 desde 5 s y apagado real a 8 s.
- [x] Despertar con PWR corto o dos toques rapidos y cercanos en la pantalla.
- [x] Tres toques rapidos sobre fondo o cara bloquean y apagan la pantalla.
- [x] Encendido con PWR mantenido durante 2 s.
- [x] BOOT conserva recuperacion al arrancar.
- [x] Eclipse abre directamente con siete toques rapidos en la version, sin PIN.
- [x] Centro Aura usa una cuadrícula compacta de dos columnas.
- [x] Descanso automatico de 15, 30, 45 o 90 segundos persistente.
- [x] Ahorro manual con brillo 35% y descanso de 15 segundos.

### Conectividad austera

- [x] Portal temporal `AURA-SETUP`, nunca permanente.
- [x] Caducidad de cinco minutos que no se prolonga al pulsar Configurar.
- [x] Parser estricto sin truncar SSID ni claves y guardado comprobado en NVS.
- [x] Recepcion HTTP con plazo de seis segundos y operaciones con timeout de dos.
- [ ] Deteccion de zona horaria desde el telefono y confirmacion de ciudad.
- [x] Wi-Fi por rafagas para hora y sincronizacion.
- [ ] Wi-Fi por rafagas para clima y otros datos.
- [ ] Clima almacenado en cache.
- [ ] Wi-Fi continuo permitido mientras carga.
- [ ] AURA Device Gateway para evitar secretos de terceros en el reloj.
- [ ] BLE como transporte preferente para notificaciones frecuentes.

### Actualizaciones y recuperacion

- [ ] Rediseniar particiones para dos slots OTA.
- [ ] Verificar la imagen antes del reinicio.
- [ ] Rollback automatico si la nueva version no confirma arranque.
- [ ] No actualizar con bateria baja.
- [ ] Mantener recuperacion por USB y BOOT.

### Herramientas locales de Eclipse

- [x] Spectrum: escaneo puntual Wi-Fi 2,4 GHz.
- [x] Canales 2.4: indice relativo sobre RSSI y proximidad de canal para 1/6/11.
- [x] Auditoria Wi-Fi: resumen de autenticacion anunciada, RSSI y canal.
- [x] IPv4/CIDR editable y offline, con prefijos /0 a /32.
- [x] System Check y preparacion de microSD con Evidence.

Las herramientas de redes reutilizan Spectrum y guardan resultados durante la
sesion del modo Eclipse. Volver al inicio, usar apps basicas o descansar cancela
los trabajos pendientes y conserva la cache. Al despertar aparece el inicio
hacker; volver al menu de Eclipse no reinicia la sesion ni sus herramientas.
"Salir de Eclipse" y reiniciar el reloj terminan el modo y borran los resultados.
El alcance y los limites estan en
[`ECLIPSE-0.1.md`](ECLIPSE-0.1.md).

## Presupuesto de energia

El firmware es `sleep-first`: pantalla, giroscopio y Wi-Fi permanecen apagados
cuando no hacen falta. En Basic 1.2 el acelerometro baja de 62,5 Hz a su modo de
bajo consumo de 21 Hz cuando la pantalla duerme; no despierta el reloj por
movimiento para evitar falsos encendidos. La personalidad se recalcula al despertar a partir del tiempo
transcurrido; no ejecuta animaciones invisibles en segundo plano.

Basic 1.4 reduce la CPU de 240 a 80 MHz al descansar, limita la frecuencia de
lectura de bateria y evita redibujar pantallas ocultas. LVGL sigue atendiendo
touch y temporizadores para poder despertar. La preparacion de microSD se
ejecuta en otra tarea para mantener disponible la interfaz. El compilador usa
optimizacion de rendimiento `-O2`.
Descansar conserva los resultados de Eclipse en RAM, pausa sus herramientas y
no inicia escaneos ni trabajos de microSD al despertar. Una operacion de archivo
que ya haya comenzado puede terminar para mantener la integridad de la tarjeta.

La configuracion del QMI8658 comprueba los resultados y reintenta las transiciones
de descanso que fallen, con avisos limitados a uno cada cinco segundos. Guardar
preferencias evita repetir commits de valores ya guardados y permite reintentar
una escritura fallida. El cierre del portal apaga la radio antes de esperar al
servidor HTTP.

Estos cambios reducen trabajo innecesario; el consumo, la mejora de autonomia
y la fluidez del menu aun requieren medidas en el dispositivo.

Las 10 comprobaciones de navegacion de dev.4 verificaron permanencia de Eclipse
y su cache, PWR entre menu e inicio, cancelacion de un escaneo nuevo sin perder
la muestra anterior, rechazo de escaneo/Evidence desde el inicio, reentrada,
descanso/despertar y cierre explicito. La regresion de hardware de dev.4 aprobo
30 transiciones y 12 desplazamientos, CPU a 80/240 MHz, modo activo durante
descanso/despertar y cero transferencias DMA durante tres segundos dormido.
La prueba de gestos aprobo 24 comprobaciones USB, incluida la conservacion de
Eclipse con doble toque para despertar. Ambas terminaron sin reintentos USB.
La prueba de funciones aprobo NTP, Spectrum, apps de redes, cancelacion del
escaneo al abrir Bateria conservando el modo y limite de linterna, sin reintentos
USB. Evidence volvio a informar `ESP_FAIL`; la tarjeta y la comprobacion fisica
con el dedo siguen pendientes.

Antes de declarar una autonomia se mediran estos estados por separado:

1. pantalla activa con animacion;
2. pantalla apagada;
3. light sleep;
4. deep sleep;
5. espera del acelerometro;
6. conexion Wi-Fi;
7. rafaga NTP/HTTPS;
8. audio y motor, cuando se validen.

## Orden de implementacion

1. interfaz curva, preferencias y personalidad visual base;
2. boton PWR y apagado real de pantalla;
3. IMU y gestos; **inclinacion y mareo implementados en Basic 1.2 dev.1**, wake pendiente;
4. alarmas, cronometro y Pomodoro;
5. portal Wi-Fi temporal con escaneo, clave emergente y NTP; **implementado en Basic 1.1 dev.6**
6. clima y gateway;
7. particiones OTA con rollback;
8. BLE, notificaciones y aplicacion movil;
9. voz e IA en una version posterior.

## Respaldo de la version anterior

La fuente completa de Offline 0.2 previa a Basic 1.0 esta en:

`backups/aura-watch-offline-0.2-source-2026-09-15.zip`

SHA-256:
`530E88B177BF7F7067C1028EE33C9BE1AA883D95E5206F1182050AF9E8066D69`

El respaldo binario original de fabrica permanece separado y no fue modificado.

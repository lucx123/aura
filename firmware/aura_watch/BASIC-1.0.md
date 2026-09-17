# AURA Watch Basic 1.0

Estado: **en desarrollo**. Esta es la linea base oficial del primer firmware de
AURA Watch sin IA. Debe seguir funcionando sin Internet y prioriza autonomia,
claridad y recuperacion segura.

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
- cuatro destinos principales como maximo en la navegacion base.

## Alcance 1.0

### Reloj y utilidades

- [x] Hora y fecha recuperadas desde RTC.
- [x] Formato de 12 o 24 horas persistente.
- [x] Reloj analogico y digital.
- [x] Temporizador local.
- [ ] Alarmas persistentes mediante RTC.
- [x] Cronometro con pausa, continuacion, reinicio y vueltas.
- [ ] Modo concentracion/Pomodoro.
- [x] Sincronizacion NTP por rafagas y copia al RTC.
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

- [x] Pantalla a brillo cero durante el descanso actual.
- [ ] Apagado real del panel y suspension de LVGL.
- [ ] Light sleep y deep sleep medidos en la placa.
- [ ] Levantar muneca para despertar usando acelerometro de bajo consumo.
- [x] PWR corto: inicio/menu.
- [x] PWR medio: alternar pantalla al soltar, sin mostrar alerta.
- [x] PWR largo: cuenta regresiva 3, 2, 1 desde 5 s y apagado real a 8 s.
- [x] Despertar con PWR corto o dos toques rapidos y cercanos en la pantalla.
- [x] Encendido con PWR mantenido durante 2 s.
- [ ] BOOT reservado para programacion y recuperacion.

### Conectividad austera

- [x] Portal temporal `AURA-SETUP`, nunca permanente.
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

## Presupuesto de energia

El firmware es `sleep-first`: pantalla, giroscopio y Wi-Fi permanecen apagados
cuando no hacen falta. En Basic 1.2 el acelerometro baja de 62,5 Hz a su modo de
bajo consumo de 21 Hz cuando la pantalla duerme; no despierta el reloj por
movimiento para evitar falsos encendidos. La personalidad se recalcula al despertar a partir del tiempo
transcurrido; no ejecuta animaciones invisibles en segundo plano.

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

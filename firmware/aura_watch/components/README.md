# Overrides de hardware AURA

El firmware usa dos componentes locales que deben distribuirse con su fuente.
No editar copias dentro de `managed_components`.

- `waveshare__esp32_s3_touch_amoled_2_06`, base Waveshare 2.0.0:
  registro del panel QSPI por la ruta SPI de LVGL; dos buffers internos DMA
  RGB565 de 410 x 20 pixeles, 16 400 bytes cada uno y 32 800 bytes en total;
  comandos de encendido/apagado del panel y contador de transferencias.
- `waveshare__esp_lcd_sh8601`, base Waveshare 2.0.0:
  propaga el resultado de `tx_color` para detectar errores de transferencia.
  La licencia original se conserva en `license.txt`. Su `CHECKSUMS.json` es
  metadata de la distribucion original y no certifica el override modificado.

La placa comercial actual se documenta como CO5300 en los ejemplos del
[fabricante](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06).
Esta unidad funciona con la inicializacion SH8601 que ya usaba dev.7.
Confirmar la revision fisica antes de reemplazar el controlador por otro.

El override admite orientacion nativa, refresco parcial RGB565 y estos dos
buffers DMA. LVGL puede dibujar mientras el otro buffer se transmite. La
interrupcion de finalizacion solo entrega un semaforo; LVGL espera antes de
enviar la siguiente transferencia o reutilizar su memoria. Antes de apagar
el panel o leer el contador se termina el ultimo DMA.

Una transferencia fallida o que tarde mas de un segundo reinicia el dispositivo
para evitar reutilizar memoria que podria seguir en uso por DMA. Rotacion, RGB,
modo directo, full refresh o buffers mayores requieren validar de nuevo el
contrato y la memoria disponible.

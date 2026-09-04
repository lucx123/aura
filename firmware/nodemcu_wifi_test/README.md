# Prueba AURA con NodeMCU V3 (ESP8266)

Este firmware no usa BLE. El NodeMCU V3 habitual contiene un ESP8266, que solo
ofrece Wi-Fi. La placa crea su propia red para que la prueba no dependa del router.
La clave de esa red es exclusivamente de testeo y este prototipo no representa el
sistema de autenticacion definitivo de AURA Watch.

## Cargar desde VS Code

1. Copia `include/wifi_secrets.example.h` como `include/wifi_secrets.h` y cambia
   `CHANGE_ME_LOCALLY` por una clave local de al menos 8 caracteres. El archivo
   resultante queda excluido de Git.
2. Conecta la placa con un cable USB que transmita datos.
3. Abre esta carpeta (`firmware/nodemcu_wifi_test`) en VS Code.
4. Espera a que PlatformIO termine de instalar la plataforma ESP8266.
5. En la barra inferior de PlatformIO, pulsa **Upload**.
6. Pulsa **Serial Monitor** y comprueba que esté configurado a 115200 baudios.

Si PlatformIO no detecta el puerto, probablemente falta el controlador USB CH340
o el cable solo sirve para cargar energía.

## Probar desde el teléfono

1. Conéctate a la red Wi-Fi `AURA-NodeMCU`.
2. Usa la clave que configuraste localmente en `include/wifi_secrets.h`.
3. Aunque el teléfono avise que la red no tiene Internet, permanece conectado.
4. Abre `http://192.168.4.1` en el navegador.
5. Pulsa los botones. Cada orden aparecerá en el monitor serie.

## Pantalla LCD 1602 I2C

Conecta `VCC` a `3V3`, `GND` a `G`, `SDA` a `D2` y `SCL` a `D1`. El firmware
busca automáticamente la dirección del adaptador I2C y muestra el último comando.
Si la retroiluminación enciende pero no aparecen letras, ajusta lentamente el
potenciómetro azul del módulo.

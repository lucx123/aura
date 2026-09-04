#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include "wifi_secrets.h"

namespace {
constexpr char kAccessPointName[] = "AURA-NodeMCU";

ESP8266WebServer server(80);
String lastCommand = "ninguno";
LiquidCrystal_I2C* lcd = nullptr;

uint8_t findI2cDevice() {
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.printf("Dispositivo I2C encontrado: 0x%02X\n", address);
      return address;
    }
  }
  return 0;
}

void showOnDisplay(const String& line1, const String& line2) {
  if (lcd == nullptr) return;
  lcd->clear();
  lcd->setCursor(0, 0);
  lcd->print(line1.substring(0, 16));
  lcd->setCursor(0, 1);
  lcd->print(line2.substring(0, 16));
}

const char kPage[] PROGMEM = R"HTML(
<!doctype html>
<html lang="es">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AURA NodeMCU</title>
<style>
body{font-family:system-ui;max-width:34rem;margin:3rem auto;padding:0 1rem;background:#10131a;color:#eef}
button{font-size:1.1rem;margin:.35rem;padding:.8rem 1rem;border:0;border-radius:.7rem}
#state{margin-top:1.2rem;color:#9fd}
</style>
<h1>AURA · prueba NodeMCU</h1>
<p>Estos botones simulan las órdenes que después recibirá la cara.</p>
<button onclick="send('feliz')">Feliz</button>
<button onclick="send('escuchando')">Escuchando</button>
<button onclick="send('pensando')">Pensando</button>
<button onclick="send('hablando')">Hablando</button>
<div id="state">Listo</div>
<script>
async function send(command){
  const response=await fetch('/command?value='+encodeURIComponent(command));
  document.querySelector('#state').textContent=await response.text();
}
</script>
</html>
)HTML";

void handleCommand() {
  if (!server.hasArg("value")) {
    server.send(400, "text/plain; charset=utf-8", "Falta el comando");
    return;
  }

  lastCommand = server.arg("value");
  Serial.printf("Comando recibido: %s\n", lastCommand.c_str());
  showOnDisplay("AURA", lastCommand);
  server.send(200, "text/plain; charset=utf-8", "Recibido: " + lastCommand);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nIniciando prueba AURA...");

  Wire.begin(D2, D1);
  const uint8_t lcdAddress = findI2cDevice();
  if (lcdAddress != 0) {
    lcd = new LiquidCrystal_I2C(lcdAddress, 16, 2);
    lcd->init();
    lcd->backlight();
    showOnDisplay("AURA iniciando", "Conectando...");
  } else {
    Serial.println("No se encontro ningun dispositivo I2C.");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(kAccessPointName, kAccessPointPassword);

  server.on("/", [] { server.send_P(200, "text/html; charset=utf-8", kPage); });
  server.on("/command", handleCommand);
  server.on("/status", [] {
    server.send(200, "application/json", "{\"lastCommand\":\"" + lastCommand + "\"}");
  });
  server.begin();

  Serial.printf("Wi-Fi: %s\n", kAccessPointName);
  Serial.printf("Clave: %s\n", kAccessPointPassword);
  Serial.printf("Abre: http://%s\n", WiFi.softAPIP().toString().c_str());
  showOnDisplay("AURA lista", "192.168.4.1");
}

void loop() {
  server.handleClient();
}

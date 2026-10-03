#include "aura_wifi.h"
#include "aura_services.h"
#include "aura_portal.h"

#include <ctype.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "mbedtls/platform_util.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#define GOT_IP_BIT BIT0
#define FAIL_BIT BIT1
#define SCAN_DONE_BIT BIT2
#define SETUP_LIFETIME_MS (5 * 60 * 1000)

typedef enum { ACTION_SYNC = 1, ACTION_SETUP, ACTION_FORGET, ACTION_SCAN, ACTION_SAVE } wifi_action_t;
typedef struct { char ssid[33]; char pass[65]; } credentials_t;
typedef struct { wifi_action_t action; credentials_t credentials; uint32_t scan_ticket; } wifi_request_t;

static const char *TAG = "aura_wifi";
static SemaphoreHandle_t lock;
static EventGroupHandle_t events;
static QueueHandle_t actions;
static httpd_handle_t server;
static aura_wifi_status_t status;
static aura_wifi_scan_t scan_status;
static char password[65];
static bool wifi_started;
static bool accepting_events;
static atomic_uint scan_ticket;
static atomic_uint scan_result_status;

static const char portal_html[] =
"<!doctype html><html lang='es'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Aura · Wi-Fi</title><style>"
"*{box-sizing:border-box}body{margin:0;min-height:100vh;display:grid;place-items:center;background:#050807;color:#f0f7f4;font:16px system-ui;padding:18px}"
"main{width:min(100%,430px);background:#101918;border:1px solid #263532;border-radius:32px;padding:26px}"
".face{display:flex;gap:18px;justify-content:center;margin:2px 0 18px}.eye{width:48px;height:62px;border-radius:22px;background:#b6a2ff;box-shadow:0 0 24px #b6a2ff55}"
"h1{font-size:29px;margin:0 0 7px}p{color:#9baca7;line-height:1.45;margin:0 0 20px}.head{display:flex;align-items:center;justify-content:space-between;margin:0 0 9px}.head strong{font-size:15px}"
"button,input{font:inherit}.refresh,.manual{width:auto;border:0;background:transparent;color:#b6a2ff;padding:7px 0;font-weight:650}.networks{display:grid;gap:8px;min-height:62px}.network{width:100%;display:flex;align-items:center;justify-content:space-between;text-align:left;border:1px solid #2b3b38;border-radius:17px;background:#08100e;color:#f0f7f4;padding:13px 15px}.network:active,.network.selected{border-color:#b6a2ff;background:#181d22}.network b{display:block;max-width:250px;overflow:hidden;text-overflow:ellipsis}.network span{color:#8fa09b;font-size:13px}.signal{color:#b6a2ff;font-size:14px;white-space:nowrap;margin-left:12px}"
".loading{color:#8fa09b;padding:18px 4px}label{display:block;margin:14px 0 7px}input{width:100%;border:1px solid #354743;border-radius:16px;background:#08100e;color:white;padding:14px;font-size:16px}.manual{display:block;margin:12px auto 0}.manualbox{display:none}.manualbox.on{display:block}"
".veil{display:none;position:fixed;inset:0;z-index:5;background:#020403cc;padding:18px;align-items:flex-end;justify-content:center}.veil.open{display:flex}.sheet{position:relative;width:min(100%,430px);background:#101918;border:1px solid #354743;border-radius:28px;padding:25px;box-shadow:0 -18px 70px #000}.close{position:absolute;right:16px;top:13px;border:0;background:#263532;color:#f0f7f4;width:38px;height:38px;border-radius:50%;font-size:22px}.sheet h2{font-size:25px;margin:0 45px 5px 0;overflow:hidden;text-overflow:ellipsis}.security{color:#8fa09b;margin:0 0 10px}.error{display:none;color:#ffb29f;margin-top:10px}.error.on{display:block}.connect{width:100%;border:0;border-radius:18px;background:#b6a2ff;color:#090b0b;padding:16px;font-size:17px;font-weight:750;margin-top:20px}small{display:block;color:#71817d;margin-top:16px;text-align:center}"
"</style><main><div class='face'><i class='eye'></i><i class='eye'></i></div><h1>Conecta a Aura</h1>"
"<p>Elige tu red. Ajustare mi hora y luego apagare Wi-Fi para ahorrar energia.</p>"
"<div class='head'><strong>Redes cercanas</strong><button class='refresh' type='button' onclick='scan()'>Buscar otra vez</button></div>"
"<div id='nets' class='networks'><div class='loading'>Estoy mirando alrededor...</div></div>"
"<button id='manual' class='manual' type='button'>No veo mi red</button>"
"<small>Este portal se apaga solo en 5 minutos.</small></main>"
"<div id='veil' class='veil'><section class='sheet'><button id='close' class='close' type='button' aria-label='Cerrar'>×</button><h2 id='netname'>Tu red</h2><p id='security' class='security'></p>"
"<form id='form' method='post' action='/save'><input id='ssid' name='s' type='hidden'><div id='manualbox' class='manualbox'><label>Nombre de la red</label><input id='manualssid' maxlength='32' autocomplete='off'></div>"
"<div id='passrow'><label>Contrasena</label><input id='pass' name='p' maxlength='64' type='password' autocomplete='current-password'></div><div id='error' class='error'>Escribe o elige una red primero.</div>"
"<button id='connect' class='connect'>Conectar y sincronizar</button></form></section></div>"
"<script>const nets=document.querySelector('#nets'),ssid=document.querySelector('#ssid'),veil=document.querySelector('#veil'),netname=document.querySelector('#netname'),security=document.querySelector('#security'),manualbox=document.querySelector('#manualbox'),manualssid=document.querySelector('#manualssid'),passrow=document.querySelector('#passrow'),pass=document.querySelector('#pass'),error=document.querySelector('#error');"
"function strength(r){return r>=-55?'Excelente':r>=-67?'Buena':r>=-75?'Media':'Debil'}"
"function show(){veil.className='veil open'}function hide(){veil.className='veil';pass.value='';error.className='error'}"
"function pick(n){ssid.value=n.s;netname.textContent=n.s;security.textContent=n.o?'Red abierta':'Red protegida';manualbox.className='manualbox';passrow.style.display=n.o?'none':'block';show();setTimeout(()=>n.o?document.querySelector('#connect').focus():pass.focus(),80)}"
"async function scan(){nets.innerHTML=\"<div class='loading'>Estoy mirando alrededor...</div>\";try{const data=await fetch('/scan',{cache:'no-store'}).then(r=>{if(!r.ok)throw 0;return r.json()});nets.innerHTML='';if(!data.length){nets.innerHTML=\"<div class='loading'>No encontre redes. Acercate al router y busca otra vez.</div>\";return}data.forEach(n=>{const b=document.createElement('button');b.type='button';b.className='network';const info=document.createElement('span'),name=document.createElement('b'),meta=document.createElement('span'),sig=document.createElement('span');name.textContent=n.s;meta.textContent=(n.o?'Sin contrasena':'Protegida');info.append(name,meta);sig.className='signal';sig.textContent=strength(n.r);b.append(info,sig);b.onclick=()=>pick(n,b);nets.append(b)})}catch(e){nets.innerHTML=\"<div class='loading'>No pude buscar ahora. Toca Buscar otra vez.</div>\"}}"
"document.querySelector('#manual').onclick=()=>{ssid.value='';netname.textContent='Red oculta';security.textContent='Escribe el nombre y la clave';manualbox.className='manualbox on';passrow.style.display='block';show();setTimeout(()=>manualssid.focus(),80)};manualssid.oninput=()=>ssid.value=manualssid.value;document.querySelector('#close').onclick=hide;veil.onclick=e=>{if(e.target===veil)hide()};document.querySelector('#form').onsubmit=e=>{if(!ssid.value){e.preventDefault();error.className='error on'}};scan()</script></html>";

static void set_state(aura_wifi_state_t state)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    status.state = state;
    xSemaphoreGive(lock);
}

static bool load_credentials(void)
{
    nvs_handle_t nvs;
    if (nvs_open_from_partition("aura_cfg", "wifi", NVS_READONLY, &nvs) != ESP_OK) return false;
    credentials_t saved = {0};
    size_t size = sizeof(saved);
    esp_err_t error = nvs_get_blob(nvs, "credentials", &saved, &size);
    bool ok = error == ESP_OK && size == sizeof(saved) && saved.ssid[0] &&
              memchr(saved.ssid, 0, sizeof(saved.ssid)) && memchr(saved.pass, 0, sizeof(saved.pass));
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        size_t ssid_len = sizeof(saved.ssid), pass_len = sizeof(saved.pass);
        ok = nvs_get_str(nvs, "ssid", saved.ssid, &ssid_len) == ESP_OK && saved.ssid[0] &&
             nvs_get_str(nvs, "pass", saved.pass, &pass_len) == ESP_OK;
    }
    int64_t last = 0;
    nvs_get_i64(nvs, "last_sync", &last);
    nvs_close(nvs);
    xSemaphoreTake(lock, portMAX_DELAY);
    if (ok) {
        memcpy(status.ssid, saved.ssid, sizeof(status.ssid));
        memcpy(password, saved.pass, sizeof(password));
    }
    status.configured = ok;
    status.last_sync = last;
    xSemaphoreGive(lock);
    mbedtls_platform_zeroize(&saved, sizeof(saved));
    return ok;
}

static esp_err_t save_credentials(const credentials_t *saved)
{
    nvs_handle_t nvs;
    esp_err_t error = nvs_open_from_partition("aura_cfg", "wifi", NVS_READWRITE, &nvs);
    if (error != ESP_OK) return error;
    error = nvs_set_blob(nvs, "credentials", saved, sizeof(*saved));
    if (error == ESP_OK) error = nvs_commit(nvs);
    nvs_close(nvs);
    if (error != ESP_OK) return error;
    xSemaphoreTake(lock, portMAX_DELAY);
    memcpy(status.ssid, saved->ssid, sizeof(status.ssid));
    memcpy(password, saved->pass, sizeof(password));
    status.configured = true;
    xSemaphoreGive(lock);
    return ESP_OK;
}

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        const wifi_event_sta_scan_done_t *done = data;
        atomic_store(&scan_result_status, done ? done->status : 1);
        xEventGroupSetBits(events, SCAN_DONE_BIT);
        return;
    }
    xSemaphoreTake(lock, portMAX_DELAY);
    bool accept = accepting_events;
    xSemaphoreGive(lock);
    if (!accept) return;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disconnected = data;
        // Switching from the setup AP to the saved network (and the later
        // battery-saving shutdown) emits ASSOC_LEAVE. It is our own action,
        // not a failed connection attempt.
        if (disconnected && disconnected->reason == WIFI_REASON_ASSOC_LEAVE) {
            ESP_LOGD(TAG, "station left intentionally");
            return;
        }
        xSemaphoreTake(lock, portMAX_DELAY);
        status.disconnect_reason = disconnected ? disconnected->reason : 0;
        xSemaphoreGive(lock);
        ESP_LOGW(TAG, "station disconnected, reason=%u", status.disconnect_reason);
        xEventGroupSetBits(events, FAIL_BIT);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        wifi_ap_record_t ap = {0};
        esp_err_t ap_error = esp_wifi_sta_get_ap_info(&ap);
        xSemaphoreTake(lock, portMAX_DELAY);
        if (ap_error == ESP_OK) status.rssi = ap.rssi;
        status.disconnect_reason = 0;
        xSemaphoreGive(lock);
        set_state(AURA_WIFI_ONLINE);
        xEventGroupSetBits(events, GOT_IP_BIT);
    }
}

static void wifi_stop(void)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    accepting_events = false;
    xSemaphoreGive(lock);
    if (wifi_started) {
        esp_wifi_disconnect();
        esp_wifi_stop();
        wifi_started = false;
    }
    xSemaphoreTake(lock, portMAX_DELAY);
    status.radio_on = false;
    status.rssi = 0;
    xSemaphoreGive(lock);
    // Stop the radio even if a slow HTTP client has not finished its handler.
    if (server) { httpd_stop(server); server = NULL; }
}

static esp_err_t wifi_start(void)
{
    esp_err_t error = wifi_started ? ESP_OK : esp_wifi_start();
    wifi_started = error == ESP_OK;
    xSemaphoreTake(lock, portMAX_DELAY);
    status.radio_on = wifi_started;
    xSemaphoreGive(lock);
    return error;
}

static esp_err_t root_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, portal_html, HTTPD_RESP_USE_STRLEN);
}

static void json_escape_ssid(char *out, size_t out_size, const uint8_t *ssid)
{
    size_t used = 0;
    for (size_t i = 0; i < 32 && ssid[i] && used + 2 < out_size; ++i) {
        unsigned char c = ssid[i];
        if (c == '"' || c == '\\') {
            out[used++] = '\\';
            out[used++] = (char)c;
        } else if (c >= 0x20) out[used++] = (char)c;
    }
    out[used] = 0;
}

static esp_err_t scan_get(httpd_req_t *req)
{
    wifi_scan_config_t config = {
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active = {.min = 80, .max = 220},
    };
    if (esp_wifi_scan_start(&config, true) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan unavailable");

    uint16_t found = 0;
    if (esp_wifi_scan_get_ap_num(&found) != ESP_OK) {
        esp_wifi_clear_ap_list();
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan failed");
    }
    if (found > 20) found = 20;
    wifi_ap_record_t *records = calloc(found ? found : 1, sizeof(*records));
    char *json = malloc(6144);
    if (!records || !json) {
        esp_wifi_clear_ap_list();
        free(records);
        free(json);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
    }
    uint16_t count = found;
    esp_err_t err = found ? esp_wifi_scan_get_ap_records(&count, records) : esp_wifi_clear_ap_list();
    if (err != ESP_OK) {
        esp_wifi_clear_ap_list();
        free(records);
        free(json);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan failed");
    }

    size_t used = 0;
    json[used++] = '[';
    int shown = 0;
    for (uint16_t i = 0; i < count; ++i) {
        if (!records[i].ssid[0]) continue;
        bool duplicate = false;
        for (uint16_t j = 0; j < i; ++j) {
            if (!strcmp((char *)records[i].ssid, (char *)records[j].ssid)) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;
        char escaped[67];
        json_escape_ssid(escaped, sizeof(escaped), records[i].ssid);
        int written = snprintf(json + used, 6144 - used,
                               "%s{\"s\":\"%s\",\"r\":%d,\"o\":%s}",
                               shown ? "," : "", escaped, records[i].rssi,
                               records[i].authmode == WIFI_AUTH_OPEN ? "true" : "false");
        if (written < 0 || (size_t)written >= 6144 - used - 2) break;
        used += (size_t)written;
        shown++;
    }
    json[used++] = ']';
    json[used] = 0;
    free(records);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    err = httpd_resp_send(req, json, used);
    free(json);
    return err;
}

static esp_err_t save_post(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > AURA_PORTAL_BODY_MAX)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Datos invalidos");
    char body[AURA_PORTAL_BODY_MAX + 1] = {0};
    int received = 0;
    int64_t deadline = esp_timer_get_time() + 6000000;
    while (received < req->content_len) {
        if (esp_timer_get_time() >= deadline) {
            mbedtls_platform_zeroize(body, sizeof(body));
            return ESP_FAIL;
        }
        int chunk = httpd_req_recv(req, body + received, req->content_len - received);
        if (chunk <= 0) {
            mbedtls_platform_zeroize(body, sizeof(body));
            return ESP_FAIL;
        }
        received += chunk;
    }
    wifi_request_t request = {.action = ACTION_SAVE};
    bool valid = aura_portal_parse(body, received, request.credentials.ssid, request.credentials.pass);
    mbedtls_platform_zeroize(body, sizeof(body));
    if (!valid) {
        mbedtls_platform_zeroize(&request, sizeof(request));
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Red o clave invalida; revisa su longitud");
    }
    BaseType_t queued = xQueueSend(actions, &request, 0);
    mbedtls_platform_zeroize(&request, sizeof(request));
    if (queued != pdTRUE) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, "Aura esta ocupada. Intenta otra vez.");
    }
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, "<html lang='es'><meta name='viewport' content='width=device-width'><h1>Solicitud recibida</h1><p>Revisa el resultado de la conexion en el reloj.</p></html>");
}

static void start_portal(void)
{
    wifi_stop();
    wifi_config_t ap = {0};
    strlcpy((char *)ap.ap.ssid, "AURA-SETUP", sizeof(ap.ap.ssid));
    ap.ap.ssid_len = strlen("AURA-SETUP");
    ap.ap.channel = 1;
    ap.ap.max_connection = 2;
    ap.ap.authmode = WIFI_AUTH_OPEN;
    // AP+STA keeps the setup portal reachable while its station radio scans.
    esp_err_t error = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (error == ESP_OK) error = esp_wifi_set_config(WIFI_IF_AP, &ap);
    if (error == ESP_OK) error = wifi_start();
    if (error != ESP_OK) { wifi_stop(); set_state(AURA_WIFI_ERROR); return; }
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.recv_wait_timeout = 2;
    cfg.send_wait_timeout = 2;
    cfg.max_uri_handlers = 4;
    if (httpd_start(&server, &cfg) == ESP_OK) {
        httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_get};
        httpd_uri_t scan = {.uri = "/scan", .method = HTTP_GET, .handler = scan_get};
        httpd_uri_t save = {.uri = "/save", .method = HTTP_POST, .handler = save_post};
        error = httpd_register_uri_handler(server, &root);
        if (error == ESP_OK) error = httpd_register_uri_handler(server, &scan);
        if (error == ESP_OK) error = httpd_register_uri_handler(server, &save);
        if (error != ESP_OK) { wifi_stop(); set_state(AURA_WIFI_ERROR); return; }
    } else { wifi_stop(); set_state(AURA_WIFI_ERROR); return; }
    set_state(AURA_WIFI_SETUP);
}

static void connect_and_sync(void)
{
    aura_wifi_status_t current;
    aura_wifi_get_status(&current);
    if (!current.configured) { set_state(AURA_WIFI_OFF); return; }
    wifi_stop();
    xEventGroupClearBits(events, GOT_IP_BIT | FAIL_BIT);
    xSemaphoreTake(lock, portMAX_DELAY);
    status.disconnect_reason = 0;
    xSemaphoreGive(lock);
    wifi_config_t sta = {0};
    xSemaphoreTake(lock, portMAX_DELAY);
    // IDF fields are fixed byte arrays; full-length SSIDs/PSKs need no terminator.
    memcpy(sta.sta.ssid, status.ssid, strlen(status.ssid));
    memcpy(sta.sta.password, password, strlen(password));
    xSemaphoreGive(lock);
    sta.sta.threshold.authmode = WIFI_AUTH_OPEN;
    sta.sta.pmf_cfg.capable = true;
    esp_err_t error = esp_wifi_set_mode(WIFI_MODE_STA);
    if (error == ESP_OK) error = esp_wifi_set_config(WIFI_IF_STA, &sta);
    mbedtls_platform_zeroize(&sta, sizeof(sta));
    if (error == ESP_OK) error = wifi_start();
    if (error == ESP_OK) {
        set_state(AURA_WIFI_CONNECTING);
        xSemaphoreTake(lock, portMAX_DELAY);
        accepting_events = true;
        xSemaphoreGive(lock);
        error = esp_wifi_connect();
    }
    if (error != ESP_OK) { wifi_stop(); set_state(AURA_WIFI_ERROR); return; }
    EventBits_t bits = xEventGroupWaitBits(events, GOT_IP_BIT | FAIL_BIT, pdTRUE, false, pdMS_TO_TICKS(18000));
    if (!(bits & GOT_IP_BIT)) {
        set_state(AURA_WIFI_ERROR);
        vTaskDelay(pdMS_TO_TICKS(2500));
        wifi_stop();
        return;
    }
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "time.cloudflare.com");
    esp_sntp_setservername(1, "pool.ntp.org");
    esp_sntp_init();
    sntp_sync_status_t sync_status = SNTP_SYNC_STATUS_RESET;
    for (int i = 0; i < 70; ++i) {
        sync_status = esp_sntp_get_sync_status();
        if (sync_status == SNTP_SYNC_STATUS_COMPLETED) break;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    // Reading COMPLETED resets the IDF status, so preserve that first result.
    bool synced = sync_status == SNTP_SYNC_STATUS_COMPLETED;
    esp_sntp_stop();
    if (synced) {
        time_t now = time(NULL);
        error = aura_clock_set(now, aura_clock_offset());
        nvs_handle_t nvs;
        if (error == ESP_OK) {
            error = nvs_open_from_partition("aura_cfg", "wifi", NVS_READWRITE, &nvs);
            if (error == ESP_OK) {
                error = nvs_set_i64(nvs, "last_sync", now);
                if (error == ESP_OK) error = nvs_commit(nvs);
                nvs_close(nvs);
            }
        }
        if (error == ESP_OK) {
            xSemaphoreTake(lock, portMAX_DELAY);
            status.last_sync = now;
            status.state = AURA_WIFI_SYNCED;
            xSemaphoreGive(lock);
        } else {
            ESP_LOGE(TAG, "SNTP received; RTC/preferences failed: %s", esp_err_to_name(error));
            set_state(AURA_WIFI_ERROR);
        }
    } else set_state(AURA_WIFI_ERROR);
    vTaskDelay(pdMS_TO_TICKS(1800));
    wifi_stop();
}

static void scan_once(uint32_t ticket)
{
    wifi_stop();
    xSemaphoreTake(lock, portMAX_DELAY);
    scan_status.scanning = true;
    scan_status.error = ESP_OK;
    scan_status.count = 0;
    xSemaphoreGive(lock);
    set_state(AURA_WIFI_SCANNING);
    atomic_store(&scan_result_status, 1);
    xEventGroupClearBits(events, SCAN_DONE_BIT);

    esp_err_t error = esp_wifi_set_mode(WIFI_MODE_STA);
    if (error == ESP_OK) {
        error = wifi_start();
        wifi_scan_config_t config = {
            .show_hidden = false,
            .scan_type = WIFI_SCAN_TYPE_ACTIVE,
            .scan_time.active = {.min = 45, .max = 120},
        };
        if (error == ESP_OK) error = esp_wifi_scan_start(&config, false);
    }

    int64_t deadline = esp_timer_get_time() + 10000000;
    while (error == ESP_OK) {
        if (ticket != atomic_load(&scan_ticket)) { error = ESP_ERR_INVALID_STATE; break; }
        if (xEventGroupWaitBits(events, SCAN_DONE_BIT, pdTRUE, false, pdMS_TO_TICKS(100)) & SCAN_DONE_BIT) {
            if (atomic_load(&scan_result_status) != 0) error = ESP_FAIL;
            break;
        }
        if (esp_timer_get_time() >= deadline) { error = ESP_ERR_TIMEOUT; break; }
    }
    bool cancelled = ticket != atomic_load(&scan_ticket);
    if (cancelled) error = ESP_ERR_INVALID_STATE;
    if (error != ESP_OK) esp_wifi_scan_stop();

    wifi_ap_record_t records[AURA_WIFI_SCAN_MAX] = {0};
    uint16_t count = AURA_WIFI_SCAN_MAX;
    if (error == ESP_OK) error = esp_wifi_scan_get_ap_records(&count, records);
    if (error != ESP_OK) esp_wifi_clear_ap_list();

    xSemaphoreTake(lock, portMAX_DELAY);
    scan_status.count = error == ESP_OK ? count : 0;
    for (uint16_t i = 0; i < scan_status.count; ++i) {
        strlcpy(scan_status.networks[i].ssid, (const char *)records[i].ssid,
                sizeof(scan_status.networks[i].ssid));
        scan_status.networks[i].rssi = records[i].rssi;
        scan_status.networks[i].channel = records[i].primary;
        scan_status.networks[i].authmode = records[i].authmode;
        memcpy(scan_status.networks[i].bssid, records[i].bssid, sizeof(records[i].bssid));
    }
    scan_status.error = error;
    scan_status.scanning = false;
    scan_status.generation++;
    xSemaphoreGive(lock);
    wifi_stop();
    set_state(error == ESP_OK || cancelled ? AURA_WIFI_OFF : AURA_WIFI_ERROR);
}

static void forget_credentials(void)
{
    wifi_stop();
    nvs_handle_t nvs;
    esp_err_t error = nvs_open_from_partition("aura_cfg", "wifi", NVS_READWRITE, &nvs);
    if (error == ESP_OK) {
        error = nvs_erase_all(nvs);
        if (error == ESP_OK) error = nvs_commit(nvs);
        nvs_close(nvs);
    }
    if (error != ESP_OK) { set_state(AURA_WIFI_ERROR); return; }
    xSemaphoreTake(lock, portMAX_DELAY);
    memset(status.ssid, 0, sizeof(status.ssid));
    mbedtls_platform_zeroize(password, sizeof(password));
    status.configured = false;
    status.last_sync = 0;
    status.disconnect_reason = 0;
    status.state = AURA_WIFI_OFF;
    xSemaphoreGive(lock);
}

static void wifi_task(void *arg)
{
    (void)arg;
    int64_t portal_deadline = 0;
    for (;;) {
        TickType_t wait = pdMS_TO_TICKS(12 * 60 * 60 * 1000);
        if (portal_deadline) {
            int64_t left = portal_deadline - esp_timer_get_time();
            if (left <= 0) {
                wifi_stop(); set_state(AURA_WIFI_OFF); portal_deadline = 0;
                continue;
            }
            wait = pdMS_TO_TICKS((left + 999) / 1000);
        }
        wifi_request_t request = {0};
        if (xQueueReceive(actions, &request, wait) != pdTRUE) {
            if (!portal_deadline) connect_and_sync();
            continue;
        }
        if (request.action == ACTION_SCAN && request.scan_ticket != atomic_load(&scan_ticket)) {
            mbedtls_platform_zeroize(&request, sizeof(request));
            continue;
        }
        if (request.action == ACTION_SETUP) {
            // Repeated taps cannot extend an already open portal indefinitely.
            if (!portal_deadline) {
                start_portal();
                if (server) portal_deadline = esp_timer_get_time() + (int64_t)SETUP_LIFETIME_MS * 1000;
            }
        } else {
            portal_deadline = 0;
            if (request.action == ACTION_SAVE) {
                // Complete the save response before switching the radio to STA.
                if (server) { httpd_stop(server); server = NULL; }
                wifi_stop();
                if (save_credentials(&request.credentials) == ESP_OK) connect_and_sync();
                else set_state(AURA_WIFI_ERROR);
            } else if (request.action == ACTION_SYNC) { wifi_stop(); connect_and_sync(); }
            else if (request.action == ACTION_FORGET) forget_credentials();
            else if (request.action == ACTION_SCAN) scan_once(request.scan_ticket);
        }
        mbedtls_platform_zeroize(&request, sizeof(request));
    }
}

void aura_wifi_init(void)
{
    lock = xSemaphoreCreateMutex();
    events = xEventGroupCreate();
    actions = xQueueCreate(4, sizeof(wifi_request_t));
    configASSERT(lock && events && actions);
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_netif_init());
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL));
    load_credentials();
    configASSERT(xTaskCreate(wifi_task, "aura_wifi", 8192, NULL, 4, NULL) == pdPASS);
    if (status.configured) aura_wifi_request_sync();
    ESP_LOGI(TAG, "configured=%d ssid=%s", status.configured, status.configured ? status.ssid : "-");
}

void aura_wifi_get_status(aura_wifi_status_t *out)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    *out = status;
    xSemaphoreGive(lock);
}

static void enqueue(wifi_action_t action)
{
    wifi_request_t request = {.action = action};
    if (xQueueSend(actions, &request, 0) != pdTRUE)
        ESP_LOGW(TAG, "Wi-Fi busy; request %d rejected", action);
}

void aura_wifi_request_sync(void) { enqueue(ACTION_SYNC); }
void aura_wifi_start_setup(void) { enqueue(ACTION_SETUP); }
void aura_wifi_forget(void) { enqueue(ACTION_FORGET); }
void aura_wifi_request_scan(void)
{
    wifi_request_t request = {.action = ACTION_SCAN,
        .scan_ticket = atomic_fetch_add(&scan_ticket, 1) + 1};
    if (xQueueSend(actions, &request, 0) != pdTRUE)
        ESP_LOGW(TAG, "Wi-Fi busy; tools scan rejected");
}
void aura_wifi_cancel_scan(void) { atomic_fetch_add(&scan_ticket, 1); }

void aura_wifi_get_scan(aura_wifi_scan_t *out)
{
    if (!out) return;
    xSemaphoreTake(lock, portMAX_DELAY);
    *out = scan_status;
    xSemaphoreGive(lock);
}

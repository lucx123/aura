#include "aura_wifi.h"
#include "aura_services.h"

#include <ctype.h>
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
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#define GOT_IP_BIT BIT0
#define FAIL_BIT BIT1
#define SETUP_LIFETIME_MS (5 * 60 * 1000)

typedef enum { ACTION_SYNC = 1, ACTION_SETUP, ACTION_FORGET } wifi_action_t;

static const char *TAG = "aura_wifi";
static SemaphoreHandle_t lock;
static EventGroupHandle_t events;
static QueueHandle_t actions;
static httpd_handle_t server;
static aura_wifi_status_t status;
static char password[65];
static bool wifi_started;

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
    size_t ssid_len = sizeof(status.ssid), pass_len = sizeof(password);
    bool ok = nvs_get_str(nvs, "ssid", status.ssid, &ssid_len) == ESP_OK && status.ssid[0] &&
              nvs_get_str(nvs, "pass", password, &pass_len) == ESP_OK;
    int64_t last = 0;
    nvs_get_i64(nvs, "last_sync", &last);
    nvs_close(nvs);
    xSemaphoreTake(lock, portMAX_DELAY);
    status.configured = ok;
    status.last_sync = last;
    xSemaphoreGive(lock);
    return ok;
}

static void save_credentials(const char *ssid, const char *pass)
{
    nvs_handle_t nvs;
    if (nvs_open_from_partition("aura_cfg", "wifi", NVS_READWRITE, &nvs) != ESP_OK) return;
    nvs_set_str(nvs, "ssid", ssid);
    nvs_set_str(nvs, "pass", pass);
    nvs_commit(nvs);
    nvs_close(nvs);
    strlcpy(status.ssid, ssid, sizeof(status.ssid));
    strlcpy(password, pass, sizeof(password));
    xSemaphoreTake(lock, portMAX_DELAY);
    status.configured = true;
    xSemaphoreGive(lock);
}

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
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
        xSemaphoreTake(lock, portMAX_DELAY);
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) status.rssi = ap.rssi;
        status.disconnect_reason = 0;
        xSemaphoreGive(lock);
        set_state(AURA_WIFI_ONLINE);
        xEventGroupSetBits(events, GOT_IP_BIT);
    }
}

static void wifi_stop(void)
{
    if (server) { httpd_stop(server); server = NULL; }
    if (wifi_started) {
        esp_wifi_disconnect();
        esp_wifi_stop();
        wifi_started = false;
    }
}

static void wifi_start(void)
{
    if (!wifi_started && esp_wifi_start() == ESP_OK) wifi_started = true;
}

static void url_decode(char *out, size_t out_size, const char *in)
{
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < out_size; ++i) {
        if (in[i] == '+') out[j++] = ' ';
        else if (in[i] == '%' && isxdigit((unsigned char)in[i + 1]) && isxdigit((unsigned char)in[i + 2])) {
            char hex[3] = {in[i + 1], in[i + 2], 0};
            out[j++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else out[j++] = in[i];
    }
    out[j] = 0;
}

static esp_err_t root_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, portal_html, HTTPD_RESP_USE_STRLEN);
}

static void json_escape_ssid(char *out, size_t out_size, const uint8_t *ssid)
{
    size_t used = 0;
    for (size_t i = 0; ssid[i] && i < 32 && used + 2 < out_size; ++i) {
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
    if (esp_wifi_scan_get_ap_num(&found) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan failed");
    if (found > 20) found = 20;
    wifi_ap_record_t *records = calloc(found ? found : 1, sizeof(*records));
    char *json = malloc(6144);
    if (!records || !json) {
        free(records);
        free(json);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
    }
    uint16_t count = found;
    esp_err_t err = esp_wifi_scan_get_ap_records(&count, records);
    if (err != ESP_OK) {
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
    if (req->content_len <= 0 || req->content_len > 220) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Datos invalidos");
    char body[221] = {0};
    int received = 0;
    while (received < req->content_len) {
        int chunk = httpd_req_recv(req, body + received, req->content_len - received);
        if (chunk <= 0) return ESP_FAIL;
        received += chunk;
    }
    char encoded_ssid[97] = {0}, encoded_pass[193] = {0}, ssid[33] = {0}, pass[65] = {0};
    char *s = strstr(body, "s=");
    char *p = strstr(body, "&p=");
    if (!s || !p || p <= s + 2) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Falta la red");
    size_t s_len = (size_t)(p - (s + 2));
    if (s_len >= sizeof(encoded_ssid)) s_len = sizeof(encoded_ssid) - 1;
    memcpy(encoded_ssid, s + 2, s_len);
    strlcpy(encoded_pass, p + 3, sizeof(encoded_pass));
    url_decode(ssid, sizeof(ssid), encoded_ssid);
    url_decode(pass, sizeof(pass), encoded_pass);
    if (!ssid[0]) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Falta la red");
    save_credentials(ssid, pass);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_sendstr(req, "<html><meta name='viewport' content='width=device-width'><body style='background:#050807;color:#f0f7f4;font:20px system-ui;padding:40px'><h1>Listo ✦</h1><p>Aura intentara conectarse y ajustara la hora.</p></body></html>");
    wifi_action_t action = ACTION_SYNC;
    xQueueSend(actions, &action, 0);
    return ESP_OK;
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
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &ap);
    wifi_start();
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 4;
    if (httpd_start(&server, &cfg) == ESP_OK) {
        httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_get};
        httpd_uri_t scan = {.uri = "/scan", .method = HTTP_GET, .handler = scan_get};
        httpd_uri_t save = {.uri = "/save", .method = HTTP_POST, .handler = save_post};
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &scan);
        httpd_register_uri_handler(server, &save);
    }
    set_state(AURA_WIFI_SETUP);
}

static void connect_and_sync(void)
{
    if (!status.configured) { start_portal(); return; }
    wifi_stop();
    xEventGroupClearBits(events, GOT_IP_BIT | FAIL_BIT);
    xSemaphoreTake(lock, portMAX_DELAY);
    status.disconnect_reason = 0;
    xSemaphoreGive(lock);
    wifi_config_t sta = {0};
    strlcpy((char *)sta.sta.ssid, status.ssid, sizeof(sta.sta.ssid));
    strlcpy((char *)sta.sta.password, password, sizeof(sta.sta.password));
    sta.sta.threshold.authmode = WIFI_AUTH_OPEN;
    sta.sta.pmf_cfg.capable = true;
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &sta);
    wifi_start();
    set_state(AURA_WIFI_CONNECTING);
    esp_wifi_connect();
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
    for (int i = 0; i < 30; ++i) {
        sync_status = esp_sntp_get_sync_status();
        if (sync_status == SNTP_SYNC_STATUS_COMPLETED) break;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    // Reading COMPLETED resets the IDF status, so preserve that first result.
    bool synced = sync_status == SNTP_SYNC_STATUS_COMPLETED;
    esp_sntp_stop();
    if (synced) {
        time_t now = time(NULL);
        aura_clock_set(now, aura_clock_offset());
        nvs_handle_t nvs;
        if (nvs_open_from_partition("aura_cfg", "wifi", NVS_READWRITE, &nvs) == ESP_OK) {
            nvs_set_i64(nvs, "last_sync", now);
            nvs_commit(nvs);
            nvs_close(nvs);
        }
        xSemaphoreTake(lock, portMAX_DELAY);
        status.last_sync = now;
        status.state = AURA_WIFI_SYNCED;
        xSemaphoreGive(lock);
    } else set_state(AURA_WIFI_ERROR);
    vTaskDelay(pdMS_TO_TICKS(1800));
    wifi_stop();
}

static void wifi_task(void *arg)
{
    (void)arg;
    wifi_action_t action;
    for (;;) {
        if (xQueueReceive(actions, &action, pdMS_TO_TICKS(12 * 60 * 60 * 1000)) != pdTRUE) {
            if (status.configured) connect_and_sync();
            continue;
        }
        if (action == ACTION_SYNC) connect_and_sync();
        else if (action == ACTION_SETUP) {
            start_portal();
            if (xQueueReceive(actions, &action, pdMS_TO_TICKS(SETUP_LIFETIME_MS)) == pdTRUE) {
                if (action == ACTION_SYNC) connect_and_sync();
                else if (action == ACTION_FORGET) wifi_stop();
            } else wifi_stop();
            if (status.state == AURA_WIFI_SETUP) set_state(AURA_WIFI_OFF);
        } else if (action == ACTION_FORGET) wifi_stop();
    }
}

void aura_wifi_init(void)
{
    lock = xSemaphoreCreateMutex();
    events = xEventGroupCreate();
    actions = xQueueCreate(4, sizeof(wifi_action_t));
    configASSERT(lock && events && actions);
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
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
    xTaskCreate(wifi_task, "aura_wifi", 8192, NULL, 4, NULL);
    if (status.configured) aura_wifi_request_sync();
    ESP_LOGI(TAG, "configured=%d ssid=%s", status.configured, status.configured ? status.ssid : "-");
}

void aura_wifi_get_status(aura_wifi_status_t *out)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    *out = status;
    xSemaphoreGive(lock);
}

void aura_wifi_request_sync(void)
{
    wifi_action_t action = ACTION_SYNC;
    xQueueSend(actions, &action, 0);
}

void aura_wifi_start_setup(void)
{
    wifi_action_t action = ACTION_SETUP;
    xQueueSend(actions, &action, 0);
}

void aura_wifi_forget(void)
{
    wifi_action_t action = ACTION_FORGET;
    xQueueSend(actions, &action, 0);
    nvs_handle_t nvs;
    if (nvs_open_from_partition("aura_cfg", "wifi", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_erase_all(nvs);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
    xSemaphoreTake(lock, portMAX_DELAY);
    memset(status.ssid, 0, sizeof(status.ssid));
    memset(password, 0, sizeof(password));
    status.configured = false;
    status.last_sync = 0;
    status.disconnect_reason = 0;
    status.state = AURA_WIFI_OFF;
    xSemaphoreGive(lock);
}

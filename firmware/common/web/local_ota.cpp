#include "local_ota.h"

#include <Update.h>
#include "local_web_security.h"
#include "ota_image_guard.h"

namespace {
WebServer *web = nullptr;
const LocalOtaOptions *settings = nullptr;
bool uploadAllowed = false;
bool updateStarted = false;
String validationError;
bool rebootPending = false;
uint32_t rebootAtMs = 0;

bool authorize()
{
    if (!web || !settings || !settings->enabled) {
        if (web) web->send(403, "text/plain", "Local OTA is disabled");
        return false;
    }
    return LocalWebSecurity::authenticate(*web, settings->adminPassword);
}

String header(const char *title)
{
    String out = "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>";
    out += title;
    out += "</title><style>body{font-family:system-ui;margin:24px;max-width:760px}button,input{padding:10px;margin:8px 0;width:100%;box-sizing:border-box}.ok{color:#087f23}.bad{color:#b00020}</style></head><body>";
    return out;
}

void page()
{
    if (!authorize()) return;
    String out = header("MOT Local OTA");
    out += "<h1>Local OTA</h1><p>" + settings->firmwareLabel + "</p>";
    out += "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='firmware' accept='.bin' required><button>Upload &amp; update</button></form>";
    out += "<p><a href='/status'>Back to status</a></p></body></html>";
    web->send(200, "text/html", out);
}

void upload()
{
    HTTPUpload &item = web->upload();
    if (item.status == UPLOAD_FILE_START) {
        uploadAllowed = authorize() && LocalWebSecurity::requireSameOrigin(*web);
        updateStarted = false;
        validationError = "";
    } else if (item.status == UPLOAD_FILE_WRITE && uploadAllowed) {
        if (!updateStarted) {
            const OtaImageGuardResult result = otaValidateImageHeader(item.buf, item.currentSize);
            if (!result.accepted) {
                validationError = result.reason;
                uploadAllowed = false;
                Serial.println("OTA rejected: " + validationError);
                return;
            }
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                validationError = "OTA partition could not be opened";
                Update.printError(Serial);
                uploadAllowed = false;
                return;
            }
            updateStarted = true;
        }
        if (Update.write(item.buf, item.currentSize) != item.currentSize) Update.printError(Serial);
    } else if (item.status == UPLOAD_FILE_END && uploadAllowed) {
        if (!updateStarted || !Update.end(true)) Update.printError(Serial);
    } else if (item.status == UPLOAD_FILE_ABORTED) {
        if (updateStarted) Update.abort();
        uploadAllowed = false;
        updateStarted = false;
    }
}

void finished()
{
    if (!validationError.isEmpty()) {
        web->send(400, "text/html", header("OTA rejected") + "<h1 class='bad'>OTA rejected</h1><p>" + validationError + ".</p><p>The running firmware was not changed.</p><p><a href='/update'>Try again</a></p></body></html>");
        validationError = "";
        return;
    }
    if (!uploadAllowed) { web->send(401, "text/plain", "OTA not authorized"); return; }
    if (Update.hasError()) {
        web->send(500, "text/html", header("OTA failed") + "<h1 class='bad'>OTA failed</h1><p>The running firmware remains active. Check that the binary matches this board and try again.</p><p><a href='/update'>Try again</a></p></body></html>");
        uploadAllowed = false;
        updateStarted = false;
        return;
    }
    web->send(200, "text/html", header("OTA complete") + "<h1 class='ok'>OTA complete</h1><p>Verified image written. Rebooting shortly.</p></body></html>");
    rebootPending = true;
    rebootAtMs = millis() + 2500;
    updateStarted = false;
}
}

void localOtaSetup(WebServer &server, const LocalOtaOptions *options)
{
    web = &server;
    settings = options;
    server.on("/update", HTTP_GET, page);
    server.on("/ota", HTTP_GET, page);
    server.on("/update", HTTP_POST, finished, upload);
}

void localOtaLoop()
{
    if (rebootPending && static_cast<int32_t>(millis() - rebootAtMs) >= 0) ESP.restart();
}

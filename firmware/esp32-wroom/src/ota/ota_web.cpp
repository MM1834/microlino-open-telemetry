#include "ota_web.h"

#include <Arduino.h>
#include <Update.h>

#include "../app_config.h"
#include "system/version.h"
#include "system/device_id.h"

static WebServer *otaServer = nullptr;
static bool otaRebootPending = false;
static unsigned long otaRebootAtMs = 0;
static bool otaUploadAllowed = false;

static bool requireOtaAuth()
{
    if (!otaServer) return false;

    // If no OTA password is configured, allow OTA from the local Web UI.
    // For production/beta use, set an OTA password in Config.
    if (config.otaPassword.isEmpty()) {
        return true;
    }

    if (!otaServer->authenticate("admin", config.otaPassword.c_str())) {
        otaServer->requestAuthentication();
        return false;
    }

    return true;
}

static String pageHeader(const char *title)
{
    String s;
    s += "<!doctype html><html><head><meta charset='utf-8'>";
    s += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    s += "<title>"; s += title; s += "</title>";
    s += "<style>";
    s += "body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Arial,sans-serif;margin:24px;max-width:760px;background:#0b1020;color:#e8eefc}";
    s += "a{color:#7dd3fc}.card{border:1px solid #26334f;border-radius:14px;padding:18px;margin:14px 0;background:#111a2e}";
    s += "input,button{width:100%;padding:12px;margin:8px 0 14px;box-sizing:border-box;border-radius:10px;border:1px solid #334155}";
    s += "input{background:#0f172a;color:#e8eefc}button{background:#22c55e;color:#07111f;font-weight:700;cursor:pointer}";
    s += ".muted{color:#94a3b8}.warn{color:#fbbf24}.ok{color:#86efac}.bad{color:#fca5a5}";
    s += "</style></head><body>";
    return s;
}

static void handleOtaPage()
{
    if (!requireOtaAuth()) return;

    String s = pageHeader("MOT OTA Update");
    s += "<h1>MOT OTA Update</h1>";
    s += "<p class='muted'>" MOT_VERSION " · ";
    s += motDeviceId();
    s += "</p>";
    s += "<div class='card'>";
    s += "<h2>Upload firmware</h2>";
    s += "<p>Select a PlatformIO firmware binary, usually:<br><code>.pio/build/esp32dev/firmware.bin</code></p>";
    if (config.otaPassword.isEmpty()) {
        s += "<p class='warn'>Warning: no OTA password is configured. Set one under Config before beta use.</p>";
    }
    s += "<form method='POST' action='/update' enctype='multipart/form-data'>";
    s += "<input type='file' name='firmware' accept='.bin' required>";
    s += "<button type='submit'>Upload & Update</button>";
    s += "</form>";
    s += "</div>";
    s += "<p><a href='/status'>Back to status</a> · <a href='/config'>Config</a></p>";
    s += "</body></html>";

    otaServer->send(200, "text/html", s);
}

static void handleOtaFinished()
{
    if (!otaUploadAllowed) {
        otaServer->send(401, "text/plain", "OTA not authorized");
        return;
    }

    if (Update.hasError()) {
        String s = pageHeader("MOT OTA Failed");
        s += "<h1 class='bad'>OTA update failed</h1>";
        s += "<p>Please check that the uploaded file is a valid ESP32 firmware.bin.</p>";
        s += "<p><a href='/update'>Try again</a></p></body></html>";
        otaServer->send(500, "text/html", s);
        return;
    }

    String s = pageHeader("MOT OTA Complete");
    s += "<h1 class='ok'>OTA update complete</h1>";
    s += "<p>The device will reboot in a few seconds.</p>";
    s += "</body></html>";
    otaServer->send(200, "text/html", s);

    otaRebootPending = true;
    otaRebootAtMs = millis() + 2500;
}

static void handleOtaUpload()
{
    if (!otaServer) return;

    HTTPUpload &upload = otaServer->upload();

    if (upload.status == UPLOAD_FILE_START) {
        otaUploadAllowed = requireOtaAuth();
        if (!otaUploadAllowed) return;

        Serial.printf("OTA: update start: %s\n", upload.filename.c_str());

        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Serial.println("OTA: Update.begin failed");
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!otaUploadAllowed) return;

        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Serial.println("OTA: write failed");
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (!otaUploadAllowed) return;

        if (Update.end(true)) {
            Serial.printf("OTA: update success, %u bytes\n", upload.totalSize);
        } else {
            Serial.println("OTA: update end failed");
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Serial.println("OTA: update aborted");
        Update.abort();
        otaUploadAllowed = false;
    }
}

void setupOtaRoutes(WebServer &server)
{
    otaServer = &server;

    server.on("/update", HTTP_GET, handleOtaPage);
    server.on("/ota", HTTP_GET, handleOtaPage);
    server.on("/update", HTTP_POST, handleOtaFinished, handleOtaUpload);
}

void otaWebLoop()
{
    if (otaRebootPending && millis() > otaRebootAtMs) {
        Serial.println("OTA: rebooting now...");
        delay(100);
        ESP.restart();
    }
}

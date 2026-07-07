#include "lilygo_lte_client.h"

#include "modem/lilygo_modem.h"

static uint32_t connectCalls = 0;
static uint32_t writeCalls = 0;
static uint32_t readCalls = 0;
static uint32_t availableCalls = 0;
static uint32_t stopCalls = 0;
static int lastAvailable = 0;
static int lastRead = 0;
static int lastWritten = 0;
static String trace;

static void traceAppend(const String& line)
{
    trace += String(millis()) + "ms " + line + "\n";

    if (trace.length() > 5000) {
        trace.remove(0, trace.length() - 5000);
    }
}

static String esc(String value)
{
    value.replace("\\", "\\\\");
    value.replace("\"", "\\\"");
    value.replace("\r", "\\r");
    value.replace("\n", "\\n");
    return value;
}

String lilygoLteClientTraceJson()
{
    String json = "{";
    json += "\"backend\":\"LewisXhe TinyGSM A76XXSSL wrapper\",";
    json += "\"connectCalls\":" + String(connectCalls) + ",";
    json += "\"writeCalls\":" + String(writeCalls) + ",";
    json += "\"readCalls\":" + String(readCalls) + ",";
    json += "\"availableCalls\":" + String(availableCalls) + ",";
    json += "\"stopCalls\":" + String(stopCalls) + ",";
    json += "\"lastAvailable\":" + String(lastAvailable) + ",";
    json += "\"lastRead\":" + String(lastRead) + ",";
    json += "\"lastWritten\":" + String(lastWritten) + ",";
    json += "\"connected\":" + String(lilygoLteTcpConnected() ? "true" : "false") + ",";
    json += "\"trace\":\"" + esc(trace) + "\"";
    json += "}";
    return json;
}

void lilygoLteClientTraceClear()
{
    trace = "";
    connectCalls = 0;
    writeCalls = 0;
    readCalls = 0;
    availableCalls = 0;
    stopCalls = 0;
    lastAvailable = 0;
    lastRead = 0;
    lastWritten = 0;
}

int LilygoLteClient::connect(const char *host, uint16_t port)
{
    connectCalls++;
    traceAppend("connect host=" + String(host) + " port=" + String(port));

    connectedFlag = lilygoLteTcpOpen(String(host), port);

    traceAppend(String("connect result=") + (connectedFlag ? "1" : "0"));
    return connectedFlag ? 1 : 0;
}

int LilygoLteClient::connect(IPAddress ip, uint16_t port)
{
    char host[24];
    snprintf(host, sizeof(host), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    return connect(host, port);
}

size_t LilygoLteClient::write(uint8_t b)
{
    return write(&b, 1);
}

size_t LilygoLteClient::write(const uint8_t *buf, size_t size)
{
    writeCalls++;

    if (!connectedFlag) {
        lastWritten = 0;
        return 0;
    }

    int n = lilygoLteTcpWrite(buf, size);
    lastWritten = n;

    if (n <= 0) {
        connectedFlag = lilygoLteTcpConnected();
    }

    traceAppend("write size=" + String(size) + " result=" + String(n));
    return n > 0 ? (size_t)n : 0;
}

int LilygoLteClient::available()
{
    availableCalls++;

    if (!connectedFlag) {
        lastAvailable = 0;
        return 0;
    }

    lastAvailable = lilygoLteTcpAvailable();
    return lastAvailable;
}

int LilygoLteClient::read()
{
    uint8_t b = 0;
    int n = read(&b, 1);
    return n == 1 ? b : -1;
}

int LilygoLteClient::read(uint8_t *buf, size_t size)
{
    readCalls++;

    if (!connectedFlag) {
        lastRead = 0;
        return 0;
    }

    int n = lilygoLteTcpRead(buf, size);
    lastRead = n;

    if (n < 0) {
        connectedFlag = lilygoLteTcpConnected();
        return 0;
    }

    traceAppend("read size=" + String(size) + " result=" + String(n));
    return n;
}

int LilygoLteClient::peek()
{
    return -1;
}

void LilygoLteClient::flush()
{
}

void LilygoLteClient::stop()
{
    stopCalls++;
    lilygoLteTcpClose();
    connectedFlag = false;
    traceAppend("stop");
}

uint8_t LilygoLteClient::connected()
{
    connectedFlag = lilygoLteTcpConnected();
    return connectedFlag ? 1 : 0;
}

LilygoLteClient::operator bool()
{
    return connected();
}

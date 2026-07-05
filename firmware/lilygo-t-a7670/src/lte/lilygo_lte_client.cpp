#include "lte/lilygo_lte_client.h"
#include "modem/lilygo_modem.h"

int LilygoLteClient::connect(IPAddress ip, uint16_t port)
{
    char host[24];
    snprintf(host, sizeof(host), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    return connect(host, port);
}

int LilygoLteClient::connect(const char *host, uint16_t port)
{
    connectedFlag = lilygoLteTcpOpen(String(host), port);
    return connectedFlag ? 1 : 0;
}

size_t LilygoLteClient::write(uint8_t b) { return write(&b, 1); }

size_t LilygoLteClient::write(const uint8_t *buf, size_t size)
{
    if (!connectedFlag) return 0;
    int written = lilygoLteTcpWrite(buf, size);
    if (written <= 0) {
        connectedFlag = lilygoLteTcpConnected();
        return 0;
    }
    return (size_t)written;
}

int LilygoLteClient::available()
{
    return connectedFlag ? lilygoLteTcpAvailable() : 0;
}

int LilygoLteClient::read()
{
    uint8_t b;
    int n = read(&b, 1);
    return n == 1 ? b : -1;
}

int LilygoLteClient::read(uint8_t *buf, size_t size)
{
    if (!connectedFlag) return 0;
    int n = lilygoLteTcpRead(buf, size);
    if (n < 0) {
        connectedFlag = lilygoLteTcpConnected();
        return 0;
    }
    return n;
}

int LilygoLteClient::peek() { return -1; }
void LilygoLteClient::flush() {}
void LilygoLteClient::stop() { lilygoLteTcpClose(); connectedFlag = false; }
uint8_t LilygoLteClient::connected() { connectedFlag = lilygoLteTcpConnected(); return connectedFlag ? 1 : 0; }
LilygoLteClient::operator bool() { return connected(); }

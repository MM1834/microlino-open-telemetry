> **Historical implementation note**  
> This document describes a specific development step. It may be superseded by the current firmware. Use it for investigation history, not as the primary user guide.

# LilyGO Network Failover

Implements network priority for LilyGO:

1. Setup AP remains active.
2. WiFi is preferred if configured and connected.
3. If WiFi is lost for more than 20 seconds, fail over to LTE.
4. While on LTE, retry WiFi every 60 seconds.
5. If LTE is not ready, retry LTE every 15 seconds and stay AP-only until a route is available.

The modem exposes `lilygoEnsureGprsConnected()` for network failover to request a GPRS reconnect.

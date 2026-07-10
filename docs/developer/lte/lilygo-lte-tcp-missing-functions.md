> **Historical implementation note**  
> This document describes a specific development step. It may be superseded by the current firmware. Use it for investigation history, not as the primary user guide.

# LilyGO LTE TCP Missing Functions Fix

Adds the missing modem-side LTE TCP functions referenced by `LilygoLteClient`:

- `lilygoLteTcpOpen`
- `lilygoLteTcpWrite`
- `lilygoLteTcpAvailable`
- `lilygoLteTcpRead`
- `lilygoLteTcpClose`
- `lilygoLteTcpConnected`

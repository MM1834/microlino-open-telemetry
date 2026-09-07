const BAUD_RATE = 115200;
const RECOVERY_COMMAND = 'admin recover';
const RECOVERY_MARKER = 'NEW LOCAL ADMIN PASSWORD:';
const USB_FILTERS = [
  { usbVendorId: 0x1a86, usbProductId: 0x55d3 },
  { usbVendorId: 0x2886, usbProductId: 0x0046 },
  { usbVendorId: 0x303a, usbProductId: 0x1001 }
];

const delay = milliseconds => new Promise(resolve => window.setTimeout(resolve, milliseconds));

function recoveryPassword(text) {
  const markerAt = text.indexOf(RECOVERY_MARKER);
  if (markerAt < 0) return '';
  const remainder = text.slice(markerAt + RECOVERY_MARKER.length);
  const line = remainder.split(/[\r\n]/, 1)[0].trim();
  return /^[\x20-\x7e]{12,63}$/.test(line) ? line : '';
}

function friendlyError(error) {
  const detail = String(error?.message || error || '');
  if (/No port selected|chooser/i.test(detail)) return 'Keine serielle Schnittstelle ausgewählt.';
  if (/open|busy|claim|access denied|in use/i.test(detail)) {
    return 'Serielle Schnittstelle ist belegt. Bitte andere serielle Konsolen schließen und erneut versuchen.';
  }
  return detail || 'Passwort-Recovery fehlgeschlagen.';
}

export function createPasswordRecovery({ onStatus, onPassword } = {}) {
  let busy = false;

  return {
    supported() {
      return Boolean(window.isSecureContext && navigator.serial);
    },

    async recover({ authorize, reportResult }) {
      if (busy) return;
      if (!this.supported()) throw new Error('Web Serial ist nicht verfügbar.');
      busy = true;
      let operationId = '';
      let port = null;
      let reader = null;
      let writer = null;
      try {
        onStatus?.('Server-Freigabe wird geprüft…');
        const authorization = await authorize();
        operationId = String(authorization?.operationId || '');
        if (!/^[A-Za-z0-9_-]{22,64}$/.test(operationId)) {
          throw new Error('Die Passwort-Recovery wurde nicht autorisiert.');
        }

        onStatus?.('USB-Gerät auswählen…');
        port = await navigator.serial.requestPort({ filters: USB_FILTERS });
        await port.open({ baudRate: BAUD_RATE });
        reader = port.readable.getReader();
        writer = port.writable.getWriter();
        const decoder = new TextDecoder();
        let received = '';
        let timedOut = false;
        const timeoutId = window.setTimeout(() => {
          timedOut = true;
          reader?.cancel().catch(() => {});
        }, 15000);

        onStatus?.('Adapter startet. Recovery-Befehl wird lokal übertragen…');
        await delay(1200);
        await writer.write(new TextEncoder().encode(`\r\n${RECOVERY_COMMAND}\r\n`));
        while (!timedOut) {
          const { value, done } = await reader.read();
          if (done) break;
          received += decoder.decode(value, { stream: true });
          const password = recoveryPassword(received);
          if (password) {
            window.clearTimeout(timeoutId);
            onPassword?.(password);
            try {
              await reportResult(operationId, 'SUCCEEDED');
              onStatus?.('Passwort wurde ersetzt. Jetzt sichern und den lokalen MOT-Hotspot neu verbinden.', 'success');
            } catch (reportError) {
              console.warn('Password recovery success audit could not be recorded:', reportError);
              onStatus?.('Passwort wurde lokal ersetzt, aber die Erfolgsmeldung an den Server ist fehlgeschlagen. Passwort jetzt sichern.', 'success');
            }
            return;
          }
          if (received.length > 8192) received = received.slice(-4096);
        }
        window.clearTimeout(timeoutId);
        throw new Error('Keine Recovery-Antwort empfangen. Adapter normal starten und erneut versuchen.');
      } catch (error) {
        if (operationId) {
          try { await reportResult(operationId, 'FAILED'); } catch (reportError) {
            console.warn('Password recovery failure audit could not be recorded:', reportError);
          }
        }
        const message = friendlyError(error);
        onStatus?.(message, 'error');
        throw new Error(message, { cause: error });
      } finally {
        busy = false;
        try { writer?.releaseLock(); } catch (error) { console.debug('Serial writer release:', error); }
        try { await reader?.cancel(); } catch (error) { console.debug('Serial reader cancel:', error); }
        try { reader?.releaseLock(); } catch (error) { console.debug('Serial reader release:', error); }
        try { await port?.close(); } catch (error) { console.debug('Serial close:', error); }
      }
    }
  };
}

export { recoveryPassword };

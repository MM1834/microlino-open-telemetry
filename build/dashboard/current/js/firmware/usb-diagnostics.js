const BAUD_RATE = 115200;
const READ_TIMEOUT_MS = 10000;
const USB_FILTERS = [
  { usbVendorId: 0x1a86, usbProductId: 0x55d3 },
  { usbVendorId: 0x2886, usbProductId: 0x0046 },
  { usbVendorId: 0x303a, usbProductId: 0x1001 }
];
const SAFE_PREFIXES = [
  'Profiles:',
  'Profiles configured:',
  'WiFi:',
  'WiFi profiles:',
  'AWS IoT:',
  'Offline cache:',
  'Support system:',
  'Support network:',
  'Support AWS:',
  'Support cache:',
  'Support time:',
  'Support CAN1:',
  'Support CAN2:'
];

export function safeDiagnosticLines(text) {
  return String(text || '')
    .split(/\r?\n/)
    .map(line => line.trim())
    .filter(line => SAFE_PREFIXES.some(prefix => line.startsWith(prefix)));
}

function friendlyError(error) {
  if (error?.name === 'NotFoundError') return 'Keine USB-Verbindung ausgewählt.';
  if (error?.name === 'NetworkError') return 'USB-Gerät ist belegt oder nicht erreichbar.';
  return error?.message || 'USB-Diagnose fehlgeschlagen.';
}

export function createUsbDiagnostics({ onStatus = () => {}, onLog = () => {} } = {}) {
  let busy = false;

  async function run({ authorize, reportResult }) {
    if (busy) throw new Error('USB-Diagnose läuft bereits.');
    if (!navigator.serial) throw new Error('Web Serial wird von diesem Browser nicht unterstützt.');
    busy = true;
    let operationId = '';
    let port;
    let reader;
    let writer;
    let result = 'FAILED';
    try {
      onStatus('Diagnose wird autorisiert…');
      operationId = String((await authorize())?.operationId || '');
      if (!operationId) throw new Error('Diagnosefreigabe konnte nicht bestätigt werden.');
      onStatus('USB-Gerät auswählen…');
      port = await navigator.serial.requestPort({ filters: USB_FILTERS });
      await port.open({ baudRate: BAUD_RATE });
      // Do not touch DTR/RTS in the read-only console path. Web Serial applies
      // these lines independently; even a final false/false state can briefly
      // trigger the ESP32-C6 auto-reset/boot circuit on CH343-backed adapters.
      await new Promise(resolve => setTimeout(resolve, 900));

      writer = port.writable.getWriter();
      reader = port.readable.getReader();
      const encoder = new TextEncoder();
      const decoder = new TextDecoder();
      await writer.write(encoder.encode('\r\nsupport status\r\nprofiles\r\nwifi status\r\naws status\r\ncache status\r\n'));
      onStatus('Adapterstatus wird ausgelesen…');

      let raw = '';
      const deadline = Date.now() + READ_TIMEOUT_MS;
      while (Date.now() < deadline) {
        const remaining = Math.max(1, deadline - Date.now());
        const chunk = await Promise.race([
          reader.read(),
          new Promise(resolve => setTimeout(() => resolve({ timeout: true }), remaining))
        ]);
        if (chunk.timeout || chunk.done) break;
        raw = `${raw}${decoder.decode(chunk.value, { stream: true })}`.slice(-16384);
        if (raw.includes('AWS IoT:') && raw.includes('Offline cache:')) break;
      }
      const lines = safeDiagnosticLines(raw);
      lines.forEach(onLog);
      if (!lines.some(line => line.startsWith('WiFi:'))
          || !lines.some(line => line.startsWith('AWS IoT:'))) {
        throw new Error('Keine vollständige WiFi-/AWS-Statusantwort empfangen. Bitte erneut versuchen.');
      }
      result = 'SUCCEEDED';
      onStatus('USB-Diagnose abgeschlossen.', 'success');
      return lines;
    } catch (error) {
      onStatus(friendlyError(error), 'error');
      throw error;
    } finally {
      if (operationId) {
        try { await reportResult(operationId, result); }
        catch (error) { console.warn('Diagnoseergebnis konnte nicht protokolliert werden:', error?.message || error); }
      }
      try { await reader?.cancel(); } catch (_) { /* already closed */ }
      try { reader?.releaseLock(); } catch (_) { /* already released */ }
      try { writer?.releaseLock(); } catch (_) { /* already released */ }
      try { await port?.close(); } catch (_) { /* already closed */ }
      busy = false;
    }
  }

  return { supported: () => Boolean(navigator.serial), run };
}

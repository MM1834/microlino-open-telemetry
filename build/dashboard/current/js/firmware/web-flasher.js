import { ESPLoader, Transport } from '../../vendor/esptool-js/bundle-0.6.0.js';

const CHIP_FAMILY = 'ESP32-C6';
const APPLICATION_OFFSET = 0x10000;
const SAFE_BAUDRATE = 115200;
const ESP32_C6_SPI_REG_BASE = 0x60003000;
const TARGETS = {
  'nanoesp32c6-n16': {
    flashSizeBytes: 16 * 1024 * 1024,
    flashSizeLabel: '16MB',
    usbIds: [[0x1a86, 0x55d3]]
  },
  'xiao-esp32c6': {
    flashSizeBytes: 4 * 1024 * 1024,
    flashSizeLabel: '4MB',
    usbIds: [[0x2886, 0x0046], [0x303a, 0x1001]]
  }
};
const USB_FILTERS = Object.values(TARGETS).flatMap(profile =>
  profile.usbIds.map(([usbVendorId, usbProductId]) => ({ usbVendorId, usbProductId }))
);

function targetProfile(release) {
  return TARGETS[String(release?.target || '')] || null;
}

function installEsp32C6RomCompatibility(loader) {
  // esptool-js 0.6.0 omits the encrypted-write argument for ESP32-C6 ROM.
  // Current official esptool sends this fifth uint32 for every post-ESP32 ROM,
  // while the older bundled stub still expects its historical 16-byte request.
  loader.flashDeflBegin = async function flashDeflBegin(size, compressedSize, offset) {
    const compressedBlocks = Math.floor(
      (compressedSize + this.FLASH_WRITE_SIZE - 1) / this.FLASH_WRITE_SIZE
    );
    const eraseBlocks = Math.floor((size + this.FLASH_WRITE_SIZE - 1) / this.FLASH_WRITE_SIZE);
    const writeSize = this.IS_STUB ? size : eraseBlocks * this.FLASH_WRITE_SIZE;
    const timeout = this.IS_STUB
      ? this.DEFAULT_TIMEOUT
      : this.timeoutPerMb(this.ERASE_REGION_TIMEOUT_PER_MB, writeSize);
    this.info(`Compressed ${size} bytes to ${compressedSize}...`);
    let packet = this._appendArray(
      this._intToByteArray(writeSize),
      this._intToByteArray(compressedBlocks)
    );
    packet = this._appendArray(packet, this._intToByteArray(this.FLASH_WRITE_SIZE));
    packet = this._appendArray(packet, this._intToByteArray(offset));
    if (!this.IS_STUB) {
      packet = this._appendArray(packet, this._intToByteArray(0));
    }
    await this.checkCommand(
      'enter compressed flash mode',
      this.ESP_FLASH_DEFL_BEGIN,
      packet,
      undefined,
      undefined,
      timeout
    );
    return compressedBlocks;
  };
}

function assertRelease(release) {
  const profile = targetProfile(release);
  if (!release || !profile || release.chipFamily !== CHIP_FAMILY
      || Number(release.flashSizeBytes) !== profile.flashSizeBytes
      || Number(release.offset) !== APPLICATION_OFFSET
      || !Number.isSafeInteger(Number(release.size)) || Number(release.size) <= 0
      || !/^[a-f0-9]{64}$/.test(String(release.sha256 || ''))
      || release.factoryErase !== false) {
    throw new Error('Die freigegebene Firmware ist nicht mit dem N16-Web-Flasher kompatibel.');
  }
  if (APPLICATION_OFFSET + Number(release.size) > profile.flashSizeBytes) {
    throw new Error('Die freigegebene Firmware passt nicht in den vorgesehenen Flash-Bereich.');
  }
  return release;
}

function releasesMatch(left, right) {
  return ['target', 'version', 'chipFamily', 'flashSizeBytes', 'offset', 'size', 'sha256', 'factoryErase']
    .every(key => left?.[key] === right?.[key]);
}

async function sha256Hex(bytes) {
  const digest = await crypto.subtle.digest('SHA-256', bytes);
  return Array.from(new Uint8Array(digest), byte => byte.toString(16).padStart(2, '0')).join('');
}

function connectionErrorMessage(error) {
  const detail = String(error?.message || error || '');
  if (/open|busy|claim|access denied|in use/i.test(detail)) {
    return 'Serielle Schnittstelle ist belegt. Bitte PlatformIO Device Monitor und andere serielle Konsolen schließen, den Adapter kurz aus- und wieder einstecken und erneut versuchen.';
  }
  if (/failed to connect|connect with the device|sync/i.test(detail)) {
    return 'Verbindung mit dem Adapter fehlgeschlagen. Bitte serielle Konsolen schließen, den Adapter kurz aus- und wieder einstecken und erneut versuchen.';
  }
  return detail || 'Adapterprüfung fehlgeschlagen.';
}

export function createWebFlasher({ onStatus, onProgress, onLog } = {}) {
  let transport = null;
  let loader = null;
  let approvedRelease = null;
  let deviceInfo = null;
  let busy = false;

  const status = (message, level = 'info') => onStatus?.(message, level);
  const log = message => onLog?.(String(message));
  const terminal = {
    clean() {},
    writeLine(data) { log(data); },
    write(data) { log(data); }
  };

  async function disconnect() {
    const currentTransport = transport;
    loader = null;
    transport = null;
    deviceInfo = null;
    if (currentTransport) {
      try { await currentTransport.disconnect(); } catch (error) { console.debug('Serial disconnect:', error); }
    }
  }

  return {
    supported() {
      return Boolean(window.isSecureContext && navigator.serial && window.crypto?.subtle);
    },

    async connect(release) {
      if (busy) return null;
      if (!this.supported()) {
        throw new Error('Web Serial ist nicht verfügbar. Bitte Chrome oder Edge über HTTPS verwenden.');
      }
      busy = true;
      approvedRelease = assertRelease(release);
      await disconnect();
      try {
        status('USB-Gerät auswählen und Verbindung prüfen…');
        const port = await navigator.serial.requestPort({ filters: USB_FILTERS });
        const usb = port.getInfo?.() || {};
        const profile = targetProfile(approvedRelease);
        const usbMatchesTarget = profile.usbIds.some(
          ([vendorId, productId]) => usb.usbVendorId === vendorId && usb.usbProductId === productId
        );
        if (!usbMatchesTarget) {
          throw new Error('Nicht unterstützter USB-Adapter ausgewählt.');
        }
        transport = new Transport(port, false);
        // CH343-backed nanoESP32-C6 units returned Flash ID 0 from the bundled
        // C6 stub. Stay in the ROM loader and keep all traffic at 115200.
        loader = new ESPLoader({ transport, baudrate: SAFE_BAUDRATE, terminal });
        await loader.detectChip();
        const chipName = String(await loader.chip.getChipDescription(loader));
        loader.info(`Chip is ${chipName}`);
        loader.info(`Features: ${(await loader.chip.getChipFeatures(loader)).join(',')}`);
        loader.info(`Crystal is ${await loader.chip.getCrystalFreq(loader)}MHz`);
        loader.info(`MAC: ${await loader.chip.readMac(loader)}`);
        if (!chipName.toUpperCase().includes(CHIP_FAMILY)) {
          throw new Error(`Falscher Chip erkannt (${chipName || 'unbekannt'} statt ESP32-C6).`);
        }
        // esptool-js 0.6.0 still uses the older C3 register base and a four-byte
        // SPI_ATTACH request for C6. Match current official Python esptool:
        // C6 SPI2 base 0x60003000 and the ROM's additional four reserved bytes.
        loader.chip.SPI_REG_BASE = ESP32_C6_SPI_REG_BASE;
        installEsp32C6RomCompatibility(loader);
        const spiAttachPacket = new Uint8Array(8);
        await loader.checkCommand('configure SPI flash pins', loader.ESP_SPI_ATTACH, spiAttachPacket);
        loader.info('SPI flash attached (ESP32-C6 ROM)');
        const flashId = await loader.readFlashId();
        if (flashId === 0 || flashId === 0xffffff) {
          throw new Error('Flash-Speicher konnte nicht ausgelesen werden. Die Verbindung ist nicht stabil; es wurde nichts geschrieben.');
        }
        const flashSize = loader.DETECTED_FLASH_SIZES[(flashId >>> 16) & 0xff];
        if (!flashSize) {
          throw new Error(`Unbekannter Flash-Speicher erkannt (ID 0x${flashId.toString(16)}).`);
        }
        const flashSizeBytes = loader.flashSizeBytes(flashSize);
        if (flashSizeBytes !== profile.flashSizeBytes) {
          throw new Error(`Falsche Flash-Größe erkannt (${flashSize || 'unbekannt'} statt ${profile.flashSizeLabel}).`);
        }
        // ESP32-C6 ROM can identify the flash but does not implement compressed
        // application writes. Start the bundled stub only after the fail-closed
        // ROM preflight, retain 115200 baud and re-check the same JEDEC flash.
        await loader.runStub();
        const stubFlashId = await loader.readFlashId();
        if (stubFlashId !== flashId) {
          throw new Error('Flash-Speicher ist nach dem Start des Schreibprogramms nicht stabil erreichbar; es wurde nichts geschrieben.');
        }
        loader.info('ESP32-C6 flasher ready at 115200 baud');
        deviceInfo = { chipName, flashSize, usbVendorId: usb.usbVendorId, usbProductId: usb.usbProductId };
        status(`Adapter geprüft: ${chipName}, ${flashSize}.`, 'success');
        return deviceInfo;
      } catch (error) {
        await disconnect();
        const message = connectionErrorMessage(error);
        status(message, 'error');
        throw new Error(message, { cause: error });
      } finally {
        busy = false;
      }
    },

    async flash({ authorizeDownload, reportResult }) {
      if (busy) return;
      if (!loader || !deviceInfo || !approvedRelease) {
        throw new Error('Bitte zuerst den Adapter verbinden und prüfen.');
      }
      busy = true;
      let operationId = '';
      try {
        status('Freigabe und Firmware werden geprüft…');
        const authorization = await authorizeDownload();
        operationId = String(authorization?.operationId || '');
        const release = assertRelease(authorization?.release);
        const profile = targetProfile(release);
        if (!operationId || !releasesMatch(approvedRelease, release)) {
          throw new Error('Die Firmware-Freigabe hat sich geändert. Bitte Seite neu laden.');
        }
        const response = await fetch(String(authorization.url || ''), { cache: 'no-store' });
        if (!response.ok) throw new Error(`Firmware-Download fehlgeschlagen (HTTP ${response.status}).`);
        const image = new Uint8Array(await response.arrayBuffer());
        if (image.byteLength !== Number(release.size)) {
          throw new Error('Die Firmware-Größe stimmt nicht mit der Freigabe überein.');
        }
        const actualSha256 = await sha256Hex(image);
        if (actualSha256 !== release.sha256) {
          throw new Error('Die Firmware-Prüfsumme stimmt nicht. Es wurde nichts geschrieben.');
        }
        status('Firmware geprüft. Update wird geschrieben – USB nicht trennen.');
        await loader.writeFlash({
          fileArray: [{ data: image, address: APPLICATION_OFFSET }],
          flashMode: 'keep',
          flashFreq: 'keep',
          flashSize: profile.flashSizeLabel,
          eraseAll: false,
          compress: true,
          reportProgress: (_fileIndex, written, total) => {
            onProgress?.(total > 0 ? Math.round((written / total) * 100) : 0);
          }
        });
        onProgress?.(100);
        await loader.after('hard_reset');
        await reportResult(operationId, 'SUCCEEDED', approvedRelease.target);
        status('Update erfolgreich. Der Adapter wurde neu gestartet.', 'success');
      } catch (error) {
        if (operationId) {
          try { await reportResult(operationId, 'FAILED', approvedRelease.target); } catch (reportError) {
            console.warn('Firmware failure audit could not be recorded:', reportError);
          }
        }
        status(error?.message || 'Firmware-Update fehlgeschlagen.', 'error');
        throw error;
      } finally {
        busy = false;
        await disconnect();
      }
    },

    disconnect,
    getDeviceInfo() { return deviceInfo; }
  };
}

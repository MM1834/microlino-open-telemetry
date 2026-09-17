import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
DASHBOARD = ROOT / "build" / "dashboard" / "current"


class DashboardWebFlasherContractTests(unittest.TestCase):
    def test_pilot_ui_has_no_arbitrary_file_or_erase_control(self) -> None:
        html = (DASHBOARD / "index.html").read_text(encoding="utf-8")
        self.assertIn('id="firmware-flasher"', html)
        self.assertIn('id="firmware-confirm"', html)
        self.assertNotIn('type="file"', html)
        self.assertNotIn('id="firmware-erase"', html)

    def test_flashing_is_fixed_to_supported_c6_application_and_preserves_configuration(self) -> None:
        source = (DASHBOARD / "js/firmware/web-flasher.js").read_text(encoding="utf-8")
        self.assertIn("'nanoesp32c6-n16'", source)
        self.assertIn("'xiao-esp32c6'", source)
        self.assertIn("const CHIP_FAMILY = 'ESP32-C6'", source)
        self.assertIn("flashSizeBytes: 16 * 1024 * 1024", source)
        self.assertIn("flashSizeBytes: 4 * 1024 * 1024", source)
        self.assertIn("const APPLICATION_OFFSET = 0x10000", source)
        self.assertIn("const SAFE_BAUDRATE = 115200", source)
        self.assertIn("const ESP32_C6_SPI_REG_BASE = 0x60003000", source)
        self.assertIn("await loader.detectChip()", source)
        self.assertNotIn("await loader.main()", source)
        self.assertIn("loader.chip.SPI_REG_BASE = ESP32_C6_SPI_REG_BASE", source)
        self.assertIn("installEsp32C6RomCompatibility(loader)", source)
        self.assertIn("loader.flashDeflBegin = async function", source)
        self.assertIn("if (!this.IS_STUB)", source)
        self.assertIn("packet = this._appendArray(packet, this._intToByteArray(0))", source)
        self.assertIn("const spiAttachPacket = new Uint8Array(8)", source)
        self.assertIn("await loader.runStub()", source)
        self.assertIn("if (stubFlashId !== flashId)", source)
        self.assertIn("let bootloaderSession = false", source)
        self.assertIn("if (resetToApplication && bootloaderSession && currentTransport)", source)
        self.assertIn("await resetToApplicationFirmware(currentTransport)", source)
        self.assertIn("await activeTransport.setDTR(false)", source)
        self.assertIn("await activeTransport.setRTS(true)", source)
        self.assertIn("await activeTransport.setRTS(false)", source)
        self.assertIn("bootloaderSession = true;\n        await loader.detectChip()", source)
        self.assertIn("factoryErase !== false", source)
        self.assertIn("address: APPLICATION_OFFSET", source)
        self.assertIn("eraseAll: false", source)
        self.assertIn("flashSize: profile.flashSizeLabel", source)
        self.assertIn("Falls der Adapter nach 60 Sekunden nicht wieder im Netzwerk ist", source)

    def test_preflight_and_artifact_integrity_fail_before_write(self) -> None:
        source = (DASHBOARD / "js/firmware/web-flasher.js").read_text(encoding="utf-8")
        connect_source = source.split("async connect(release)", 1)[1].split("async flash(", 1)[0]
        flash_source = source.split("async flash(", 1)[1]
        write_at = source.index("await loader.writeFlash")
        self.assertLess(source.index("navigator.serial.requestPort"), write_at)
        self.assertLess(source.index("loader.readFlashId()"), write_at)
        self.assertLess(source.index("loader.chip.SPI_REG_BASE = ESP32_C6_SPI_REG_BASE"), source.index("loader.readFlashId()"))
        self.assertLess(source.index("flashId === 0 || flashId === 0xffffff"), write_at)
        self.assertLess(source.index("await loader.runStub()"), write_at)
        self.assertLess(source.index("if (stubFlashId !== flashId)"), write_at)
        self.assertLess(source.index("loader.DETECTED_FLASH_SIZES"), write_at)
        self.assertLess(source.index("crypto.subtle.digest('SHA-256'"), write_at)
        self.assertLess(source.index("actualSha256 !== release.sha256"), write_at)
        self.assertNotIn("new Transport", connect_source)
        self.assertNotIn("loader.detectChip", connect_source)
        self.assertNotIn("port.open", connect_source)
        self.assertIn("selectedPort = port", connect_source)
        self.assertLess(flash_source.index("actualSha256 !== release.sha256"), flash_source.index("new Transport"))
        self.assertLess(flash_source.index("new Transport"), flash_source.index("loader.detectChip"))

    def test_usb_selection_keeps_adapter_online_until_confirmed_update(self) -> None:
        html = (DASHBOARD / "index.html").read_text(encoding="utf-8")
        source = (DASHBOARD / "js/firmware/web-flasher.js").read_text(encoding="utf-8")
        self.assertIn("USB-Gerät auswählen", html)
        self.assertIn("Der Adapter bleibt dabei normal im Netzwerk", html)
        self.assertIn("Erst dann wird der Adapter", html)
        self.assertIn("remains online until the", source)

    def test_backend_authorization_controls_visibility_and_download(self) -> None:
        app = (DASHBOARD / "js/app.js").read_text(encoding="utf-8")
        provider = (DASHBOARD / "js/providers/aws-backend-provider.js").read_text(encoding="utf-8")
        self.assertIn("access?.authorized === true", app)
        self.assertIn("panel.hidden = !authorized", app)
        self.assertIn("Number(release.flashSizeBytes) / (1024 * 1024)", app)
        self.assertIn("authorizeFirmwareDownload", app)
        self.assertIn("reportFirmwareResult", app)
        admin_html = (DASHBOARD / "administration/index.html").read_text(encoding="utf-8")
        admin_js = (DASHBOARD / "js/administration.js").read_text(encoding="utf-8")
        self.assertIn('id="admin-firmware-target"', admin_html)
        self.assertIn("auth.hasGroup('mot-beta-admins')", admin_js)
        self.assertIn("'/api/firmware/grants'", admin_js)
        self.assertIn("'/api/firmware/grants/revoke'", admin_js)
        self.assertIn("approvedRelease.target", (DASHBOARD / "js/firmware/web-flasher.js").read_text(encoding="utf-8"))
        for path in (
            "/api/firmware/access", "/api/firmware/download", "/api/firmware/result",
            "/api/firmware/grants", "/api/firmware/grants/revoke",
        ):
            self.assertIn(path, provider)

    def test_mobile_flasher_remains_near_end_after_settings_move(self) -> None:
        css = (DASHBOARD / "css/dashboard.css").read_text(encoding="utf-8")
        html = (DASHBOARD / "index.html").read_text(encoding="utf-8")
        self.assertNotIn('id="settings"', html)
        self.assertIn('href="settings/"', html)
        self.assertIn("#firmware-flasher{order:71;width:100%}", css)
        self.assertIn("#usb-diagnostics{order:72;width:100%}", css)
        self.assertIn("#password-recovery{order:73;width:100%}", css)
        self.assertIn("dashboard.css?v=20260917-usbdiag1", html)

    def test_esptool_is_vendored_with_license(self) -> None:
        bundle = DASHBOARD / "vendor/esptool-js/bundle-0.6.0.js"
        license_file = DASHBOARD / "vendor/esptool-js/LICENSE-0.6.0.txt"
        self.assertTrue(bundle.is_file())
        self.assertTrue(license_file.is_file())
        self.assertIn("Apache License", license_file.read_text(encoding="utf-8"))

    def test_password_recovery_has_separate_admin_grant_and_local_serial_flow(self) -> None:
        html = (DASHBOARD / "index.html").read_text(encoding="utf-8")
        admin_html = (DASHBOARD / "administration/index.html").read_text(encoding="utf-8")
        admin_js = (DASHBOARD / "js/administration.js").read_text(encoding="utf-8")
        provider = (DASHBOARD / "js/providers/aws-backend-provider.js").read_text(encoding="utf-8")
        recovery = (DASHBOARD / "js/firmware/password-recovery.js").read_text(encoding="utf-8")
        self.assertIn('id="password-recovery"', html)
        self.assertIn('id="admin-password-recovery-form"', admin_html)
        self.assertIn("'/api/password-recovery/grants'", admin_js)
        self.assertIn("'/api/password-recovery/grants/revoke'", admin_js)
        for path in (
            "/api/password-recovery/access", "/api/password-recovery/start",
            "/api/password-recovery/result",
        ):
            self.assertIn(path, provider)
        self.assertIn("const RECOVERY_COMMAND = 'admin recover'", recovery)
        self.assertIn("const BAUD_RATE = 115200", recovery)
        self.assertIn("NEW LOCAL ADMIN PASSWORD:", recovery)
        self.assertNotIn("console.log", recovery)
        self.assertIn('id="password-recovery-result" hidden', html)
        self.assertLess(html.index('id="password-recovery-output"'), html.index('id="password-recovery-clear"'))
        self.assertIn("output.textContent = password", (DASHBOARD / "js/app.js").read_text(encoding="utf-8"))
        self.assertIn("$('password-recovery-result').hidden = true", (DASHBOARD / "js/app.js").read_text(encoding="utf-8"))

    def test_usb_diagnostics_has_separate_grant_and_allowlisted_local_output(self) -> None:
        html = (DASHBOARD / "index.html").read_text(encoding="utf-8")
        admin_html = (DASHBOARD / "administration/index.html").read_text(encoding="utf-8")
        admin_js = (DASHBOARD / "js/administration.js").read_text(encoding="utf-8")
        provider = (DASHBOARD / "js/providers/aws-backend-provider.js").read_text(encoding="utf-8")
        diagnostics = (DASHBOARD / "js/firmware/usb-diagnostics.js").read_text(encoding="utf-8")
        self.assertIn('id="usb-diagnostics"', html)
        self.assertIn('id="admin-usb-diagnostics-form"', admin_html)
        self.assertIn("'/api/usb-diagnostics/grants'", admin_js)
        self.assertIn("'/api/usb-diagnostics/grants/revoke'", admin_js)
        for path in (
            "/api/usb-diagnostics/access", "/api/usb-diagnostics/start",
            "/api/usb-diagnostics/result",
        ):
            self.assertIn(path, provider)
        for prefix in (
            "Profiles:", "WiFi:", "WiFi profiles:", "AWS IoT:", "Offline cache:",
            "Support system:", "Support network:", "Support AWS:", "Support cache:",
            "Support time:", "Support CAN1:", "Support CAN2:",
        ):
            self.assertIn(prefix, diagnostics)
        self.assertIn("support status\\r\\nprofiles\\r\\nwifi status\\r\\naws status\\r\\ncache status", diagnostics)
        self.assertNotIn("setSignals", diagnostics)
        self.assertIn("safeDiagnosticLines(raw)", diagnostics)
        self.assertIn("reportResult(operationId, result)", diagnostics)
        self.assertNotIn("reportResult(operationId, result, raw)", diagnostics)
        self.assertNotIn("console.log", diagnostics)


if __name__ == "__main__":
    unittest.main()

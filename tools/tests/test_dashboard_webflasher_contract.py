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
        self.assertIn("factoryErase !== false", source)
        self.assertIn("address: APPLICATION_OFFSET", source)
        self.assertIn("eraseAll: false", source)
        self.assertIn("flashSize: profile.flashSizeLabel", source)

    def test_preflight_and_artifact_integrity_fail_before_write(self) -> None:
        source = (DASHBOARD / "js/firmware/web-flasher.js").read_text(encoding="utf-8")
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
        self.assertIn("dashboard.css?v=20260903-settings-page1", html)

    def test_esptool_is_vendored_with_license(self) -> None:
        bundle = DASHBOARD / "vendor/esptool-js/bundle-0.6.0.js"
        license_file = DASHBOARD / "vendor/esptool-js/LICENSE-0.6.0.txt"
        self.assertTrue(bundle.is_file())
        self.assertTrue(license_file.is_file())
        self.assertIn("Apache License", license_file.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()

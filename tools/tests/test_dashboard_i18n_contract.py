import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
INDEX = (ROOT / "build/dashboard/current/index.html").read_text(encoding="utf-8")
I18N = (ROOT / "build/dashboard/current/js/i18n.js").read_text(encoding="utf-8")
APP = (ROOT / "build/dashboard/current/js/app.js").read_text(encoding="utf-8")
HISTORY = (ROOT / "build/dashboard/current/js/history/history-chart.js").read_text(encoding="utf-8")


class DashboardI18nContractTests(unittest.TestCase):
    def test_german_is_stable_default_and_fallback(self):
        self.assertIn("const SUPPORTED = ['de', 'en', 'fr'];", I18N)
        self.assertIn("[stored, configured, 'de']", I18N)
        self.assertIn("<html lang=\"de\">", INDEX)

    def test_language_selector_and_catalog_are_loaded(self):
        self.assertIn('src="js/i18n.js?v=20260827-i18n1"', INDEX)
        self.assertIn('id="dashboard-language"', INDEX)
        for language in ("de", "en", "fr"):
            self.assertIn(f'<option value="{language}">', INDEX)

    def test_english_and_french_cover_all_catalog_keys(self):
        blocks = re.search(r"const en = \{(.*?)\n  \};\n\n  const fr = \{(.*?)\n  \};", I18N, re.S)
        self.assertIsNotNone(blocks)
        key_pattern = re.compile(r"'((?:[^'\\]|\\.)*)'\s*:")
        english = set(key_pattern.findall(blocks.group(1)))
        french = set(key_pattern.findall(blocks.group(2)))
        self.assertEqual(english, french)
        self.assertGreaterEqual(len(english), 120)

    def test_dynamic_status_vocabulary_is_localized(self):
        for source in (
            "Nicht am Laden", "Eingesteckt", "Lädt", "Bereit", "Verbrauch",
            "Rekuperation", "Verbunden", "Getrennt", "MQTT getrennt",
            "Abonniert:", "WebSocket verbunden", "gerade eben", "Stand:",
            "Letzte Aktualisierung", "Basierend auf", "Nach SoC:",
            "E-Mail-Adresse bestätigt", "Bestätigung ausstehend",
            "SMS-Status vorübergehend nicht verfügbar.", "Sitzung wird geprüft…",
        ):
            self.assertIn(f"'{source}':", I18N)

    def test_value_dependent_time_and_reserve_patterns_are_localized(self):
        for marker in (
            ".replace(/\\bvor (\\d+) min\\b/g, '$1 min ago')",
            ".replace(/\\bvor (\\d+) min\\b/g, 'il y a $1 min')",
            ".replace(/ · bis (\\d+)%/g, ' · to $1%')",
            ".replace(/ · bis (\\d+)%/g, ' · jusqu’à $1 %')",
        ):
            self.assertIn(marker, I18N)

    def test_locale_updates_are_announced_to_charts(self):
        self.assertIn("mot-language-change", I18N)
        self.assertIn("document.documentElement.lang = language", I18N)
        self.assertIn("window.MOT_I18N?.locale", APP)
        self.assertIn('window.addEventListener("mot-language-change"', HISTORY)


if __name__ == "__main__":
    unittest.main()

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
SOURCE = (ROOT / "tools/serve_dashboard.py").read_text(encoding="utf-8")


class DashboardServerTests(unittest.TestCase):
    def test_server_redacts_query_from_request_logs(self):
        compile(SOURCE, "serve-dashboard", "exec")
        self.assertIn("urlsplit(self.path).path", SOURCE)
        self.assertNotIn("self.requestline", SOURCE)


if __name__ == "__main__":
    unittest.main()

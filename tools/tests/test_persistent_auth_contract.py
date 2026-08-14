from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
AUTH = (ROOT / "build/dashboard/current/js/auth/auth-manager.js").read_text(encoding="utf-8")
STORE = (ROOT / "build/dashboard/current/js/auth/token-store.js").read_text(encoding="utf-8")
APP = (ROOT / "build/dashboard/current/js/app.js").read_text(encoding="utf-8")
HTML = (ROOT / "build/dashboard/current/index.html").read_text(encoding="utf-8")
CSS = (ROOT / "build/dashboard/current/css/dashboard.css").read_text(encoding="utf-8")
CONFIG = (ROOT / "build/dashboard/current/config.js").read_text(encoding="utf-8")
SPRINT = (ROOT / "docs/project/sprints/AUTH-PERSIST-001.md").read_text(encoding="utf-8")


class PersistentAuthContractTests(unittest.TestCase):
    def test_remember_me_is_explicit_and_unchecked(self) -> None:
        self.assertIn('id="auth-remember" type="checkbox"', HTML)
        self.assertNotIn('id="auth-remember" type="checkbox" checked', HTML)
        self.assertIn("auth.login({ remember: Boolean($('auth-remember')?.checked) })", APP)
        self.assertIn(".auth-remember", CSS)

    def test_pkce_transaction_carries_opt_in_without_persisting_verifier(self) -> None:
        self.assertIn("transactionStorage || window.sessionStorage", AUTH)
        self.assertIn("remember: Boolean(options.remember)", AUTH)
        self.assertNotIn("localStorage.setItem(TRANSACTION_KEY", AUTH)

    def test_persistent_record_contains_refresh_state_only(self) -> None:
        persistent_save = AUTH.split("persistentTokenStore?.save({", 1)[1].split("});", 1)[0]
        self.assertIn("refreshToken:", persistent_save)
        self.assertIn("refreshExpiresAt:", persistent_save)
        self.assertNotIn("accessToken:", persistent_save)
        self.assertNotIn("idToken:", persistent_save)
        self.assertIn("(!session.accessToken || tokenStore.isExpired(session))", AUTH)

    def test_refresh_grant_is_bounded_and_deduplicated(self) -> None:
        self.assertIn("grant_type: 'refresh_token'", AUTH)
        self.assertIn("if (refreshPromise) return refreshPromise", AUTH)
        self.assertIn("Math.min(days, 30)", AUTH)
        self.assertIn("rememberDays: 30", CONFIG)
        self.assertIn("isRefreshExpired", STORE)

    def test_logout_and_permanent_rejection_clear_both_stores(self) -> None:
        self.assertIn("tokenStore?.clear()", AUTH)
        self.assertIn("persistentTokenStore?.clear()", AUTH)
        self.assertIn("clearStoredSessions();", AUTH.split("async function logout()", 1)[1])
        self.assertIn("['invalid_grant', 'invalid_client', 'unauthorized_client']", AUTH)
        self.assertNotIn("response.status >= 400 && response.status < 500", AUTH)

    def test_sprint_records_completed_hosted_acceptance(self) -> None:
        self.assertIn("Completed — hosted desktop and smartphone acceptance passed", SPRINT)
        self.assertIn("[x] Hosted smartphone close-and-reopen acceptance passes", SPRINT)
        self.assertIn("[x] Hosted desktop, default-login regression", SPRINT)


if __name__ == "__main__":
    unittest.main()

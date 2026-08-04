(function () {
  const TRANSACTION_KEY = 'mot.auth.transaction';

  function decodeJwtPayload(token) {
    try {
      const part = String(token || '').split('.')[1];
      if (!part) return null;
      const normalized = part.replace(/-/g, '+').replace(/_/g, '/');
      const padded = normalized.padEnd(Math.ceil(normalized.length / 4) * 4, '=');
      const binary = window.atob(padded);
      const bytes = Uint8Array.from(binary, char => char.charCodeAt(0));
      return JSON.parse(new TextDecoder().decode(bytes));
    } catch (_) {
      return null;
    }
  }

  function createAuthManager(options = {}) {
    const config = options.config || {};
    const tokenStore = options.tokenStore || window.MOTTokenStore?.create();
    const transactionStorage = options.transactionStorage || window.sessionStorage;
    let session = null;
    let lastError = null;

    function requiredConfiguration() {
      return ['clientId', 'authorizeEndpoint', 'tokenEndpoint', 'redirectUri'];
    }

    function missingConfiguration() {
      return requiredConfiguration().filter(key => !String(config[key] || '').trim());
    }

    function isConfigured() { return missingConfiguration().length === 0; }

    function normalizeScopes() {
      const scopes = Array.isArray(config.scopes) ? config.scopes : ['openid'];
      return [...new Set(scopes.map(String).map(v => v.trim()).filter(Boolean))];
    }

    function callbackParameters() {
      const params = new URLSearchParams(window.location.search);
      return {
        code: params.get('code'),
        state: params.get('state'),
        error: params.get('error'),
        errorDescription: params.get('error_description')
      };
    }

    function isCallback() {
      const callback = callbackParameters();
      return Boolean(callback.code || callback.error);
    }

    function saveTransaction(transaction) {
      transactionStorage.setItem(TRANSACTION_KEY, JSON.stringify(transaction));
    }

    function loadTransaction() {
      try {
        const value = transactionStorage.getItem(TRANSACTION_KEY);
        return value ? JSON.parse(value) : null;
      } catch (_) {
        return null;
      }
    }

    function clearTransaction() { transactionStorage.removeItem(TRANSACTION_KEY); }

    function cleanCallbackUrl() {
      const url = new URL(window.location.href);
      ['code', 'state', 'error', 'error_description', 'error_uri'].forEach(key => {
        url.searchParams.delete(key);
      });
      window.history.replaceState({}, document.title, `${url.pathname}${url.search}${url.hash}`);
    }

    function assertConfigured() {
      const missing = missingConfiguration();
      if (missing.length) {
        throw new Error(`Cognito configuration is incomplete: ${missing.join(', ')}`);
      }
    }

    function validateTokenClaims(tokens, transaction) {
      const idClaims = decodeJwtPayload(tokens.id_token);
      if (idClaims?.nonce && idClaims.nonce !== transaction.nonce) {
        throw new Error('Authentication callback nonce validation failed');
      }
      if (config.issuer && idClaims?.iss && idClaims.iss !== config.issuer) {
        throw new Error('Authentication token issuer does not match the configured issuer');
      }
      const audience = idClaims?.aud || idClaims?.client_id;
      if (audience && audience !== config.clientId) {
        throw new Error('Authentication token audience does not match the configured client');
      }
    }

    async function exchangeCode(code, transaction) {
      const body = new URLSearchParams({
        grant_type: 'authorization_code',
        client_id: config.clientId,
        code,
        redirect_uri: config.redirectUri,
        code_verifier: transaction.verifier
      });
      const response = await fetch(config.tokenEndpoint, {
        method: 'POST',
        headers: {
          Accept: 'application/json',
          'Content-Type': 'application/x-www-form-urlencoded'
        },
        body: body.toString(),
        cache: 'no-store'
      });
      const payload = await response.json().catch(() => ({}));
      if (!response.ok || !payload.access_token) {
        const message = payload.error_description || payload.error || `HTTP ${response.status}`;
        throw new Error(`Cognito token exchange failed: ${message}`);
      }
      validateTokenClaims(payload, transaction);
      const expiresIn = Number(payload.expires_in || 3600);
      return {
        accessToken: payload.access_token,
        idToken: payload.id_token || null,
        refreshToken: payload.refresh_token || null,
        tokenType: payload.token_type || 'Bearer',
        scope: payload.scope || normalizeScopes().join(' '),
        expiresAt: Date.now() + Math.max(1, expiresIn) * 1000,
        obtainedAt: Date.now()
      };
    }

    async function handleCallback() {
      const callback = callbackParameters();
      const transaction = loadTransaction();
      try {
        if (callback.error) {
          throw new Error(callback.errorDescription || callback.error);
        }
        if (!callback.code) return false;
        if (!transaction?.state || !transaction?.verifier || !transaction?.nonce) {
          throw new Error('Authentication transaction is missing or expired');
        }
        if (!callback.state || callback.state !== transaction.state) {
          throw new Error('Authentication callback state validation failed');
        }
        session = await exchangeCode(callback.code, transaction);
        tokenStore.save(session);
        lastError = null;
        return true;
      } catch (error) {
        session = null;
        tokenStore.clear();
        lastError = error;
        throw error;
      } finally {
        clearTransaction();
        cleanCallbackUrl();
      }
    }

    async function restoreSession() {
      if (isCallback()) await handleCallback();
      session = tokenStore.load();
      if (session && tokenStore.isExpired(session)) {
        session = null;
        tokenStore.clear();
      }
      return Boolean(session?.accessToken);
    }

    async function login() {
      assertConfigured();
      const verifier = window.MOTPkce.createVerifier();
      const challenge = await window.MOTPkce.createChallenge(verifier);
      const state = window.MOTPkce.createState();
      const nonce = window.MOTPkce.createNonce();
      saveTransaction({ verifier, state, nonce, createdAt: Date.now() });

      const url = new URL(config.authorizeEndpoint);
      url.search = new URLSearchParams({
        response_type: 'code',
        client_id: config.clientId,
        redirect_uri: config.redirectUri,
        scope: normalizeScopes().join(' '),
        code_challenge_method: 'S256',
        code_challenge: challenge,
        state,
        nonce
      }).toString();
      window.location.assign(url.toString());
    }

    async function logout() {
      session = null;
      lastError = null;
      tokenStore.clear();
      clearTransaction();
      if (!config.logoutEndpoint || !config.clientId || !config.redirectUri) return;
      const url = new URL(config.logoutEndpoint);
      url.search = new URLSearchParams({
        client_id: config.clientId,
        logout_uri: config.redirectUri
      }).toString();
      window.location.assign(url.toString());
    }

    return Object.freeze({
      restoreSession,
      login,
      logout,
      async refresh() {
        throw new Error('Refresh-token rotation is outside SPR-0004B.3 Phase 2');
      },
      async getAccessToken() {
        if (!session || tokenStore.isExpired(session)) {
          session = null;
          tokenStore.clear();
          return null;
        }
        return session.accessToken;
      },
      isAuthenticated() { return Boolean(session?.accessToken) && !tokenStore.isExpired(session); },
      isConfigured,
      getLastError() { return lastError; },
      describe() {
        return {
          configured: isConfigured(),
          missingConfiguration: missingConfiguration(),
          authenticated: Boolean(session?.accessToken) && !tokenStore.isExpired(session),
          expiresAt: session?.expiresAt || null,
          implementationPhase: 'SPR-0004B.3-phase-2-pkce'
        };
      }
    });
  }

  window.MOTAuth = Object.freeze({ create: createAuthManager });
})();

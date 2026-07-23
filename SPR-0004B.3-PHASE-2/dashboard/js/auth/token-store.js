(function () {
  const DEFAULT_KEY = 'mot.auth.session';

  function createTokenStore(options = {}) {
    const storage = options.storage || window.sessionStorage;
    const key = options.key || DEFAULT_KEY;

    function load() {
      try {
        const value = storage.getItem(key);
        if (!value) return null;
        const session = JSON.parse(value);
        return session && typeof session === 'object' ? session : null;
      } catch (error) {
        console.warn('MOT auth token store could not load a session:', error);
        return null;
      }
    }

    return Object.freeze({
      load,
      save(session) {
        if (!session || typeof session !== 'object' || !session.accessToken) {
          throw new Error('A valid authentication session is required');
        }
        storage.setItem(key, JSON.stringify(session));
      },
      clear() { storage.removeItem(key); },
      isExpired(session = load(), clockSkewSeconds = 30) {
        const expiresAt = Number(session?.expiresAt || 0);
        return !expiresAt || Date.now() >= expiresAt - (clockSkewSeconds * 1000);
      }
    });
  }

  window.MOTTokenStore = Object.freeze({ create: createTokenStore });
})();

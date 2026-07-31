(function () {
  const VERIFIER_BYTES = 64;
  const RANDOM_BYTES = 32;

  function randomBytes(length) {
    if (!window.crypto?.getRandomValues) {
      throw new Error('Secure browser cryptography is not available');
    }
    const bytes = new Uint8Array(length);
    window.crypto.getRandomValues(bytes);
    return bytes;
  }

  function base64Url(bytes) {
    let binary = '';
    bytes.forEach(byte => { binary += String.fromCharCode(byte); });
    return window.btoa(binary)
      .replace(/\+/g, '-')
      .replace(/\//g, '_')
      .replace(/=+$/g, '');
  }

  function createRandomValue(length = RANDOM_BYTES) {
    return base64Url(randomBytes(length));
  }

  async function createChallenge(verifier) {
    if (!verifier || typeof verifier !== 'string') {
      throw new Error('A PKCE verifier is required');
    }
    if (!window.crypto?.subtle) {
      throw new Error('Web Crypto digest support is not available');
    }
    const data = new TextEncoder().encode(verifier);
    const digest = await window.crypto.subtle.digest('SHA-256', data);
    return base64Url(new Uint8Array(digest));
  }

  window.MOTPkce = Object.freeze({
    createVerifier() { return createRandomValue(VERIFIER_BYTES); },
    createChallenge,
    createState() { return createRandomValue(); },
    createNonce() { return createRandomValue(); }
  });
})();

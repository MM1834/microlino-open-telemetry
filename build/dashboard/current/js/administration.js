(function () {
  const cfg = window.MOT_CONFIG || {};
  const auth = window.MOTAuth?.create({ config: cfg.auth || {} });
  const $ = id => document.getElementById(id);
  const activeLocale = () => window.MOT_I18N?.locale || cfg.dashboard?.locale || 'de-CH';
  let busy = false;

  function setBusy(value) {
    busy = Boolean(value);
    document.querySelectorAll('#admin-content button, #admin-content input, #admin-content select')
      .forEach(element => { element.disabled = busy; });
  }

  function showAccess(mode, message = '') {
    const authorized = mode === 'authorized';
    $('admin-content').hidden = !authorized;
    $('admin-denied').hidden = authorized;
    $('admin-login').hidden = mode !== 'signed-out';
    $('admin-logout').hidden = mode === 'signed-out';
    $('admin-auth-status').textContent = authorized ? 'Als Administrator angemeldet'
      : (mode === 'signed-out' ? 'Nicht angemeldet' : 'Keine Administratorberechtigung');
    if (message) $('admin-denied-message').textContent = message;
  }

  async function request(path, body = undefined) {
    const base = String(cfg.awsBackend?.onboardingApiBaseUrl || '').replace(/\/$/, '');
    if (!base) throw new Error('Onboarding API URL fehlt');
    const token = await auth.getAccessToken();
    if (!token) throw new Error('Anmeldung erforderlich');
    const method = body === undefined ? 'GET' : 'POST';
    const options = {
      method,
      headers: { Accept: 'application/json', Authorization: `Bearer ${token}`, 'content-type': 'application/json' },
      cache: 'no-store'
    };
    if (body !== undefined) options.body = JSON.stringify(body);
    const response = await fetch(`${base}${path}`, options);
    const payload = await response.json().catch(() => ({}));
    if (!response.ok) {
      if (response.status === 401) showAccess('signed-out', 'Die Sitzung ist abgelaufen. Bitte erneut anmelden.');
      if (response.status === 403) showAccess('denied', 'Für dieses Konto fehlt die Administratorberechtigung.');
      throw new Error(payload.error || `Administrations-API HTTP ${response.status}`);
    }
    return payload;
  }

  function dateTime(timestamp) {
    return timestamp ? new Date(timestamp * 1000).toLocaleString(activeLocale()) : '--';
  }

  function appendCells(row, values) {
    values.forEach(value => {
      const cell = document.createElement('td');
      cell.textContent = value || '--';
      row.appendChild(cell);
    });
  }

  function renderActiveGrants(result = {}) {
    const firmware = Array.isArray(result.firmware) ? result.firmware : [];
    const recovery = Array.isArray(result.passwordRecovery) ? result.passwordRecovery : [];
    const firmwareBody = $('admin-firmware-grants');
    const recoveryBody = $('admin-password-recovery-grants');
    firmwareBody.replaceChildren();
    recoveryBody.replaceChildren();
    firmware.forEach(grant => {
      const row = document.createElement('tr');
      appendCells(row, [grant.email, grant.target, grant.version, dateTime(grant.expiresAt)]);
      firmwareBody.appendChild(row);
    });
    recovery.forEach(grant => {
      const row = document.createElement('tr');
      appendCells(row, [grant.email, dateTime(grant.grantedAt), dateTime(grant.expiresAt)]);
      recoveryBody.appendChild(row);
    });
    $('admin-firmware-grants-empty').hidden = firmware.length > 0;
    $('admin-password-recovery-grants-empty').hidden = recovery.length > 0;
    $('admin-firmware-grants-table').hidden = firmware.length === 0;
    $('admin-password-recovery-grants-table').hidden = recovery.length === 0;
  }

  async function loadActiveGrants() {
    $('admin-grants-status').textContent = 'Berechtigungen werden geladen…';
    try {
      const result = await request('/api/admin/grants');
      renderActiveGrants(result);
      $('admin-grants-status').textContent = `Stand: ${dateTime(result.generatedAt)}`;
    } catch (error) {
      $('admin-grants-status').textContent = error.message || 'Berechtigungen konnten nicht geladen werden.';
    }
  }

  async function issueClaim(event) {
    event.preventDefault();
    if (busy) return;
    const vehicleId = String($('admin-vehicle-id').value || '').trim();
    const output = $('admin-claim-output');
    output.hidden = true;
    output.textContent = '';
    $('admin-claim-status').textContent = 'Claim wird erstellt…';
    setBusy(true);
    try {
      const result = await request('/api/onboarding/claims', { vehicleId });
      output.textContent = result.claim || '';
      output.hidden = false;
      $('admin-claim-status').textContent = `Claim für ${result.vehicleId} erstellt; gültig bis ${new Date(result.expiresAt * 1000).toLocaleString(activeLocale())}.`;
    } catch (error) {
      $('admin-claim-status').textContent = error.message || 'Claim-Ausgabe fehlgeschlagen';
    } finally { setBusy(false); }
  }

  function clearClaim() {
    $('admin-claim-output').textContent = '';
    $('admin-claim-output').hidden = true;
    $('admin-claim-status').textContent = 'Claim-Anzeige wurde geleert.';
  }

  async function changeFirmwareAccess(revoke = false) {
    if (busy) return;
    const username = String($('admin-firmware-user').value || '').trim();
    const target = String($('admin-firmware-target').value || 'nanoesp32c6-n16');
    const expiresInHours = Number($('admin-firmware-hours').value || 48);
    if (!username) {
      $('admin-firmware-status').textContent = 'Bitte Benutzer-E-Mail eingeben.';
      return;
    }
    $('admin-firmware-status').textContent = revoke ? 'Freigabe wird entzogen…' : 'Freigabe wird erstellt…';
    setBusy(true);
    try {
      const path = revoke ? '/api/firmware/grants/revoke' : '/api/firmware/grants';
      const body = revoke ? { username, target } : { username, target, expiresInHours };
      const result = await request(path, body);
      $('admin-firmware-status').textContent = revoke
        ? `Web-Flasher-Freigabe ${target} für ${username} entzogen.`
        : `Web-Flasher ${target} für ${username} bis ${new Date(result.expiresAt * 1000).toLocaleString(activeLocale())} freigegeben.`;
      await loadActiveGrants();
    } catch (error) {
      $('admin-firmware-status').textContent = error.message || (revoke ? 'Freigabe konnte nicht entzogen werden.' : 'Freigabe fehlgeschlagen.');
    } finally { setBusy(false); }
  }

  async function changePasswordRecoveryAccess(revoke = false) {
    if (busy) return;
    const username = String($('admin-password-recovery-user').value || '').trim();
    const expiresInHours = Number($('admin-password-recovery-hours').value || 24);
    if (!username) {
      $('admin-password-recovery-status').textContent = 'Bitte Benutzer-E-Mail eingeben.';
      return;
    }
    $('admin-password-recovery-status').textContent = revoke ? 'Freigabe wird entzogen…' : 'Freigabe wird erstellt…';
    setBusy(true);
    try {
      const path = revoke ? '/api/password-recovery/grants/revoke' : '/api/password-recovery/grants';
      const body = revoke ? { username } : { username, expiresInHours };
      const result = await request(path, body);
      $('admin-password-recovery-status').textContent = revoke
        ? `Passwort-Recovery-Freigabe für ${username} entzogen.`
        : `Passwort-Recovery für ${username} bis ${new Date(result.expiresAt * 1000).toLocaleString(activeLocale())} freigegeben.`;
      await loadActiveGrants();
    } catch (error) {
      $('admin-password-recovery-status').textContent = error.message || (revoke ? 'Freigabe konnte nicht entzogen werden.' : 'Freigabe fehlgeschlagen.');
    } finally { setBusy(false); }
  }

  async function bootstrap() {
    if (!auth?.isConfigured()) {
      showAccess('signed-out', 'Cognito ist nicht vollständig konfiguriert.');
      $('admin-login').disabled = true;
      return;
    }
    try { await auth.restoreSession(); }
    catch (error) {
      showAccess('signed-out', error.message || 'Anmeldung fehlgeschlagen');
      return;
    }
    if (!auth.isAuthenticated()) {
      showAccess('signed-out', 'Bitte zuerst im Dashboard anmelden. Nach der Anmeldung erscheint dort der Menüpunkt Administration.');
      return;
    }
    if (!auth.hasGroup('mot-beta-admins')) {
      showAccess('denied', 'Diese Seite ist nur für berechtigte Administratoren verfügbar.');
      return;
    }
    showAccess('authorized');
    await loadActiveGrants();
  }

  $('admin-login')?.addEventListener('click', () => auth.login({ remember: false }));
  $('admin-logout')?.addEventListener('click', () => auth.logout());
  $('admin-claim-form')?.addEventListener('submit', issueClaim);
  $('admin-claim-clear')?.addEventListener('click', clearClaim);
  $('admin-firmware-form')?.addEventListener('submit', event => {
    event.preventDefault();
    changeFirmwareAccess(false);
  });
  $('admin-firmware-revoke')?.addEventListener('click', () => changeFirmwareAccess(true));
  $('admin-password-recovery-form')?.addEventListener('submit', event => {
    event.preventDefault();
    changePasswordRecoveryAccess(false);
  });
  $('admin-password-recovery-revoke')?.addEventListener('click', () => changePasswordRecoveryAccess(true));
  $('admin-grants-refresh')?.addEventListener('click', loadActiveGrants);
  bootstrap().catch(error => showAccess('denied', error.message || 'Administration konnte nicht geladen werden.'));
})();

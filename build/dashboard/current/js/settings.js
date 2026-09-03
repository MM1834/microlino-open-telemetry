(function () {
  const cfg = window.MOT_CONFIG || {};
  const auth = window.MOTAuth?.create({ config: cfg.auth || {} });
  const $ = id => document.getElementById(id);
  const state = {
    provider: null,
    vehicles: [],
    vehicleId: null,
    busy: false,
    readOnly: false,
    smsStatus: null,
    smsError: ''
  };

  function showAccess(mode, message = '') {
    const authorized = mode === 'authorized';
    $('settings').hidden = !authorized;
    $('settings-denied').hidden = authorized;
    $('settings-login').hidden = mode !== 'signed-out';
    $('settings-logout').hidden = mode === 'signed-out';
    $('settings-auth-status').textContent = authorized ? 'Angemeldet'
      : (mode === 'signed-out' ? 'Nicht angemeldet' : 'Einstellungen nicht verfügbar');
    document.querySelectorAll('[data-admin-nav]').forEach(link => {
      link.hidden = !(authorized && auth?.hasGroup?.('mot-beta-admins'));
    });
    if (message) $('settings-denied-message').textContent = message;
  }

  function setBusy(value) {
    state.busy = Boolean(value);
    renderControls();
  }

  function renderControls() {
    const disabled = state.busy || state.readOnly;
    [
      'notification-enabled', 'notification-threshold', 'notification-email-enabled',
      'notification-journey-email-enabled', 'notification-charging-summary-email-enabled',
      'notification-daily-summary-email-enabled', 'notification-charging-stop-email-enabled',
      'notification-charging-stop-threshold', 'notification-email', 'notification-sms-phone',
      'range-km-at-100', 'range-reserve-soc', 'notification-save'
    ].forEach(id => { if ($(id)) $(id).disabled = disabled; });
    $('settings-vehicle').disabled = state.busy;
    renderSmsStatus();
  }

  function updateEmailConfirmationHelp() {
    const email = $('notification-email');
    const help = $('notification-email-confirmation-help');
    const confirmedEmail = email.dataset.confirmedEmail || '';
    const currentEmail = String(email.value || '').trim().toLowerCase();
    const stillConfirmed = Boolean(confirmedEmail) && currentEmail === confirmedEmail;
    help.hidden = stillConfirmed;
    if (confirmedEmail && !stillConfirmed) {
      $('notification-email-state').textContent = 'Neue E-Mail-Adresse muss bestätigt werden';
    }
  }

  function renderSmsStatus() {
    const status = state.smsStatus;
    const phone = $('notification-sms-phone');
    if (!status) {
      $('notification-sms-state').textContent = state.smsError
        ? 'SMS-Status vorübergehend nicht verfügbar.'
        : 'Mobilnummer noch nicht bestätigt.';
    } else {
      if (status.phoneE164 && !phone.value) phone.value = status.phoneE164;
      $('notification-sms-state').textContent = status.verificationStatus !== 'VERIFIED'
        ? (status.verificationStatus === 'PENDING' ? 'Bestätigungscode ausstehend.' : 'Mobilnummer noch nicht bestätigt.')
        : (status.smsApproved
          ? 'Mobilnummer bestätigt und administrativ freigegeben.'
          : 'Mobilnummer bestätigt; administrative Freigabe ausstehend.');
      $('notification-sms-enabled').checked = status.smsEnabled === true;
    }
    const phoneUnchanged = String(phone.value || '').trim() === String(status?.phoneE164 || '');
    const locked = state.busy || state.readOnly;
    $('notification-sms-request').disabled = locked;
    $('notification-sms-confirm').disabled = locked || status?.verificationStatus !== 'PENDING';
    $('notification-sms-code').disabled = locked || status?.verificationStatus !== 'PENDING';
    $('notification-sms-confirmation-fields').hidden = status?.verificationStatus !== 'PENDING';
    $('notification-sms-enabled').disabled = locked || status?.smsReady !== true || !phoneUnchanged;
    if (status?.verificationStatus !== 'VERIFIED' || !phoneUnchanged) {
      $('notification-sms-enabled').checked = false;
    }
  }

  function renderPreferences(preferences, message = '') {
    if (preferences) {
      state.readOnly = preferences.readOnly === true;
      $('notification-enabled').checked = preferences.enabled === true;
      $('notification-threshold').value = Number(preferences.threshold || 80);
      $('notification-email-enabled').checked = preferences.emailEnabled === true;
      $('notification-journey-email-enabled').checked = preferences.journeyEmailEnabled === true;
      $('notification-charging-summary-email-enabled').checked = preferences.chargingSummaryEmailEnabled === true;
      $('notification-daily-summary-email-enabled').checked = preferences.dailySummaryEmailEnabled === true;
      $('notification-charging-stop-email-enabled').checked = preferences.chargingStopEmailEnabled === true;
      $('notification-charging-stop-threshold').value = Number(preferences.chargingStopThreshold || 80);
      $('range-km-at-100').value = Number(preferences.rangeKmAt100 || cfg.vehicle?.defaultRangeKmAt100 || 140);
      $('range-reserve-soc').value = Number(preferences.rangeReserveSoc || 0);
      $('notification-email').value = preferences.email || '';
      $('notification-email-state').textContent = preferences.emailConfirmed
        ? 'E-Mail-Adresse bestätigt'
        : (preferences.emailEnabled ? 'Bestätigung ausstehend' : 'E-Mail deaktiviert');
      $('notification-email').dataset.confirmedEmail = preferences.emailConfirmed === true
        ? String(preferences.email || '').trim().toLowerCase()
        : '';
      $('notification-sms-enabled').checked = preferences.smsEnabled === true;
      updateEmailConfirmationHelp();
    }
    renderControls();
    $('notification-status').textContent = state.readOnly
      ? 'Demo-Zugang: Benachrichtigungen sind deaktiviert.'
      : message;
  }

  async function loadPreferences() {
    if (!state.provider || !state.vehicleId) return;
    state.smsStatus = null;
    state.smsError = '';
    setBusy(true);
    $('notification-status').textContent = 'Einstellungen werden geladen…';
    const [preferencesResult, smsResult] = await Promise.allSettled([
      state.provider.getNotificationPreferences(),
      state.provider.getSmsNotificationStatus()
    ]);
    const preferences = preferencesResult.status === 'fulfilled' ? preferencesResult.value : null;
    if (smsResult.status === 'fulfilled') state.smsStatus = smsResult.value;
    else state.smsError = smsResult.reason?.message || 'SMS-Status nicht verfügbar';
    setBusy(false);
    if (!preferences && !state.smsStatus) {
      $('notification-status').textContent = preferencesResult.reason?.message || 'Einstellungen konnten nicht geladen werden.';
      return;
    }
    renderPreferences(preferences,
      preferencesResult.status === 'rejected' || smsResult.status === 'rejected'
        ? 'Ein Teil der Einstellungen ist vorübergehend nicht verfügbar.'
        : '');
  }

  async function selectVehicle(vehicleId) {
    if (!vehicleId || vehicleId === state.vehicleId) return;
    state.vehicleId = vehicleId;
    state.readOnly = false;
    state.smsStatus = null;
    $('notification-sms-phone').value = '';
    await state.provider.selectVehicle(vehicleId);
    await loadPreferences();
  }

  function validateEmailDependencies() {
    if (!$('notification-email-enabled').checked) {
      const selected = [
        ['notification-journey-email-enabled', 'Für Fahrtzusammenfassungen zuerst den E-Mail-Kanal aktivieren.'],
        ['notification-charging-stop-email-enabled', 'Für Ladestopp-Meldungen zuerst den E-Mail-Kanal aktivieren.'],
        ['notification-charging-summary-email-enabled', 'Für Ladezusammenfassungen zuerst den E-Mail-Kanal aktivieren.'],
        ['notification-daily-summary-email-enabled', 'Für Tagesübersichten zuerst den E-Mail-Kanal aktivieren.']
      ].find(([id]) => $(id).checked);
      if (selected) return selected[1];
    }
    return '';
  }

  async function savePreferences(event) {
    event.preventDefault();
    if (state.busy || state.readOnly) return;
    const validationMessage = validateEmailDependencies();
    if (validationMessage) {
      $('notification-status').textContent = validationMessage;
      return;
    }
    const requested = {
      enabled: $('notification-enabled').checked,
      threshold: Number($('notification-threshold').value),
      emailEnabled: $('notification-email-enabled').checked,
      journeyEmailEnabled: $('notification-journey-email-enabled').checked,
      chargingSummaryEmailEnabled: $('notification-charging-summary-email-enabled').checked,
      dailySummaryEmailEnabled: $('notification-daily-summary-email-enabled').checked,
      chargingStopEmailEnabled: $('notification-charging-stop-email-enabled').checked,
      chargingStopThreshold: Number($('notification-charging-stop-threshold').value),
      rangeKmAt100: Number($('range-km-at-100').value),
      rangeReserveSoc: Number($('range-reserve-soc').value),
      email: String($('notification-email').value || '').trim(),
      phoneE164: String($('notification-sms-phone').value || '').trim(),
      smsEnabled: $('notification-sms-enabled').checked
    };
    setBusy(true);
    $('notification-status').textContent = 'Wird gespeichert…';
    try {
      const result = await state.provider.saveNotificationPreferences(requested);
      if (state.smsStatus) state.smsStatus.smsEnabled = result.smsEnabled === true;
      setBusy(false);
      renderPreferences({ ...requested, ...result }, 'Gespeichert');
    } catch (error) {
      setBusy(false);
      $('notification-status').textContent = error.message || 'Speichern fehlgeschlagen';
    }
  }

  async function requestSmsVerification() {
    if (state.busy || state.readOnly) return;
    setBusy(true);
    $('notification-status').textContent = 'Bestätigungscode wird angefordert…';
    try {
      state.smsStatus = await state.provider.requestSmsVerification(String($('notification-sms-phone').value || '').trim());
      setBusy(false);
      renderPreferences(null, state.smsStatus.verificationStatus === 'VERIFIED'
        ? 'Bereits bestätigte Mobilnummer übernommen.' : 'Bestätigungscode gesendet.');
    } catch (error) {
      setBusy(false);
      renderPreferences(null, error?.status === 429
        ? 'Bitte mindestens 60 Sekunden bis zum nächsten Code warten.'
        : (error.message || 'Bestätigungscode konnte nicht gesendet werden.'));
    }
  }

  async function confirmSmsVerification() {
    if (state.busy || state.readOnly) return;
    setBusy(true);
    $('notification-status').textContent = 'Code wird geprüft…';
    try {
      state.smsStatus = await state.provider.confirmSmsVerification(String($('notification-sms-code').value || '').trim());
      $('notification-sms-code').value = '';
      setBusy(false);
      renderPreferences(null, 'Mobilnummer bestätigt.');
    } catch (error) {
      setBusy(false);
      renderPreferences(null, error.message || 'Code konnte nicht bestätigt werden.');
    }
  }

  async function bootstrap() {
    if (!auth?.isConfigured()) {
      showAccess('signed-out', 'Cognito ist nicht vollständig konfiguriert.');
      $('settings-login').disabled = true;
      return;
    }
    try { await auth.restoreSession(); }
    catch (error) {
      showAccess('signed-out', error.message || 'Anmeldung fehlgeschlagen');
      return;
    }
    if (!auth.isAuthenticated()) {
      showAccess('signed-out', 'Bitte zuerst im Dashboard anmelden.');
      return;
    }
    if (cfg.dataSource?.type !== 'aws-backend' || !window.MOTDataProviders) {
      showAccess('unavailable', 'Einstellungen sind für diese Datenquelle nicht verfügbar.');
      return;
    }
    state.provider = window.MOTDataProviders.create('aws-backend', {
      config: {
        ...(cfg.awsBackend || {}),
        vehicleId: cfg.mqtt?.vehicleId || 'pioneer',
        getAccessToken: auth.getAccessToken,
        onUnauthorized: () => showAccess('signed-out', 'Die Sitzung ist abgelaufen. Bitte erneut anmelden.')
      }
    });
    state.vehicles = await state.provider.getVehicles();
    if (!state.vehicles.length) {
      showAccess('unavailable', 'Diesem Konto ist noch kein Fahrzeug zugeordnet.');
      return;
    }
    const configured = cfg.mqtt?.vehicleId;
    state.vehicleId = state.vehicles.some(vehicle => vehicle.vehicleId === configured)
      ? configured : state.vehicles[0].vehicleId;
    const select = $('settings-vehicle');
    state.vehicles.forEach(vehicle => {
      const option = document.createElement('option');
      option.value = vehicle.vehicleId;
      option.textContent = vehicle.vehicleId;
      select.appendChild(option);
    });
    select.value = state.vehicleId;
    await state.provider.selectVehicle(state.vehicleId);
    showAccess('authorized');
    await loadPreferences();
  }

  $('settings-login')?.addEventListener('click', () => auth.login({ remember: false }));
  $('settings-logout')?.addEventListener('click', () => auth.logout());
  $('settings-vehicle')?.addEventListener('change', event => selectVehicle(event.target.value));
  $('notification-form')?.addEventListener('submit', savePreferences);
  $('notification-email')?.addEventListener('input', updateEmailConfirmationHelp);
  $('notification-sms-request')?.addEventListener('click', requestSmsVerification);
  $('notification-sms-confirm')?.addEventListener('click', confirmSmsVerification);
  $('notification-sms-phone')?.addEventListener('input', () => {
    if (String($('notification-sms-phone').value || '').trim() !== String(state.smsStatus?.phoneE164 || '')) {
      $('notification-sms-enabled').checked = false;
    }
    renderSmsStatus();
  });
  bootstrap().catch(error => showAccess('unavailable', error.message || 'Einstellungen konnten nicht geladen werden.'));
})();

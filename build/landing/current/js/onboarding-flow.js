(() => {
  const root = document.getElementById("access-flow");
  if (!root) return;

  let currentPhase = "1";
  const activeParts = {
    "1": ["portal"],
    "2": ["adapter", "ap", "home", "mobile"],
    "3": ["adapter", "home", "mobile", "portal"]
  };
  const activeAccess = {
    "1": ["portal"],
    "2": ["ap", "local-ip"],
    "3": ["local-ip", "portal"]
  };
  const detail = {
    de: {
      "1": "Phase 1: Benutzer → Internet → MOT Portal. Der Adapter kann noch ausgeschaltet sein.",
      "2": "Phase 2: Benutzer → lokaler AP oder Home-/Mobile-WLAN-IP → MOT Adapter.",
      "3": "Phase 3: Benutzer → MOT Portal; für spätere Einstellungen auch direkt → lokale Adapter-IP."
    },
    en: {
      "1": "Phase 1: User → Internet → MOT portal. The adapter may still be switched off.",
      "2": "Phase 2: User → local AP or home/mobile WiFi IP → MOT adapter.",
      "3": "Phase 3: User → MOT portal; for later settings also directly → local adapter IP."
    },
    fr: {
      "1": "Phase 1 : Utilisateur → Internet → portail MOT. L’adaptateur peut encore être éteint.",
      "2": "Phase 2 : Utilisateur → AP local ou adresse IP WiFi domestique/mobile → adaptateur MOT.",
      "3": "Phase 3 : Utilisateur → portail MOT ; pour les réglages ultérieurs aussi directement → adresse IP locale de l’adaptateur."
    }
  };

  function selectPhase(phase) {
    currentPhase = phase;
    root.querySelectorAll("[data-phase]").forEach((button) => button.setAttribute("aria-pressed", String(button.dataset.phase === phase)));
    root.querySelectorAll("[data-flow-part]").forEach((node) => node.classList.toggle("active", activeParts[phase].includes(node.dataset.flowPart)));
    root.querySelectorAll("[data-access]").forEach((route) => route.classList.toggle("active", activeAccess[phase].includes(route.dataset.access)));
    root.querySelectorAll("[data-access-target]").forEach((target) => target.classList.toggle("active", activeAccess[phase].includes(target.dataset.accessTarget)));
    const language = window.motLandingI18n ? window.motLandingI18n.language() : "de";
    document.getElementById("phase-detail").textContent = detail[language][phase];
  }

  root.querySelectorAll("[data-phase]").forEach((button) => button.addEventListener("click", () => selectPhase(button.dataset.phase)));
  document.addEventListener("mot-language-change", () => selectPhase(currentPhase));
  selectPhase(currentPhase);
})();

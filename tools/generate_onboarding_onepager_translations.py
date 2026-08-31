#!/usr/bin/env python3
"""Generate English and French translations of the German pilot one-pager."""

from reportlab.lib.colors import Color
from reportlab.lib.pagesizes import A4
from reportlab.pdfgen import canvas

from generate_onboarding_onepager import (
    BLUE, GREEN, LINE, MID_GREEN, MUTED, ORANGE, OUTPUT, PALE_BLUE,
    PALE_GREEN, PALE_ORANGE, PALE_RED, RED, TEXT, paragraph,
)


CONTENT = {
    "en": {
        "filename": "MOT_Pilot_Onboarding_OnePager_EN.pdf",
        "title": "MOT Pilot: Set up adapter and portal",
        "intro": "The local wizard guides you through adapter setup. The portal account, vehicle access and optional services remain separate, secure steps.",
        "ready": "Have ready", "ready_body": "Adapter, USB power, smartphone/notebook, 2.4 GHz Wi-Fi or personal hotspot, and the protected inventory sheet.",
        "safety": "Safety", "safety_body": "Park the vehicle. Do not change CAN wiring, perform a factory reset or update firmware without approval.",
        "flow": "Process", "pilot": "PILOT",
        "steps": [
            ("Request and activate the MOT Portal account", "If you do not have one, request an MOT account, open the Cognito invitation, set your own portal password and confirm your email. This can be done before device setup."),
            ("Secure local access", "Connect to the MOT-xxxx Wi-Fi access point using the password on the inventory sheet, open http://192.168.4.1 and sign in as setup. Enter the new admin/hotspot password twice. Then reconnect to MOT-xxxx with the new password and sign in as admin."),
            ("Complete the wizard", "Check the hardware; GPS is enabled by default when detected. Configure Home and optionally Mobile/WiFi2 (2.4 GHz; iPhone: Maximize Compatibility), confirm the CAN profiles, select optional local services and start validation. The wizard resumes after restarts."),
            ("Connect device, vehicle and portal", "MOT provisions the inventory, AWS device identity and vehicle ID. Briefly switch on the vehicle, verify the first telemetry and create a one-time claim. In the portal choose Connect vehicle and enter the claim code once."),
            ("Verify completion", "Check the online status and plausible values. After the wizard, local access uses the active Wi-Fi and displayed IP address; the address may change and appears in the MOT Portal after linking. If no configured Wi-Fi is reachable, MOT-xxxx returns."),
        ],
        "optional": "Optional services - coordinate with the MOT administrator",
        "optional_intro": "These services can be enabled during onboarding or later. They do not block the account, vehicle link or adapter operation.",
        "services": [("History", "MOT enables the vehicle ID administratively."), ("Email", "Enable per vehicle in the portal, confirm the destination/subscription, and have MOT verify the effective state."), ("SMS", "Verify the number, wait for MOT pilot approval, then enable SMS for the vehicle and save.")],
        "stop": "Stop and contact MOT", "stop_body": "If the MOT access point or sign-in is missing, validation reports an error, CAN counters do not increase, AWS/portal remains offline, the claim fails, or another vehicle is visible. Never send passwords, claim/SMS codes or certificates.",
        "footer": "MOT pilot guide - never configure the adapter while driving", "date": "Version 27.08.2026 | Page 1/1",
    },
    "fr": {
        "filename": "MOT_Pilot_Onboarding_OnePager_FR.pdf",
        "title": "Pilote MOT : configurer l’adaptateur et le portail",
        "intro": "L’assistant local vous guide dans la configuration de l’adaptateur. Le compte du portail, l’accès au véhicule et les services optionnels restent des étapes distinctes et sécurisées.",
        "ready": "À préparer", "ready_body": "Adaptateur, alimentation USB, smartphone/ordinateur, Wi-Fi 2,4 GHz ou partage de connexion, et fiche d’inventaire protégée.",
        "safety": "Sécurité", "safety_body": "Immobilisez le véhicule. Ne modifiez pas le câblage CAN et n’effectuez ni réinitialisation d’usine ni mise à jour du firmware sans autorisation.",
        "flow": "Déroulement", "pilot": "PILOTE",
        "steps": [
            ("Demander et activer le compte MOT Portal", "Si nécessaire, demandez un compte MOT, ouvrez l’invitation Cognito, définissez votre propre mot de passe du portail et confirmez votre e-mail. Cette étape peut précéder la configuration de l’appareil."),
            ("Sécuriser l’accès local", "Connectez-vous au point d’accès Wi-Fi MOT-xxxx avec le mot de passe de la fiche d’inventaire, ouvrez http://192.168.4.1 et connectez-vous comme setup. Saisissez deux fois le nouveau mot de passe admin/hotspot. Reconnectez-vous ensuite à MOT-xxxx avec ce mot de passe comme admin."),
            ("Terminer l’assistant", "Contrôlez le matériel ; le GPS est activé par défaut s’il est détecté. Configurez Home et éventuellement Mobile/WiFi2 (2,4 GHz ; iPhone : Maximize Compatibility), confirmez les profils CAN, choisissez les services locaux optionnels et lancez la validation. L’assistant reprend après les redémarrages."),
            ("Associer appareil, véhicule et portail", "MOT prépare l’inventaire, l’identité AWS de l’appareil et l’identifiant du véhicule. Mettez brièvement le véhicule sous tension, vérifiez la première télémétrie et créez un rattachement à usage unique. Dans le portail, choisissez Associer un véhicule et saisissez le code une seule fois."),
            ("Vérifier la finalisation", "Contrôlez l’état en ligne et la plausibilité des valeurs. Après l’assistant, l’accès local utilise le Wi-Fi actif et l’adresse IP affichée ; elle peut changer et apparaît dans MOT Portal après l’association. Si aucun Wi-Fi configuré n’est joignable, MOT-xxxx réapparaît."),
        ],
        "optional": "Services optionnels - à coordonner avec l’administrateur MOT",
        "optional_intro": "Ces services peuvent être activés pendant l’onboarding ou plus tard. Ils ne bloquent ni le compte, ni l’association du véhicule, ni le fonctionnement de l’adaptateur.",
        "services": [("Historique", "MOT active administrativement l’identifiant du véhicule."), ("E-mail", "Activez-le par véhicule dans le portail, confirmez la destination/l’abonnement et faites vérifier l’état effectif par MOT."), ("SMS", "Vérifiez le numéro, attendez l’autorisation pilote MOT, puis activez les SMS pour le véhicule et enregistrez.")],
        "stop": "Arrêtez et contactez MOT", "stop_body": "Si le point d’accès MOT ou la connexion manque, si la validation signale une erreur, si les compteurs CAN n’augmentent pas, si AWS/le portail reste hors ligne, si le rattachement échoue ou si un autre véhicule est visible. N’envoyez jamais de mots de passe, codes de rattachement/SMS ou certificats.",
        "footer": "Guide pilote MOT - ne jamais configurer l’adaptateur en roulant", "date": "Version 27.08.2026 | Page 1/1",
    },
}


def step(c, number, owner, title, body, x, y, width, height, pilot_label):
    badge_w, owner_w = 28, 49
    c.setStrokeColor(LINE); c.setFillColor(Color(1, 1, 1)); c.rect(x, y-height, width, height, stroke=1, fill=1)
    c.setFillColor(MID_GREEN if owner == "PILOT" else BLUE); c.rect(x, y-height, badge_w, height, stroke=0, fill=1)
    c.setFillColor(Color(1, 1, 1)); c.setFont("Helvetica-Bold", 11); c.drawCentredString(x+badge_w/2, y-height/2-4, str(number))
    c.setFillColor(MID_GREEN if owner == "PILOT" else BLUE); c.setFont("Helvetica-Bold", 7.5); c.drawString(x+badge_w+6, y-14, pilot_label if owner == "PILOT" else "MOT")
    body_x = x+badge_w+owner_w
    c.setFillColor(TEXT); c.setFont("Helvetica-Bold", 9.2); c.drawString(body_x, y-13, title)
    paragraph(c, body, body_x, y-25, width-badge_w-owner_w-8, size=8.1, leading=9.6)
    return y-height


def generate_language(content):
    target = OUTPUT.parent / content["filename"]
    c = canvas.Canvas(str(target), pagesize=A4)
    c.setTitle(content["title"])
    width, height = A4; margin = 36; content_w = width-2*margin
    c.setFillColor(GREEN); c.rect(0, height-14, width, 14, stroke=0, fill=1)
    c.setFont("Helvetica-Bold", 21); c.drawString(margin, height-54, content["title"])
    paragraph(c, content["intro"], margin, height-72, content_w, size=9.2, leading=11.5, color=MUTED)
    y = height-103; box_h = 45
    c.setFillColor(PALE_GREEN); c.setStrokeColor(MID_GREEN); c.rect(margin, y-box_h, content_w/2, box_h, stroke=1, fill=1)
    c.setFont("Helvetica-Bold", 9); c.setFillColor(GREEN); c.drawString(margin+8, y-13, content["ready"])
    paragraph(c, content["ready_body"], margin+8, y-25, content_w/2-16, size=7.8, leading=9.1)
    c.setFillColor(PALE_ORANGE); c.setStrokeColor(ORANGE); c.rect(margin+content_w/2, y-box_h, content_w/2, box_h, stroke=1, fill=1)
    c.setFont("Helvetica-Bold", 9); c.setFillColor(ORANGE); c.drawString(margin+content_w/2+8, y-13, content["safety"])
    paragraph(c, content["safety_body"], margin+content_w/2+8, y-25, content_w/2-16, size=7.8, leading=9.1)
    y -= box_h+18; c.setFillColor(GREEN); c.setFont("Helvetica-Bold", 14); c.drawString(margin, y, content["flow"]); y -= 8
    heights = [55, 65, 65, 57, 62]
    for index, ((title, body), step_height) in enumerate(zip(content["steps"], heights), 1):
        y = step(c, index, "MOT" if index == 4 else "PILOT", title, body, margin, y, content_w, step_height, content["pilot"])
    y -= 10; optional_h = 112
    c.setFillColor(PALE_BLUE); c.setStrokeColor(BLUE); c.rect(margin, y-optional_h, content_w, optional_h, stroke=1, fill=1)
    c.setFillColor(BLUE); c.setFont("Helvetica-Bold", 11); c.drawString(margin+10, y-17, content["optional"])
    paragraph(c, content["optional_intro"], margin+10, y-31, content_w-20, size=8, leading=9.5)
    for offset, (label, body) in zip((55, 74, 94), content["services"]):
        c.setFillColor(TEXT); c.setFont("Helvetica-Bold", 8.4); c.drawString(margin+10, y-offset, label)
        paragraph(c, body, margin+78, y-offset, content_w-88, size=8, leading=9.2)
    y -= optional_h+10; stop_h = 45
    c.setFillColor(PALE_RED); c.setStrokeColor(RED); c.rect(margin, y-stop_h, content_w, stop_h, stroke=1, fill=1)
    c.setFillColor(RED); c.setFont("Helvetica-Bold", 8.5); c.drawString(margin+9, y-13, content["stop"])
    paragraph(c, content["stop_body"], margin+9, y-25, content_w-18, size=7.4, leading=8.5)
    c.setStrokeColor(LINE); c.line(margin, 38, width-margin, 38); c.setFillColor(MUTED); c.setFont("Helvetica", 7.5)
    c.drawString(margin, 25, content["footer"]); c.drawRightString(width-margin, 25, content["date"]); c.save()


def generate_translations():
    for content in CONTENT.values():
        generate_language(content)

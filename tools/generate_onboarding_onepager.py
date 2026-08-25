#!/usr/bin/env python3
"""Generate the German MOT pilot onboarding one-pager."""

from pathlib import Path

from reportlab.lib.colors import Color, HexColor
from reportlab.lib.pagesizes import A4
from reportlab.pdfbase.pdfmetrics import stringWidth
from reportlab.pdfgen import canvas


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "output/pdf/MOT_Pilot_Onboarding_OnePager.pdf"

GREEN = HexColor("#0F5D4A")
MID_GREEN = HexColor("#178B6B")
PALE_GREEN = HexColor("#E8F3EF")
BLUE = HexColor("#246B91")
PALE_BLUE = HexColor("#EAF2F7")
ORANGE = HexColor("#E57A18")
PALE_ORANGE = HexColor("#FFF3E8")
RED = HexColor("#C93C3C")
PALE_RED = HexColor("#FFF0F0")
TEXT = HexColor("#26333D")
MUTED = HexColor("#60717E")
LINE = HexColor("#CAD5DC")


def wrapped_lines(text: str, font: str, size: float, width: float) -> list[str]:
    words = text.split()
    lines: list[str] = []
    current = ""
    for word in words:
        candidate = word if not current else f"{current} {word}"
        if stringWidth(candidate, font, size) <= width:
            current = candidate
        else:
            if current:
                lines.append(current)
            current = word
    if current:
        lines.append(current)
    return lines


def paragraph(c: canvas.Canvas, text: str, x: float, y: float, width: float,
              font: str = "Helvetica", size: float = 8.7,
              leading: float = 11.0, color=TEXT) -> float:
    c.setFont(font, size)
    c.setFillColor(color)
    for line in wrapped_lines(text, font, size, width):
        c.drawString(x, y, line)
        y -= leading
    return y


def step(c: canvas.Canvas, number: int, owner: str, title: str, body: str,
         x: float, y: float, width: float, height: float) -> float:
    badge_w = 28
    owner_w = 49
    c.setStrokeColor(LINE)
    c.setFillColor(Color(1, 1, 1))
    c.rect(x, y - height, width, height, stroke=1, fill=1)
    c.setFillColor(MID_GREEN if owner == "PILOT" else BLUE)
    c.rect(x, y - height, badge_w, height, stroke=0, fill=1)
    c.setFillColor(Color(1, 1, 1))
    c.setFont("Helvetica-Bold", 11)
    c.drawCentredString(x + badge_w / 2, y - height / 2 - 4, str(number))
    c.setFillColor(MID_GREEN if owner == "PILOT" else BLUE)
    c.setFont("Helvetica-Bold", 7.5)
    c.drawString(x + badge_w + 6, y - 14, owner)
    body_x = x + badge_w + owner_w
    c.setFillColor(TEXT)
    c.setFont("Helvetica-Bold", 9.2)
    c.drawString(body_x, y - 13, title)
    paragraph(c, body, body_x, y - 25, width - badge_w - owner_w - 8,
              size=8.1, leading=9.6)
    return y - height


def generate() -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    c = canvas.Canvas(str(OUTPUT), pagesize=A4)
    c.setTitle("MOT Pilot - Onboarding mit lokalem Wizard")
    width, height = A4
    margin = 36
    content_w = width - 2 * margin

    c.setFillColor(GREEN)
    c.rect(0, height - 14, width, 14, stroke=0, fill=1)
    c.setFont("Helvetica-Bold", 23)
    c.drawString(margin, height - 54, "MOT Pilot: Adapter und Portal einrichten")
    paragraph(
        c,
        "Der lokale Wizard fuehrt durch die Adaptereinrichtung. Portal-Konto, Fahrzeugfreigabe und optionale Dienste bleiben getrennte, sichere Schritte.",
        margin, height - 72, content_w, size=9.2, leading=11.5, color=MUTED,
    )

    y = height - 103
    box_h = 45
    c.setFillColor(PALE_GREEN)
    c.setStrokeColor(MID_GREEN)
    c.rect(margin, y - box_h, content_w / 2, box_h, stroke=1, fill=1)
    c.setFont("Helvetica-Bold", 9)
    c.setFillColor(GREEN)
    c.drawString(margin + 8, y - 13, "Bereithalten")
    paragraph(c, "Adapter, USB-Strom, Smartphone/Notebook, 2.4-GHz-WLAN oder persoenlicher Hotspot und Inventarblatt.", margin + 8, y - 25, content_w / 2 - 16, size=7.8, leading=9.1)
    c.setFillColor(PALE_ORANGE)
    c.setStrokeColor(ORANGE)
    c.rect(margin + content_w / 2, y - box_h, content_w / 2, box_h, stroke=1, fill=1)
    c.setFont("Helvetica-Bold", 9)
    c.setFillColor(ORANGE)
    c.drawString(margin + content_w / 2 + 8, y - 13, "Sicherheit")
    paragraph(c, "Fahrzeug abstellen. Keine CAN-Verkabelung, kein Factory Reset und kein Firmware-Update ohne Freigabe.", margin + content_w / 2 + 8, y - 25, content_w / 2 - 16, size=7.8, leading=9.1)

    y -= box_h + 18
    c.setFillColor(GREEN)
    c.setFont("Helvetica-Bold", 14)
    c.drawString(margin, y, "Ablauf")
    y -= 8

    y = step(c, 1, "PILOT", "MOT Portal Account beantragen und aktivieren",
             "Falls noch nicht vorhanden: Account bei MOT beantragen, Cognito-Einladung oeffnen, eigenes Portal-Passwort setzen und E-Mail bestaetigen. Dies ist bereits vor der Geraeteeinrichtung moeglich.",
             margin, y, content_w, 55)
    y = step(c, 2, "PILOT", "Lokalen Zugang absichern",
             "Mit WLAN MOT-xxxx verbinden (Passwort laut Inventarblatt), http://192.168.4.1 oeffnen und als setup anmelden. Neues Admin-/Hotspot-Passwort zweimal eingeben. Danach erneut mit MOT-xxxx und dem neuen Passwort verbinden und als admin anmelden.",
             margin, y, content_w, 65)
    y = step(c, 3, "PILOT", "Wizard abschliessen",
             "Hardware pruefen; GPS ist bei erkanntem Modul standardmaessig aktiv. Home und optional Mobile/WiFi2 einrichten (2.4 GHz; iPhone: Maximize Compatibility), CAN-Profile bestaetigen, optionale lokale Dienste waehlen und Validation starten. Der Wizard setzt nach Neustarts fort.",
             margin, y, content_w, 65)
    y = step(c, 4, "MOT", "Geraet, Fahrzeug und Portal verbinden",
             "MOT provisioniert Inventar, AWS-Geraeteidentitaet und Fahrzeug-ID. Fahrzeug kurz einschalten, erste Telemetrie pruefen und Einmal-Claim erstellen. Im Portal Fahrzeug verbinden waehlen und den Claim-Code einmal eingeben.",
             margin, y, content_w, 57)
    y = step(c, 5, "PILOT", "Abschluss pruefen",
             "Online-Status und plausible Werte kontrollieren. Nach Wizard-Abschluss erfolgt lokaler Zugriff ueber das aktive WLAN und die angezeigte IP; diese kann sich aendern und wird nach erfolgreicher Verknuepfung auch im MOT Portal angezeigt. Ohne erreichbares WLAN kehrt MOT-xxxx zurueck.",
             margin, y, content_w, 62)

    y -= 10
    optional_h = 112
    c.setFillColor(PALE_BLUE)
    c.setStrokeColor(BLUE)
    c.rect(margin, y - optional_h, content_w, optional_h, stroke=1, fill=1)
    c.setFillColor(BLUE)
    c.setFont("Helvetica-Bold", 11)
    c.drawString(margin + 10, y - 17, "Optionale Dienste - mit MOT-Administrator abstimmen")
    paragraph(c, "Diese Dienste koennen beim Onboarding oder spaeter freigegeben werden. Sie blockieren Account, Fahrzeugverknuepfung und Adapterbetrieb nicht.", margin + 10, y - 31, content_w - 20, size=8, leading=9.5)
    c.setFillColor(TEXT)
    c.setFont("Helvetica-Bold", 8.4)
    c.drawString(margin + 10, y - 55, "History")
    paragraph(c, "MOT schaltet die Fahrzeug-ID administrativ frei.", margin + 78, y - 55, content_w - 88, size=8, leading=9.2)
    c.setFont("Helvetica-Bold", 8.4)
    c.drawString(margin + 10, y - 74, "E-Mail")
    paragraph(c, "Im Portal pro Fahrzeug aktivieren; Zieladresse/Subscription bestaetigen. MOT prueft den wirksamen Zustand.", margin + 78, y - 74, content_w - 88, size=8, leading=9.2)
    c.setFont("Helvetica-Bold", 8.4)
    c.drawString(margin + 10, y - 94, "SMS")
    paragraph(c, "Nummer bestaetigen, Pilot-Freigabe durch MOT abwarten, danach SMS pro Fahrzeug aktivieren und speichern.", margin + 78, y - 94, content_w - 88, size=8, leading=9.2)
    y -= optional_h + 10

    stop_h = 45
    c.setFillColor(PALE_RED)
    c.setStrokeColor(RED)
    c.rect(margin, y - stop_h, content_w, stop_h, stroke=1, fill=1)
    c.setFillColor(RED)
    c.setFont("Helvetica-Bold", 8.5)
    c.drawString(margin + 9, y - 13, "Stoppen und MOT kontaktieren")
    paragraph(c, "Wenn MOT-AP oder Anmeldung fehlt, Validation Fehler zeigt, CAN-Zaehler nicht steigen, AWS/Portal offline bleibt, der Claim scheitert oder ein fremdes Fahrzeug sichtbar ist. Nie Passwoerter, Claim-/SMS-Codes oder Zertifikate senden.", margin + 9, y - 25, content_w - 18, size=7.6, leading=8.8)

    c.setStrokeColor(LINE)
    c.line(margin, 38, width - margin, 38)
    c.setFillColor(MUTED)
    c.setFont("Helvetica", 7.5)
    c.drawString(margin, 25, "MOT Pilotunterlage - Adapter niemals waehrend der Fahrt konfigurieren")
    c.drawRightString(width - margin, 25, "Stand 25.08.2026 | Seite 1/1")
    c.save()


if __name__ == "__main__":
    generate()

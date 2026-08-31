#!/usr/bin/env python3
"""Generate the printable MOT ESP32-C6 N16 Dual-CAN and GPS wiring sheet."""

from pathlib import Path

from reportlab.lib.colors import HexColor, white
from reportlab.lib.pagesizes import A4, landscape
from reportlab.pdfgen import canvas


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "docs/assets/pdfs/hardware/nanoesp32-c6-n16-dual-twai-verkabelung.pdf"
W, H = landscape(A4)

INK = HexColor("#17324D")
MUTED = HexColor("#5D6B82")
BLUE = HexColor("#246BFD")
BLUE_BG = HexColor("#EDF3FF")
GREEN = HexColor("#008C7A")
GREEN_BG = HexColor("#EAF8F4")
ORANGE = HexColor("#E77700")
ORANGE_BG = HexColor("#FFF4E8")
GOLD = HexColor("#A86B00")
GOLD_BG = HexColor("#FFF8E3")
RED = HexColor("#C62828")
GRAY_BG = HexColor("#F3F6F9")


def box(c, x, y, w, h, stroke, fill, title, title_color=None):
    c.setFillColor(fill)
    c.setStrokeColor(stroke)
    c.setLineWidth(1.5)
    c.roundRect(x, y, w, h, 9, fill=1, stroke=1)
    c.setFillColor(title_color or stroke)
    c.setFont("Helvetica-Bold", 12)
    c.drawString(x + 12, y + h - 20, title)


def label(c, x, y, text, size=8.3, color=INK, bold=False):
    c.setFillColor(color)
    c.setFont("Helvetica-Bold" if bold else "Helvetica", size)
    c.drawString(x, y, text)


def line(c, x1, y1, x2, y2, color, width=2, dash=None):
    c.setStrokeColor(color)
    c.setLineWidth(width)
    c.setDash(dash or [])
    c.line(x1, y1, x2, y2)
    c.setDash([])


def pin(c, x, y, w, text, stroke=BLUE):
    c.setFillColor(white)
    c.setStrokeColor(stroke)
    c.setLineWidth(1)
    c.roundRect(x, y, w, 18, 4, fill=1, stroke=1)
    c.setFillColor(INK)
    c.setFont("Helvetica", 7.7)
    c.drawCentredString(x + w / 2, y + 5.5, text)


def can_block(c, x, y, color, fill, number, default_name, rx_gpio, tx_gpio):
    box(c, x, y, 205, 145, color, fill, f"{number}  CAN{number} - SN65HVD230")
    pin(c, x + 12, y + 91, 82, "VCC -> 3V3", color)
    pin(c, x + 108, y + 91, 82, "GND -> GND", color)
    pin(c, x + 12, y + 63, 82, f"RXD -> GPIO{rx_gpio}", color)
    pin(c, x + 108, y + 63, 82, f"TXD -> JP -> GPIO{tx_gpio}", color)
    pin(c, x + 12, y + 35, 82, "CAN-H -> Bus", color)
    pin(c, x + 108, y + 35, 82, "CAN-L -> Bus", color)
    label(c, x + 12, y + 17, f"TXD: 10 kOhm an 3V3; JP{number} OFFEN", 6.8, RED)
    label(c, x + 12, y + 5, default_name, 6.8, MUTED)


def generate():
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    c = canvas.Canvas(str(OUTPUT), pagesize=landscape(A4))
    c.setTitle("nanoESP32-C6-N16 Dual-CAN und GPS Verkabelung")
    c.setAuthor("Microlino Open Telemetry")

    label(c, 30, H - 38, "nanoESP32-C6-N16 - Dual-CAN und GPS Verkabelung", 20, INK, True)
    label(c, 30, H - 56, "MOT ESP32-C6 Gen.2 | passive CAN-Eingänge | optionales DA37+DA10 GNSS", 9.5, MUTED)
    label(c, W - 119, H - 38, "Rev. 0.2 | 25.08.2026", 8, MUTED)

    # N16 board
    box(c, 30, 182, 205, 330, BLUE, BLUE_BG, "1  nanoESP32-C6-N16")
    board_pins = [
        (455, "3V3 OUT"), (428, "GND"),
        (384, "GPIO0 - CAN1 RX"), (357, "GPIO1 - CAN1 TX"),
        (313, "GPIO2 - CAN2 RX"), (286, "GPIO3 - CAN2 TX"),
        (234, "GPIO20 - GPS RX"), (207, "GPIO21 - GPS TX"),
    ]
    for yy, text in board_pins:
        pin(c, 52, yy, 158, text)

    # CAN blocks
    can_block(c, 275, 367, GREEN, GREEN_BG, "1", "Default: Standard-CAN V1 Pioneer", 0, 1)
    can_block(c, 275, 202, ORANGE, ORANGE_BG, "2", "Default: Display-CAN", 2, 3)

    # GPS block
    box(c, 510, 327, 300, 185, GOLD, GOLD_BG, "4  Optional: DA37+DA10 GNSS")
    label(c, 524, 468, "Separate Antenne - Empfänger bleibt im Gehäuse", 8, MUTED)
    pin(c, 524, 427, 82, "TX -> GPIO20", GOLD)
    pin(c, 620, 427, 82, "RX <- GPIO21", GOLD)
    pin(c, 716, 427, 76, "PPS: OFFEN", GOLD)
    pin(c, 524, 393, 82, "VCC 3V3", GOLD)
    pin(c, 620, 393, 82, "GND", GOLD)
    label(c, 524, 360, "Direkt am GPS: 100 nF Keramik parallel 47 uF Elko", 8, INK, True)
    label(c, 524, 344, "Elko Plus an 3V3, Minus an GND; Kondensatoren nicht an Backup-Batterie", 7.3, MUTED)

    # Vehicle buses
    box(c, 510, 202, 300, 105, INK, GRAY_BG, "5  Microlino Fahrzeugbusse")
    label(c, 530, 266, "CAN1: Standard-CAN | 500 kbit/s", 9, GREEN, True)
    label(c, 530, 239, "CAN2: Display-CAN  | 500 kbit/s", 9, ORANGE, True)
    label(c, 530, 217, "CAN-H/CAN-L je Bus verdrillt; gemeinsame Signalmasse", 7.5, MUTED)

    # Rules footer
    box(c, 30, 30, 780, 125, ORANGE, ORANGE_BG, "Wichtige Hardware-Regeln", INK)
    rules_left = [
        "- Beide CAN-TX-Jumper bleiben im Pilotbetrieb offen.",
        "- Je CAN-Modul 100 nF; gemeinsam nahe CAN 10 uF.",
        "- 120-Ohm-Abschluss auf beiden Adaptermodulen entfernen/deaktivieren.",
    ]
    rules_right = [
        "- GPS: 100 nF + 47 uF direkt an VCC/GND; PPS offen.",
        "- Externes 5V und USB-VBUS nicht ungeschutzt parallel speisen.",
        "- Firmware: beide TWAI-Controller 500 kbit/s, Listen-only.",
    ]
    for i, text in enumerate(rules_left):
        label(c, 50, 119 - i * 25, text, 8.2, INK)
    for i, text in enumerate(rules_right):
        label(c, 425, 119 - i * 25, text, 8.2, INK)
    label(c, 30, 14, "Microlino Open Telemetry - C6 Gen.2 pilot wiring", 7.2, MUTED)
    c.save()


if __name__ == "__main__":
    generate()

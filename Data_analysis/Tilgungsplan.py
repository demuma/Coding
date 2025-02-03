import datetime
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.platypus import SimpleDocTemplate, Table, TableStyle, Paragraph
from reportlab.lib.styles import getSampleStyleSheet

def tilgungsplan(kreditsumme, monatliche_rate, start_jahr, start_monat):
  """
  Erstellt einen Tilgungsplan für einen zinslosen Kredit mit "Abgezahlt"-Spalte
  und berechnet die Restschuld basierend auf dem aktuellen Datum.

  Args:
    kreditsumme: Die Höhe des Kredits.
    monatliche_rate: Die Höhe der monatlichen Rate.
    start_jahr: Das Jahr, in dem die Tilgung beginnt.
    start_monat: Der Monat, in dem die Tilgung beginnt.

  Returns:
    Eine Liste von Tupeln, wobei jedes Tupel (Jahr, Monat, Rate, Restschuld, Abgezahlt)
    einen Eintrag im Tilgungsplan darstellt.
  """
  restschuld = kreditsumme
  jahr = start_jahr
  monat = start_monat
  plan = []
  abgezahlt_gesamt = 0

  heute = datetime.date.today()

  while restschuld > 0:
    abgezahlt = 0
    faelligkeitsdatum = datetime.date(jahr, monat, 1)

    if faelligkeitsdatum < heute:
      abgezahlt = min(monatliche_rate, restschuld)
      abgezahlt_gesamt += abgezahlt

    plan.append((jahr, monat, min(monatliche_rate, restschuld), restschuld, abgezahlt))

    restschuld -= min(monatliche_rate, restschuld)
    if restschuld < 0:
      restschuld = 0

    monat += 1
    if monat > 12:
      monat = 1
      jahr += 1

  return plan, abgezahlt_gesamt

def erstelle_tilgungsplan_pdf(kreditsumme, monatliche_rate, start_jahr, start_monat):
    """
    Erstellt einen Tilgungsplan als PDF mit Titel, Linien und aktuellem Datum.

    Args:
        kreditsumme: Die Höhe des Kredits.
        monatliche_rate: Die Höhe der monatlichen Rate.
        start_jahr: Das Jahr, in dem die Tilgung beginnt.
        start_monat: Der Monat, in dem die Tilgung beginnt.
    """
    # Tilgungsplan berechnen
    plan, abgezahlt_gesamt = tilgungsplan(kreditsumme, monatliche_rate, start_jahr, start_monat)

    # Daten für die PDF-Tabelle vorbereiten
    data = [["Monat", "Jahr", "Rate", "Restschuld", "Abgezahlt"]]
    for jahr, monat, rate, restschuld, abgezahlt in plan:
        monat_name = {
            1: "Januar", 2: "Februar", 3: "März", 4: "April", 5: "Mai", 6: "Juni",
            7: "Juli", 8: "August", 9: "September", 10: "Oktober", 11: "November", 12: "Dezember"
        }[monat]
        data.append([monat_name, str(jahr), f"{rate:.2f}€", f"{restschuld:.2f}€", f"{abgezahlt:.2f}€"])

    # PDF-Dokument erstellen
    doc = SimpleDocTemplate("Tilgungsplan.pdf", pagesize=A4)
    styles = getSampleStyleSheet()
    elements = []

    # Titel hinzufügen
    title = "Tilgungsplan"
    elements.append(Paragraph(title, styles['Title']))

    # Kreditinformationen hinzufügen
    info = [
        f"Kreditsumme: {kreditsumme:.2f}€",
        f"Monatliche Rate: {monatliche_rate:.2f}€",
        f"Startdatum: {start_monat:02d}.{start_jahr}",
        f"Aktuelles Datum: {datetime.date.today().strftime('%d.%m.%Y')}",
        f"Bereits abgezahlt: {abgezahlt_gesamt:.2f}€",
        f"Restschuld (Stand {datetime.date.today().strftime('%d.%m.%Y')}): {kreditsumme - abgezahlt_gesamt:.2f}€"
    ]
    for line in info:
      elements.append(Paragraph(line, styles['Normal']))
    elements.append(Paragraph("<br />", styles['Normal'])) # Leerzeile einfügen

    # Tabelle erstellen
    table = Table(data)

    # Tabellenstil definieren
    style = TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), colors.grey),
        ('TEXTCOLOR', (0, 0), (-1, 0), colors.black),
        ('ALIGN', (0, 0), (-1, -1), 'CENTER'),
        ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
        ('BOTTOMPADDING', (0, 0), (-1, 0), 12),
        ('BACKGROUND', (0, 1), (-1, -1), colors.white),
        ('GRID', (0, 0), (-1, -1), 1, colors.black)
    ])
    table.setStyle(style)

    # Tabelle zum Dokument hinzufügen
    elements.append(table)

    # PDF-Datei erstellen
    doc.build(elements)

    print("Tilgungsplan als PDF-Datei 'Tilgungsplan.pdf' erstellt.")

# Parameter für den Tilgungsplan (hier kannst du die Werte anpassen)
kreditsumme = 6450
monatliche_rate = 130
start_jahr = 2025
start_monat = 2

# Tilgungsplan als PDF erstellen
erstelle_tilgungsplan_pdf(kreditsumme, monatliche_rate, start_jahr, start_monat)
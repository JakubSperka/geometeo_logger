# GeoMeteo Logger

**Prenosný záznamník atmosférických parametrov pre geodetické merania**

> [!NOTE]
> Dokumentácia opisuje aktuálnu vývojovú vetvu **v0.8.0**.  
> Posledný overený stabilný build základnej funkcionality je **v0.8.0**.

GeoMeteo Logger je prenosné zariadenie určené na meranie a záznam:

- teploty vzduchu,
- relatívnej vlhkosti,
- atmosférického tlaku,
- napätia a odhadovaného stavu batérie.

Merania sa ukladajú na SD kartu vo formáte CSV. Zariadenie podporuje automatické aj manuálne záznamy, meracie relácie, živé grafy, obnovu po reštarte, kalibráciu, úsporný režim displeja a bezdrôtový export ukončených relácií cez Wi‑Fi.

> [!NOTE]
> **PLACEHOLDER – FOTO ZARIADENIA**  
> Doplňte fotografiu kompletného zariadenia.  
> Odporúčaný súbor: `docs/images/geometeo-logger-overview.jpg`

---

## Obsah

1. [Hlavné funkcie](#hlavné-funkcie)
2. [Hardvér](#hardvér)
3. [Prvé spustenie](#prvé-spustenie)
4. [Hlavná obrazovka](#hlavná-obrazovka)
5. [Meracie relácie](#meracie-relácie)
6. [Automatické a manuálne záznamy](#automatické-a-manuálne-záznamy)
7. [Grafy](#grafy)
8. [Archív relácií](#archív-relácií)
9. [Kalibrácia](#kalibrácia)
10. [Nastavenia](#nastavenia)
11. [Úspora energie](#úspora-energie)
12. [Wi‑Fi export](#wi-fi-export)
13. [CSV súbory](#csv-súbory)
14. [Odporúčaný postup merania](#odporúčaný-postup-merania)
15. [Stavové a chybové hlásenia](#stavové-a-chybové-hlásenia)
16. [Obnova relácie po reštarte](#obnova-relácie-po-reštarte)
17. [Údržba](#údržba)
18. [Riešenie problémov](#riešenie-problémov)
19. [Štruktúra repozitára](#štruktúra-repozitára)
20. [Licencia](#licencia)

---

## Hlavné funkcie

GeoMeteo Logger poskytuje:

- živé zobrazenie teploty, vlhkosti a tlaku,
- automatický zápis v intervale 1, 5, 10, 30 alebo 60 sekúnd,
- manuálny záznam priradený ku konkrétnemu meranému bodu,
- meraciu reláciu s pevným stanoviskom,
- ukladanie údajov na SD kartu,
- oddelený CSV súbor pre každú reláciu,
- živé grafy posledných 120 záznamov alebo celej relácie,
- archív ukončených relácií,
- obnovenie nedokončenej relácie po reštarte,
- lineárnu kalibráciu každého senzora,
- uloženie surových aj korigovaných hodnôt,
- nastaviteľný jas displeja,
- automatické stlmenie a vypnutie podsvietenia,
- voľbu farebnej schémy používateľského rozhrania,
- export CSV cez vlastnú Wi‑Fi sieť zariadenia,
- validáciu meraných hodnôt a opakovaný pokus o zápis na SD kartu.

---

## Hardvér

### Hlavné komponenty

| Komponent | Použitie |
|---|---|
| LaskaKit ESPD‑35 v3.2 | hlavná doska s ESP32‑S3, TFT displejom a dotykom |
| TFT ILI9488, 480 × 320 px | používateľské rozhranie |
| FT5436/FT6236 | dotyková vrstva |
| SHT40 | teplota a relatívna vlhkosť |
| BMP280 | atmosférický tlak |
| DS3231 | dátum a čas |
| SD karta | ukladanie meracích relácií |
| Li‑Po batéria | prenosné napájanie |

> [!NOTE]
> **PLACEHOLDER – SCHÉMA HARDVÉRU**  
> Doplňte blokovú schému zapojenia zariadenia.  
> Odporúčaný súbor: `docs/images/hardware-block-diagram.png`

### Použité zbernice a piny

#### Interná I²C zbernica

| Signál | GPIO |
|---|---:|
| SDA | 42 |
| SCL | 2 |

Interná zbernica je použitá najmä pre dotykový ovládač. Interné senzory dosky sa vo firmvéri GeoMeteo Logger nepoužívajú.

#### Externá I²C zbernica

| Signál | GPIO |
|---|---:|
| SDA | 5 |
| SCL | 6 |

| Zariadenie | I²C adresa |
|---|---:|
| SHT40 | `0x44` |
| DS3231 | `0x68` |
| BMP280 | `0x77` |

#### SD karta

| Signál | GPIO |
|---|---:|
| SCK | 14 |
| MISO | 21 |
| MOSI | 38 |
| CS | 17 |

#### Batéria

Meranie napätia batérie je pripojené na `GPIO9`.

---

## Prvé spustenie

1. Vložte SD kartu naformátovanú na **FAT32**.
2. Pripojte externý meteorologický modul a RTC.
3. Zapnite zariadenie.
4. Počkajte na dokončenie inicializácie.
5. Skontrolujte stavové indikátory na hlavnej obrazovke.
6. Nastavte správny dátum a čas.
7. Nastavte požadovaný interval automatického záznamu.
8. Pred prvým ostrým meraním vytvorte krátku skúšobnú reláciu.

Po úspešnom štarte sa zobrazí názov projektu a verzia firmvéru:

```text
GEOMETEO LOGGER
vX.Y.Z
```

> [!NOTE]
> **PLACEHOLDER – FOTO ÚVODNEJ OBRAZOVKY**  
> Doplňte fotografiu alebo snímku úvodnej obrazovky.  
> Odporúčaný súbor: `docs/images/startup-screen.jpg`

---

## Hlavná obrazovka

Hlavná obrazovka zobrazuje:

- aktuálny dátum a čas,
- stav batérie,
- teplotu,
- relatívnu vlhkosť,
- atmosférický tlak,
- stanovisko,
- aktuálny meraný bod,
- počet manuálnych záznamov,
- stav senzorov, SD karty, relácie a záznamu,
- tlačidlo na spustenie alebo ukončenie relácie,
- tlačidlo na manuálne uloženie merania,
- skratky do archívu, grafov, Wi‑Fi exportu a nastavení.

> [!NOTE]
> **PLACEHOLDER – HLAVNÁ OBRAZOVKA**  
> Doplňte snímku hlavnej obrazovky s popisom jednotlivých prvkov.  
> Odporúčaný súbor: `docs/images/main-screen-annotated.png`

### Stavové indikátory

Farba bodu vyjadruje stav:

| Farba | Význam |
|---|---|
| zelená | funkcia je pripravená alebo zápis prebehol správne |
| oranžová | upozornenie alebo prechodný stav |
| červená | chyba alebo nedostupná funkcia |
| sivá | funkcia nie je aktívna |

Sledované stavy:

- `SENZOR`
- `SD`
- `RELACIA`
- `ZAZNAM`

---

## Meracie relácie

Každé meranie sa organizuje do samostatnej relácie.

Relácia obsahuje:

- identifikátor relácie,
- stanovisko,
- čas začiatku a konca,
- automatické záznamy,
- manuálne záznamy,
- merané body,
- kalibračné nastavenia použité pri meraní.

### Spustenie relácie

1. Stlačte **SPUSTIT RELACIU**.
2. Zadajte názov alebo číslo stanoviska.
3. Potvrďte zadanie.
4. Zariadenie vytvorí nový CSV súbor.
5. Automatický záznam začne podľa nastaveného intervalu.

Stanovisko je počas relácie uzamknuté.

### Ukončenie relácie

1. Stlačte **UKONCIT RELACIU**.
2. Potvrďte ukončenie.
3. Do CSV sa uloží systémový záznam `SESSION_END`.
4. Súbor zostane dostupný v archíve a cez Wi‑Fi export.

> [!WARNING]
> Pred vypnutím zariadenia odporúčame reláciu vždy korektne ukončiť. Pri neočakávanom vypnutí je však možné reláciu obnoviť.

---

## Automatické a manuálne záznamy

### Automatický záznam

Automatické záznamy sa ukladajú počas aktívnej relácie v nastavenom intervale.

Dostupné intervaly:

- 1 sekunda,
- 5 sekúnd,
- 10 sekúnd,
- 30 sekúnd,
- 60 sekúnd.

Automatický záznam má typ:

```text
AUTO
```

Meraný bod zostáva pri automatickom zázname prázdny.

### Manuálny záznam

Manuálny záznam sa používa na priradenie atmosférických podmienok ku konkrétnemu cieľovému bodu.

Postup:

1. Spustite reláciu.
2. Zadajte meraný bod.
3. Stlačte **ULOZIT MERANIE**.
4. Po úspešnom zápise sa zvýši počítadlo `MANUAL`.
5. Pole meraného bodu sa vymaže a je pripravené na ďalšie zadanie.

Manuálny záznam má typ:

```text
MANUAL
```

Maximálna dĺžka identifikátora stanoviska alebo bodu je 16 znakov.

Podporované sú:

- písmená `A–Z`,
- číslice `0–9`,
- `_`
- `.`
- `,`
- `-`

---

## Grafy

Grafické menu je dostupné počas aktívnej relácie aj z archívu ukončených relácií.

Zobrazované veličiny:

- teplota,
- relatívna vlhkosť,
- atmosférický tlak.

### Rozsah grafu

#### Posledných 120 záznamov

Voľba `120` zobrazí posledných 120 automatických záznamov.

Približný časový rozsah:

| Interval zápisu | Rozsah 120 záznamov |
|---:|---:|
| 1 s | 2 minúty |
| 5 s | 10 minút |
| 10 s | 20 minút |
| 30 s | 1 hodina |
| 60 s | 2 hodiny |

#### Všetky záznamy

Voľba `VSETKY` spracuje celú reláciu. Na displeji sa zobrazí najviac 240 grafických bodov. Pri väčšom počte záznamov sa údaje agregujú, ale minimum, maximum a priemer sa počítajú zo všetkých dostupných záznamov.

### Živé grafy

Počas aktívnej relácie sa graf posledných 120 záznamov aktualizuje po každom úspešnom automatickom zápise.

> [!NOTE]
> **PLACEHOLDER – OBRAZOVKA GRAFOV**  
> Doplňte snímku grafu teploty, vlhkosti a tlaku.  
> Odporúčaný súbor: `docs/images/graphs-screen.png`

---

## Archív relácií

Archív zobrazuje korektne ukončené relácie uložené na SD karte.

Relácie sú:

- zoradené od najnovšej,
- stránkované,
- označené stanoviskom, dátumom, časom a veľkosťou súboru.

Detail relácie obsahuje:

- stanovisko,
- začiatok a koniec,
- trvanie,
- počet automatických záznamov,
- počet manuálnych záznamov,
- minimum, maximum a priemer meraných veličín,
- prístup ku grafom relácie.

Do archívu sa zaraďujú relácie ukončené záznamom:

- `SESSION_END`
- `SESSION_END_RECOVERY`
- `SESSION_ABANDONED`

Aktívna alebo nedokončená relácia sa v archíve nezobrazuje.

> [!NOTE]
> **PLACEHOLDER – ARCHÍV RELÁCIÍ**  
> Doplňte snímku zoznamu relácií a detailu jednej relácie.  
> Odporúčané súbory:  
> `docs/images/archive-list.png`  
> `docs/images/archive-detail.png`

---

## Kalibrácia

Kalibrácia slúži na korekciu rozdielu medzi GeoMeteo Loggerom a referenčným meradlom.

Pre každú veličinu sa používa lineárna korekcia:

```text
korigovaná hodnota = a × meraná hodnota + b
```

Kde:

- `a` je mierkový koeficient,
- `b` je offset.

Kalibrovať možno samostatne:

- teplotu,
- relatívnu vlhkosť,
- atmosférický tlak.

### Neutrálne nastavenie

```text
a = 1.000000
b = 0.000
```

Pri neutrálnom nastavení sa meraná hodnota nemení.

### Povolené rozsahy

| Parameter | Minimum | Maximum |
|---|---:|---:|
| mierkový koeficient `a` | 0,5 | 1,5 |
| teplotný offset `b` | −20 °C | +20 °C |
| vlhkostný offset `b` | −50 % | +50 % |
| tlakový offset `b` | −150 hPa | +150 hPa |

### Uloženie kalibrácie

Kalibračné koeficienty sa ukladajú do internej pamäte ESP32 a zostávajú zachované po reštarte alebo vypnutí zariadenia.

Kalibráciu nemožno meniť počas aktívnej relácie.

> [!IMPORTANT]
> Zmena kalibračných koeficientov je používateľská konfigurácia. Nemení číslo verzie firmvéru.

### Odporúčané získanie koeficientov

1. Umiestnite GeoMeteo Logger a referenčné meradlo vedľa seba.
2. Nechajte oba prístroje ustáliť.
3. Vykonajte viacero súčasných odčítaní.
4. Použite údaje na výpočet lineárnej korekcie.
5. Koeficienty zadajte v kalibračnom menu.
6. Vykonajte kontrolné meranie.

> [!NOTE]
> **PLACEHOLDER – KALIBRAČNÉ MENU**  
> Doplňte snímku obrazovky kalibrácie.  
> Odporúčaný súbor: `docs/images/calibration-screen.png`

---

## Nastavenia

V nastaveniach možno upraviť:

- dátum a čas RTC,
- interval automatického záznamu,
- jas displeja,
- kalibráciu,
- úsporu energie,
- farebnú schému používateľského rozhrania.

### Dátum a čas

Dátum a čas sa nastavujú v samostatnom editore. Zmena ostatných nastavení nemení RTC.

### Jas displeja

Jas možno nastaviť v rozsahu približne 10 až 100 %.

### Farebná schéma

Dostupné akcentové farby:

- oranžová,
- modrá,
- zelená,
- červená,
- fialová.

Výber sa uloží do internej pamäte a zostane zachovaný po reštarte.

Farebná schéma ovplyvňuje najmä:

- panely meraných hodnôt,
- aktívne tlačidlá,
- ikony,
- rámčeky,
- posuvník jasu,
- grafické akcenty,
- webovú stránku Wi‑Fi exportu.

Stavové farby upozornení zostávajú nezávislé od zvolenej schémy.

> [!NOTE]
> **PLACEHOLDER – NASTAVENIA A FARBY**  
> Doplňte snímku hlavného nastavenia a výberu farebnej schémy.  
> Odporúčané súbory:  
> `docs/images/settings-screen.png`  
> `docs/images/accent-theme-screen.png`

---

## Úspora energie

Úsporný režim ovplyvňuje iba podsvietenie displeja. Meranie a zápis na SD kartu pokračujú bez prerušenia.

Dostupný čas nečinnosti pred stlmením:

- 30 sekúnd,
- 1 minúta,
- 2 minúty,
- 5 minút.

Po uplynutí nastaveného času:

1. displej sa stlmí približne na 5 %,
2. po ďalších 30 sekundách sa podsvietenie vypne,
3. prvý dotyk displej iba prebudí,
4. až ďalší dotyk vykoná požadovanú akciu.

Predvolené nastavenie:

```text
úspora energie: zapnutá
stlmenie: po 1 minúte
vypnutie: o ďalších 30 sekúnd
```

Počas Wi‑Fi exportu zostáva displej aktívny.

---

## Wi‑Fi export

GeoMeteo Logger umožňuje stiahnuť ukončené CSV relácie bez vyberania SD karty.

Zariadenie vytvorí vlastnú Wi‑Fi sieť. Router ani internet nie sú potrebné.

### Spustenie exportu

1. Ukončite aktívnu meraciu reláciu.
2. Otvorte Wi‑Fi menu.
3. Stlačte **SPUSTIT WI-FI**.
4. V počítači alebo telefóne vyhľadajte sieť:

```text
GeoMeteo-XXXX
```

5. Pripojte sa pomocou hesla:

```text
geometeo
```

6. V prehliadači otvorte:

```text
http://192.168.4.1
```

7. Vyberte reláciu a stlačte **STIAHNUT CSV**.

### Dôležité vlastnosti

- exportujú sa iba ukončené relácie,
- aktívnu reláciu nemožno exportovať,
- Wi‑Fi sa po piatich minútach bez aktivity automaticky vypne,
- odchod z Wi‑Fi menu export ukončí,
- CSV sa prenáša priamo zo SD karty,
- pripojenie nemá prístup na internet.

> [!NOTE]
> Telefón alebo počítač môže oznámiť, že Wi‑Fi sieť nemá internet. Zostaňte k nej pripojení a ručne otvorte adresu `192.168.4.1`.

> [!NOTE]
> **PLACEHOLDER – WI‑FI EXPORT**  
> Doplňte fotografiu Wi‑Fi menu a snímku webovej stránky.  
> Odporúčané súbory:  
> `docs/images/wifi-export-device.png`  
> `docs/images/wifi-export-browser.png`

---

## CSV súbory

Každá relácia sa ukladá do samostatného súboru v priečinku:

```text
/sessions
```

### Stĺpce CSV

| Stĺpec | Význam |
|---|---|
| `session_id` | identifikátor relácie |
| `timestamp` | dátum a čas záznamu |
| `record_type` | typ záznamu |
| `station` | stanovisko |
| `target_point` | meraný bod |
| `temperature_c` | korigovaná teplota |
| `humidity_pct` | korigovaná relatívna vlhkosť |
| `pressure_hpa` | korigovaný atmosférický tlak |
| `battery_voltage_v` | napätie batérie |
| `battery_percent` | odhad stavu batérie |
| `temperature_raw_c` | pôvodná hodnota teploty |
| `humidity_raw_pct` | pôvodná hodnota vlhkosti |
| `pressure_raw_hpa` | pôvodná hodnota tlaku |
| `calibration_enabled` | stav kalibrácie |
| `temperature_scale` | koeficient `a` pre teplotu |
| `temperature_offset_c` | koeficient `b` pre teplotu |
| `humidity_scale` | koeficient `a` pre vlhkosť |
| `humidity_offset_pct` | koeficient `b` pre vlhkosť |
| `pressure_scale` | koeficient `a` pre tlak |
| `pressure_offset_hpa` | koeficient `b` pre tlak |

### Typy záznamov

| Typ | Význam |
|---|---|
| `SESSION_START` | začiatok relácie |
| `AUTO` | automatický merací záznam |
| `MANUAL` | manuálny záznam ku konkrétnemu bodu |
| `SESSION_RESUME` | pokračovanie obnovenej relácie |
| `SESSION_END` | korektné ukončenie relácie |
| `SESSION_END_RECOVERY` | ukončenie obnovenej relácie |
| `SESSION_ABANDONED` | opustenie nedokončenej relácie |

### Ukážka

```csv
session_id,timestamp,record_type,station,target_point,temperature_c,humidity_pct,pressure_hpa,battery_voltage_v,battery_percent,temperature_raw_c,humidity_raw_pct,pressure_raw_hpa,calibration_enabled,temperature_scale,temperature_offset_c,humidity_scale,humidity_offset_pct,pressure_scale,pressure_offset_hpa
20260727_083200,2026-07-27 08:32:00,SESSION_START,ST_01,,,,4.08,87,,,,0,1.000000,0.000,1.000000,0.000,1.000000,0.000
20260727_083200,2026-07-27 08:32:05,AUTO,ST_01,,24.53,46.10,999.35,4.08,87,24.53,46.10,999.35,0,1.000000,0.000,1.000000,0.000,1.000000,0.000
20260727_083200,2026-07-27 08:33:12,MANUAL,ST_01,BOD_101,24.55,46.20,999.34,4.07,86,24.55,46.20,999.34,0,1.000000,0.000,1.000000,0.000,1.000000,0.000
```

> [!NOTE]
> Uvedené hodnoty sú iba ilustračné.

---

## Odporúčaný postup merania

### Pred meraním

1. Skontrolujte nabitie batérie.
2. Skontrolujte vloženie SD karty.
3. Overte dátum a čas.
4. Skontrolujte stav senzorov.
5. Nechajte zariadenie ustáliť v prostredí merania.
6. Až potom spustite reláciu.

### Odporúčaná doba ustálenia

Firmvér neobsahuje povinnú čakaciu dobu. Používateľ môže reláciu spustiť okamžite.

Pre spoľahlivejšie výsledky sa odporúča:

| Situácia | Odporúčaná doba |
|---|---:|
| bežné spustenie | približne 60 sekúnd |
| výrazná zmena prostredia | 5 až 10 minút |
| kalibračné porovnanie | 10 až 15 minút |

### Umiestnenie senzora

Senzor umiestnite:

- mimo priameho slnečného žiarenia,
- mimo tepla ruky používateľa,
- mimo výduchu teplého vzduchu,
- s dostatočným prúdením vzduchu,
- čo najďalej od elektroniky a displeja,
- v ochrannom, ale vetranom kryte.

> [!NOTE]
> **PLACEHOLDER – UMIESTNENIE SENZORA**  
> Doplňte fotografiu odporúčaného umiestnenia externého senzora v kryte.  
> Odporúčaný súbor: `docs/images/sensor-placement.jpg`

---

## Stavové a chybové hlásenia

Zariadenie kontroluje:

- komunikáciu so SHT40,
- komunikáciu s BMP280,
- platnosť RTC,
- dostupnosť SD karty,
- úspešnosť zápisu,
- platnosť meraných rozsahov,
- stav batérie.

Príklady hlásení:

```text
CHYBA CITANIA SHT40
CHYBA CITANIA BMP280
NEPLATNY DATUM ALEBO CAS RTC
TEPLOTA MIMO PLATNEHO ROZSAHU
VLHKOST MIMO PLATNEHO ROZSAHU
TLAK MIMO PLATNEHO ROZSAHU
CHYBA ZAPISU NA SD
BATERIA KRITICKA
```

Neplatné meranie sa neuloží ako `AUTO` ani `MANUAL`.

### Platné rozsahy meraní

| Veličina | Minimum | Maximum |
|---|---:|---:|
| teplota | −40 °C | 85 °C |
| relatívna vlhkosť | 0 % | 100 % |
| tlak | 300 hPa | 1100 hPa |
| rok RTC | 2020 | 2099 |

### Batéria

| Stav | Indikácia |
|---|---|
| viac ako 25 % | zelená |
| 11 až 25 % | oranžová |
| 0 až 10 % | červená |

Pri kritickej úrovni zariadenie odporučí ukončiť reláciu, ale nevypne ju automaticky.

---

## Obnova relácie po reštarte

Po neočakávanom vypnutí alebo reštarte zariadenie rozpozná nedokončenú reláciu.

Používateľ môže zvoliť:

### Pokračovať

- obnoví stanovisko,
- obnoví CSV súbor,
- obnoví počet manuálnych záznamov,
- zapíše `SESSION_RESUME`.

### Ukončiť

- uzavrie pôvodnú reláciu,
- zapíše `SESSION_END_RECOVERY`.

### Nová relácia

- označí pôvodnú reláciu ako `SESSION_ABANDONED`,
- vymaže uložený aktívny stav,
- umožní založiť novú reláciu.

> [!NOTE]
> **PLACEHOLDER – OBNOVA RELÁCIE**  
> Doplňte snímku obrazovky obnovy po reštarte.  
> Odporúčaný súbor: `docs/images/session-recovery.png`

---

## Údržba

### SD karta

- používajte kartu naformátovanú na FAT32,
- kartu nevyberajte počas zápisu,
- pred vybratím ukončite reláciu a vypnite zariadenie,
- pravidelne zálohujte CSV súbory,
- pri opakovaných chybách zápisu kartu preformátujte alebo vymeňte.

### Senzory

- chráňte ich pred kondenzáciou a priamym dažďom,
- nevystavujte ich zbytočne prachu,
- neumiestňujte ich do uzavretého nevetraného priestoru,
- pred kalibráciou zabezpečte rovnaké podmienky pre oba prístroje.

### Batéria

- zariadenie nabíjajte vhodným USB zdrojom,
- nenechávajte batériu dlhodobo úplne vybitú,
- pri dlhšom skladovaní zariadenie pravidelne skontrolujte.

---

## Riešenie problémov

### Zariadenie nevidí SD kartu

1. Vypnite zariadenie.
2. Skontrolujte vloženie karty.
3. Overte formát FAT32.
4. Skúste inú SD kartu.
5. Skontrolujte stav `SD` po opätovnom zapnutí.

### Meranie sa neukladá

Skontrolujte:

- či je aktívna relácia,
- či je zadané stanovisko,
- pri manuálnom zázname aj meraný bod,
- stav SD karty,
- platnosť hodnôt senzorov.

Počítadlo manuálnych záznamov sa zvýši iba po potvrdenom zápise.

### Zobrazované hodnoty sa líšia od referenčného meradla

1. Nechajte oba prístroje ustáliť.
2. Skontrolujte ich umiestnenie.
3. Pri tlaku overte, že oba prístroje zobrazujú rovnaký typ tlaku.
4. Vykonajte viacero porovnávacích meraní.
5. Vypočítajte a nastavte kalibračné koeficienty.

### Wi‑Fi stránka sa neotvorí

1. Overte, že je Wi‑Fi export spustený.
2. Skontrolujte pripojenie k sieti `GeoMeteo-XXXX`.
3. Zostaňte pripojení aj bez internetu.
4. Otvorte priamo `http://192.168.4.1`.
5. Skontrolujte, či sa Wi‑Fi po časovom limite nevypnula.

### CSV súbor sa nedá stiahnuť

- relácia musí byť ukončená,
- SD karta musí byť dostupná,
- Wi‑Fi musí zostať aktívna počas celého prenosu,
- pri veľkom súbore počkajte na dokončenie prenosu.

### Displej nereaguje na prvý dotyk

Pri stlmenom alebo vypnutom displeji je to očakávané správanie. Prvý dotyk iba prebudí podsvietenie. Druhý dotyk vykoná požadovanú akciu.

---

## Štruktúra repozitára

Odporúčaná štruktúra:

```text
GeoMeteo-Logger/
├── README.md
├── LICENSE
├── firmware/
│   ├── geometeo_logger.ino
│   ├── FT6236.h
│   └── FT6236.cpp
├── docs/
│   ├── user-guide.md
│   ├── hardware.md
│   ├── csv-format.md
│   └── images/
│       ├── geometeo-logger-overview.jpg
│       ├── hardware-block-diagram.png
│       ├── main-screen-annotated.png
│       ├── graphs-screen.png
│       ├── archive-list.png
│       ├── archive-detail.png
│       ├── calibration-screen.png
│       ├── settings-screen.png
│       ├── accent-theme-screen.png
│       ├── wifi-export-device.png
│       ├── wifi-export-browser.png
│       ├── sensor-placement.jpg
│       └── session-recovery.png
└── examples/
    └── sample-session.csv
```

---

## Vývojové prostredie

Projekt bol vyvíjaný a testovaný s:

- Arduino IDE 2.3.10,
- balíkom ESP32 3.3.11,
- doskou LaskaKit ESPD‑35 v3.2.

Použité knižnice:

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include "FT6236.h"
#include <Adafruit_SHT4x.h>
#include <Adafruit_BMP280.h>
#include <RTClib.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
```

Pre TFT_eSPI sa používa konfigurácia displeja určená pre ESPD‑35 v3:

```text
Setup303_ILI9488_ESPD-3_5_v3.h
```

---

## Verzionovanie

Projekt používa verzie vo formáte:

```text
MAJOR.MINOR.PATCH
```

Príklad:

```text
v0.8.0
```

- `MAJOR` – zásadná zmena alebo stabilné verejné vydanie,
- `MINOR` – nová funkcia,
- `PATCH` – oprava alebo menšia úprava.

Zmena používateľských nastavení, napríklad kalibračných koeficientov, jasu, intervalu alebo farebnej schémy, nemení verziu firmvéru.

---

## Licencia

> [!NOTE]
> **PLACEHOLDER – LICENCIA**  
> Doplňte vybranú licenciu projektu do súboru `LICENSE`.

Odporúčaná licencia pre otvorený firmvér:

```text
GNU General Public License v3.0
```

alebo:

```text
MIT License
```

Výber závisí od požadovaných podmienok ďalšieho používania a úprav.

---

## Autor a kontakt

```text
Autor: Jakub Špekra
Pracovisko: Karedra globálnej geodézie a geoinformatiky, Stavebná Fakulta STU v Bratislava
E-mail: jakub.sperka@stuba.sk
Repozitár: geometeo_logger
```

---

## Stav projektu

GeoMeteo Logger poskytuje kompletný pracovný tok pre terénny záznam atmosférických parametrov:

```text
spustenie
→ kontrola zariadení
→ založenie relácie
→ automatické a manuálne meranie
→ grafická kontrola
→ ukončenie relácie
→ archív
→ export CSV cez Wi‑Fi
→ spracovanie údajov v počítači
```

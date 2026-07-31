# Changelog – GeoMeteo Logger

Všetky významné zmeny firmvéru GeoMeteo Logger sú uvedené v tomto dokumente.

Formát verzie:

```text
MAJOR.MINOR.PATCH
```

Zmena používateľských nastavení, najmä kalibračných koeficientov, jasu, intervalu záznamu alebo farebnej schémy, nemení číslo verzie firmvéru.

---

## Stav vydaní

| Verzia | Stav |
|---|---|
| v0.9.0 | kandidát na overenie – ochrana úložiska |
| v0.8.2 | vývojová verzia – rozšírené farebné schémy |
| v0.7.6 | označená ako stabilný referenčný release |
| v0.7.5 | experimentálna, následne zrušená |
| v0.2.0–v0.7.4 | historické vývojové míľniky |

---

## v0.9.0 – ochrana úložiska

### Pridané

- kontrola celkovej, využitej a voľnej kapacity SD karty,
- pravidelná obnova informácie o voľnom priestore,
- upozornenie pri menej ako 32 MiB voľného miesta,
- zastavenie bežných zápisov pri zostávajúcej rezerve približne 4 MiB,
- núdzová rezerva 64 KiB na korektné ukončenie relácie,
- preventívne delenie CSV pred limitom FAT32 pri približne 3800 MiB,
- pokračovacie súbory `_part02`, `_part03` a ďalšie,
- systémové udalosti `FILE_CONTINUE` a `FILE_RESUME`,
- uloženie čísla aktuálnej časti do stavu obnovy,
- podpora najviac 32 častí jednej relácie,
- testovací režim rotácie pri približne 64 KiB,
- spracovanie viacdielnych relácií v grafoch,
- spracovanie viacdielnych relácií v archíve,
- zobrazenie počtu častí a celkovej veľkosti relácie,
- samostatné sťahovanie častí cez Wi‑Fi export.

### Poznámka

Produkčné nastavenie musí používať:

```cpp
constexpr bool STORAGE_ROTATION_TEST_MODE = false;
```

---

## v0.8.2 – konzistentné stavové farby

### Zmenené

- stavové indikátory `SENZOR`, `SD`, `RELACIA` a `ZAZNAM` prestali podliehať farebnej téme,
- zavedená pevná zelená farba pre stav OK,
- zavedená pevná oranžová farba pre upozornenie alebo neaktívny stav,
- zavedená pevná červená farba pre chybu,
- ostatné prvky používateľského rozhrania naďalej používajú zvolenú tému.

---

## v0.8.1 – rozšírené farebné schémy

### Pridané

K pôvodným témam pribudli:

- sivá,
- žltá,
- limetková zelená,
- krémová,
- ružová.

### Zmenené

- výber tém bol upravený na rozloženie dva stĺpce × päť riadkov,
- menší font názvov tém zlepšil čitateľnosť v kompaktnejšom menu.

---

## v0.8.0 – voľba akcentovej farby

### Pridané

- samostatné menu farebnej schémy,
- oranžová téma,
- modrá téma,
- zelená téma,
- červená téma,
- fialová téma,
- okamžitý náhľad vybranej farby,
- uloženie témy do `Preferences`,
- aplikovanie témy na Wi‑Fi webovú stránku.

### Zmenené

Farebná téma začala ovplyvňovať:

- panely meraných hodnôt,
- tlačidlá,
- ikony,
- aktívne položky,
- rámčeky,
- posuvník jasu,
- grafické akcenty.

---

## v0.7.6 – stabilný referenčný release

### Stav

Táto verzia bola používateľom označená ako stable release.

### Zmenené

- návrat k čistému GUI založenému na v0.7.4,
- odstránenie experimentálne dokresľovanej diakritiky,
- odstránenie bieleho obrysu meraných hodnôt,
- zachovaný iba jemný bold efekt meraných hodnôt,
- verzia označená ako v0.7.6.

### Zachované

- relácie,
- automatické a manuálne záznamy,
- grafy,
- archív,
- kalibrácia,
- úspora energie,
- Wi‑Fi export,
- stabilizované GUI.

---

## v0.7.5 – experiment čitateľnosti a diakritiky

### Pridané

- biely obrys okolo meraných hodnôt,
- graficky dokresľované dĺžne a mäkčene na hlavných tlačidlách.

### Problémy

- prvá verzia neprešla kompiláciou pre umiestnenie typov používaných automatickými Arduino prototypmi,
- po oprave pôsobil biely obrys aj dokresľovaná diakritika príliš rušivo.

### Stav

Verzia bola zrušená a nebola použitá ako základ stabilného releasu.

---

## v0.7.4 – jedno tlačidlo relácie

### Zmenené

- samostatné tlačidlá založenia a ukončenia relácie boli spojené,
- bez aktívnej relácie tlačidlo reláciu spustí,
- počas aktívnej relácie rovnaké tlačidlo otvorí potvrdenie ukončenia,
- tlačidlo využíva celú šírku hlavnej obrazovky.

---

## v0.7.3 – prvá väčšia úprava GUI

### Zmenené

- počet manuálnych záznamov sa zobrazuje ako `MANUAL:`,
- stavový riadok používa menší alebo viacriadkový text podľa dĺžky správy,
- zväčšené skratky a dotykové plochy podmenu,
- v kalibrácii bol názov `SUROVA` zmenený na `MERANA`,
- kalibrácia používa jedno tlačidlo zapnutia alebo vypnutia,
- tlačidlo úspory bolo premenované na `USPORA ENERGIE`,
- kalibračný editor bol vizuálne zjednotený s ostatnými vstupmi,
- stavové indikátory používajú celé názvy,
- panely meraných veličín dostali farebné pozadie,
- vykresľovanie viacerých obrazoviek bolo zmenené na čiastočné.

### Opravené

- znížené blikanie hlavnej obrazovky,
- znížené blikanie kalibračného menu,
- znížené blikanie Wi‑Fi menu,
- menej časté prekresľovanie súhrnu grafov.

---

## v0.7.2 – opravy Wi‑Fi exportu

### Pridané

- dodaná bitmapová Wi‑Fi ikona 32 × 32 px.

### Opravené

- Wi‑Fi sa už nevypne pri kliknutí na stiahnutie CSV,
- čas nečinnosti sa obnoví po dokončení prenosu,
- odstránené podtečenie časovača spôsobené starou hodnotou `millis()`,
- Wi‑Fi ikona sa už nevykresľuje cez obrazovku exportu,
- ikona sa zobrazuje iba v spodnej lište hlavnej obrazovky.

---

## v0.7.1 – oprava kompilácie Wi‑Fi verzie

### Opravené

- pridaná explicitná dopredná deklarácia funkcie `formatArchiveDateTime()`,
- odstránená chyba:

```text
'formatArchiveDateTime' was not declared in this scope
```

---

## v0.7.0 – Wi‑Fi export CSV

### Pridané

- režim Wi‑Fi SoftAP,
- vlastná sieť `GeoMeteo-XXXX`,
- heslo `geometeo`,
- lokálna adresa `192.168.4.1`,
- webový server na porte 80,
- zoznam ukončených relácií v prehliadači,
- stiahnutie CSV zo SD karty,
- automatické vypnutie Wi‑Fi po piatich minútach nečinnosti,
- najviac štyria klienti,
- blokovanie exportu počas aktívnej relácie,
- dočasné vypnutie úspory displeja počas exportu.

---

## v0.6.0 – úspora energie

### Pridané

- samostatné menu úspory energie,
- voľba stlmenia po 30 sekundách, 1 minúte, 2 minútach alebo 5 minútach,
- stlmenie displeja na približne 5 %,
- úplné vypnutie podsvietenia po ďalších 30 sekundách,
- prvý dotyk iba prebúdza displej,
- uloženie nastavenia do `Preferences`.

### Zachované počas vypnutia displeja

- meranie senzorov,
- RTC,
- automatický zápis,
- kontrola batérie,
- kontrola SD,
- relácia a štatistiky.

---

## v0.5.0 – kalibrácia

### Pridané

- lineárna korekcia `y = a × x + b`,
- samostatná kalibrácia teploty,
- samostatná kalibrácia vlhkosti,
- samostatná kalibrácia tlaku,
- editor koeficientov,
- živé zobrazenie meranej a korigovanej hodnoty,
- uloženie koeficientov do `Preferences`,
- zapnutie a vypnutie kalibrácie,
- blokovanie zmeny počas aktívnej relácie,
- validácia povolených rozsahov koeficientov.

### Zmenené

CSV bolo rozšírené o:

- surovú teplotu,
- surovú vlhkosť,
- surový tlak,
- stav kalibrácie,
- všetky mierkové a offsetové koeficienty.

Grafy a štatistiky používajú korigované hodnoty.

---

## v0.4.0 – archív relácií

### Pridané

- skratka archívu na hlavnej obrazovke,
- zoznam ukončených relácií,
- zoradenie od najnovšej,
- stránkovanie po piatich položkách,
- manuálne obnovenie zoznamu,
- kapacita najviac 96 relácií,
- detail relácie,
- počet automatických a manuálnych záznamov,
- trvanie relácie,
- minimum, priemer a maximum veličín,
- grafy archivovaných relácií.

### Filtrovanie

Do archívu sa zaraďujú relácie ukončené udalosťou:

- `SESSION_END`
- `SESSION_END_RECOVERY`
- `SESSION_ABANDONED`

---

## v0.3.1 – živé grafy

### Pridané

- priebežná aktualizácia grafu počas aktívnej relácie,
- režim `120` sa aktualizuje po každom úspešnom `AUTO` zázname,
- režim `VSETKY` sa pri menších reláciách aktualizuje priamo,
- veľká relácia sa znovu načíta najviac raz za 10 sekúnd,
- súhrn a trvanie sa obnovujú nezávisle.

---

## v0.3.0 – grafy a súhrn relácie

### Pridané

- bitmapová ikona grafov,
- skratka do grafického menu,
- graf teploty,
- graf vlhkosti,
- graf tlaku,
- rozsah posledných 120 automatických záznamov,
- rozsah všetkých záznamov,
- agregácia celej relácie do najviac 240 bodov,
- minimum, priemer a maximum,
- časový rozsah grafu,
- súhrn stanoviska a počtu záznamov.

---

## v0.2.0 – stabilizovaný prototyp

### Pridané

- centrálne označenie názvu a verzie firmvéru,
- obnova nedokončenej relácie po reštarte,
- uloženie aktívneho stavu do `Preferences`,
- možnosti pokračovať, ukončiť alebo opustiť reláciu,
- opakovaný pokus o inicializáciu a zápis na SD kartu,
- maximálne dva pokusy o zápis,
- validácia teploty, vlhkosti, tlaku a RTC,
- upozornenie na nízky a kritický stav batérie,
- bezpečné zvýšenie manuálneho počítadla až po úspešnom zápise.

### Základná funkcionalita zahrnutá v tomto míľniku

- externý SHT40 a BMP280,
- RTC DS3231,
- SD karta,
- dotykové GUI,
- automatický záznam,
- manuálny záznam,
- stanovisko a meraný bod,
- samostatný CSV súbor pre reláciu,
- `SESSION_START` a `SESSION_END`.

---

## Pred v0.2.0 – nečíslovaný prototyp

Prvé funkčné prototypy overili:

- oddelenie internej a externej I²C zbernice,
- komunikáciu so SHT40,
- komunikáciu s BMP280,
- komunikáciu s DS3231,
- inicializáciu SD karty,
- čítanie napätia batérie,
- portrétnu orientáciu displeja,
- správnu transformáciu dotykových súradníc,
- základnú hlavnú obrazovku,
- prvé CSV záznamy,
- zadávanie stanoviska a meraného bodu.

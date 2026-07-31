# GeoMeteo Logger – prehľad funkcionalít

Tento dokument sumarizuje funkcionalitu zariadenia **GeoMeteo Logger** implementovanú vo firmvéri do verzie **v0.9.0**.

> [!NOTE]
> Verzia **v0.7.6** bola označená ako stabilný referenčný build.  
> Verzia **v0.9.0** obsahuje všetky neskoršie rozšírenia vrátane farebných schém a ochrany úložiska a je určená na overenie pred označením za nový stable release.

---

## 1. Merané veličiny

Zariadenie meria a zobrazuje:

- teplotu vzduchu zo senzora SHT40,
- relatívnu vlhkosť zo senzora SHT40,
- absolútny atmosférický tlak zo senzora BMP280,
- napätie Li‑Po batérie,
- odhad percentuálneho stavu batérie,
- dátum a čas z RTC DS3231.

Interné meteorologické senzory hlavnej dosky sa nepoužívajú. Firmvér pracuje s externým meteorologickým modulom na samostatnej I²C zbernici.

### Frekvencia čítania senzorov

Atmosférické hodnoty sa interne obnovujú približne každú sekundu bez ohľadu na nastavený interval zápisu do CSV.

### Kontrola platnosti meraní

Pred uložením sa kontrolujú rozsahy:

| Veličina | Minimum | Maximum |
|---|---:|---:|
| teplota | −40 °C | 85 °C |
| relatívna vlhkosť | 0 % | 100 % |
| tlak | 300 hPa | 1100 hPa |
| rok RTC | 2020 | 2099 |

Pri neplatnej hodnote sa automatický ani manuálny merací záznam neuloží. Systémové udalosti relácie sa môžu uložiť aj bez platných atmosferických hodnôt.

---

## 2. Hlavná obrazovka

Hlavná obrazovka v portrétnej orientácii zobrazuje:

- dátum a čas,
- percento a napätie batérie,
- teplotu,
- relatívnu vlhkosť,
- tlak,
- stanovisko,
- aktuálny meraný bod,
- počet manuálnych záznamov vo formáte `MANUAL: n`,
- stavový riadok s aktuálnou informáciou alebo chybou,
- stavové indikátory zariadenia,
- tlačidlo na spustenie alebo ukončenie relácie,
- tlačidlo manuálneho uloženia,
- skratky do archívu, grafov, Wi‑Fi exportu a nastavení.

Merané hodnoty používajú jemne zosilnený čierny font na farebnom pozadí.

### Stavové indikátory

Indikátory používajú celé názvy:

- `SENZOR`
- `SD`
- `RELACIA`
- `ZAZNAM`

Ich farby nepodliehajú používateľskej farebnej téme:

| Farba | Význam |
|---|---|
| zelená | pripravené alebo úspešné |
| oranžová | upozornenie, čakanie alebo neaktívny stav |
| červená | chyba |

---

## 3. Meracie relácie

Každé meranie sa organizuje do samostatnej relácie.

### Spustenie relácie

Jedno kontextové tlačidlo slúži na spustenie aj ukončenie relácie:

```text
SPUSTIT RELACIU
UKONCIT RELACIU
```

Pri spustení používateľ zadá stanovisko. Stanovisko zostáva počas celej relácie nemenné.

### Identifikátory

Stanovisko a meraný bod môžu mať najviac 16 znakov.

Podporované znaky:

- `A–Z`
- `0–9`
- `_`
- `.`
- `,`
- `-`

CSV zápis správne uzatvára textové polia do úvodzoviek, ak obsahujú čiarku alebo úvodzovku.

### Ukončenie relácie

Ukončenie sa vykonáva cez potvrdzovaciu obrazovku. Pri úspešnom ukončení sa zapíše systémová udalosť `SESSION_END`.

---

## 4. Automatický záznam

Počas aktívnej relácie sa údaje automaticky zapisujú do CSV.

Dostupné intervaly:

- 1 sekunda,
- 5 sekúnd,
- 10 sekúnd,
- 30 sekúnd,
- 60 sekúnd.

Predvolený interval je 5 sekúnd.

Automatický záznam má typ:

```text
AUTO
```

Pole meraného bodu zostáva pri automatickom zázname prázdne.

---

## 5. Manuálny záznam

Manuálny záznam umožňuje priradiť aktuálne atmosferické podmienky ku konkrétnemu meranému bodu.

Pracovný postup:

1. spustenie relácie,
2. zadanie meraného bodu,
3. uloženie merania,
4. potvrdenie úspešného zápisu,
5. zvýšenie počítadla `MANUAL`,
6. automatické vymazanie poľa meraného bodu.

Manuálny záznam má typ:

```text
MANUAL
```

Počítadlo sa zvýši iba po potvrdenom zápise na SD kartu.

---

## 6. Softvérová klávesnica

Zariadenie obsahuje dotykovú alfanumerickú klávesnicu.

Podporuje:

- číselný režim,
- abecedný režim,
- prepínanie režimov,
- mazanie posledného znaku,
- vymazanie celého vstupu,
- potvrdenie a zrušenie zadávania,
- samostatný numerický editor kalibračných koeficientov.

---

## 7. Ukladanie na SD kartu

Každá relácia má vlastný CSV súbor v priečinku:

```text
/sessions
```

### Bezpečný zápis

Pri každom zázname firmvér:

1. otvorí súbor v režime append,
2. zapíše celý CSV riadok,
3. vykoná `flush()`,
4. skontroluje chybu zápisu,
5. zatvorí súbor.

Pri chybe zápisu:

- označí SD kartu ako chybnú,
- ukončí aktuálne spojenie,
- znovu inicializuje SPI a SD kartu,
- zopakuje zápis,
- vykoná najviac dva pokusy.

Opakovaná inicializácia SD karty sa vykonáva najskôr po približne piatich sekundách.

### CSV obsah

CSV uchováva:

- identifikátor relácie,
- čas záznamu,
- typ záznamu,
- stanovisko,
- meraný bod,
- korigované hodnoty,
- napätie a percento batérie,
- surové hodnoty senzorov,
- stav kalibrácie,
- všetky kalibračné koeficienty.

### Systémové typy záznamov

Firmvér používa:

- `SESSION_START`
- `AUTO`
- `MANUAL`
- `SESSION_RESUME`
- `SESSION_END`
- `SESSION_END_RECOVERY`
- `SESSION_ABANDONED`
- `FILE_CONTINUE`
- `FILE_RESUME`

---

## 8. Ochrana FAT32 a kontrola voľného miesta

Od verzie v0.9.0 firmvér aktívne kontroluje veľkosť súborov aj kapacitu SD karty.

### Automatické delenie súborov

FAT32 povoľuje maximálnu veľkosť jedného súboru 4 GiB − 1 bajt. Firmvér reláciu preventívne rozdelí pri približne:

```text
3800 MiB
```

Pokračovacie súbory používajú názvy:

```text
session.csv
session_part02.csv
session_part03.csv
```

Pri prechode medzi časťami sa zapíšu udalosti:

```text
FILE_CONTINUE
FILE_RESUME
```

Všetky časti majú rovnaké `session_id`.

Podporovaných je najviac 32 častí jednej relácie.

### Testovací režim rotácie

Na overenie bez vytvárania veľkého súboru možno dočasne nastaviť:

```cpp
constexpr bool STORAGE_ROTATION_TEST_MODE = true;
```

V testovacom režime sa súbor rozdelí približne pri 64 KiB. V produkčnej verzii musí zostať hodnota `false`.

### Kontrola voľného priestoru

Firmvér rozlišuje:

| Hranica | Správanie |
|---:|---|
| 32 MiB | upozornenie na nízky voľný priestor |
| 4 MiB | zastavenie bežných meracích zápisov |
| 64 KiB | núdzová rezerva pre korektné ukončenie relácie |

Kapacita SD sa pravidelne kontroluje približne každých päť sekúnd.

Grafy, archív a Wi‑Fi export dokážu pracovať s viacdielnou reláciou ako s jedným logickým meraním.

---

## 9. Obnova relácie po reštarte

Stav aktívnej relácie sa priebežne ukladá do internej pamäte ESP32.

Uchováva sa:

- identifikátor relácie,
- stanovisko,
- meraný bod,
- aktuálny CSV súbor,
- základná cesta relácie,
- číslo aktuálnej časti súboru,
- počet manuálnych záznamov.

Po neočakávanom reštarte možno zvoliť:

### Pokračovať

- obnoví sa pôvodná relácia,
- pokračuje sa v existujúcom súbore,
- zapíše sa `SESSION_RESUME`.

### Ukončiť

- pôvodná relácia sa uzavrie,
- zapíše sa `SESSION_END_RECOVERY`.

### Nová relácia

- pôvodná relácia sa označí ako opustená,
- zapíše sa `SESSION_ABANDONED`,
- používateľ môže založiť novú reláciu.

---

## 10. Kalibrácia

Pre teplotu, vlhkosť a tlak možno nastaviť samostatnú lineárnu korekciu:

```text
korigovaná hodnota = a × meraná hodnota + b
```

### Dostupné parametre

- mierkový koeficient `a`,
- offset `b`,
- globálne zapnutie alebo vypnutie kalibrácie.

### Rozsahy

| Parameter | Rozsah |
|---|---:|
| mierka `a` | 0,5 až 1,5 |
| teplotný offset | −20 až +20 °C |
| vlhkostný offset | −50 až +50 % |
| tlakový offset | −150 až +150 hPa |

### Vlastnosti

- kalibračné menu zobrazuje meranú aj korigovanú hodnotu,
- koeficienty sa ukladajú do `Preferences`,
- zostávajú zachované po reštarte,
- počas aktívnej relácie ich nemožno meniť,
- vypnutie kalibrácie koeficienty nevymaže,
- surové aj korigované hodnoty sa ukladajú do CSV.

Zmena koeficientov je používateľská konfigurácia a nemení verziu firmvéru.

---

## 11. Grafy

Grafy sú dostupné:

- počas aktívnej relácie,
- z detailu archivovanej relácie.

Zobrazujú:

- teplotu,
- vlhkosť,
- tlak.

### Rozsahy

#### `120`

Zobrazí posledných 120 automatických záznamov.

#### `VSETKY`

Spracuje celú reláciu. Na displeji sa vykreslí najviac 240 bodov. Pri väčšom počte záznamov sa údaje agregujú do intervalov.

### Štatistiky

Pre vybraný rozsah sa zobrazujú:

- minimum,
- priemer,
- maximum,
- čas prvého a posledného bodu.

### Živá aktualizácia

- režim `120` sa aktualizuje po každom úspešnom automatickom zápise,
- režim `VSETKY` sa pri menších reláciách aktualizuje priamo,
- pri veľkých reláciách sa znovu načíta najviac raz za 10 sekúnd,
- trvanie relácie a počítadlá sa obnovujú samostatne,
- používa sa čiastočné prekresľovanie na obmedzenie blikania obrazovky.

Viacdielne relácie sa pri grafoch spracujú v správnom poradí.

---

## 12. Archív relácií

Archív zobrazuje ukončené relácie uložené na SD karte.

### Vlastnosti

- najviac 96 najnovších relácií,
- päť relácií na jednu stránku,
- zoradenie od najnovšej,
- stránkovanie,
- manuálne obnovenie zoznamu,
- zobrazenie stanoviska, času a veľkosti,
- detail relácie,
- otvorenie grafov.

### Detail relácie

Zobrazuje:

- stanovisko,
- začiatok a koniec,
- trvanie,
- počet `AUTO` záznamov,
- počet `MANUAL` záznamov,
- minimum, priemer a maximum veličín,
- počet častí súboru,
- celkovú veľkosť všetkých častí.

Do archívu sa zaradia iba korektne ukončené alebo uzavreté relácie.

---

## 13. Wi‑Fi export

Zariadenie dokáže vytvoriť vlastnú Wi‑Fi sieť bez routera a internetu.

### Prístupový bod

```text
SSID: GeoMeteo-XXXX
Heslo: geometeo
Adresa: 192.168.4.1
Port: 80
```

Posledná časť SSID sa odvodí od konkrétneho ESP32.

### Funkcionalita webovej stránky

- zoznam ukončených relácií,
- dátum, čas a veľkosť súborov,
- stiahnutie CSV,
- samostatné stiahnutie jednotlivých častí viacdielnej relácie,
- farebné prispôsobenie podľa zvolenej témy zariadenia.

### Ochrany

- Wi‑Fi nemožno spustiť počas aktívnej relácie,
- exportujú sa iba ukončené relácie,
- pred stiahnutím sa súbor znovu overí,
- podporované sú najviac štyri súčasné klienty,
- Wi‑Fi sa vypne po piatich minútach bez aktivity,
- časový limit sa obnoví po dokončení prenosu,
- počas exportu zostáva displej aktívny,
- úsporný režim sa po ukončení exportu obnoví.

---

## 14. Batéria

Napätie batérie sa meria cez ADC s priemerovaním 16 vzoriek.

Používa sa nelineárna aproximačná krivka:

| Napätie | Stav |
|---:|---:|
| 3,20 V | 0 % |
| 3,50 V | 5 % |
| 3,60 V | 10 % |
| 3,70 V | 20 % |
| 3,75 V | 30 % |
| 3,79 V | 40 % |
| 3,83 V | 50 % |
| 3,87 V | 60 % |
| 3,92 V | 70 % |
| 4,00 V | 80 % |
| 4,10 V | 90 % |
| 4,20 V | 100 % |

Medzi bodmi sa používa lineárna interpolácia.

### Stavové hranice

| Stav batérie | Indikácia |
|---:|---|
| viac ako 25 % | zelená |
| 11 až 25 % | oranžová |
| 0 až 10 % | červená |

Pri kritickej batérii zariadenie používateľa upozorní, ale reláciu automaticky nevypne.

Detekcia aktívneho nabíjania nie je implementovaná.

---

## 15. Úspora energie

Firmvér umožňuje automaticky stlmiť a vypnúť podsvietenie displeja.

Dostupné časy nečinnosti:

- 30 sekúnd,
- 1 minúta,
- 2 minúty,
- 5 minút.

Správanie:

1. po nastavenom čase sa jas zníži na približne 5 %,
2. po ďalších 30 sekundách sa podsvietenie vypne,
3. prvý dotyk iba prebudí displej,
4. druhý dotyk vykoná akciu.

Počas vypnutého displeja pokračuje:

- čítanie senzorov,
- RTC,
- automatický zápis,
- kontrola SD,
- počítanie relácie,
- monitoring batérie.

Deep sleep sa nepoužíva.

---

## 16. Nastavenia

Používateľ môže nastaviť:

- dátum a čas RTC,
- interval automatického zápisu,
- jas displeja,
- kalibračné koeficienty,
- zapnutie úspory energie,
- čas stlmenia,
- farebnú schému.

Nastavenia sa ukladajú do `Preferences`.

Zmena jasu alebo intervalu nemení čas RTC.

---

## 17. Farebné schémy

Dostupné sú:

- oranžová,
- modrá,
- zelená,
- červená,
- fialová,
- sivá,
- žltá,
- limetková zelená,
- krémová,
- ružová.

Téma ovplyvňuje:

- panely meraných hodnôt,
- tlačidlá,
- ikony,
- aktívne voľby,
- rámčeky,
- posuvník jasu,
- grafické akcenty,
- Wi‑Fi webovú stránku.

Stavové indikátory používajú vždy pevnú zelenú, oranžovú a červenú.

---

## 18. RTC a čas

DS3231 poskytuje dátum a čas nezávislý od internetového pripojenia.

Firmvér obsahuje:

- editor dátumu,
- editor času,
- kontrolu platného roku,
- časové pečiatky pre všetky CSV udalosti,
- tvorbu identifikátora relácie z dátumu a času.

---

## 19. Verzia firmvéru

Názov a verzia sa zobrazujú:

- na úvodnej obrazovke,
- v nastaveniach,
- v Serial Monitore.

Verzia je definovaná centrálne:

```cpp
constexpr char APP_NAME[] = "GEOMETEO LOGGER";
constexpr char APP_VERSION[] = "v0.9.0";
```

---

## 20. Diagnostika a sériový výstup

Serial Monitor poskytuje informácie o:

- inicializácii zariadení,
- stave senzorov,
- SD karte,
- kapacite úložiska,
- vytvorení relácie,
- názve a časti súboru,
- manuálnych záznamoch,
- chybách zápisu,
- rotácii súborov,
- obnove relácie,
- Wi‑Fi exporte,
- úspore energie,
- kalibračných nastaveniach.

---

## 21. Zámerne neimplementované funkcie

Do aktuálneho firmvéru nie sú zahrnuté:

- povinná stabilizačná čakacia doba po zapnutí,
- automatické ukončenie relácie pri nízkej batérii,
- detekcia nabíjania,
- poznámky k reláciám a záznamom,
- mazanie alebo premenovanie relácií v zariadení,
- USB Mass Storage,
- deep sleep,
- cloudové odosielanie údajov,
- závislosť od internetu.

Odporúčaná stabilizácia senzora sa uvádza iba v dokumentácii:

- približne 60 sekúnd pri bežnom spustení,
- 5–10 minút po výraznej zmene prostredia,
- 10–15 minút pri kalibračnom porovnaní.

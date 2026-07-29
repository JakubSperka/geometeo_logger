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
#include <esp_arduino_version.h>
#include <pgmspace.h>

// ============================================================
// GEOMETEO LOGGER - INFORMACIE O FIRMVÉRI
// Pri dalsej verzii staci upravit hodnotu APP_VERSION.
// ============================================================

constexpr char APP_NAME[] = "GEOMETEO LOGGER";
constexpr char APP_VERSION[] = "v0.7.6";

// ============================================================
// HARDVER: INTERNA I2C ZBERNICA - DOTYK
// ============================================================

constexpr uint8_t INTERNAL_I2C_SDA = 42;
constexpr uint8_t INTERNAL_I2C_SCL = 2;

// ============================================================
// HARDVER: EXTERNA I2C ZBERNICA - METEO SENZOR A RTC
// ============================================================

constexpr uint8_t EXTERNAL_I2C_SDA = 5;
constexpr uint8_t EXTERNAL_I2C_SCL = 6;
constexpr uint32_t I2C_FREQUENCY = 100000;

// ============================================================
// HARDVER: SD KARTA - ESPD-35 v3.2
// ============================================================

constexpr uint8_t SD_SCK_PIN = 14;
constexpr uint8_t SD_MISO_PIN = 21;
constexpr uint8_t SD_MOSI_PIN = 38;
constexpr uint8_t SD_CS_PIN = 17;
constexpr uint32_t SD_SPI_FREQUENCY = 4000000;

// ============================================================
// SENZORY
// ============================================================

constexpr uint8_t BMP280_I2C_ADDRESS = 0x77;

// ============================================================
// BATERIA - ESPD-35 v3.2
// ============================================================

constexpr uint8_t BATTERY_ADC_PIN = 9;
constexpr float BATTERY_DIVIDER_RATIO = 1.7693877551F;
constexpr uint32_t BATTERY_INTERVAL_MS = 5000;
constexpr uint8_t BATTERY_SAMPLE_COUNT = 16;

constexpr uint8_t BATTERY_WARNING_PERCENT = 25;
constexpr uint8_t BATTERY_CRITICAL_PERCENT = 10;

// ============================================================
// SPOĽAHLIVOST A VALIDACIA
// ============================================================

constexpr uint32_t SD_RECOVERY_INTERVAL_MS = 5000;
constexpr uint8_t SD_WRITE_ATTEMPTS = 2;

constexpr float MIN_VALID_TEMPERATURE_C = -40.0F;
constexpr float MAX_VALID_TEMPERATURE_C = 85.0F;
constexpr float MIN_VALID_HUMIDITY_PERCENT = 0.0F;
constexpr float MAX_VALID_HUMIDITY_PERCENT = 100.0F;
constexpr float MIN_VALID_PRESSURE_HPA = 300.0F;
constexpr float MAX_VALID_PRESSURE_HPA = 1100.0F;

constexpr uint16_t MIN_VALID_RTC_YEAR = 2020;
constexpr uint16_t MAX_VALID_RTC_YEAR = 2099;

// ============================================================
// KALIBRACIA - LINEARNA KOREKCIA y = a*x + b
// Neutralne hodnoty nemenia meranie. Konkretne koeficienty
// sa doplnia po porovnani s laboratornym meradlom.
// ============================================================

constexpr float DEFAULT_CALIBRATION_SCALE = 1.0F;
constexpr float DEFAULT_CALIBRATION_OFFSET = 0.0F;

constexpr float MIN_CALIBRATION_SCALE = 0.5F;
constexpr float MAX_CALIBRATION_SCALE = 1.5F;

constexpr float MIN_TEMPERATURE_OFFSET_C = -20.0F;
constexpr float MAX_TEMPERATURE_OFFSET_C = 20.0F;
constexpr float MIN_HUMIDITY_OFFSET_PERCENT = -50.0F;
constexpr float MAX_HUMIDITY_OFFSET_PERCENT = 50.0F;
constexpr float MIN_PRESSURE_OFFSET_HPA = -150.0F;
constexpr float MAX_PRESSURE_OFFSET_HPA = 150.0F;

// ============================================================
// CASOVANIE
// ============================================================

constexpr uint32_t SENSOR_INTERVAL_MS = 1000;
constexpr uint32_t SAVE_FEEDBACK_MS = 1300;
constexpr uint32_t MESSAGE_DURATION_MS = 2500;

constexpr uint16_t DEFAULT_AUTO_LOG_INTERVAL_SECONDS = 5;
constexpr uint8_t DEFAULT_BRIGHTNESS_PERCENT = 100;

constexpr uint16_t AUTO_LOG_INTERVAL_OPTIONS_SECONDS[] = {
    1,
    5,
    10,
    30,
    60
};

constexpr uint8_t MIN_BRIGHTNESS_PERCENT = 10;
constexpr uint8_t MAX_BRIGHTNESS_PERCENT = 100;

// ============================================================
// USPORA ENERGIE DISPLEJA
// Po nastavenej necinnosti sa displej stlmi na 5 %.
// Po dalsich 30 sekundach sa podsvietenie vypne.
// Senzory, RTC a zapis na SD pokracuju bez prerusenia.
// ============================================================

constexpr bool DEFAULT_POWER_SAVING_ENABLED = true;
constexpr uint16_t DEFAULT_POWER_DIM_TIMEOUT_SECONDS = 60;

constexpr uint16_t POWER_DIM_TIMEOUT_OPTIONS_SECONDS[] = {
    30,
    60,
    120,
    300
};

constexpr size_t POWER_DIM_TIMEOUT_OPTION_COUNT =
    sizeof(POWER_DIM_TIMEOUT_OPTIONS_SECONDS) /
    sizeof(POWER_DIM_TIMEOUT_OPTIONS_SECONDS[0]);

constexpr uint8_t POWER_DIM_BRIGHTNESS_PERCENT = 5;
constexpr uint16_t POWER_OFF_DELAY_SECONDS = 30;

constexpr size_t AUTO_LOG_INTERVAL_OPTION_COUNT =
    sizeof(AUTO_LOG_INTERVAL_OPTIONS_SECONDS) /
    sizeof(AUTO_LOG_INTERVAL_OPTIONS_SECONDS[0]);

constexpr uint32_t BACKLIGHT_PWM_FREQUENCY = 5000;
constexpr uint8_t BACKLIGHT_PWM_RESOLUTION = 8;
constexpr uint8_t BACKLIGHT_PWM_CHANNEL = 0;

// ============================================================
// RELACIE A IDENTIFIKATORY
// ============================================================

constexpr size_t IDENTIFIER_MAX_LENGTH = 16;

constexpr char SESSION_DIRECTORY[] = "/sessions";

// ============================================================
// WI-FI EXPORT UKONCENYCH RELACII
// ============================================================

// Zariadenie vytvori vlastny pristupovy bod. Internet ani router
// nie su potrebne. Ukoncene CSV relacie sa stahuju cez prehliadac.
constexpr char WIFI_EXPORT_PASSWORD[] = "geometeo";
constexpr uint16_t WIFI_EXPORT_PORT = 80;
constexpr uint32_t WIFI_EXPORT_IDLE_TIMEOUT_MS = 5UL * 60UL * 1000UL;
constexpr uint32_t WIFI_EXPORT_UI_REFRESH_MS = 1000;
constexpr uint8_t WIFI_EXPORT_MAX_CLIENTS = 4;

const IPAddress WIFI_EXPORT_IP(192, 168, 4, 1);
const IPAddress WIFI_EXPORT_GATEWAY(192, 168, 4, 1);
const IPAddress WIFI_EXPORT_SUBNET(255, 255, 255, 0);

// ============================================================
// GRAFY RELACIE
// ============================================================

// Na displeji sa vykresli najviac 240 bodov. Pri volbe VSETKY
// sa vacsia relacia priebezne zmensi do priemerovych intervalov.
constexpr size_t GRAPH_LAST_RECORD_COUNT = 120;
constexpr size_t GRAPH_MAX_POINT_COUNT = 240;
constexpr size_t GRAPH_CSV_LINE_LENGTH = 384;

// Suhrn relacie sa prekresli kazdu sekundu.
// Rozsah 120 sa aktualizuje po kazdom uspesnom AUTO zapise.
// Rozsah VSETKY sa pri velkej relacii znovu nacita najviac
// raz za 10 sekund, aby sa zbytocne nezatazovala SD karta.
constexpr uint32_t GRAPH_SUMMARY_REFRESH_INTERVAL_MS = 5000;
constexpr uint32_t GRAPH_ALL_REFRESH_MIN_INTERVAL_MS = 10000;

// ============================================================
// ARCHIV UKONCENYCH RELACII
// ============================================================

// V pamati sa uchovava zoznam najnovsich ukoncenych relacii.
// Pri vacsom pocte suborov zostane dostupnych 96 najnovsich.
constexpr size_t ARCHIVE_MAX_SESSIONS = 96;
constexpr size_t ARCHIVE_PAGE_SIZE = 5;
constexpr size_t ARCHIVE_PATH_LENGTH = 96;
constexpr size_t ARCHIVE_SESSION_ID_LENGTH = 32;
constexpr size_t ARCHIVE_CSV_LINE_LENGTH = 384;

// ============================================================
// OBJEKTY
// ============================================================

TFT_eSPI tft = TFT_eSPI();

// FT6236 pouziva fyzicke rozmery dotykovej vrstvy.
// Transformaciu do portretnej orientacie vykona setRotation(0).
FT6236 touch = FT6236(480, 320);

Adafruit_SHT4x sht40;
Adafruit_BMP280 bmp280(&Wire1);
RTC_DS3231 rtc;

SPIClass sdSpi(FSPI);
Preferences preferences;
WebServer wifiExportServer(WIFI_EXPORT_PORT);

// ============================================================
// FARBY
// ============================================================

// Oranzovo-cierna paleta ladena s krytom zariadenia.
constexpr uint16_t COLOR_BACKGROUND = TFT_BLACK;
constexpr uint16_t COLOR_PANEL = 0x18E3;          // tmava antracitova
constexpr uint16_t COLOR_PANEL_BORDER = 0x5980;   // tmava oranzova
constexpr uint16_t COLOR_DISABLED = 0x4208;
constexpr uint16_t COLOR_INPUT = 0x28E2;          // tepla ciernosiva

constexpr uint16_t COLOR_PRIMARY = 0xFC00;         // vyrazna oranzova
constexpr uint16_t COLOR_ACCENT = 0xFD46;          // svetla jantárova
constexpr uint16_t COLOR_TEXT = TFT_WHITE;
constexpr uint16_t COLOR_SECONDARY_TEXT = 0xD699;  // tepla svetlosiva

constexpr uint16_t COLOR_OK = TFT_GREEN;
constexpr uint16_t COLOR_WARNING = COLOR_PRIMARY;
constexpr uint16_t COLOR_ERROR = TFT_RED;

// ============================================================
// BITMAPA IKONY NASTAVENI - 32 x 32 px
// Zdroj: gear.txt. Nulove bity tvoria samotny symbol ozubenia.
// ============================================================

constexpr uint8_t SETTINGS_GEAR_BITMAP_WIDTH = 32;
constexpr uint8_t SETTINGS_GEAR_BITMAP_HEIGHT = 32;

const uint8_t SETTINGS_GEAR_BITMAP[] PROGMEM = {
    0x00, 0x07, 0xE0, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x01, 0x0F, 0xF0, 0x00, 0x07, 0x8F, 0xF1, 0xE0,
    0x0F, 0xDF, 0xFB, 0xF0, 0x1F, 0xFF, 0xFF, 0xF8, 0x1F, 0xFF, 0xFF, 0xF8, 0x1F, 0xFF, 0xFF, 0xF8,
    0x0F, 0xFF, 0xFF, 0xF0, 0x07, 0xFF, 0xFF, 0xE0, 0x07, 0xFF, 0xFF, 0xE0, 0x0F, 0xFC, 0x3F, 0xF0,
    0x7F, 0xF8, 0x1F, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xE0, 0x07, 0xFF, 0xFF, 0xE0, 0x07, 0xFF,
    0xFF, 0xE0, 0x07, 0xFF, 0xFF, 0xF0, 0x0F, 0xFF, 0xFF, 0xF8, 0x1F, 0xFF, 0x0F, 0xFC, 0x3F, 0xF0,
    0x07, 0xFF, 0xFF, 0xE0, 0x07, 0xFF, 0xFF, 0xE0, 0x0F, 0xFF, 0xFF, 0xF0, 0x1F, 0xFF, 0xFF, 0xF8,
    0x1F, 0xFF, 0xFF, 0xF8, 0x1F, 0xFF, 0xFF, 0xF8, 0x0F, 0xFF, 0xFF, 0xF0, 0x07, 0x8F, 0xF1, 0xE0,
    0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x07, 0xE0, 0x00, 0x00, 0x07, 0xE0, 0x00,
};


// ============================================================
// BITMAPA IKONY WI-FI EXPORTU - 32 x 32 px
// Zdroj: wifi(1).txt.
// ============================================================

constexpr uint8_t WIFI_BITMAP_WIDTH = 32;
constexpr uint8_t WIFI_BITMAP_HEIGHT = 32;

const uint8_t WIFI_BITMAP[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0xFC, 0x00, 0x01, 0xFF, 0xFF, 0x00,
    0x07, 0xFF, 0xFF, 0xC0, 0x1F, 0xFF, 0xFF, 0xF0, 0x3F, 0xFF, 0xFF, 0xFC, 0x7F, 0xF0, 0x1F, 0xFE,
    0xFF, 0x80, 0x03, 0xFF, 0xFE, 0x1F, 0xF8, 0x7F, 0xFC, 0x7F, 0xFE, 0x3F, 0xF8, 0xFF, 0xFF, 0x9F,
    0x63, 0xFF, 0xFF, 0xEF, 0x07, 0xFF, 0xFF, 0xE0, 0x0F, 0xF8, 0x1F, 0xF0, 0x0F, 0xE0, 0x07, 0xF0,
    0x0F, 0xC3, 0xE1, 0xF0, 0x0F, 0x9F, 0xF8, 0xF0, 0x00, 0x3F, 0xFE, 0x00, 0x00, 0x7F, 0xFE, 0x00,
    0x00, 0x7F, 0xFF, 0x00, 0x00, 0x7E, 0x3F, 0x00, 0x00, 0x78, 0x1E, 0x00, 0x00, 0x03, 0xC0, 0x00,
    0x00, 0x07, 0xE0, 0x00, 0x00, 0x07, 0xE0, 0x00, 0x00, 0x07, 0xE0, 0x00, 0x00, 0x07, 0xE0, 0x00,
    0x00, 0x03, 0xE0, 0x00, 0x00, 0x01, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


// ============================================================
// BITMAPA IKONY GRAFOV - 32 x 32 px
// Zdroj: graph.txt.
// ============================================================

constexpr uint8_t GRAPH_BITMAP_WIDTH = 32;
constexpr uint8_t GRAPH_BITMAP_HEIGHT = 32;

const uint8_t GRAPH_BITMAP[] PROGMEM = {
    0x20, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x0E, 0xF8, 0x00, 0x00, 0x1E, 0xF8, 0x00, 0x00, 0x1E,
    0xF8, 0x00, 0x00, 0x3E, 0xF8, 0x00, 0x00, 0x3E, 0xF8, 0x00, 0x00, 0x3C, 0xF8, 0x00, 0x0E, 0x78,
    0xF8, 0x00, 0x0F, 0xF8, 0xF8, 0x00, 0x1F, 0xF0, 0xF8, 0x00, 0x3F, 0xF0, 0xF8, 0x00, 0x3F, 0xE0,
    0xF8, 0x00, 0x7F, 0xE0, 0xF8, 0x1C, 0xF3, 0xC0, 0xF8, 0x3F, 0xF1, 0x80, 0xF8, 0x7F, 0xE0, 0x00,
    0xF8, 0xFF, 0xE0, 0x00, 0xF8, 0xFF, 0xC0, 0x00, 0xF9, 0xF3, 0xC0, 0x00, 0xF9, 0xE0, 0x00, 0x00,
    0xFB, 0xE0, 0x00, 0x00, 0xFB, 0xC0, 0x00, 0x00, 0xFF, 0x80, 0x00, 0x00, 0xFF, 0x80, 0x00, 0x00,
    0xFF, 0x00, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFE,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFE
};


// ============================================================
// BITMAPA IKONY ARCHIVU RELACII - 32 x 32 px
// Zdroj: save.txt.
// ============================================================

constexpr uint8_t ARCHIVE_BITMAP_WIDTH = 32;
constexpr uint8_t ARCHIVE_BITMAP_HEIGHT = 32;

const uint8_t ARCHIVE_BITMAP[] PROGMEM = {
    0x3F, 0xFF, 0xF8, 0x00, 0x3F, 0xFF, 0xFC, 0x00, 0x3F, 0xFF, 0xFE, 0x00, 0x3F, 0xFF, 0xFF, 0x00,
  0x3F, 0xFF, 0xFF, 0x80, 0x3C, 0xF8, 0x1F, 0xC0, 0x3C, 0xF8, 0x1F, 0xE0, 0x3C, 0xF8, 0x1F, 0xF8,
  0x3C, 0xFF, 0xFF, 0xFC, 0x3C, 0xFF, 0xFF, 0xFC, 0x3C, 0xFF, 0xFF, 0x7C, 0x3C, 0xFF, 0xFF, 0x3C,
  0x3C, 0x00, 0x00, 0x3C, 0x3C, 0x00, 0x00, 0x3C, 0x3C, 0x00, 0x00, 0x3C, 0x3C, 0x00, 0x00, 0x3C,
  0x3C, 0x00, 0x00, 0x3C, 0x3C, 0x00, 0x00, 0x3C, 0x3C, 0xFF, 0xFF, 0x3C, 0x3C, 0xFF, 0xFF, 0x3C,
  0x3C, 0xFF, 0xFF, 0x3C, 0x3C, 0xFF, 0xFF, 0x3C, 0x3C, 0xF8, 0x1F, 0x3C, 0x3C, 0xF8, 0x1F, 0x3C,
  0x3C, 0xF8, 0x1F, 0x3C, 0x3C, 0xF8, 0x1F, 0x3C, 0x3C, 0xF8, 0x1F, 0x3C, 0x3F, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC
};


// ============================================================
// DATOVE TYPY
// ============================================================

struct Measurement
{
    DateTime timestamp;

    // Hodnoty pouzivane na displeji, v grafoch a v hlavnych
    // CSV stlpcoch. Pri zapnutej kalibracii su korigovane.
    float temperatureC;
    float humidityPercent;
    float pressureHpa;

    // Povodne hodnoty priamo zo senzorov. Ukladaju sa do CSV,
    // aby bolo mozne kalibraciu neskor spatne skontrolovat.
    float rawTemperatureC;
    float rawHumidityPercent;
    float rawPressureHpa;

    bool valid;
};

struct CalibrationConfig
{
    bool enabled;
    float temperatureScale;
    float temperatureOffsetC;
    float humidityScale;
    float humidityOffsetPercent;
    float pressureScale;
    float pressureOffsetHpa;
};

enum class CalibrationParameter : uint8_t
{
    Temperature,
    Humidity,
    Pressure
};

enum class CalibrationEditField : uint8_t
{
    None,
    Scale,
    Offset
};

enum class Screen : uint8_t
{
    Main,
    Keyboard,
    ConfirmEndSession,
    Settings,
    DateTimeEditor,
    Recovery,
    Graphs,
    ArchiveList,
    ArchiveDetail,
    Calibration,
    CalibrationEditor,
    PowerSettings,
    WifiExport
};

enum class DisplayPowerState : uint8_t
{
    Awake,
    Dimmed,
    Off
};

enum class EditField : uint8_t
{
    None,
    Station,
    TargetPoint
};

enum class KeyboardMode : uint8_t
{
    Numeric,
    Alphabetic
};

enum class SaveButtonState : uint8_t
{
    Idle,
    Success,
    Error
};

enum class StatusState : uint8_t
{
    Ok,
    Warning,
    Error
};

enum class MeasurementError : uint8_t
{
    None,
    DevicesNotReady,
    RtcInvalid,
    ShtReadFailed,
    PressureReadFailed,
    TemperatureOutOfRange,
    HumidityOutOfRange,
    PressureOutOfRange
};


enum class GraphRange : uint8_t
{
    Last120,
    All
};

enum class GraphParameter : uint8_t
{
    Temperature,
    Humidity,
    Pressure
};

struct GraphSample
{
    uint32_t unixTime;
    float temperatureC;
    float humidityPercent;
    float pressureHpa;
};

struct GraphStatistics
{
    float minimum;
    float maximum;
    double sum;
    uint32_t count;
};

struct ArchiveSession
{
    char filePath[ARCHIVE_PATH_LENGTH];
    char sessionId[ARCHIVE_SESSION_ID_LENGTH];
    char station[IDENTIFIER_MAX_LENGTH + 1];
    uint32_t startUnix;
    uint32_t endUnix;
    uint32_t fileSize;
};

struct ArchiveSessionDetailData
{
    bool loaded;
    char filePath[ARCHIVE_PATH_LENGTH];
    char sessionId[ARCHIVE_SESSION_ID_LENGTH];
    char station[IDENTIFIER_MAX_LENGTH + 1];
    uint32_t startUnix;
    uint32_t endUnix;
    uint32_t autoRecordCount;
    uint32_t manualRecordCount;
    GraphStatistics temperatureStats;
    GraphStatistics humidityStats;
    GraphStatistics pressureStats;
};

struct Rect
{
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;

    bool contains(int16_t pointX, int16_t pointY) const
    {
        return
            pointX >= x &&
            pointX < x + width &&
            pointY >= y &&
            pointY < y + height;
    }
};

// ============================================================
// ROZMERY HLAVNEJ OBRAZOVKY - PORTRET 320 x 480
// ============================================================

constexpr Rect STATION_FIELD_RECT{10, 202, 300, 40};
constexpr Rect TARGET_FIELD_RECT{10, 248, 300, 40};

constexpr Rect SESSION_TOGGLE_BUTTON_RECT{10, 294, 300, 42};

constexpr Rect SAVE_BUTTON_RECT{10, 342, 300, 54};

constexpr Rect CONFIRM_BACK_BUTTON_RECT{10, 410, 145, 52};
constexpr Rect CONFIRM_END_BUTTON_RECT{165, 410, 145, 52};

constexpr uint8_t MAIN_SHORTCUT_ICON_SIZE = 36;

constexpr Rect WIFI_SHORTCUT_RECT{160, 440, 40, 40};
constexpr Rect ARCHIVE_SHORTCUT_RECT{200, 440, 40, 40};
constexpr Rect GRAPH_SHORTCUT_RECT{240, 440, 40, 40};
constexpr Rect SETTINGS_SHORTCUT_RECT{280, 440, 40, 40};

constexpr Rect STATUS_MESSAGE_RECT{0, 440, 158, 40};

// ============================================================
// WI-FI EXPORT
// ============================================================

constexpr Rect WIFI_STATUS_PANEL_RECT{10, 52, 300, 224};
constexpr Rect WIFI_ACTION_RECT{10, 292, 300, 58};
constexpr Rect WIFI_REFRESH_RECT{10, 364, 145, 44};
constexpr Rect WIFI_BACK_RECT{10, 420, 300, 44};

// ============================================================
// ARCHIV UKONCENYCH RELACII
// ============================================================

constexpr Rect ARCHIVE_ROW_RECTS[ARCHIVE_PAGE_SIZE] = {
    {10, 50, 300, 58},
    {10, 116, 300, 58},
    {10, 182, 300, 58},
    {10, 248, 300, 58},
    {10, 314, 300, 58}
};

constexpr Rect ARCHIVE_BACK_RECT{10, 414, 72, 50};
constexpr Rect ARCHIVE_PREVIOUS_RECT{90, 414, 64, 50};
constexpr Rect ARCHIVE_NEXT_RECT{162, 414, 64, 50};
constexpr Rect ARCHIVE_REFRESH_RECT{234, 414, 76, 50};

constexpr Rect ARCHIVE_DETAIL_PANEL_RECT{10, 50, 300, 350};
constexpr Rect ARCHIVE_DETAIL_BACK_RECT{10, 414, 145, 50};
constexpr Rect ARCHIVE_DETAIL_GRAPH_RECT{165, 414, 145, 50};

// ============================================================
// OBRAZOVKA GRAFOV A SUHRNU RELACIE
// ============================================================

constexpr Rect GRAPH_TEMPERATURE_RECT{10, 48, 94, 40};
constexpr Rect GRAPH_HUMIDITY_RECT{113, 48, 94, 40};
constexpr Rect GRAPH_PRESSURE_RECT{216, 48, 94, 40};

constexpr Rect GRAPH_SUMMARY_RECT{10, 96, 300, 58};
constexpr Rect GRAPH_PANEL_RECT{10, 162, 300, 238};

constexpr Rect GRAPH_BACK_RECT{10, 414, 80, 50};
constexpr Rect GRAPH_LAST120_RECT{98, 414, 104, 50};
constexpr Rect GRAPH_ALL_RECT{210, 414, 100, 50};

// ============================================================
// OBRAZOVKA OBNOVY NEDOKONCENEJ RELACIE
// ============================================================

constexpr Rect RECOVERY_CONTINUE_RECT{10, 300, 300, 50};
constexpr Rect RECOVERY_END_RECT{10, 360, 145, 50};
constexpr Rect RECOVERY_NEW_RECT{165, 360, 145, 50};

// ============================================================
// ROZMERY OBRAZOVKY NASTAVENI
// ============================================================

constexpr Rect SETTINGS_DATETIME_PANEL_RECT{10, 54, 300, 82};
constexpr Rect SETTINGS_DATETIME_BUTTON_RECT{20, 92, 280, 34};

constexpr Rect SETTINGS_INTERVAL_BUTTON_RECTS[] = {
    {10, 178, 54, 48},
    {70, 178, 54, 48},
    {130, 178, 54, 48},
    {190, 178, 54, 48},
    {250, 178, 60, 48}
};

constexpr Rect SETTINGS_BRIGHTNESS_TOUCH_RECT{20, 270, 280, 64};
constexpr Rect SETTINGS_BRIGHTNESS_TRACK_RECT{28, 294, 264, 12};

constexpr Rect SETTINGS_CALIBRATION_RECT{10, 354, 145, 44};
constexpr Rect SETTINGS_POWER_RECT{165, 354, 145, 44};
constexpr Rect SETTINGS_BACK_RECT{10, 410, 145, 54};
constexpr Rect SETTINGS_GENERAL_SAVE_RECT{165, 410, 145, 54};

// ============================================================
// ROZMERY USPORY ENERGIE
// ============================================================

constexpr Rect POWER_ENABLED_RECT{10, 58, 300, 54};

constexpr Rect POWER_TIMEOUT_RECTS[
    POWER_DIM_TIMEOUT_OPTION_COUNT
] = {
    {10, 166, 67, 52},
    {87, 166, 67, 52},
    {164, 166, 67, 52},
    {241, 166, 69, 52}
};

constexpr Rect POWER_INFO_RECT{10, 238, 300, 156};
constexpr Rect POWER_BACK_RECT{10, 414, 145, 50};
constexpr Rect POWER_SAVE_RECT{165, 414, 145, 50};

// ============================================================
// ROZMERY KALIBRACIE
// ============================================================

constexpr Rect CALIBRATION_TEMPERATURE_RECT{10, 50, 94, 40};
constexpr Rect CALIBRATION_HUMIDITY_RECT{113, 50, 94, 40};
constexpr Rect CALIBRATION_PRESSURE_RECT{216, 50, 94, 40};

constexpr Rect CALIBRATION_LIVE_RECT{10, 98, 300, 88};
constexpr Rect CALIBRATION_SCALE_RECT{10, 196, 300, 48};
constexpr Rect CALIBRATION_OFFSET_RECT{10, 252, 300, 48};
constexpr Rect CALIBRATION_ENABLED_RECT{10, 308, 300, 44};
constexpr Rect CALIBRATION_INFO_RECT{10, 360, 300, 38};
constexpr Rect CALIBRATION_BACK_RECT{10, 414, 145, 50};
constexpr Rect CALIBRATION_SAVE_RECT{165, 414, 145, 50};

constexpr Rect CALIBRATION_EDITOR_DISPLAY_RECT{10, 52, 240, 54};
constexpr Rect CALIBRATION_EDITOR_BACKSPACE_RECT{260, 52, 50, 54};
constexpr Rect CALIBRATION_EDITOR_CLEAR_RECT{10, 114, 300, 28};
constexpr Rect CALIBRATION_EDITOR_BACK_RECT{10, 414, 145, 50};
constexpr Rect CALIBRATION_EDITOR_SAVE_RECT{165, 414, 145, 50};

// ============================================================
// ROZMERY EDITORA DATUMU A CASU
// ============================================================

constexpr Rect DATETIME_DISPLAY_RECT{10, 50, 300, 76};

constexpr Rect DATETIME_BACK_RECT{10, 414, 145, 52};
constexpr Rect DATETIME_SAVE_RECT{165, 414, 145, 52};

// ============================================================
// ROZMERY KLAVESNICE
// ============================================================

constexpr Rect KEYBOARD_INPUT_RECT{10, 48, 244, 48};
constexpr Rect KEYBOARD_BACKSPACE_RECT{262, 48, 48, 48};

constexpr Rect KEYBOARD_MODE_RECT{10, 426, 80, 46};
constexpr Rect KEYBOARD_CANCEL_RECT{98, 426, 104, 46};
constexpr Rect KEYBOARD_OK_RECT{210, 426, 100, 46};

// ============================================================
// STAV ZARIADENI
// ============================================================

bool internalWireReady = false;
bool externalWireReady = false;

bool touchReady = false;
bool sht40Ready = false;
bool bmp280Ready = false;
bool rtcReady = false;
bool sdReady = false;
bool sdSpiStarted = false;
bool batteryAvailable = false;

bool sessionActive = false;
bool pendingSessionRecovery = false;
bool lastAutoWriteOk = false;
bool lastManualWriteOk = false;
bool touchWasPressed = false;

// ============================================================
// STAV APLIKACIE
// ============================================================

Screen currentScreen = Screen::Main;
EditField editedField = EditField::None;
KeyboardMode keyboardMode = KeyboardMode::Numeric;
SaveButtonState saveButtonState = SaveButtonState::Idle;

Measurement currentMeasurement{
    DateTime(2000, 1, 1, 0, 0, 0),
    NAN,
    NAN,
    NAN,
    NAN,
    NAN,
    NAN,
    false
};

Measurement readMeasurement();

char station[IDENTIFIER_MAX_LENGTH + 1] = "";
char targetPoint[IDENTIFIER_MAX_LENGTH + 1] = "";
char editBuffer[IDENTIFIER_MAX_LENGTH + 1] = "";

char sessionId[32] = "";
char sessionFilePath[80] = "";

char recoveredStation[IDENTIFIER_MAX_LENGTH + 1] = "";
char recoveredTargetPoint[IDENTIFIER_MAX_LENGTH + 1] = "";
char recoveredSessionId[32] = "";
char recoveredSessionFilePath[80] = "";
uint32_t recoveredManualRecordCount = 0;

char statusMessage[64] = "";
uint16_t statusMessageColor = COLOR_SECONDARY_TEXT;

uint32_t lastSensorMillis = 0;
uint32_t lastAutoLogMillis = 0;
uint32_t lastBatteryMillis = 0;
uint32_t lastSdRecoveryAttemptMillis = 0;
uint32_t saveFeedbackUntil = 0;
uint32_t statusMessageUntil = 0;
uint32_t manualRecordCount = 0;

float batteryVoltage = NAN;
uint8_t batteryPercent = 0;
uint8_t lastBatteryWarningLevel = 0;

MeasurementError lastMeasurementError =
    MeasurementError::None;

uint16_t autoLogIntervalSeconds =
    DEFAULT_AUTO_LOG_INTERVAL_SECONDS;

uint8_t brightnessPercent =
    DEFAULT_BRIGHTNESS_PERCENT;

uint16_t pendingAutoLogIntervalSeconds =
    DEFAULT_AUTO_LOG_INTERVAL_SECONDS;

uint8_t pendingBrightnessPercent =
    DEFAULT_BRIGHTNESS_PERCENT;

bool displayPwmReady = false;
bool brightnessSliderDragging = false;

// Uspora energie displeja.
bool powerSavingEnabled =
    DEFAULT_POWER_SAVING_ENABLED;

uint16_t powerDimTimeoutSeconds =
    DEFAULT_POWER_DIM_TIMEOUT_SECONDS;

bool pendingPowerSavingEnabled =
    DEFAULT_POWER_SAVING_ENABLED;

uint16_t pendingPowerDimTimeoutSeconds =
    DEFAULT_POWER_DIM_TIMEOUT_SECONDS;

DisplayPowerState displayPowerState =
    DisplayPowerState::Awake;

uint32_t lastUserActivityMillis = 0;

// Wi-Fi export je standardne vypnuty a aktivuje sa iba manualne.
bool wifiExportRunning = false;
bool wifiExportRoutesConfigured = false;
char wifiExportSsid[32] = "GeoMeteo";
uint32_t wifiExportStartedMillis = 0;
uint32_t wifiExportLastActivityMillis = 0;
uint32_t wifiExportLastUiRefreshMillis = 0;
uint8_t wifiExportLastClientCount = 0;
char wifiExportStatusMessage[64] = "";

CalibrationConfig calibration{
    false,
    DEFAULT_CALIBRATION_SCALE,
    DEFAULT_CALIBRATION_OFFSET,
    DEFAULT_CALIBRATION_SCALE,
    DEFAULT_CALIBRATION_OFFSET,
    DEFAULT_CALIBRATION_SCALE,
    DEFAULT_CALIBRATION_OFFSET
};

CalibrationConfig pendingCalibration = calibration;
CalibrationParameter calibrationParameter =
    CalibrationParameter::Temperature;
CalibrationEditField calibrationEditField =
    CalibrationEditField::None;

char calibrationEditBuffer[20] = "";
char calibrationEditorError[48] = "";

// 14 cislic: DDMMYYYYHHMMSS
char dateTimeDigits[15] = "";


// ============================================================
// STAV GRAFOV
// ============================================================

GraphRange graphRange = GraphRange::Last120;
GraphParameter graphParameter = GraphParameter::Temperature;

GraphSample graphSamples[GRAPH_MAX_POINT_COUNT];
GraphSample graphRecentBuffer[GRAPH_LAST_RECORD_COUNT];

size_t graphPointCount = 0;
uint32_t graphTotalAutoRecords = 0;
uint32_t graphTotalManualRecords = 0;
uint32_t graphSessionStartUnix = 0;
uint32_t graphSessionLastUnix = 0;

GraphStatistics graphDisplayTemperatureStats;
GraphStatistics graphDisplayHumidityStats;
GraphStatistics graphDisplayPressureStats;

GraphStatistics graphTotalTemperatureStats;
GraphStatistics graphTotalHumidityStats;
GraphStatistics graphTotalPressureStats;

bool graphDataReady = false;
char graphErrorMessage[48] = "";

uint32_t lastGraphSummaryRefreshMillis = 0;
uint32_t lastGraphDataRefreshMillis = 0;
bool graphFullRangeRefreshPending = false;

// Explicitna deklaracia, pretoze funkcia sa pouziva skor,
// nez je v subore uvedena jej implementacia.
void drawGraphSummary(bool redrawPanel);

// Pri zobrazeni archivovanej relacie grafy citaju iny CSV subor
// a neprebieha ziadna ziva aktualizacia.
bool graphViewingArchive = false;

// ============================================================
// STAV ARCHIVU RELACII
// ============================================================

ArchiveSession archiveSessions[ARCHIVE_MAX_SESSIONS];
size_t archiveSessionCount = 0;
size_t archiveCurrentPage = 0;
int16_t selectedArchiveSessionIndex = -1;

ArchiveSessionDetailData archiveDetailData{};
char archiveStatusMessage[64] = "";

// ============================================================
// CACHE PRE CIASTOCNE PREKRESLOVANIE GUI
// ============================================================

bool mainDateTimeCacheValid = false;
bool mainMeasurementCacheValid = false;
bool mainStatusCacheValid = false;
bool mainMessageCacheValid = false;
bool mainSaveButtonCacheValid = false;

char cachedMainDate[16] = "";
char cachedMainTime[16] = "";
char cachedMainBattery[20] = "";
uint16_t cachedMainBatteryColor = 0;

char cachedTemperatureValue[24] = "";
char cachedHumidityValue[24] = "";
char cachedPressureValue[24] = "";

char cachedMainStatusSignature[64] = "";
char cachedMainMessageSignature[96] = "";

char cachedSaveButtonText[32] = "";
uint16_t cachedSaveFillColor = 0;
uint16_t cachedSaveBorderColor = 0;

// ============================================================
// VSEOBECNE POMOCNE FUNKCIE
// ============================================================

bool sensorsAreReady()
{
    return sht40Ready && bmp280Ready && rtcReady;
}

bool identifierIsEmpty(const char *value)
{
    return value == nullptr || value[0] == '\0';
}

bool manualSaveAvailable()
{
    return
        touchReady &&
        sdReady &&
        sessionActive &&
        currentMeasurement.valid &&
        !identifierIsEmpty(station) &&
        !identifierIsEmpty(targetPoint);
}

bool timedEventActive(uint32_t endTime)
{
    if (endTime == 0)
    {
        return false;
    }

    return static_cast<int32_t>(endTime - millis()) > 0;
}

bool saveFeedbackActive()
{
    return
        saveButtonState != SaveButtonState::Idle &&
        timedEventActive(saveFeedbackUntil);
}

void copyIdentifier(
    char *destination,
    size_t destinationSize,
    const char *source
)
{
    if (destinationSize == 0)
    {
        return;
    }

    snprintf(
        destination,
        destinationSize,
        "%s",
        source == nullptr ? "" : source
    );
}

bool identifiersEqual(const char *first, const char *second)
{
    return strcmp(first, second) == 0;
}

void setStatusMessage(
    const char *message,
    uint16_t color = COLOR_SECONDARY_TEXT,
    uint32_t durationMs = MESSAGE_DURATION_MS
)
{
    snprintf(
        statusMessage,
        sizeof(statusMessage),
        "%s",
        message == nullptr ? "" : message
    );

    statusMessageColor = color;

    statusMessageUntil =
        durationMs == 0
            ? 0
            : millis() + durationMs;
}

void clearStatusMessage()
{
    statusMessage[0] = '\0';
    statusMessageUntil = 0;
}

// ============================================================
// CHYBY MERANIA
// ============================================================

const char *measurementErrorText(
    MeasurementError error
)
{
    switch (error)
    {
        case MeasurementError::None:
            return "MERANIE OK";

        case MeasurementError::DevicesNotReady:
            return "SENZORY ALEBO RTC NIE SU PRIPRAVENE";

        case MeasurementError::RtcInvalid:
            return "NEPLATNY DATUM ALEBO CAS RTC";

        case MeasurementError::ShtReadFailed:
            return "CHYBA CITANIA SHT40";

        case MeasurementError::PressureReadFailed:
            return "CHYBA CITANIA BMP280";

        case MeasurementError::TemperatureOutOfRange:
            return "TEPLOTA MIMO PLATNEHO ROZSAHU";

        case MeasurementError::HumidityOutOfRange:
            return "VLHKOST MIMO PLATNEHO ROZSAHU";

        case MeasurementError::PressureOutOfRange:
            return "TLAK MIMO PLATNEHO ROZSAHU";

        default:
            return "NEZNAMA CHYBA MERANIA";
    }
}

bool recordTypeAllowsInvalidMeasurement(
    const char *recordType
)
{
    return
        recordType != nullptr &&
        strncmp(recordType, "SESSION_", 8) == 0;
}

// ============================================================
// OBNOVA MERACEJ RELACIE V INTERNEJ PAMATI
// ============================================================

void clearPersistentSessionState()
{
    preferences.begin("meteo", false);

    preferences.putBool("sess_act", false);
    preferences.remove("sess_st");
    preferences.remove("sess_tgt");
    preferences.remove("sess_id");
    preferences.remove("sess_file");
    preferences.remove("sess_man");

    preferences.end();
}

void persistActiveSessionState()
{
    preferences.begin("meteo", false);

    preferences.putBool(
        "sess_act",
        sessionActive
    );

    if (sessionActive)
    {
        preferences.putString(
            "sess_st",
            station
        );

        preferences.putString(
            "sess_tgt",
            targetPoint
        );

        preferences.putString(
            "sess_id",
            sessionId
        );

        preferences.putString(
            "sess_file",
            sessionFilePath
        );

        preferences.putULong(
            "sess_man",
            manualRecordCount
        );
    }
    else
    {
        preferences.remove("sess_st");
        preferences.remove("sess_tgt");
        preferences.remove("sess_id");
        preferences.remove("sess_file");
        preferences.remove("sess_man");
    }

    preferences.end();
}

bool loadPendingSessionRecovery()
{
    preferences.begin("meteo", true);

    const bool storedActive =
        preferences.getBool(
            "sess_act",
            false
        );

    const String storedStation =
        preferences.getString(
            "sess_st",
            ""
        );

    const String storedTarget =
        preferences.getString(
            "sess_tgt",
            ""
        );

    const String storedSessionId =
        preferences.getString(
            "sess_id",
            ""
        );

    const String storedFilePath =
        preferences.getString(
            "sess_file",
            ""
        );

    const uint32_t storedManualCount =
        preferences.getULong(
            "sess_man",
            0
        );

    preferences.end();

    if (
        !storedActive ||
        storedStation.length() == 0 ||
        storedSessionId.length() == 0 ||
        storedFilePath.length() == 0
    )
    {
        pendingSessionRecovery = false;
        return false;
    }

    storedStation.toCharArray(
        recoveredStation,
        sizeof(recoveredStation)
    );

    storedTarget.toCharArray(
        recoveredTargetPoint,
        sizeof(recoveredTargetPoint)
    );

    storedSessionId.toCharArray(
        recoveredSessionId,
        sizeof(recoveredSessionId)
    );

    storedFilePath.toCharArray(
        recoveredSessionFilePath,
        sizeof(recoveredSessionFilePath)
    );

    recoveredManualRecordCount =
        storedManualCount;

    pendingSessionRecovery = true;

    Serial.println(
        "NAJDENA NEDOKONCENA RELACIA"
    );

    Serial.printf(
        "STANOVISKO: %s\n",
        recoveredStation
    );

    Serial.printf(
        "SUBOR: %s\n",
        recoveredSessionFilePath
    );

    Serial.printf(
        "MANUALNE ZAZNAMY: %lu\n",
        static_cast<unsigned long>(
            recoveredManualRecordCount
        )
    );

    return true;
}

void resetRuntimeSessionState()
{
    sessionActive = false;
    lastAutoWriteOk = false;
    lastManualWriteOk = false;

    station[0] = '\0';
    targetPoint[0] = '\0';

    sessionId[0] = '\0';
    sessionFilePath[0] = '\0';

    manualRecordCount = 0;
    saveButtonState = SaveButtonState::Idle;
    saveFeedbackUntil = 0;
}

void restoreRecoveredSessionToRuntime()
{
    copyIdentifier(
        station,
        sizeof(station),
        recoveredStation
    );

    copyIdentifier(
        targetPoint,
        sizeof(targetPoint),
        recoveredTargetPoint
    );

    copyIdentifier(
        sessionId,
        sizeof(sessionId),
        recoveredSessionId
    );

    snprintf(
        sessionFilePath,
        sizeof(sessionFilePath),
        "%s",
        recoveredSessionFilePath
    );

    manualRecordCount =
        recoveredManualRecordCount;

    sessionActive = true;
    lastAutoWriteOk = false;
    lastManualWriteOk = false;
    lastAutoLogMillis = millis();
}

// ============================================================
// MERANIE BATERIE
// ============================================================

uint8_t batteryPercentFromVoltage(float voltage)
{
    struct CurvePoint
    {
        float voltage;
        uint8_t percent;
    };

    static const CurvePoint CURVE[] = {
        {3.20F, 0},
        {3.50F, 5},
        {3.60F, 10},
        {3.70F, 20},
        {3.75F, 30},
        {3.79F, 40},
        {3.83F, 50},
        {3.87F, 60},
        {3.92F, 70},
        {4.00F, 80},
        {4.10F, 90},
        {4.20F, 100}
    };

    constexpr size_t POINT_COUNT =
        sizeof(CURVE) / sizeof(CURVE[0]);

    if (voltage <= CURVE[0].voltage)
    {
        return CURVE[0].percent;
    }

    if (voltage >= CURVE[POINT_COUNT - 1].voltage)
    {
        return CURVE[POINT_COUNT - 1].percent;
    }

    for (size_t index = 1; index < POINT_COUNT; index++)
    {
        if (voltage <= CURVE[index].voltage)
        {
            const CurvePoint &lower = CURVE[index - 1];
            const CurvePoint &upper = CURVE[index];

            const float fraction =
                (voltage - lower.voltage) /
                (upper.voltage - lower.voltage);

            const float interpolated =
                lower.percent +
                fraction * (upper.percent - lower.percent);

            return static_cast<uint8_t>(
                roundf(interpolated)
            );
        }
    }

    return 0;
}

uint16_t batteryStatusColor()
{
    if (!batteryAvailable)
    {
        return COLOR_DISABLED;
    }

    if (batteryPercent <= BATTERY_CRITICAL_PERCENT)
    {
        return COLOR_ERROR;
    }

    if (batteryPercent <= BATTERY_WARNING_PERCENT)
    {
        return COLOR_WARNING;
    }

    return COLOR_OK;
}

void readBatteryState()
{
    // Prve citanie stabilizuje ADC po dlhsom intervale necinnosti.
    analogReadMilliVolts(BATTERY_ADC_PIN);
    delayMicroseconds(250);

    uint32_t totalMilliVolts = 0;

    for (uint8_t sample = 0; sample < BATTERY_SAMPLE_COUNT; sample++)
    {
        totalMilliVolts +=
            analogReadMilliVolts(BATTERY_ADC_PIN);

        delayMicroseconds(150);
    }

    const float adcMilliVolts =
        static_cast<float>(totalMilliVolts) /
        BATTERY_SAMPLE_COUNT;

    const float measuredVoltage =
        adcMilliVolts *
        BATTERY_DIVIDER_RATIO /
        1000.0F;

    if (
        isnan(measuredVoltage) ||
        measuredVoltage < 2.5F ||
        measuredVoltage > 4.5F
    )
    {
        batteryAvailable = false;
        batteryVoltage = NAN;
        batteryPercent = 0;

        Serial.println(
            "BATERIA: MERANIE NIE JE DOSTUPNE"
        );

        return;
    }

    batteryAvailable = true;

    if (isnan(batteryVoltage))
    {
        batteryVoltage = measuredVoltage;
    }
    else
    {
        // Jednoduche exponencialne vyhladenie ADC sumu.
        batteryVoltage =
            0.70F * batteryVoltage +
            0.30F * measuredVoltage;
    }

    batteryPercent =
        batteryPercentFromVoltage(batteryVoltage);

    Serial.printf(
        "BATERIA: %.2f V | %u %%\n",
        batteryVoltage,
        batteryPercent
    );
}

void updateBatteryWarning()
{
    uint8_t warningLevel = 0;

    if (batteryAvailable)
    {
        if (batteryPercent <= BATTERY_CRITICAL_PERCENT)
        {
            warningLevel = 2;
        }
        else if (batteryPercent <= BATTERY_WARNING_PERCENT)
        {
            warningLevel = 1;
        }
    }

    if (warningLevel == lastBatteryWarningLevel)
    {
        return;
    }

    lastBatteryWarningLevel = warningLevel;

    if (warningLevel == 2)
    {
        setStatusMessage(
            sessionActive
                ? "BATERIA KRITICKA - UKONCI RELACIU"
                : "BATERIA KRITICKA",
            COLOR_ERROR,
            6000
        );
    }
    else if (warningLevel == 1)
    {
        setStatusMessage(
            "NIZKA UROVEN BATERIE",
            COLOR_WARNING,
            5000
        );
    }
}

void initializeBatteryMonitor()
{
    pinMode(BATTERY_ADC_PIN, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(
        BATTERY_ADC_PIN,
        ADC_11db
    );

    readBatteryState();
    updateBatteryWarning();
}

// ============================================================
// TRVALE NASTAVENIA A PODSVIETENIE
// ============================================================

bool valueIsAllowed(
    uint16_t value,
    const uint16_t *options,
    size_t optionCount
)
{
    for (size_t index = 0; index < optionCount; index++)
    {
        if (options[index] == value)
        {
            return true;
        }
    }

    return false;
}

bool brightnessIsAllowed(uint8_t value)
{
    return
        value >= MIN_BRIGHTNESS_PERCENT &&
        value <= MAX_BRIGHTNESS_PERCENT;
}

bool calibrationValueInRange(
    float value,
    float minimum,
    float maximum
)
{
    return
        isfinite(value) &&
        value >= minimum &&
        value <= maximum;
}

bool calibrationConfigIsValid(
    const CalibrationConfig &config
)
{
    return
        calibrationValueInRange(
            config.temperatureScale,
            MIN_CALIBRATION_SCALE,
            MAX_CALIBRATION_SCALE
        ) &&
        calibrationValueInRange(
            config.temperatureOffsetC,
            MIN_TEMPERATURE_OFFSET_C,
            MAX_TEMPERATURE_OFFSET_C
        ) &&
        calibrationValueInRange(
            config.humidityScale,
            MIN_CALIBRATION_SCALE,
            MAX_CALIBRATION_SCALE
        ) &&
        calibrationValueInRange(
            config.humidityOffsetPercent,
            MIN_HUMIDITY_OFFSET_PERCENT,
            MAX_HUMIDITY_OFFSET_PERCENT
        ) &&
        calibrationValueInRange(
            config.pressureScale,
            MIN_CALIBRATION_SCALE,
            MAX_CALIBRATION_SCALE
        ) &&
        calibrationValueInRange(
            config.pressureOffsetHpa,
            MIN_PRESSURE_OFFSET_HPA,
            MAX_PRESSURE_OFFSET_HPA
        );
}

void resetCalibrationConfig(
    CalibrationConfig &config
)
{
    config.enabled = false;
    config.temperatureScale =
        DEFAULT_CALIBRATION_SCALE;
    config.temperatureOffsetC =
        DEFAULT_CALIBRATION_OFFSET;
    config.humidityScale =
        DEFAULT_CALIBRATION_SCALE;
    config.humidityOffsetPercent =
        DEFAULT_CALIBRATION_OFFSET;
    config.pressureScale =
        DEFAULT_CALIBRATION_SCALE;
    config.pressureOffsetHpa =
        DEFAULT_CALIBRATION_OFFSET;
}

float applyLinearCalibration(
    float rawValue,
    float scale,
    float offset,
    bool enabled
)
{
    if (!enabled)
    {
        return rawValue;
    }

    return rawValue * scale + offset;
}

void loadPersistentSettings()
{
    preferences.begin("meteo", true);

    autoLogIntervalSeconds = preferences.getUShort(
        "log_seconds",
        DEFAULT_AUTO_LOG_INTERVAL_SECONDS
    );

    brightnessPercent = preferences.getUChar(
        "brightness",
        DEFAULT_BRIGHTNESS_PERCENT
    );

    powerSavingEnabled = preferences.getBool(
        "power_save",
        DEFAULT_POWER_SAVING_ENABLED
    );

    powerDimTimeoutSeconds = preferences.getUShort(
        "power_dim_s",
        DEFAULT_POWER_DIM_TIMEOUT_SECONDS
    );

    calibration.enabled = preferences.getBool(
        "cal_enabled",
        false
    );

    calibration.temperatureScale = preferences.getFloat(
        "cal_t_a",
        DEFAULT_CALIBRATION_SCALE
    );

    calibration.temperatureOffsetC = preferences.getFloat(
        "cal_t_b",
        DEFAULT_CALIBRATION_OFFSET
    );

    calibration.humidityScale = preferences.getFloat(
        "cal_h_a",
        DEFAULT_CALIBRATION_SCALE
    );

    calibration.humidityOffsetPercent = preferences.getFloat(
        "cal_h_b",
        DEFAULT_CALIBRATION_OFFSET
    );

    calibration.pressureScale = preferences.getFloat(
        "cal_p_a",
        DEFAULT_CALIBRATION_SCALE
    );

    calibration.pressureOffsetHpa = preferences.getFloat(
        "cal_p_b",
        DEFAULT_CALIBRATION_OFFSET
    );

    preferences.end();

    if (!valueIsAllowed(
            autoLogIntervalSeconds,
            AUTO_LOG_INTERVAL_OPTIONS_SECONDS,
            AUTO_LOG_INTERVAL_OPTION_COUNT
        ))
    {
        autoLogIntervalSeconds =
            DEFAULT_AUTO_LOG_INTERVAL_SECONDS;
    }

    if (!brightnessIsAllowed(brightnessPercent))
    {
        brightnessPercent =
            DEFAULT_BRIGHTNESS_PERCENT;
    }

    if (!valueIsAllowed(
            powerDimTimeoutSeconds,
            POWER_DIM_TIMEOUT_OPTIONS_SECONDS,
            POWER_DIM_TIMEOUT_OPTION_COUNT
        ))
    {
        powerDimTimeoutSeconds =
            DEFAULT_POWER_DIM_TIMEOUT_SECONDS;
    }

    pendingPowerSavingEnabled =
        powerSavingEnabled;

    pendingPowerDimTimeoutSeconds =
        powerDimTimeoutSeconds;

    if (!calibrationConfigIsValid(calibration))
    {
        resetCalibrationConfig(calibration);
    }

    pendingCalibration = calibration;
}

void saveGeneralPersistentSettings()
{
    preferences.begin("meteo", false);

    preferences.putUShort(
        "log_seconds",
        autoLogIntervalSeconds
    );

    preferences.putUChar(
        "brightness",
        brightnessPercent
    );

    preferences.end();
}

void savePowerPersistentSettings()
{
    preferences.begin("meteo", false);

    preferences.putBool(
        "power_save",
        powerSavingEnabled
    );

    preferences.putUShort(
        "power_dim_s",
        powerDimTimeoutSeconds
    );

    preferences.end();
}

void saveCalibrationPersistentSettings()
{
    preferences.begin("meteo", false);

    preferences.putBool(
        "cal_enabled",
        calibration.enabled
    );

    preferences.putFloat(
        "cal_t_a",
        calibration.temperatureScale
    );

    preferences.putFloat(
        "cal_t_b",
        calibration.temperatureOffsetC
    );

    preferences.putFloat(
        "cal_h_a",
        calibration.humidityScale
    );

    preferences.putFloat(
        "cal_h_b",
        calibration.humidityOffsetPercent
    );

    preferences.putFloat(
        "cal_p_a",
        calibration.pressureScale
    );

    preferences.putFloat(
        "cal_p_b",
        calibration.pressureOffsetHpa
    );

    preferences.end();
}

uint32_t brightnessDuty(uint8_t percent)
{
    if (percent > 100)
    {
        percent = 100;
    }

    uint32_t duty =
        static_cast<uint32_t>(percent) * 255UL / 100UL;

#if defined(TFT_BACKLIGHT_ON) && TFT_BACKLIGHT_ON == LOW
    duty = 255UL - duty;
#endif

    return duty;
}

void applyBrightness(uint8_t percent)
{
    if (!displayPwmReady)
    {
        digitalWrite(
            TFT_BL,
            percent == 0
                ? !TFT_BACKLIGHT_ON
                : TFT_BACKLIGHT_ON
        );

        return;
    }

    const uint32_t duty =
        brightnessDuty(percent);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(TFT_BL, duty);
#else
    ledcWrite(BACKLIGHT_PWM_CHANNEL, duty);
#endif
}

uint8_t activeAwakeBrightnessPercent()
{
    if (currentScreen == Screen::Settings)
    {
        return pendingBrightnessPercent;
    }

    return brightnessPercent;
}

void setDisplayPowerState(
    DisplayPowerState newState
)
{
    if (displayPowerState == newState)
    {
        return;
    }

    displayPowerState = newState;

    switch (displayPowerState)
    {
        case DisplayPowerState::Awake:
            applyBrightness(
                activeAwakeBrightnessPercent()
            );
            Serial.println(
                "DISPLEJ: AKTIVNY"
            );
            break;

        case DisplayPowerState::Dimmed:
            applyBrightness(
                POWER_DIM_BRIGHTNESS_PERCENT
            );
            Serial.println(
                "DISPLEJ: STLMENTY"
            );
            break;

        case DisplayPowerState::Off:
            applyBrightness(0);
            Serial.println(
                "DISPLEJ: VYPNUTY"
            );
            break;
    }
}

void registerUserActivity()
{
    lastUserActivityMillis = millis();

    if (
        displayPowerState !=
        DisplayPowerState::Awake
    )
    {
        setDisplayPowerState(
            DisplayPowerState::Awake
        );
    }
}

bool wakeDisplayAndConsumeTouch()
{
    if (
        displayPowerState ==
        DisplayPowerState::Awake
    )
    {
        lastUserActivityMillis = millis();
        return false;
    }

    registerUserActivity();

    // Prvy dotyk po stlmeni alebo vypnuti iba rozsvieti
    // displej. Ziadne tlacidlo sa tymto dotykom neaktivuje.
    return true;
}

void updateDisplayPowerSaving(
    uint32_t currentMillis
)
{
    // Pri aktivnom Wi-Fi exporte zostava displej rozsvieteny,
    // aby boli prihlasovacie udaje a stav prenosu stale viditelne.
    if (wifiExportRunning)
    {
        if (
            displayPowerState !=
            DisplayPowerState::Awake
        )
        {
            setDisplayPowerState(
                DisplayPowerState::Awake
            );
        }

        lastUserActivityMillis = currentMillis;
        return;
    }

    if (!powerSavingEnabled)
    {
        if (
            displayPowerState !=
            DisplayPowerState::Awake
        )
        {
            setDisplayPowerState(
                DisplayPowerState::Awake
            );
        }

        return;
    }

    const uint32_t idleMillis =
        currentMillis -
        lastUserActivityMillis;

    const uint32_t dimAfterMillis =
        static_cast<uint32_t>(
            powerDimTimeoutSeconds
        ) * 1000UL;

    const uint32_t offAfterMillis =
        dimAfterMillis +
        static_cast<uint32_t>(
            POWER_OFF_DELAY_SECONDS
        ) * 1000UL;

    if (idleMillis >= offAfterMillis)
    {
        setDisplayPowerState(
            DisplayPowerState::Off
        );
    }
    else if (idleMillis >= dimAfterMillis)
    {
        setDisplayPowerState(
            DisplayPowerState::Dimmed
        );
    }
    else
    {
        setDisplayPowerState(
            DisplayPowerState::Awake
        );
    }
}

void initializeBacklight()
{
    pinMode(TFT_BL, OUTPUT);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
    displayPwmReady = ledcAttach(
        TFT_BL,
        BACKLIGHT_PWM_FREQUENCY,
        BACKLIGHT_PWM_RESOLUTION
    );
#else
    ledcSetup(
        BACKLIGHT_PWM_CHANNEL,
        BACKLIGHT_PWM_FREQUENCY,
        BACKLIGHT_PWM_RESOLUTION
    );

    ledcAttachPin(
        TFT_BL,
        BACKLIGHT_PWM_CHANNEL
    );

    displayPwmReady = true;
#endif

    applyBrightness(brightnessPercent);
}

// ============================================================
// DISPLEJ - ZAKLADNA INICIALIZACIA
// ============================================================

void initializeDisplay()
{
    tft.init();

    // Portretna orientacia, 90 stupnov proti smeru hodinovych
    // ruciciek oproti povodnej orientacii rotation = 1.
    tft.setRotation(0);

    initializeBacklight();

    tft.fillScreen(COLOR_BACKGROUND);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);

    tft.drawString(
        APP_NAME,
        tft.width() / 2,
        195,
        4
    );

    tft.setTextColor(COLOR_SECONDARY_TEXT, COLOR_BACKGROUND);

    tft.drawString(
        APP_VERSION,
        tft.width() / 2,
        235,
        2
    );

    tft.drawString(
        "Inicializacia...",
        tft.width() / 2,
        270,
        2
    );
}

void drawPanel(const Rect &rect, uint8_t radius = 8)
{
    tft.fillRoundRect(
        rect.x,
        rect.y,
        rect.width,
        rect.height,
        radius,
        COLOR_PANEL
    );

    tft.drawRoundRect(
        rect.x,
        rect.y,
        rect.width,
        rect.height,
        radius,
        COLOR_PANEL_BORDER
    );
}

void drawButton(
    const Rect &rect,
    const char *text,
    uint16_t fillColor,
    uint16_t borderColor,
    uint16_t textColor = COLOR_TEXT,
    uint8_t font = 2
)
{
    tft.fillRoundRect(
        rect.x,
        rect.y,
        rect.width,
        rect.height,
        7,
        fillColor
    );

    tft.drawRoundRect(
        rect.x,
        rect.y,
        rect.width,
        rect.height,
        7,
        borderColor
    );

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(textColor, fillColor);

    tft.drawString(
        text,
        rect.x + rect.width / 2,
        rect.y + rect.height / 2,
        font
    );
}

void drawColoredPanel(
    const Rect &rect,
    uint16_t fillColor,
    uint16_t borderColor,
    uint8_t radius = 8
)
{
    tft.fillRoundRect(
        rect.x,
        rect.y,
        rect.width,
        rect.height,
        radius,
        fillColor
    );

    tft.drawRoundRect(
        rect.x,
        rect.y,
        rect.width,
        rect.height,
        radius,
        borderColor
    );
}

void invalidateMainUiCaches()
{
    mainDateTimeCacheValid = false;
    mainMeasurementCacheValid = false;
    mainStatusCacheValid = false;
    mainMessageCacheValid = false;
    mainSaveButtonCacheValid = false;

    cachedMainDate[0] = '\0';
    cachedMainTime[0] = '\0';
    cachedMainBattery[0] = '\0';

    cachedTemperatureValue[0] = '\0';
    cachedHumidityValue[0] = '\0';
    cachedPressureValue[0] = '\0';

    cachedMainStatusSignature[0] = '\0';
    cachedMainMessageSignature[0] = '\0';
    cachedSaveButtonText[0] = '\0';
}


// ============================================================
// HLAVNA OBRAZOVKA - STATICKY VZHLAD
// ============================================================

void drawMainStaticLayout()
{
    invalidateMainUiCaches();

    tft.fillScreen(COLOR_BACKGROUND);

    // Datum a cas.
    tft.fillRect(0, 0, 320, 40, COLOR_PANEL);
    tft.drawFastHLine(0, 39, 320, COLOR_PANEL_BORDER);

    // Atmosfericke hodnoty - vyrazne oranzove panely.
    drawColoredPanel(
        Rect{10, 48, 300, 74},
        COLOR_PRIMARY,
        COLOR_ACCENT
    );

    drawColoredPanel(
        Rect{10, 130, 145, 64},
        COLOR_PRIMARY,
        COLOR_ACCENT
    );

    drawColoredPanel(
        Rect{165, 130, 145, 64},
        COLOR_PRIMARY,
        COLOR_ACCENT
    );

    // Stanovisko a merany bod.
    drawPanel(STATION_FIELD_RECT);
    drawPanel(TARGET_FIELD_RECT);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_BLACK, COLOR_PRIMARY);

    // Teplotna jednotka so samostatne vykreslenym znakom stupna.
    const char *temperatureLabelStart = "TEPLOTA (";
    constexpr int16_t temperatureLabelX = 20;
    constexpr int16_t temperatureLabelY = 56;

    tft.drawString(
        temperatureLabelStart,
        temperatureLabelX,
        temperatureLabelY,
        1
    );

    const int16_t degreeX =
        temperatureLabelX +
        tft.textWidth(temperatureLabelStart, 1) +
        2;

    tft.drawCircle(
        degreeX,
        temperatureLabelY + 2,
        2,
        TFT_BLACK
    );

    tft.drawString(
        "C)",
        degreeX + 5,
        temperatureLabelY,
        1
    );

    tft.drawString("VLHKOST (%)", 20, 138, 2);
    tft.drawString("TLAK (hPa)", 175, 138, 2);

    // Stav zariadenia a pocet manualnych zaznamov.
    drawPanel(Rect{10, 402, 300, 36});

    // Spodny informacny riadok a zvacsene skratky.
    tft.fillRect(0, 440, 320, 40, COLOR_BACKGROUND);
}

// ============================================================
// HLAVNA OBRAZOVKA - DATUM A CAS
// ============================================================

void drawBatteryHeaderStatus()
{
    char batteryBuffer[20];

    if (batteryAvailable)
    {
        snprintf(
            batteryBuffer,
            sizeof(batteryBuffer),
            "%u%% %.2f V",
            batteryPercent,
            batteryVoltage
        );
    }
    else
    {
        snprintf(
            batteryBuffer,
            sizeof(batteryBuffer),
            "--%% --.--V"
        );
    }

    const uint16_t color =
        batteryStatusColor();

    if (
        mainDateTimeCacheValid &&
        strcmp(
            cachedMainBattery,
            batteryBuffer
        ) == 0 &&
        cachedMainBatteryColor == color
    )
    {
        return;
    }

    tft.fillRect(
        214,
        4,
        100,
        32,
        COLOR_PANEL
    );

    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(color, COLOR_PANEL);

    tft.drawString(
        batteryBuffer,
        314,
        20,
        2
    );

    snprintf(
        cachedMainBattery,
        sizeof(cachedMainBattery),
        "%s",
        batteryBuffer
    );

    cachedMainBatteryColor = color;
}

void updateDateTimeDisplay(const DateTime &dateTime)
{
    char dateBuffer[16];
    char timeBuffer[16];

    snprintf(
        dateBuffer,
        sizeof(dateBuffer),
        "%02u.%02u.%04u",
        dateTime.day(),
        dateTime.month(),
        dateTime.year()
    );

    snprintf(
        timeBuffer,
        sizeof(timeBuffer),
        "%02u:%02u:%02u",
        dateTime.hour(),
        dateTime.minute(),
        dateTime.second()
    );

    if (
        !mainDateTimeCacheValid ||
        strcmp(cachedMainDate, dateBuffer) != 0
    )
    {
        tft.fillRect(6, 4, 112, 32, COLOR_PANEL);

        tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
        tft.setTextDatum(ML_DATUM);

        tft.drawString(
            dateBuffer,
            8,
            20,
            2
        );

        snprintf(
            cachedMainDate,
            sizeof(cachedMainDate),
            "%s",
            dateBuffer
        );
    }

    if (
        !mainDateTimeCacheValid ||
        strcmp(cachedMainTime, timeBuffer) != 0
    )
    {
        tft.fillRect(118, 4, 96, 32, COLOR_PANEL);

        tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
        tft.setTextDatum(MC_DATUM);

        tft.drawString(
            timeBuffer,
            166,
            20,
            2
        );

        snprintf(
            cachedMainTime,
            sizeof(cachedMainTime),
            "%s",
            timeBuffer
        );
    }

    drawBatteryHeaderStatus();

    mainDateTimeCacheValid = true;
}

// ============================================================
// HLAVNA OBRAZOVKA - HODNOTY
// ============================================================

void drawCenteredValue(
    const Rect &rect,
    const char *value,
    uint8_t font,
    uint8_t textSize = 1
)
{
    tft.fillRect(
        rect.x,
        rect.y,
        rect.width,
        rect.height,
        COLOR_PRIMARY
    );

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_BLACK, COLOR_PRIMARY);
    tft.setTextSize(textSize);

    const int16_t centerX =
        rect.x + rect.width / 2;

    const int16_t centerY =
        rect.y + rect.height / 2;

    // Jemny bold efekt bez obrysu: text sa vykresli dvakrat
    // s horizontalnym posunom jedneho pixelu.
    tft.drawString(
        value,
        centerX - 1,
        centerY,
        font
    );

    tft.drawString(
        value,
        centerX,
        centerY,
        font
    );

    tft.setTextSize(1);
}

void updateMeasurementDisplay(const Measurement &measurement)
{
    char temperatureBuffer[24];
    char humidityBuffer[24];
    char pressureBuffer[24];

    if (!measurement.valid)
    {
        snprintf(
            temperatureBuffer,
            sizeof(temperatureBuffer),
            "---"
        );

        snprintf(
            humidityBuffer,
            sizeof(humidityBuffer),
            "---"
        );

        snprintf(
            pressureBuffer,
            sizeof(pressureBuffer),
            "---"
        );
    }
    else
    {
        snprintf(
            temperatureBuffer,
            sizeof(temperatureBuffer),
            "%.2f",
            measurement.temperatureC
        );

        snprintf(
            humidityBuffer,
            sizeof(humidityBuffer),
            "%.2f",
            measurement.humidityPercent
        );

        snprintf(
            pressureBuffer,
            sizeof(pressureBuffer),
            "%.2f",
            measurement.pressureHpa
        );
    }

    if (
        !mainMeasurementCacheValid ||
        strcmp(
            cachedTemperatureValue,
            temperatureBuffer
        ) != 0
    )
    {
        drawCenteredValue(
            Rect{20, 68, 280, 50},
            temperatureBuffer,
            6
        );

        snprintf(
            cachedTemperatureValue,
            sizeof(cachedTemperatureValue),
            "%s",
            temperatureBuffer
        );
    }

    if (
        !mainMeasurementCacheValid ||
        strcmp(
            cachedHumidityValue,
            humidityBuffer
        ) != 0
    )
    {
        drawCenteredValue(
            Rect{20, 156, 125, 28},
            humidityBuffer,
            4
        );

        snprintf(
            cachedHumidityValue,
            sizeof(cachedHumidityValue),
            "%s",
            humidityBuffer
        );
    }

    if (
        !mainMeasurementCacheValid ||
        strcmp(
            cachedPressureValue,
            pressureBuffer
        ) != 0
    )
    {
        drawCenteredValue(
            Rect{175, 156, 125, 28},
            pressureBuffer,
            4
        );

        snprintf(
            cachedPressureValue,
            sizeof(cachedPressureValue),
            "%s",
            pressureBuffer
        );
    }

    mainMeasurementCacheValid = true;
}

// ============================================================
// HLAVNA OBRAZOVKA - EDITOVATELNE POLIA
// ============================================================

void drawEditableField(
    const Rect &rect,
    const char *label,
    const char *value,
    const char *emptyText,
    bool editable
)
{
    const uint16_t borderColor =
        editable ? COLOR_PANEL_BORDER : COLOR_DISABLED;

    tft.fillRoundRect(
        rect.x + 1,
        rect.y + 1,
        rect.width - 2,
        rect.height - 2,
        7,
        COLOR_PANEL
    );

    tft.drawRoundRect(
        rect.x,
        rect.y,
        rect.width,
        rect.height,
        8,
        borderColor
    );

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_SECONDARY_TEXT, COLOR_PANEL);
    tft.drawString(label, rect.x + 10, rect.y + 5, 1);

    tft.setTextDatum(ML_DATUM);

    if (identifierIsEmpty(value))
    {
        tft.setTextColor(
            editable ? COLOR_WARNING : COLOR_DISABLED,
            COLOR_PANEL
        );

        tft.drawString(
            emptyText,
            rect.x + 10,
            rect.y + 27,
            2
        );
    }
    else
    {
        tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
        tft.drawString(
            value,
            rect.x + 10,
            rect.y + 27,
            2
        );
    }

    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(
        editable ? COLOR_PRIMARY : COLOR_DISABLED,
        COLOR_PANEL
    );

    tft.drawString(
        editable ? "UPRAV" : "ZAMKNUTE",
        rect.x + rect.width - 10,
        rect.y + 27,
        1
    );
}

void updateEditableFields()
{
    drawEditableField(
        STATION_FIELD_RECT,
        "STANOVISKO",
        station,
        "NOVA RELACIA",
        !sessionActive
    );

    drawEditableField(
        TARGET_FIELD_RECT,
        "MERANY BOD",
        targetPoint,
        sessionActive
            ? "DOTYKOM ZADAJ"
            : "NAJPRV RELACIA",
        sessionActive
    );
}

// ============================================================
// HLAVNA OBRAZOVKA - STAVOVE INDIKATORY
// ============================================================

uint16_t statusColor(StatusState state)
{
    switch (state)
    {
        case StatusState::Ok:
            return COLOR_OK;

        case StatusState::Warning:
            return COLOR_WARNING;

        case StatusState::Error:
        default:
            return COLOR_ERROR;
    }
}

void drawCompactStatus(
    int16_t centerX,
    int16_t centerY,
    const char *label,
    int8_t acuteAccentIndex,
    StatusState state
)
{
    const uint16_t color = statusColor(state);

    tft.fillCircle(centerX, centerY - 7, 5, color);

    const int16_t labelY = centerY + 4;

    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COLOR_SECONDARY_TEXT, COLOR_PANEL);
    tft.drawString(label, centerX, labelY, 1);

    // Vstavany font nema plnu podporu UTF-8. Pre dve stavove
    // slova preto vykreslime dlzen/ciarku nad pismenom A rucne.
    if (acuteAccentIndex >= 0)
    {
        char prefix[16];

        const size_t prefixLength =
            static_cast<size_t>(
                acuteAccentIndex
            );

        snprintf(
            prefix,
            sizeof(prefix),
            "%.*s",
            static_cast<int>(prefixLength),
            label
        );

        const int16_t labelWidth =
            tft.textWidth(label, 1);

        const int16_t prefixWidth =
            tft.textWidth(prefix, 1);

        const int16_t letterWidth =
            tft.textWidth("A", 1);

        const int16_t accentX =
            centerX -
            labelWidth / 2 +
            prefixWidth +
            letterWidth / 2;

        tft.drawPixel(
            accentX,
            labelY - 2,
            COLOR_SECONDARY_TEXT
        );

        tft.drawPixel(
            accentX + 1,
            labelY - 3,
            COLOR_SECONDARY_TEXT
        );
    }
}

void updateStatusPanel()
{
    StatusState autoState = StatusState::Warning;

    if (sessionActive)
    {
        autoState =
            lastAutoWriteOk
                ? StatusState::Ok
                : StatusState::Error;
    }

    char signature[64];

    snprintf(
        signature,
        sizeof(signature),
        "%u|%u|%u|%u|%lu",
        sensorsAreReady() ? 1U : 0U,
        sdReady ? 1U : 0U,
        sessionActive ? 1U : 0U,
        static_cast<unsigned int>(autoState),
        static_cast<unsigned long>(
            manualRecordCount
        )
    );

    if (
        mainStatusCacheValid &&
        strcmp(
            cachedMainStatusSignature,
            signature
        ) == 0
    )
    {
        return;
    }

    tft.fillRoundRect(11, 403, 298, 34, 7, COLOR_PANEL);

    drawCompactStatus(
        31,
        420,
        "SENZOR",
        -1,
        sensorsAreReady()
            ? StatusState::Ok
            : StatusState::Error
    );

    drawCompactStatus(
        82,
        420,
        "SD",
        -1,
        sdReady
            ? StatusState::Ok
            : StatusState::Error
    );

    drawCompactStatus(
        144,
        420,
        "RELACIA",
        3,
        sessionActive
            ? StatusState::Ok
            : StatusState::Warning
    );

    drawCompactStatus(
        209,
        420,
        "ZAZNAM",
        1,
        autoState
    );

    char countLabel[24];

    snprintf(
        countLabel,
        sizeof(countLabel),
        "MANUAL: %lu",
        static_cast<unsigned long>(
            manualRecordCount
        )
    );

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(
        sessionActive
            ? COLOR_TEXT
            : COLOR_DISABLED,
        COLOR_PANEL
    );

    tft.drawString(
        countLabel,
        270,
        420,
        1
    );

    snprintf(
        cachedMainStatusSignature,
        sizeof(cachedMainStatusSignature),
        "%s",
        signature
    );

    mainStatusCacheValid = true;
}

// ============================================================
// HLAVNA OBRAZOVKA - INFORMACNY RIADOK
// ============================================================

void drawMsbBitmap(
    int16_t x,
    int16_t y,
    const uint8_t *bitmap,
    uint8_t width,
    uint8_t height,
    uint16_t foregroundColor,
    uint16_t backgroundColor
)
{
    const uint8_t bytesPerRow =
        (width + 7) / 8;

    // Pozadie tlacidla zostava cierne.
    tft.fillRect(
        x,
        y,
        width,
        height,
        backgroundColor
    );

    for (uint8_t row = 0; row < height; row++)
    {
        for (uint8_t column = 0; column < width; column++)
        {
            const uint8_t byteValue =
                pgm_read_byte(
                    bitmap +
                    row * bytesPerRow +
                    column / 8
                );

            const bool sourceBitIsSet =
                byteValue &
                (0x80U >> (column % 8));

            // Nastavene bity tvoria oranzove ozubene koliesko.
            if (sourceBitIsSet)
            {
                tft.drawPixel(
                    x + column,
                    y + row,
                    foregroundColor
                );
            }
        }
    }
}

void drawScaledMsbBitmap(
    int16_t x,
    int16_t y,
    const uint8_t *bitmap,
    uint8_t sourceWidth,
    uint8_t sourceHeight,
    uint8_t destinationWidth,
    uint8_t destinationHeight,
    uint16_t foregroundColor,
    uint16_t backgroundColor
)
{
    const uint8_t bytesPerRow =
        (sourceWidth + 7) / 8;

    tft.fillRect(
        x,
        y,
        destinationWidth,
        destinationHeight,
        backgroundColor
    );

    for (
        uint8_t destinationY = 0;
        destinationY < destinationHeight;
        destinationY++
    )
    {
        const uint8_t sourceY =
            static_cast<uint8_t>(
                (
                    static_cast<uint16_t>(
                        destinationY
                    ) *
                    sourceHeight
                ) /
                destinationHeight
            );

        for (
            uint8_t destinationX = 0;
            destinationX < destinationWidth;
            destinationX++
        )
        {
            const uint8_t sourceX =
                static_cast<uint8_t>(
                    (
                        static_cast<uint16_t>(
                            destinationX
                        ) *
                        sourceWidth
                    ) /
                    destinationWidth
                );

            const uint8_t byteValue =
                pgm_read_byte(
                    bitmap +
                    sourceY * bytesPerRow +
                    sourceX / 8
                );

            if (
                byteValue &
                (0x80U >> (sourceX % 8))
            )
            {
                tft.drawPixel(
                    x + destinationX,
                    y + destinationY,
                    foregroundColor
                );
            }
        }
    }
}


void drawSettingsGearIcon()
{
    if (currentScreen != Screen::Main)
    {
        return;
    }

    const int16_t iconX =
        SETTINGS_SHORTCUT_RECT.x +
        (
            SETTINGS_SHORTCUT_RECT.width -
            MAIN_SHORTCUT_ICON_SIZE
        ) / 2;

    const int16_t iconY =
        SETTINGS_SHORTCUT_RECT.y +
        (
            SETTINGS_SHORTCUT_RECT.height -
            MAIN_SHORTCUT_ICON_SIZE
        ) / 2;

    drawScaledMsbBitmap(
        iconX,
        iconY,
        SETTINGS_GEAR_BITMAP,
        SETTINGS_GEAR_BITMAP_WIDTH,
        SETTINGS_GEAR_BITMAP_HEIGHT,
        MAIN_SHORTCUT_ICON_SIZE,
        MAIN_SHORTCUT_ICON_SIZE,
        COLOR_PRIMARY,
        COLOR_BACKGROUND
    );
}


void drawWifiShortcutIcon()
{
    if (currentScreen != Screen::Main)
    {
        return;
    }

    uint16_t color =
        (
            sdReady &&
            !sessionActive
        )
            ? COLOR_PRIMARY
            : COLOR_DISABLED;

    if (wifiExportRunning)
    {
        color =
            WiFi.softAPgetStationNum() > 0
                ? COLOR_OK
                : COLOR_ACCENT;
    }

    const int16_t iconX =
        WIFI_SHORTCUT_RECT.x +
        (
            WIFI_SHORTCUT_RECT.width -
            MAIN_SHORTCUT_ICON_SIZE
        ) / 2;

    const int16_t iconY =
        WIFI_SHORTCUT_RECT.y +
        (
            WIFI_SHORTCUT_RECT.height -
            MAIN_SHORTCUT_ICON_SIZE
        ) / 2;

    drawScaledMsbBitmap(
        iconX,
        iconY,
        WIFI_BITMAP,
        WIFI_BITMAP_WIDTH,
        WIFI_BITMAP_HEIGHT,
        MAIN_SHORTCUT_ICON_SIZE,
        MAIN_SHORTCUT_ICON_SIZE,
        color,
        COLOR_BACKGROUND
    );
}

void drawArchiveShortcutIcon()
{
    if (currentScreen != Screen::Main)
    {
        return;
    }

    const int16_t iconX =
        ARCHIVE_SHORTCUT_RECT.x +
        (
            ARCHIVE_SHORTCUT_RECT.width -
            MAIN_SHORTCUT_ICON_SIZE
        ) / 2;

    const int16_t iconY =
        ARCHIVE_SHORTCUT_RECT.y +
        (
            ARCHIVE_SHORTCUT_RECT.height -
            MAIN_SHORTCUT_ICON_SIZE
        ) / 2;

    drawScaledMsbBitmap(
        iconX,
        iconY,
        ARCHIVE_BITMAP,
        ARCHIVE_BITMAP_WIDTH,
        ARCHIVE_BITMAP_HEIGHT,
        MAIN_SHORTCUT_ICON_SIZE,
        MAIN_SHORTCUT_ICON_SIZE,
        sdReady ? COLOR_PRIMARY : COLOR_DISABLED,
        COLOR_BACKGROUND
    );
}


void drawGraphShortcutIcon()
{
    if (currentScreen != Screen::Main)
    {
        return;
    }

    const int16_t iconX =
        GRAPH_SHORTCUT_RECT.x +
        (
            GRAPH_SHORTCUT_RECT.width -
            MAIN_SHORTCUT_ICON_SIZE
        ) / 2;

    const int16_t iconY =
        GRAPH_SHORTCUT_RECT.y +
        (
            GRAPH_SHORTCUT_RECT.height -
            MAIN_SHORTCUT_ICON_SIZE
        ) / 2;

    drawScaledMsbBitmap(
        iconX,
        iconY,
        GRAPH_BITMAP,
        GRAPH_BITMAP_WIDTH,
        GRAPH_BITMAP_HEIGHT,
        MAIN_SHORTCUT_ICON_SIZE,
        MAIN_SHORTCUT_ICON_SIZE,
        sessionActive ? COLOR_PRIMARY : COLOR_DISABLED,
        COLOR_BACKGROUND
    );
}

void updateMessageStrip()
{
    const char *message = statusMessage;
    uint16_t color = statusMessageColor;

    if (
        statusMessage[0] != '\0' &&
        statusMessageUntil != 0 &&
        !timedEventActive(statusMessageUntil)
    )
    {
        clearStatusMessage();
        message = "";
    }

    if (message[0] == '\0')
    {
        if (!sdReady)
        {
            message = "SD KARTA NIE JE DOSTUPNA";
            color = COLOR_ERROR;
        }
        else if (!sensorsAreReady())
        {
            message = "CHYBA MERACICH ZARIADENI";
            color = COLOR_ERROR;
        }
        else if (!currentMeasurement.valid)
        {
            message = measurementErrorText(
                lastMeasurementError
            );

            color = COLOR_ERROR;
        }
        else if (
            batteryAvailable &&
            batteryPercent <=
                BATTERY_CRITICAL_PERCENT
        )
        {
            message = sessionActive
                ? "BATERIA KRITICKA - UKONCI RELACIU"
                : "BATERIA KRITICKA";

            color = COLOR_ERROR;
        }
        else if (
            batteryAvailable &&
            batteryPercent <=
                BATTERY_WARNING_PERCENT
        )
        {
            message = "NIZKA UROVEN BATERIE";
            color = COLOR_WARNING;
        }
        else if (!sessionActive)
        {
            message = "STLAC NOVA RELACIA";
            color = COLOR_WARNING;
        }
        else if (identifierIsEmpty(targetPoint))
        {
            message = "ZADAJ MERANY BOD";
            color = COLOR_WARNING;
        }
        else
        {
            message = "PRIPRAVENE NA ZAZNAM";
            color = COLOR_SECONDARY_TEXT;
        }
    }

    char signature[96];

    snprintf(
        signature,
        sizeof(signature),
        "%s|%u|%u|%u|%u|%u",
        message,
        static_cast<unsigned int>(color),
        sdReady ? 1U : 0U,
        sessionActive ? 1U : 0U,
        wifiExportRunning ? 1U : 0U,
        static_cast<unsigned int>(
            wifiExportRunning
                ? WiFi.softAPgetStationNum()
                : 0
        )
    );

    if (
        mainMessageCacheValid &&
        strcmp(
            cachedMainMessageSignature,
            signature
        ) == 0
    )
    {
        return;
    }

    tft.fillRect(
        STATUS_MESSAGE_RECT.x,
        STATUS_MESSAGE_RECT.y,
        STATUS_MESSAGE_RECT.width,
        STATUS_MESSAGE_RECT.height,
        COLOR_BACKGROUND
    );

    const int16_t maximumTextWidth =
        STATUS_MESSAGE_RECT.width - 8;

    tft.setTextColor(
        color,
        COLOR_BACKGROUND
    );

    if (
        tft.textWidth(message, 2) <=
        maximumTextWidth
    )
    {
        tft.setTextDatum(MC_DATUM);

        tft.drawString(
            message,
            STATUS_MESSAGE_RECT.x +
                STATUS_MESSAGE_RECT.width / 2,
            STATUS_MESSAGE_RECT.y +
                STATUS_MESSAGE_RECT.height / 2,
            2
        );
    }
    else
    {
        char firstLine[40] = "";
        char secondLine[40] = "";

        const size_t messageLength =
            strlen(message);

        int16_t bestSplit = -1;
        int16_t bestDifference = 32767;

        for (
            size_t index = 1;
            index < messageLength;
            index++
        )
        {
            if (message[index] != ' ')
            {
                continue;
            }

            char leftCandidate[40];
            char rightCandidate[40];

            snprintf(
                leftCandidate,
                sizeof(leftCandidate),
                "%.*s",
                static_cast<int>(index),
                message
            );

            snprintf(
                rightCandidate,
                sizeof(rightCandidate),
                "%s",
                message + index + 1
            );

            const int16_t leftWidth =
                tft.textWidth(
                    leftCandidate,
                    1
                );

            const int16_t rightWidth =
                tft.textWidth(
                    rightCandidate,
                    1
                );

            if (
                leftWidth <= maximumTextWidth &&
                rightWidth <= maximumTextWidth
            )
            {
                const int16_t difference =
                    abs(leftWidth - rightWidth);

                if (difference < bestDifference)
                {
                    bestDifference = difference;
                    bestSplit =
                        static_cast<int16_t>(
                            index
                        );
                }
            }
        }

        if (bestSplit >= 0)
        {
            snprintf(
                firstLine,
                sizeof(firstLine),
                "%.*s",
                bestSplit,
                message
            );

            snprintf(
                secondLine,
                sizeof(secondLine),
                "%s",
                message + bestSplit + 1
            );
        }
        else
        {
            snprintf(
                firstLine,
                sizeof(firstLine),
                "%s",
                message
            );

            while (
                strlen(firstLine) > 3 &&
                tft.textWidth(
                    firstLine,
                    1
                ) > maximumTextWidth
            )
            {
                firstLine[
                    strlen(firstLine) - 1
                ] = '\0';
            }

            if (strlen(firstLine) > 3)
            {
                const size_t length =
                    strlen(firstLine);

                firstLine[length - 3] = '.';
                firstLine[length - 2] = '.';
                firstLine[length - 1] = '.';
            }
        }

        tft.setTextDatum(MC_DATUM);

        tft.drawString(
            firstLine,
            STATUS_MESSAGE_RECT.x +
                STATUS_MESSAGE_RECT.width / 2,
            STATUS_MESSAGE_RECT.y + 11,
            1
        );

        if (secondLine[0] != '\0')
        {
            tft.drawString(
                secondLine,
                STATUS_MESSAGE_RECT.x +
                    STATUS_MESSAGE_RECT.width / 2,
                STATUS_MESSAGE_RECT.y + 28,
                1
            );
        }
    }

    drawWifiShortcutIcon();
    drawArchiveShortcutIcon();
    drawGraphShortcutIcon();
    drawSettingsGearIcon();

    snprintf(
        cachedMainMessageSignature,
        sizeof(cachedMainMessageSignature),
        "%s",
        signature
    );

    mainMessageCacheValid = true;
}

// ============================================================
// HLAVNA OBRAZOVKA - RIADENIE RELACIE
// ============================================================

void drawSessionButtons()
{
    drawButton(
        SESSION_TOGGLE_BUTTON_RECT,
        sessionActive
            ? "UKONCIT RELACIU"
            : "SPUSTIT RELACIU",
        sessionActive
            ? COLOR_PANEL
            : COLOR_PRIMARY,
        sessionActive
            ? COLOR_WARNING
            : COLOR_PRIMARY,
        sessionActive
            ? COLOR_TEXT
            : TFT_BLACK,
        2
    );
}

// ============================================================
// HLAVNA OBRAZOVKA - TLACIDLO ULOZENIA
// ============================================================

void drawSaveButton()
{
    uint16_t fillColor = COLOR_BACKGROUND;
    uint16_t borderColor = COLOR_DISABLED;
    const char *buttonText = "ULOZENIE NEDOSTUPNE";

    if (saveButtonState == SaveButtonState::Success)
    {
        fillColor = COLOR_OK;
        borderColor = COLOR_OK;
        buttonText = "MERANIE ULOZENE";
    }
    else if (saveButtonState == SaveButtonState::Error)
    {
        fillColor = COLOR_ERROR;
        borderColor = COLOR_ERROR;
        buttonText = "CHYBA ZAPISU";
    }
    else if (manualSaveAvailable())
    {
        fillColor = COLOR_PANEL;
        borderColor = COLOR_PRIMARY;
        buttonText = "ULOZIT MERANIE";
    }
    else if (!sessionActive)
    {
        buttonText = "NAJPRV STANOVISKO";
    }
    else if (identifierIsEmpty(targetPoint))
    {
        buttonText = "NAJPRV MERANY BOD";
    }

    if (
        mainSaveButtonCacheValid &&
        strcmp(
            cachedSaveButtonText,
            buttonText
        ) == 0 &&
        cachedSaveFillColor == fillColor &&
        cachedSaveBorderColor == borderColor
    )
    {
        return;
    }

    drawButton(
        SAVE_BUTTON_RECT,
        buttonText,
        fillColor,
        borderColor,
        COLOR_TEXT,
        2
    );

    snprintf(
        cachedSaveButtonText,
        sizeof(cachedSaveButtonText),
        "%s",
        buttonText
    );

    cachedSaveFillColor = fillColor;
    cachedSaveBorderColor = borderColor;
    mainSaveButtonCacheValid = true;
}

void drawMainScreen()
{
    currentScreen = Screen::Main;

    drawMainStaticLayout();
    updateDateTimeDisplay(currentMeasurement.timestamp);
    updateMeasurementDisplay(currentMeasurement);
    updateEditableFields();
    drawSessionButtons();
    drawSaveButton();
    updateStatusPanel();
    updateMessageStrip();
}

void updateMainScreen()
{
    if (currentScreen != Screen::Main)
    {
        return;
    }

    // Prekresluju sa iba dynamicke oblasti, nie cela obrazovka.
    updateDateTimeDisplay(
        currentMeasurement.timestamp
    );

    updateMeasurementDisplay(
        currentMeasurement
    );

    drawSaveButton();
    updateStatusPanel();
    updateMessageStrip();
}


// ============================================================
// GRAFY RELACIE - POMOCNE FUNKCIE
// ============================================================

void resetGraphStatistics(GraphStatistics &statistics)
{
    statistics.minimum = INFINITY;
    statistics.maximum = -INFINITY;
    statistics.sum = 0.0;
    statistics.count = 0;
}

void addGraphStatistic(
    GraphStatistics &statistics,
    float value
)
{
    if (!isfinite(value))
    {
        return;
    }

    if (value < statistics.minimum)
    {
        statistics.minimum = value;
    }

    if (value > statistics.maximum)
    {
        statistics.maximum = value;
    }

    statistics.sum += value;
    statistics.count++;
}

float graphAverage(const GraphStatistics &statistics)
{
    if (statistics.count == 0)
    {
        return NAN;
    }

    return static_cast<float>(
        statistics.sum /
        static_cast<double>(statistics.count)
    );
}

void resetAllGraphStatistics()
{
    resetGraphStatistics(
        graphDisplayTemperatureStats
    );

    resetGraphStatistics(
        graphDisplayHumidityStats
    );

    resetGraphStatistics(
        graphDisplayPressureStats
    );

    resetGraphStatistics(
        graphTotalTemperatureStats
    );

    resetGraphStatistics(
        graphTotalHumidityStats
    );

    resetGraphStatistics(
        graphTotalPressureStats
    );
}

void addSampleToStatistics(
    const GraphSample &sample,
    GraphStatistics &temperatureStats,
    GraphStatistics &humidityStats,
    GraphStatistics &pressureStats
)
{
    addGraphStatistic(
        temperatureStats,
        sample.temperatureC
    );

    addGraphStatistic(
        humidityStats,
        sample.humidityPercent
    );

    addGraphStatistic(
        pressureStats,
        sample.pressureHpa
    );
}

bool readCsvLine(
    File &file,
    char *buffer,
    size_t bufferSize
)
{
    if (
        !file.available() ||
        bufferSize < 2
    )
    {
        return false;
    }

    size_t length = 0;

    while (file.available())
    {
        const int value = file.read();

        if (value < 0)
        {
            break;
        }

        const char character =
            static_cast<char>(value);

        if (character == '\n')
        {
            break;
        }

        if (character == '\r')
        {
            continue;
        }

        if (length + 1 < bufferSize)
        {
            buffer[length++] = character;
        }
    }

    buffer[length] = '\0';

    return true;
}

size_t splitCsvFields(
    char *line,
    char **fields,
    size_t maximumFields
)
{
    if (
        line == nullptr ||
        fields == nullptr ||
        maximumFields == 0
    )
    {
        return 0;
    }

    char *source = line;
    char *destination = line;

    size_t fieldCount = 1;
    fields[0] = destination;

    bool insideQuotes = false;

    while (*source != '\0')
    {
        const char character = *source++;

        if (insideQuotes)
        {
            if (character == '"')
            {
                if (*source == '"')
                {
                    *destination++ = '"';
                    source++;
                }
                else
                {
                    insideQuotes = false;
                }
            }
            else
            {
                *destination++ = character;
            }

            continue;
        }

        if (
            character == '"' &&
            destination == fields[fieldCount - 1]
        )
        {
            insideQuotes = true;
        }
        else if (character == ',')
        {
            *destination++ = '\0';

            if (fieldCount < maximumFields)
            {
                fields[fieldCount++] = destination;
            }
        }
        else
        {
            *destination++ = character;
        }
    }

    *destination = '\0';

    return fieldCount;
}

bool parseIsoTimestamp(
    const char *timestampText,
    uint32_t &unixTime
)
{
    unsigned int year = 0;
    unsigned int month = 0;
    unsigned int day = 0;
    unsigned int hour = 0;
    unsigned int minute = 0;
    unsigned int second = 0;

    const int parsed = sscanf(
        timestampText,
        "%4u-%2u-%2uT%2u:%2u:%2u",
        &year,
        &month,
        &day,
        &hour,
        &minute,
        &second
    );

    if (parsed != 6)
    {
        return false;
    }

    if (
        year < MIN_VALID_RTC_YEAR ||
        year > MAX_VALID_RTC_YEAR ||
        month < 1 ||
        month > 12 ||
        day < 1 ||
        day > daysInMonth(year, month) ||
        hour > 23 ||
        minute > 59 ||
        second > 59
    )
    {
        return false;
    }

    unixTime = DateTime(
        year,
        month,
        day,
        hour,
        minute,
        second
    ).unixtime();

    return true;
}

bool parseGraphCsvRecord(
    char *line,
    GraphSample &sample,
    bool &isAutoRecord,
    bool &isManualRecord,
    bool &hasTimestamp
)
{
    isAutoRecord = false;
    isManualRecord = false;
    hasTimestamp = false;

    char *fields[12];

    const size_t fieldCount =
        splitCsvFields(
            line,
            fields,
            12
        );

    if (fieldCount < 8)
    {
        return false;
    }

    hasTimestamp = parseIsoTimestamp(
        fields[1],
        sample.unixTime
    );

    isAutoRecord =
        strcmp(fields[2], "AUTO") == 0;

    isManualRecord =
        strcmp(fields[2], "MANUAL") == 0;

    if (!isAutoRecord)
    {
        return true;
    }

    char *endPointer = nullptr;

    sample.temperatureC = strtof(
        fields[5],
        &endPointer
    );

    if (
        endPointer == fields[5] ||
        !isfinite(sample.temperatureC)
    )
    {
        return false;
    }

    sample.humidityPercent = strtof(
        fields[6],
        &endPointer
    );

    if (
        endPointer == fields[6] ||
        !isfinite(sample.humidityPercent)
    )
    {
        return false;
    }

    sample.pressureHpa = strtof(
        fields[7],
        &endPointer
    );

    if (
        endPointer == fields[7] ||
        !isfinite(sample.pressureHpa)
    )
    {
        return false;
    }

    return hasTimestamp;
}

void calculateDisplayedGraphStatistics()
{
    resetGraphStatistics(
        graphDisplayTemperatureStats
    );

    resetGraphStatistics(
        graphDisplayHumidityStats
    );

    resetGraphStatistics(
        graphDisplayPressureStats
    );

    for (
        size_t index = 0;
        index < graphPointCount;
        index++
    )
    {
        addSampleToStatistics(
            graphSamples[index],
            graphDisplayTemperatureStats,
            graphDisplayHumidityStats,
            graphDisplayPressureStats
        );
    }
}

const char *currentGraphFilePath()
{
    if (graphViewingArchive)
    {
        return archiveDetailData.filePath;
    }

    return sessionFilePath;
}

const char *currentGraphStation()
{
    if (graphViewingArchive)
    {
        return archiveDetailData.station;
    }

    return station;
}


bool scanGraphSessionFile(
    GraphRange requestedRange
)
{
    graphPointCount = 0;
    graphTotalAutoRecords = 0;
    graphTotalManualRecords = 0;
    graphSessionStartUnix = 0;
    graphSessionLastUnix = 0;
    graphDataReady = false;
    graphErrorMessage[0] = '\0';

    resetAllGraphStatistics();

    const char *sourceFilePath =
        currentGraphFilePath();

    if (
        sourceFilePath == nullptr ||
        sourceFilePath[0] == '\0' ||
        (
            !graphViewingArchive &&
            !sessionActive
        )
    )
    {
        snprintf(
            graphErrorMessage,
            sizeof(graphErrorMessage),
            "NIE JE VYBRANA RELACIA"
        );

        return false;
    }

    if (!ensureSDReady(false))
    {
        snprintf(
            graphErrorMessage,
            sizeof(graphErrorMessage),
            "SD KARTA NIE JE DOSTUPNA"
        );

        return false;
    }

    File file = SD.open(
        sourceFilePath,
        FILE_READ
    );

    if (!file)
    {
        snprintf(
            graphErrorMessage,
            sizeof(graphErrorMessage),
            "CSV SUBOR SA NEDA OTVORIT"
        );

        return false;
    }

    char line[GRAPH_CSV_LINE_LENGTH];
    size_t recentCount = 0;
    size_t recentWriteIndex = 0;

    while (
        readCsvLine(
            file,
            line,
            sizeof(line)
        )
    )
    {
        GraphSample sample{};
        bool isAutoRecord = false;
        bool isManualRecord = false;
        bool hasTimestamp = false;

        const bool parsed =
            parseGraphCsvRecord(
                line,
                sample,
                isAutoRecord,
                isManualRecord,
                hasTimestamp
            );

        if (
            hasTimestamp &&
            graphSessionStartUnix == 0
        )
        {
            graphSessionStartUnix =
                sample.unixTime;
        }

        if (hasTimestamp)
        {
            graphSessionLastUnix =
                sample.unixTime;
        }

        if (isManualRecord)
        {
            graphTotalManualRecords++;
        }

        if (
            !parsed ||
            !isAutoRecord
        )
        {
            continue;
        }

        graphTotalAutoRecords++;

        addSampleToStatistics(
            sample,
            graphTotalTemperatureStats,
            graphTotalHumidityStats,
            graphTotalPressureStats
        );

        if (
            requestedRange ==
            GraphRange::Last120
        )
        {
            graphRecentBuffer[
                recentWriteIndex
            ] = sample;

            recentWriteIndex =
                (
                    recentWriteIndex + 1
                ) %
                GRAPH_LAST_RECORD_COUNT;

            if (
                recentCount <
                GRAPH_LAST_RECORD_COUNT
            )
            {
                recentCount++;
            }
        }
    }

    file.close();

    if (graphTotalAutoRecords == 0)
    {
        snprintf(
            graphErrorMessage,
            sizeof(graphErrorMessage),
            "RELACIA ESTE NEMA AUTO DATA"
        );

        return false;
    }

    if (
        requestedRange ==
        GraphRange::Last120
    )
    {
        graphPointCount = recentCount;

        const size_t oldestIndex =
            recentCount <
            GRAPH_LAST_RECORD_COUNT
                ? 0
                : recentWriteIndex;

        for (
            size_t index = 0;
            index < recentCount;
            index++
        )
        {
            graphSamples[index] =
                graphRecentBuffer[
                    (
                        oldestIndex + index
                    ) %
                    GRAPH_LAST_RECORD_COUNT
                ];
        }

        calculateDisplayedGraphStatistics();
        graphDataReady = graphPointCount > 0;

        return graphDataReady;
    }

    // Druhy prechod pre volbu VSETKY. Data sa priebezne
    // priemeruju do najviac 240 zobrazovanych bodov.
    file = SD.open(
        sourceFilePath,
        FILE_READ
    );

    if (!file)
    {
        snprintf(
            graphErrorMessage,
            sizeof(graphErrorMessage),
            "CSV SUBOR SA NEDA OTVORIT"
        );

        return false;
    }

    const uint32_t bucketSize =
        (
            graphTotalAutoRecords +
            GRAPH_MAX_POINT_COUNT -
            1
        ) /
        GRAPH_MAX_POINT_COUNT;

    uint32_t bucketCount = 0;
    double temperatureSum = 0.0;
    double humiditySum = 0.0;
    double pressureSum = 0.0;
    uint32_t bucketUnixTime = 0;

    while (
        readCsvLine(
            file,
            line,
            sizeof(line)
        )
    )
    {
        GraphSample sample{};
        bool isAutoRecord = false;
        bool isManualRecord = false;
        bool hasTimestamp = false;

        if (
            !parseGraphCsvRecord(
                line,
                sample,
                isAutoRecord,
                isManualRecord,
                hasTimestamp
            ) ||
            !isAutoRecord
        )
        {
            continue;
        }

        if (bucketCount == 0)
        {
            bucketUnixTime =
                sample.unixTime;
        }

        temperatureSum +=
            sample.temperatureC;

        humiditySum +=
            sample.humidityPercent;

        pressureSum +=
            sample.pressureHpa;

        bucketCount++;

        if (
            bucketCount >= bucketSize &&
            graphPointCount <
            GRAPH_MAX_POINT_COUNT
        )
        {
            graphSamples[
                graphPointCount++
            ] = GraphSample{
                bucketUnixTime,
                static_cast<float>(
                    temperatureSum /
                    bucketCount
                ),
                static_cast<float>(
                    humiditySum /
                    bucketCount
                ),
                static_cast<float>(
                    pressureSum /
                    bucketCount
                )
            };

            bucketCount = 0;
            temperatureSum = 0.0;
            humiditySum = 0.0;
            pressureSum = 0.0;
        }
    }

    file.close();

    if (
        bucketCount > 0 &&
        graphPointCount <
        GRAPH_MAX_POINT_COUNT
    )
    {
        graphSamples[
            graphPointCount++
        ] = GraphSample{
            bucketUnixTime,
            static_cast<float>(
                temperatureSum /
                bucketCount
            ),
            static_cast<float>(
                humiditySum /
                bucketCount
            ),
            static_cast<float>(
                pressureSum /
                bucketCount
            )
        };
    }

    // Statistiky VSETKY zostavaju presne a nevychadzaju
    // zo zmenseneho grafickeho vyberu.
    graphDisplayTemperatureStats =
        graphTotalTemperatureStats;

    graphDisplayHumidityStats =
        graphTotalHumidityStats;

    graphDisplayPressureStats =
        graphTotalPressureStats;

    graphDataReady = graphPointCount > 0;

    return graphDataReady;
}

// ============================================================
// GRAFY - PRIEBEZNA AKTUALIZACIA
// ============================================================

GraphSample graphSampleFromMeasurement(
    const Measurement &measurement
)
{
    return GraphSample{
        measurement.timestamp.unixtime(),
        measurement.temperatureC,
        measurement.humidityPercent,
        measurement.pressureHpa
    };
}

void appendGraphSampleToLast120(
    const GraphSample &sample
)
{
    if (
        graphPointCount <
        GRAPH_LAST_RECORD_COUNT
    )
    {
        graphSamples[
            graphPointCount++
        ] = sample;
    }
    else
    {
        for (
            size_t index = 1;
            index < GRAPH_LAST_RECORD_COUNT;
            index++
        )
        {
            graphSamples[
                index - 1
            ] = graphSamples[index];
        }

        graphSamples[
            GRAPH_LAST_RECORD_COUNT - 1
        ] = sample;

        graphPointCount =
            GRAPH_LAST_RECORD_COUNT;
    }

    calculateDisplayedGraphStatistics();
    graphDataReady = graphPointCount > 0;
}

void registerSuccessfulAutoRecordForGraph(
    const Measurement &measurement
)
{
    if (
        currentScreen != Screen::Graphs ||
        graphViewingArchive ||
        !measurement.valid
    )
    {
        return;
    }

    const GraphSample sample =
        graphSampleFromMeasurement(
            measurement
        );

    graphTotalAutoRecords++;

    if (graphSessionStartUnix == 0)
    {
        graphSessionStartUnix =
            sample.unixTime;
    }

    graphSessionLastUnix =
        sample.unixTime;

    addSampleToStatistics(
        sample,
        graphTotalTemperatureStats,
        graphTotalHumidityStats,
        graphTotalPressureStats
    );

    graphErrorMessage[0] = '\0';

    if (
        graphRange ==
        GraphRange::Last120
    )
    {
        appendGraphSampleToLast120(
            sample
        );

        drawGraphSummary(false);
        drawGraphPlot();

        lastGraphDataRefreshMillis =
            millis();

        lastGraphSummaryRefreshMillis =
            lastGraphDataRefreshMillis;

        return;
    }

    // Kym ma cela relacia najviac 240 AUTO zaznamov,
    // je mozne novy bod pridat priamo bez citania CSV.
    if (
        graphTotalAutoRecords <=
            GRAPH_MAX_POINT_COUNT &&
        graphPointCount <
            GRAPH_MAX_POINT_COUNT
    )
    {
        graphSamples[
            graphPointCount++
        ] = sample;

        graphDisplayTemperatureStats =
            graphTotalTemperatureStats;

        graphDisplayHumidityStats =
            graphTotalHumidityStats;

        graphDisplayPressureStats =
            graphTotalPressureStats;

        graphDataReady = true;

        drawGraphSummary(false);
        drawGraphPlot();

        lastGraphDataRefreshMillis =
            millis();

        lastGraphSummaryRefreshMillis =
            lastGraphDataRefreshMillis;

        return;
    }

    // Pri vacsej relacii je graf VSETKY zmenseny do intervalov.
    // Po novom zapise preto oznacime data na bezpecne obnovenie.
    graphDisplayTemperatureStats =
        graphTotalTemperatureStats;

    graphDisplayHumidityStats =
        graphTotalHumidityStats;

    graphDisplayPressureStats =
        graphTotalPressureStats;

    graphFullRangeRefreshPending = true;

    // Pocitadlo a trvanie relacie sa aktualizuju okamzite.
    drawGraphSummary(false);
    lastGraphSummaryRefreshMillis =
        millis();
}

void updateLiveGraphScreen(
    uint32_t currentMillis
)
{
    if (
        currentScreen != Screen::Graphs ||
        graphViewingArchive
    )
    {
        return;
    }

    if (
        currentMillis -
            lastGraphSummaryRefreshMillis >=
        GRAPH_SUMMARY_REFRESH_INTERVAL_MS
    )
    {
        lastGraphSummaryRefreshMillis =
            currentMillis;

        drawGraphSummary(false);
    }

    if (
        graphRange != GraphRange::All ||
        !graphFullRangeRefreshPending
    )
    {
        return;
    }

    if (
        currentMillis -
            lastGraphDataRefreshMillis <
        GRAPH_ALL_REFRESH_MIN_INTERVAL_MS
    )
    {
        return;
    }

    scanGraphSessionFile(
        GraphRange::All
    );

    graphFullRangeRefreshPending = false;
    lastGraphDataRefreshMillis =
        currentMillis;
    lastGraphSummaryRefreshMillis =
        currentMillis;

    drawGraphSummary(false);
    drawGraphPlot();
}

float graphSampleValue(
    const GraphSample &sample,
    GraphParameter parameter
)
{
    switch (parameter)
    {
        case GraphParameter::Humidity:
            return sample.humidityPercent;

        case GraphParameter::Pressure:
            return sample.pressureHpa;

        case GraphParameter::Temperature:
        default:
            return sample.temperatureC;
    }
}

const GraphStatistics &selectedGraphStatistics()
{
    switch (graphParameter)
    {
        case GraphParameter::Humidity:
            return graphDisplayHumidityStats;

        case GraphParameter::Pressure:
            return graphDisplayPressureStats;

        case GraphParameter::Temperature:
        default:
            return graphDisplayTemperatureStats;
    }
}

const char *graphParameterTitle()
{
    switch (graphParameter)
    {
        case GraphParameter::Humidity:
            return "VLHKOST (%)";

        case GraphParameter::Pressure:
            return "TLAK (hPa)";

        case GraphParameter::Temperature:
        default:
            return "TEPLOTA (C)";
    }
}

uint16_t graphLineColor()
{
    switch (graphParameter)
    {
        case GraphParameter::Humidity:
            return TFT_CYAN;

        case GraphParameter::Pressure:
            return COLOR_OK;

        case GraphParameter::Temperature:
        default:
            return COLOR_PRIMARY;
    }
}

void formatGraphDuration(
    uint32_t durationSeconds,
    char *buffer,
    size_t bufferSize
)
{
    const uint32_t hours =
        durationSeconds / 3600UL;

    const uint32_t minutes =
        (
            durationSeconds % 3600UL
        ) /
        60UL;

    const uint32_t seconds =
        durationSeconds % 60UL;

    if (hours > 99)
    {
        snprintf(
            buffer,
            bufferSize,
            "%luh %02lum",
            static_cast<unsigned long>(hours),
            static_cast<unsigned long>(minutes)
        );
    }
    else
    {
        snprintf(
            buffer,
            bufferSize,
            "%02lu:%02lu:%02lu",
            static_cast<unsigned long>(hours),
            static_cast<unsigned long>(minutes),
            static_cast<unsigned long>(seconds)
        );
    }
}

void formatGraphTime(
    uint32_t unixTime,
    char *buffer,
    size_t bufferSize
)
{
    if (unixTime == 0)
    {
        snprintf(
            buffer,
            bufferSize,
            "--:--"
        );

        return;
    }

    const DateTime time(unixTime);

    snprintf(
        buffer,
        bufferSize,
        "%02u:%02u",
        time.hour(),
        time.minute()
    );
}

void drawGraphLoadingScreen()
{
    currentScreen = Screen::Graphs;

    tft.fillScreen(COLOR_BACKGROUND);
    tft.fillRect(0, 0, 320, 40, COLOR_PANEL);
    tft.drawFastHLine(
        0,
        39,
        320,
        COLOR_PANEL_BORDER
    );

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
    tft.drawString(
        "GRAFY RELACIE",
        160,
        20,
        2
    );

    tft.setTextColor(
        COLOR_PRIMARY,
        COLOR_BACKGROUND
    );

    tft.drawString(
        "NACITAVAM CSV...",
        160,
        230,
        2
    );
}

void drawGraphParameterButtons()
{
    drawButton(
        GRAPH_TEMPERATURE_RECT,
        "TEPLOTA",
        graphParameter ==
            GraphParameter::Temperature
                ? COLOR_PRIMARY
                : COLOR_PANEL,
        graphParameter ==
            GraphParameter::Temperature
                ? COLOR_PRIMARY
                : COLOR_PANEL_BORDER,
        graphParameter ==
            GraphParameter::Temperature
                ? TFT_BLACK
                : COLOR_TEXT,
        1
    );

    drawButton(
        GRAPH_HUMIDITY_RECT,
        "VLHKOST",
        graphParameter ==
            GraphParameter::Humidity
                ? COLOR_PRIMARY
                : COLOR_PANEL,
        graphParameter ==
            GraphParameter::Humidity
                ? COLOR_PRIMARY
                : COLOR_PANEL_BORDER,
        graphParameter ==
            GraphParameter::Humidity
                ? TFT_BLACK
                : COLOR_TEXT,
        1
    );

    drawButton(
        GRAPH_PRESSURE_RECT,
        "TLAK",
        graphParameter ==
            GraphParameter::Pressure
                ? COLOR_PRIMARY
                : COLOR_PANEL,
        graphParameter ==
            GraphParameter::Pressure
                ? COLOR_PRIMARY
                : COLOR_PANEL_BORDER,
        graphParameter ==
            GraphParameter::Pressure
                ? TFT_BLACK
                : COLOR_TEXT,
        1
    );
}

void drawGraphSummary(
    bool redrawPanel
)
{
    if (redrawPanel)
    {
        drawPanel(GRAPH_SUMMARY_RECT);
    }
    else
    {
        tft.fillRect(
            GRAPH_SUMMARY_RECT.x + 2,
            GRAPH_SUMMARY_RECT.y + 2,
            GRAPH_SUMMARY_RECT.width - 4,
            GRAPH_SUMMARY_RECT.height - 4,
            COLOR_PANEL
        );
    }

    char firstLine[48];
    char secondLine[56];
    char durationText[20];

    uint32_t durationSeconds = 0;

    if (
        graphSessionStartUnix > 0 &&
        graphSessionLastUnix >=
            graphSessionStartUnix
    )
    {
        uint32_t endUnix =
            graphSessionLastUnix;

        if (
            !graphViewingArchive &&
            sessionActive &&
            rtcReady
        )
        {
            const uint32_t currentUnix =
                rtc.now().unixtime();

            if (currentUnix > endUnix)
            {
                endUnix = currentUnix;
            }
        }

        durationSeconds =
            endUnix -
            graphSessionStartUnix;
    }

    formatGraphDuration(
        durationSeconds,
        durationText,
        sizeof(durationText)
    );

    snprintf(
        firstLine,
        sizeof(firstLine),
        "ST: %s",
        currentGraphStation()
    );

    snprintf(
        secondLine,
        sizeof(secondLine),
        "AUTO:%lu MANUAL:%lu  %s",
        static_cast<unsigned long>(
            graphTotalAutoRecords
        ),
        static_cast<unsigned long>(
            graphTotalManualRecords
        ),
        durationText
    );

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);

    tft.drawString(
        firstLine,
        20,
        113,
        2
    );

    tft.setTextColor(
        COLOR_SECONDARY_TEXT,
        COLOR_PANEL
    );

    tft.drawString(
        secondLine,
        20,
        138,
        1
    );
}

void drawGraphPlot()
{
    drawPanel(GRAPH_PANEL_RECT);

    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(
        graphLineColor(),
        COLOR_PANEL
    );

    tft.drawString(
        graphParameterTitle(),
        160,
        169,
        1
    );

    if (
        !graphDataReady ||
        graphPointCount == 0
    )
    {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(
            COLOR_ERROR,
            COLOR_PANEL
        );

        tft.drawString(
            graphErrorMessage[0] == '\0'
                ? "GRAF NIE JE DOSTUPNY"
                : graphErrorMessage,
            160,
            280,
            2
        );

        return;
    }

    const GraphStatistics &statistics =
        selectedGraphStatistics();

    float minimum = statistics.minimum;
    float maximum = statistics.maximum;

    if (
        !isfinite(minimum) ||
        !isfinite(maximum)
    )
    {
        return;
    }

    float range = maximum - minimum;

    if (range < 0.001F)
    {
        float margin = 0.5F;

        if (
            graphParameter ==
            GraphParameter::Pressure
        )
        {
            margin = 0.1F;
        }

        minimum -= margin;
        maximum += margin;
        range = maximum - minimum;
    }
    else
    {
        const float margin =
            range * 0.08F;

        minimum -= margin;
        maximum += margin;
        range = maximum - minimum;
    }

    constexpr int16_t plotLeft = 44;
    constexpr int16_t plotTop = 205;
    constexpr int16_t plotWidth = 252;
    constexpr int16_t plotHeight = 154;
    constexpr int16_t plotBottom =
        plotTop + plotHeight;

    const uint16_t gridColor = 0x39E7;

    for (uint8_t lineIndex = 0;
         lineIndex <= 4;
         lineIndex++)
    {
        const int16_t y =
            plotTop +
            (
                plotHeight *
                lineIndex
            ) /
            4;

        tft.drawFastHLine(
            plotLeft,
            y,
            plotWidth,
            gridColor
        );
    }

    tft.drawFastVLine(
        plotLeft,
        plotTop,
        plotHeight,
        COLOR_SECONDARY_TEXT
    );

    tft.drawFastHLine(
        plotLeft,
        plotBottom,
        plotWidth,
        COLOR_SECONDARY_TEXT
    );

    char maximumText[16];
    char minimumText[16];

    const uint8_t decimals =
        graphParameter ==
            GraphParameter::Pressure
                ? 1
                : 1;

    snprintf(
        maximumText,
        sizeof(maximumText),
        "%.*f",
        decimals,
        statistics.maximum
    );

    snprintf(
        minimumText,
        sizeof(minimumText),
        "%.*f",
        decimals,
        statistics.minimum
    );

    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(
        COLOR_SECONDARY_TEXT,
        COLOR_PANEL
    );

    tft.drawString(
        maximumText,
        plotLeft - 4,
        plotTop,
        1
    );

    tft.drawString(
        minimumText,
        plotLeft - 4,
        plotBottom,
        1
    );

    int16_t previousX = plotLeft;
    int16_t previousY = plotBottom;

    for (
        size_t index = 0;
        index < graphPointCount;
        index++
    )
    {
        const float value =
            graphSampleValue(
                graphSamples[index],
                graphParameter
            );

        const int16_t x =
            graphPointCount <= 1
                ? plotLeft
                : plotLeft +
                    static_cast<int16_t>(
                        (
                            static_cast<uint32_t>(
                                plotWidth - 1
                            ) *
                            index
                        ) /
                        (
                            graphPointCount - 1
                        )
                    );

        float normalized =
            (value - minimum) /
            range;

        if (normalized < 0.0F)
        {
            normalized = 0.0F;
        }

        if (normalized > 1.0F)
        {
            normalized = 1.0F;
        }

        const int16_t y =
            plotBottom -
            static_cast<int16_t>(
                normalized *
                plotHeight
            );

        if (index > 0)
        {
            tft.drawLine(
                previousX,
                previousY,
                x,
                y,
                graphLineColor()
            );
        }

        previousX = x;
        previousY = y;
    }

    char firstTime[12];
    char lastTime[12];

    formatGraphTime(
        graphSamples[0].unixTime,
        firstTime,
        sizeof(firstTime)
    );

    formatGraphTime(
        graphSamples[
            graphPointCount - 1
        ].unixTime,
        lastTime,
        sizeof(lastTime)
    );

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(
        COLOR_SECONDARY_TEXT,
        COLOR_PANEL
    );

    tft.drawString(
        firstTime,
        plotLeft,
        plotBottom + 8,
        1
    );

    tft.setTextDatum(TR_DATUM);

    tft.drawString(
        lastTime,
        plotLeft + plotWidth,
        plotBottom + 8,
        1
    );

    char statisticsText[64];

    snprintf(
        statisticsText,
        sizeof(statisticsText),
        "MIN %.2f  PRIEM %.2f  MAX %.2f",
        statistics.minimum,
        graphAverage(statistics),
        statistics.maximum
    );

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(
        COLOR_TEXT,
        COLOR_PANEL
    );

    tft.drawString(
        statisticsText,
        160,
        388,
        1
    );
}

void drawGraphFooter()
{
    drawButton(
        GRAPH_BACK_RECT,
        "SPAT",
        COLOR_PANEL,
        COLOR_WARNING,
        COLOR_TEXT,
        2
    );

    drawButton(
        GRAPH_LAST120_RECT,
        "120",
        graphRange ==
            GraphRange::Last120
                ? COLOR_PRIMARY
                : COLOR_PANEL,
        graphRange ==
            GraphRange::Last120
                ? COLOR_PRIMARY
                : COLOR_PANEL_BORDER,
        graphRange ==
            GraphRange::Last120
                ? TFT_BLACK
                : COLOR_TEXT,
        2
    );

    drawButton(
        GRAPH_ALL_RECT,
        "VSETKY",
        graphRange ==
            GraphRange::All
                ? COLOR_PRIMARY
                : COLOR_PANEL,
        graphRange ==
            GraphRange::All
                ? COLOR_PRIMARY
                : COLOR_PANEL_BORDER,
        graphRange ==
            GraphRange::All
                ? TFT_BLACK
                : COLOR_TEXT,
        1
    );
}

void drawGraphScreen()
{
    currentScreen = Screen::Graphs;

    tft.fillScreen(COLOR_BACKGROUND);
    tft.fillRect(0, 0, 320, 40, COLOR_PANEL);
    tft.drawFastHLine(
        0,
        39,
        320,
        COLOR_PANEL_BORDER
    );

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);

    tft.drawString(
        graphViewingArchive
            ? "GRAFY ARCHIVU"
            : "GRAFY RELACIE",
        160,
        20,
        2
    );

    drawGraphParameterButtons();
    drawGraphSummary(true);
    drawGraphPlot();
    drawGraphFooter();
}

void reloadGraphScreen()
{
    drawGraphLoadingScreen();

    scanGraphSessionFile(
        graphRange
    );

    graphFullRangeRefreshPending = false;
    lastGraphDataRefreshMillis = millis();
    lastGraphSummaryRefreshMillis =
        lastGraphDataRefreshMillis;

    drawGraphScreen();
}

void openGraphs()
{
    if (!sessionActive)
    {
        setStatusMessage(
            "GRAFY SU DOSTUPNE LEN POCAS RELACIE",
            COLOR_WARNING
        );

        updateMessageStrip();
        return;
    }

    graphViewingArchive = false;

    graphRange =
        GraphRange::Last120;

    graphParameter =
        GraphParameter::Temperature;

    reloadGraphScreen();
}

void processGraphTouch(
    int16_t x,
    int16_t y
)
{
    if (GRAPH_BACK_RECT.contains(x, y))
    {
        if (graphViewingArchive)
        {
            graphViewingArchive = false;
            drawArchiveDetailScreen();
        }
        else
        {
            drawMainScreen();
        }

        return;
    }

    if (
        GRAPH_TEMPERATURE_RECT.contains(
            x,
            y
        )
    )
    {
        graphParameter =
            GraphParameter::Temperature;

        drawGraphScreen();
        return;
    }

    if (
        GRAPH_HUMIDITY_RECT.contains(
            x,
            y
        )
    )
    {
        graphParameter =
            GraphParameter::Humidity;

        drawGraphScreen();
        return;
    }

    if (
        GRAPH_PRESSURE_RECT.contains(
            x,
            y
        )
    )
    {
        graphParameter =
            GraphParameter::Pressure;

        drawGraphScreen();
        return;
    }

    if (
        GRAPH_LAST120_RECT.contains(
            x,
            y
        )
    )
    {
        graphRange =
            GraphRange::Last120;

        reloadGraphScreen();
        return;
    }

    if (
        GRAPH_ALL_RECT.contains(
            x,
            y
        )
    )
    {
        graphRange =
            GraphRange::All;

        reloadGraphScreen();
    }
}


// ============================================================
// WI-FI EXPORT - POMOCNE FUNKCIE
// ============================================================

void buildWifiExportSsid()
{
    const uint64_t chipId =
        ESP.getEfuseMac();

    const uint16_t suffix =
        static_cast<uint16_t>(
            chipId & 0xFFFFU
        );

    snprintf(
        wifiExportSsid,
        sizeof(wifiExportSsid),
        "GeoMeteo-%04X",
        suffix
    );
}

const char *pathFileName(
    const char *path
)
{
    if (path == nullptr)
    {
        return "session.csv";
    }

    const char *lastSlash =
        strrchr(path, '/');

    return
        lastSlash != nullptr
            ? lastSlash + 1
            : path;
}

String htmlEscape(
    const char *text
)
{
    String escaped;

    if (text == nullptr)
    {
        return escaped;
    }

    escaped.reserve(
        strlen(text) + 8
    );

    for (
        size_t index = 0;
        text[index] != '\0';
        index++
    )
    {
        switch (text[index])
        {
            case '&':
                escaped += F("&amp;");
                break;

            case '<':
                escaped += F("&lt;");
                break;

            case '>':
                escaped += F("&gt;");
                break;

            case '"':
                escaped += F("&quot;");
                break;

            case '\'':
                escaped += F("&#39;");
                break;

            default:
                escaped += text[index];
                break;
        }
    }

    return escaped;
}

// Explicit forward declaration required by the Wi-Fi export handler.
// The function implementation is located in the archive section below.
void formatArchiveDateTime(
    uint32_t unixTime,
    char *buffer,
    size_t bufferSize,
    bool includeSeconds = false
);

String formatWebFileSize(
    uint32_t bytes
)
{
    if (bytes < 1024U)
    {
        return
            String(bytes) +
            F(" B");
    }

    if (bytes < 1024UL * 1024UL)
    {
        return
            String(
                bytes / 1024.0f,
                1
            ) +
            F(" kB");
    }

    return
        String(
            bytes /
            (1024.0f * 1024.0f),
            2
        ) +
        F(" MB");
}

void markWifiExportActivity()
{
    wifiExportLastActivityMillis =
        millis();
}

void sendWifiExportPageHeader(
    const char *title
)
{
    wifiExportServer.sendContent(
        F(
            "<!doctype html><html lang='sk'><head>"
            "<meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<meta http-equiv='Cache-Control' content='no-store'>"
            "<style>"
            ":root{color-scheme:dark}"
            "body{margin:0;background:#080808;color:#f5f5f5;"
            "font-family:Arial,sans-serif}"
            "main{max-width:760px;margin:auto;padding:18px}"
            "h1{color:#ff8a00;margin:0 0 4px}"
            ".sub{color:#bbb;margin-bottom:18px}"
            ".info,.session{background:#1b1b1b;border:1px solid #6b4317;"
            "border-radius:12px;padding:14px;margin:10px 0}"
            ".session{display:grid;grid-template-columns:1fr auto;"
            "gap:10px;align-items:center}"
            ".meta{color:#bbb;font-size:.92rem;margin-top:5px}"
            "a.button{display:inline-block;background:#ff8a00;color:#111;"
            "text-decoration:none;font-weight:700;border-radius:9px;"
            "padding:11px 14px}"
            ".empty{padding:24px;text-align:center;color:#bbb}"
            ".footer{color:#888;font-size:.85rem;margin-top:20px}"
            "@media(max-width:520px){.session{grid-template-columns:1fr}}"
            "</style><title>"
        )
    );

    wifiExportServer.sendContent(
        htmlEscape(title)
    );

    wifiExportServer.sendContent(
        F("</title></head><body><main>")
    );
}

void sendWifiExportPageFooter()
{
    wifiExportServer.sendContent(
        F(
            "<div class='footer'>"
            "Wi-Fi export sa automaticky vypne po 5 minutach "
            "bez poziadavky z prehliadaca."
            "</div></main></body></html>"
        )
    );

    wifiExportServer.sendContent("");
}

void handleWifiExportRoot()
{
    markWifiExportActivity();

    loadArchiveSessions();

    wifiExportServer.sendHeader(
        "Cache-Control",
        "no-store"
    );

    wifiExportServer.setContentLength(
        CONTENT_LENGTH_UNKNOWN
    );

    wifiExportServer.send(
        200,
        "text/html; charset=utf-8",
        ""
    );

    sendWifiExportPageHeader(
        "GeoMeteo Logger"
    );

    wifiExportServer.sendContent(
        F("<h1>GeoMeteo Logger</h1><div class='sub'>")
    );

    wifiExportServer.sendContent(
        htmlEscape(APP_VERSION)
    );

    wifiExportServer.sendContent(
        F(" &middot; export ukoncenych relacii</div>")
    );

    String info =
        F("<div class='info'><strong>Siet:</strong> ");

    info += htmlEscape(wifiExportSsid);
    info += F("<br><strong>IP adresa:</strong> ");
    info += WiFi.softAPIP().toString();
    info += F("<br><strong>Ukoncene relacie:</strong> ");
    info += String(archiveSessionCount);
    info += F("<br><strong>Pripojene zariadenia:</strong> ");
    info += String(WiFi.softAPgetStationNum());
    info += F("</div>");

    wifiExportServer.sendContent(info);

    if (archiveSessionCount == 0)
    {
        wifiExportServer.sendContent(
            F(
                "<div class='empty'>"
                "Na SD karte nie je dostupna ziadna "
                "ukoncena relacia."
                "</div>"
            )
        );
    }
    else
    {
        for (
            size_t index = 0;
            index < archiveSessionCount;
            index++
        )
        {
            const ArchiveSession &item =
                archiveSessions[index];

            char dateBuffer[32];

            formatArchiveDateTime(
                item.startUnix,
                dateBuffer,
                sizeof(dateBuffer),
                true
            );

            String row;
            row.reserve(420);

            row += F("<div class='session'><div><strong>");
            row += htmlEscape(item.station);
            row += F("</strong><div class='meta'>");
            row += htmlEscape(dateBuffer);
            row += F(" &middot; ");
            row += formatWebFileSize(item.fileSize);
            row += F("<br>");
            row += htmlEscape(
                pathFileName(item.filePath)
            );
            row += F("</div></div><div><a class='button' href='/download?i=");
            row += String(index);
            row += F("'>STIAHNUT CSV</a></div></div>");

            wifiExportServer.sendContent(row);
        }
    }

    sendWifiExportPageFooter();
}

void handleWifiExportDownload()
{
    markWifiExportActivity();

    if (!wifiExportServer.hasArg("i"))
    {
        wifiExportServer.send(
            400,
            "text/plain; charset=utf-8",
            "Chyba indexu suboru."
        );
        return;
    }

    const String indexText =
        wifiExportServer.arg("i");

    char *endPointer = nullptr;

    const unsigned long index =
        strtoul(
            indexText.c_str(),
            &endPointer,
            10
        );

    if (
        endPointer == indexText.c_str() ||
        *endPointer != '\0' ||
        index >= archiveSessionCount
    )
    {
        wifiExportServer.send(
            404,
            "text/plain; charset=utf-8",
            "Relacia sa nenasla."
        );
        return;
    }

    const ArchiveSession &item =
        archiveSessions[index];

    // Dodatochna kontrola zaruci, ze sa neposkytne aktivny
    // alebo nedokonceny subor ani po zmene obsahu SD karty.
    ArchiveSession verified{};

    if (
        !loadArchiveSessionOverview(
            item.filePath,
            verified
        )
    )
    {
        wifiExportServer.send(
            409,
            "text/plain; charset=utf-8",
            "Subor nie je ukoncena relacia."
        );
        return;
    }

    File downloadFile = SD.open(
        verified.filePath,
        FILE_READ
    );

    if (!downloadFile)
    {
        wifiExportServer.send(
            404,
            "text/plain; charset=utf-8",
            "CSV subor sa neda otvorit."
        );
        return;
    }

    const char *fileName =
        pathFileName(
            verified.filePath
        );

    String disposition =
        F("attachment; filename=\"");

    disposition += fileName;
    disposition += '"';

    wifiExportServer.sendHeader(
        "Content-Disposition",
        disposition
    );

    wifiExportServer.sendHeader(
        "Cache-Control",
        "no-store"
    );

    wifiExportServer.streamFile(
        downloadFile,
        "text/csv; charset=utf-8"
    );

    downloadFile.close();

    // Prenos moze trvat dlhsie ako bezna HTTP poziadavka.
    // Casovy limit preto pocitame az od jeho dokoncenia.
    markWifiExportActivity();

    Serial.printf(
        "WI-FI EXPORT: odoslany %s\n",
        verified.filePath
    );
}

void handleWifiExportNotFound()
{
    markWifiExportActivity();

    wifiExportServer.sendHeader(
        "Location",
        "/",
        true
    );

    wifiExportServer.send(
        302,
        "text/plain",
        ""
    );
}

void configureWifiExportRoutes()
{
    if (wifiExportRoutesConfigured)
    {
        return;
    }

    wifiExportServer.on(
        "/",
        HTTP_GET,
        handleWifiExportRoot
    );

    wifiExportServer.on(
        "/download",
        HTTP_GET,
        handleWifiExportDownload
    );

    wifiExportServer.onNotFound(
        handleWifiExportNotFound
    );

    wifiExportRoutesConfigured = true;
}

bool startWifiExport()
{
    if (wifiExportRunning)
    {
        return true;
    }

    if (sessionActive)
    {
        snprintf(
            wifiExportStatusMessage,
            sizeof(wifiExportStatusMessage),
            "NAJPRV UKONCI MERACIU RELACIU"
        );

        return false;
    }

    if (!ensureSDReady(true))
    {
        snprintf(
            wifiExportStatusMessage,
            sizeof(wifiExportStatusMessage),
            "SD KARTA NIE JE DOSTUPNA"
        );

        return false;
    }

    buildWifiExportSsid();
    configureWifiExportRoutes();

    WiFi.mode(WIFI_AP);

    if (
        !WiFi.softAPConfig(
            WIFI_EXPORT_IP,
            WIFI_EXPORT_GATEWAY,
            WIFI_EXPORT_SUBNET
        )
    )
    {
        WiFi.mode(WIFI_OFF);

        snprintf(
            wifiExportStatusMessage,
            sizeof(wifiExportStatusMessage),
            "CHYBA KONFIGURACIE WI-FI"
        );

        return false;
    }

    const bool started =
        WiFi.softAP(
            wifiExportSsid,
            WIFI_EXPORT_PASSWORD,
            1,
            false,
            WIFI_EXPORT_MAX_CLIENTS
        );

    if (!started)
    {
        WiFi.mode(WIFI_OFF);

        snprintf(
            wifiExportStatusMessage,
            sizeof(wifiExportStatusMessage),
            "WI-FI SA NEPODARILO SPUSTIT"
        );

        return false;
    }

    wifiExportServer.begin();

    wifiExportRunning = true;
    wifiExportStartedMillis = millis();
    wifiExportLastActivityMillis =
        wifiExportStartedMillis;
    wifiExportLastUiRefreshMillis = 0;
    wifiExportLastClientCount = 0;

    loadArchiveSessions();

    snprintf(
        wifiExportStatusMessage,
        sizeof(wifiExportStatusMessage),
        "EXPORT JE PRIPRAVENY"
    );

    setDisplayPowerState(
        DisplayPowerState::Awake
    );

    Serial.printf(
        "WI-FI EXPORT: SSID=%s, IP=%s\n",
        wifiExportSsid,
        WiFi.softAPIP().toString().c_str()
    );

    return true;
}

void stopWifiExport(
    const char *reason = nullptr
)
{
    if (wifiExportRunning)
    {
        wifiExportServer.stop();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_OFF);
    }

    wifiExportRunning = false;
    wifiExportLastClientCount = 0;

    if (
        reason != nullptr &&
        reason[0] != '\0'
    )
    {
        snprintf(
            wifiExportStatusMessage,
            sizeof(wifiExportStatusMessage),
            "%s",
            reason
        );
    }
    else
    {
        snprintf(
            wifiExportStatusMessage,
            sizeof(wifiExportStatusMessage),
            "WI-FI JE VYPNUTE"
        );
    }

    lastUserActivityMillis = millis();

    Serial.printf(
        "WI-FI EXPORT: STOP (%s)\n",
        wifiExportStatusMessage
    );
}

uint32_t wifiExportRemainingSeconds()
{
    if (!wifiExportRunning)
    {
        return 0;
    }

    const uint32_t elapsed =
        millis() -
        wifiExportLastActivityMillis;

    if (
        elapsed >=
        WIFI_EXPORT_IDLE_TIMEOUT_MS
    )
    {
        return 0;
    }

    return
        (
            WIFI_EXPORT_IDLE_TIMEOUT_MS -
            elapsed +
            999UL
        ) /
        1000UL;
}

void drawWifiExportStatusPanel()
{
    drawPanel(
        WIFI_STATUS_PANEL_RECT
    );

    tft.setTextDatum(MC_DATUM);

    if (!wifiExportRunning)
    {
        tft.setTextColor(
            COLOR_SECONDARY_TEXT,
            COLOR_PANEL
        );

        tft.drawString(
            "WI-FI JE VYPNUTE",
            160,
            84,
            2
        );

        tft.setTextColor(
            sdReady
                ? COLOR_TEXT
                : COLOR_ERROR,
            COLOR_PANEL
        );

        tft.drawString(
            sessionActive
                ? "Najprv ukonci meraciu relaciu"
                : sdReady
                    ? "Spusti vlastnu siet zariadenia"
                    : "SD karta nie je dostupna",
            160,
            122,
            1
        );

        tft.setTextColor(
            COLOR_SECONDARY_TEXT,
            COLOR_PANEL
        );

        tft.drawString(
            "Internet ani router nie su potrebne",
            160,
            164,
            1
        );

        tft.drawString(
            "Exportuju sa iba ukoncene relacie",
            160,
            190,
            1
        );

        tft.drawString(
            wifiExportStatusMessage,
            160,
            242,
            1
        );

        return;
    }

    tft.setTextColor(
        COLOR_OK,
        COLOR_PANEL
    );

    tft.drawString(
        "WI-FI EXPORT AKTIVNY",
        160,
        78,
        2
    );

    tft.setTextColor(
        COLOR_SECONDARY_TEXT,
        COLOR_PANEL
    );

    tft.setTextDatum(TL_DATUM);

    tft.drawString(
        "SIET",
        24,
        110,
        1
    );

    tft.drawString(
        "HESLO",
        24,
        146,
        1
    );

    tft.drawString(
        "ADRESA",
        24,
        182,
        1
    );

    tft.drawString(
        "KLIENTI",
        24,
        218,
        1
    );

    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(
        COLOR_TEXT,
        COLOR_PANEL
    );

    tft.drawString(
        wifiExportSsid,
        296,
        110,
        1
    );

    tft.drawString(
        WIFI_EXPORT_PASSWORD,
        296,
        146,
        1
    );

    tft.drawString(
        "192.168.4.1",
        296,
        182,
        1
    );

    char clientBuffer[32];

    snprintf(
        clientBuffer,
        sizeof(clientBuffer),
        "%u | %lu s",
        WiFi.softAPgetStationNum(),
        static_cast<unsigned long>(
            wifiExportRemainingSeconds()
        )
    );

    tft.drawString(
        clientBuffer,
        296,
        218,
        1
    );

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(
        COLOR_ACCENT,
        COLOR_PANEL
    );

    tft.drawString(
        "Otvor v prehliadaci 192.168.4.1",
        160,
        252,
        1
    );
}

void updateWifiExportDynamicStatus()
{
    if (
        currentScreen != Screen::WifiExport ||
        !wifiExportRunning
    )
    {
        return;
    }

    char clientBuffer[32];

    snprintf(
        clientBuffer,
        sizeof(clientBuffer),
        "%u | %lu s",
        WiFi.softAPgetStationNum(),
        static_cast<unsigned long>(
            wifiExportRemainingSeconds()
        )
    );

    tft.fillRect(
        174,
        205,
        128,
        26,
        COLOR_PANEL
    );

    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(
        COLOR_TEXT,
        COLOR_PANEL
    );

    tft.drawString(
        clientBuffer,
        296,
        218,
        1
    );
}


void drawWifiExportScreen()
{
    currentScreen = Screen::WifiExport;

    tft.fillScreen(COLOR_BACKGROUND);
    tft.fillRect(0, 0, 320, 42, COLOR_PANEL);
    tft.drawFastHLine(
        0,
        41,
        320,
        COLOR_PANEL_BORDER
    );

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(
        COLOR_TEXT,
        COLOR_PANEL
    );

    tft.drawString(
        "WI-FI EXPORT",
        160,
        21,
        2
    );

    drawWifiExportStatusPanel();

    const bool wifiCanStart =
        !sessionActive &&
        sdReady;

    drawButton(
        WIFI_ACTION_RECT,
        wifiExportRunning
            ? "ZASTAVIT WI-FI"
            : sessionActive
                ? "UKONCI RELACIU"
                : "SPUSTIT WI-FI",
        wifiExportRunning
            ? COLOR_BACKGROUND
            : wifiCanStart
                ? COLOR_PRIMARY
                : COLOR_BACKGROUND,
        wifiExportRunning
            ? COLOR_WARNING
            : wifiCanStart
                ? COLOR_PRIMARY
                : COLOR_DISABLED,
        wifiExportRunning
            ? COLOR_TEXT
            : wifiCanStart
                ? TFT_BLACK
                : COLOR_DISABLED,
        2
    );

    drawButton(
        WIFI_REFRESH_RECT,
        "OBNOV RELACIE",
        COLOR_PANEL,
        sdReady
            ? COLOR_PRIMARY
            : COLOR_DISABLED,
        sdReady
            ? COLOR_TEXT
            : COLOR_DISABLED,
        1
    );

    char countBuffer[32];

    snprintf(
        countBuffer,
        sizeof(countBuffer),
        "UKONCENE: %u",
        static_cast<unsigned int>(
            archiveSessionCount
        )
    );

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(
        COLOR_SECONDARY_TEXT,
        COLOR_BACKGROUND
    );

    tft.drawString(
        countBuffer,
        238,
        386,
        1
    );

    drawButton(
        WIFI_BACK_RECT,
        "SPAT",
        COLOR_PANEL,
        COLOR_PANEL_BORDER,
        COLOR_TEXT,
        2
    );
}

void openWifiExport()
{
    if (sdReady)
    {
        loadArchiveSessions();
    }

    wifiExportStatusMessage[0] = '\0';

    drawWifiExportScreen();
}

void processWifiExportTouch(
    int16_t x,
    int16_t y
)
{
    if (WIFI_BACK_RECT.contains(x, y))
    {
        stopWifiExport(
            "WI-FI VYPNUTE"
        );

        drawMainScreen();
        return;
    }

    if (WIFI_ACTION_RECT.contains(x, y))
    {
        if (wifiExportRunning)
        {
            stopWifiExport(
                "WI-FI VYPNUTE"
            );
        }
        else
        {
            startWifiExport();
        }

        drawWifiExportScreen();
        return;
    }

    if (WIFI_REFRESH_RECT.contains(x, y))
    {
        if (ensureSDReady(true))
        {
            loadArchiveSessions();

            snprintf(
                wifiExportStatusMessage,
                sizeof(wifiExportStatusMessage),
                "ZOZNAM RELACII OBNOVENY"
            );
        }
        else
        {
            snprintf(
                wifiExportStatusMessage,
                sizeof(wifiExportStatusMessage),
                "SD KARTA NIE JE DOSTUPNA"
            );
        }

        drawWifiExportScreen();
    }
}

void updateWifiExport(
    uint32_t currentMillis
)
{
    if (!wifiExportRunning)
    {
        return;
    }

    wifiExportServer.handleClient();

    // HTTP obsluha moze pocas handleClient() aktualizovat cas
    // poslednej aktivity. Pouzitie povodneho currentMillis by pri
    // unsigned aritmetike mohlo vytvorit podtecenie a Wi-Fi
    // okamzite vypnut po kliknuti na stiahnutie.
    currentMillis = millis();

    if (
        currentMillis -
            wifiExportLastActivityMillis >=
        WIFI_EXPORT_IDLE_TIMEOUT_MS
    )
    {
        stopWifiExport(
            "CASOVY LIMIT - WI-FI VYPNUTE"
        );

        if (
            currentScreen ==
            Screen::WifiExport
        )
        {
            drawWifiExportScreen();
        }

        return;
    }

    if (
        currentScreen !=
        Screen::WifiExport
    )
    {
        return;
    }

    const uint8_t clientCount =
        WiFi.softAPgetStationNum();

    if (
        currentMillis -
            wifiExportLastUiRefreshMillis >=
            WIFI_EXPORT_UI_REFRESH_MS ||
        clientCount !=
            wifiExportLastClientCount
    )
    {
        wifiExportLastUiRefreshMillis =
            currentMillis;

        wifiExportLastClientCount =
            clientCount;

        updateWifiExportDynamicStatus();
    }
}

// ============================================================
// ARCHIV UKONCENYCH RELACII - POMOCNE FUNKCIE
// ============================================================

bool hasCsvExtension(const char *fileName)
{
    if (fileName == nullptr)
    {
        return false;
    }

    const size_t length = strlen(fileName);

    if (length < 4)
    {
        return false;
    }

    const char *extension =
        fileName + length - 4;

    return
        strcmp(extension, ".csv") == 0 ||
        strcmp(extension, ".CSV") == 0;
}

bool isClosedSessionRecordType(
    const char *recordType
)
{
    if (recordType == nullptr)
    {
        return false;
    }

    return
        strcmp(
            recordType,
            "SESSION_END"
        ) == 0 ||
        strcmp(
            recordType,
            "SESSION_END_RECOVERY"
        ) == 0 ||
        strcmp(
            recordType,
            "SESSION_ABANDONED"
        ) == 0;
}

bool parseArchiveIdentityLine(
    char *line,
    char *parsedSessionId,
    size_t sessionIdSize,
    char *parsedStation,
    size_t stationSize,
    char *parsedRecordType,
    size_t recordTypeSize,
    uint32_t &parsedUnixTime
)
{
    if (
        line == nullptr ||
        parsedSessionId == nullptr ||
        parsedStation == nullptr ||
        parsedRecordType == nullptr
    )
    {
        return false;
    }

    char *fields[12];

    const size_t fieldCount =
        splitCsvFields(
            line,
            fields,
            12
        );

    if (fieldCount < 5)
    {
        return false;
    }

    if (
        strcmp(
            fields[0],
            "session_id"
        ) == 0
    )
    {
        return false;
    }

    if (
        !parseIsoTimestamp(
            fields[1],
            parsedUnixTime
        )
    )
    {
        return false;
    }

    snprintf(
        parsedSessionId,
        sessionIdSize,
        "%s",
        fields[0]
    );

    snprintf(
        parsedRecordType,
        recordTypeSize,
        "%s",
        fields[2]
    );

    snprintf(
        parsedStation,
        stationSize,
        "%s",
        fields[3]
    );

    return true;
}

bool readFirstArchiveRecord(
    File &file,
    char *sessionIdBuffer,
    size_t sessionIdBufferSize,
    char *stationBuffer,
    size_t stationBufferSize,
    uint32_t &startUnix
)
{
    if (!file.seek(0))
    {
        return false;
    }

    char line[ARCHIVE_CSV_LINE_LENGTH];
    char recordType[32];

    while (
        readCsvLine(
            file,
            line,
            sizeof(line)
        )
    )
    {
        if (
            parseArchiveIdentityLine(
                line,
                sessionIdBuffer,
                sessionIdBufferSize,
                stationBuffer,
                stationBufferSize,
                recordType,
                sizeof(recordType),
                startUnix
            )
        )
        {
            return true;
        }
    }

    return false;
}

bool readLastNonEmptyCsvLine(
    File &file,
    char *buffer,
    size_t bufferSize
)
{
    if (
        buffer == nullptr ||
        bufferSize < 2
    )
    {
        return false;
    }

    const uint32_t fileSize =
        file.size();

    if (fileSize == 0)
    {
        return false;
    }

    int64_t position =
        static_cast<int64_t>(fileSize) - 1;

    // Preskoc koncove CR/LF znaky.
    while (position >= 0)
    {
        if (
            !file.seek(
                static_cast<uint32_t>(
                    position
                )
            )
        )
        {
            return false;
        }

        const int value = file.read();

        if (
            value != '\n' &&
            value != '\r'
        )
        {
            break;
        }

        position--;
    }

    if (position < 0)
    {
        return false;
    }

    // Najdi zaciatok posledneho neprazdneho riadka.
    while (position >= 0)
    {
        if (
            !file.seek(
                static_cast<uint32_t>(
                    position
                )
            )
        )
        {
            return false;
        }

        const int value = file.read();

        if (value == '\n')
        {
            position++;
            break;
        }

        position--;
    }

    if (position < 0)
    {
        position = 0;
    }

    if (
        !file.seek(
            static_cast<uint32_t>(
                position
            )
        )
    )
    {
        return false;
    }

    return readCsvLine(
        file,
        buffer,
        bufferSize
    );
}

bool loadArchiveSessionOverview(
    const char *filePath,
    ArchiveSession &session
)
{
    memset(
        &session,
        0,
        sizeof(session)
    );

    if (
        filePath == nullptr ||
        filePath[0] == '\0'
    )
    {
        return false;
    }

    File file = SD.open(
        filePath,
        FILE_READ
    );

    if (!file)
    {
        return false;
    }

    session.fileSize =
        file.size();

    char firstSessionId[
        ARCHIVE_SESSION_ID_LENGTH
    ] = "";

    char firstStation[
        IDENTIFIER_MAX_LENGTH + 1
    ] = "";

    uint32_t firstUnix = 0;

    const bool firstRecordOk =
        readFirstArchiveRecord(
            file,
            firstSessionId,
            sizeof(firstSessionId),
            firstStation,
            sizeof(firstStation),
            firstUnix
        );

    char lastLine[
        ARCHIVE_CSV_LINE_LENGTH
    ];

    const bool lastLineOk =
        readLastNonEmptyCsvLine(
            file,
            lastLine,
            sizeof(lastLine)
        );

    file.close();

    if (
        !firstRecordOk ||
        !lastLineOk
    )
    {
        return false;
    }

    char lastSessionId[
        ARCHIVE_SESSION_ID_LENGTH
    ] = "";

    char lastStation[
        IDENTIFIER_MAX_LENGTH + 1
    ] = "";

    char lastRecordType[32] = "";
    uint32_t lastUnix = 0;

    if (
        !parseArchiveIdentityLine(
            lastLine,
            lastSessionId,
            sizeof(lastSessionId),
            lastStation,
            sizeof(lastStation),
            lastRecordType,
            sizeof(lastRecordType),
            lastUnix
        )
    )
    {
        return false;
    }

    if (
        !isClosedSessionRecordType(
            lastRecordType
        )
    )
    {
        return false;
    }

    snprintf(
        session.filePath,
        sizeof(session.filePath),
        "%s",
        filePath
    );

    snprintf(
        session.sessionId,
        sizeof(session.sessionId),
        "%s",
        firstSessionId
    );

    snprintf(
        session.station,
        sizeof(session.station),
        "%s",
        firstStation[0] != '\0'
            ? firstStation
            : "---"
    );

    session.startUnix = firstUnix;
    session.endUnix = lastUnix;

    return true;
}

void insertArchiveSessionSorted(
    const ArchiveSession &candidate
)
{
    size_t insertIndex = 0;

    while (
        insertIndex <
            archiveSessionCount &&
        archiveSessions[
            insertIndex
        ].startUnix >=
            candidate.startUnix
    )
    {
        insertIndex++;
    }

    if (
        archiveSessionCount >=
            ARCHIVE_MAX_SESSIONS &&
        insertIndex >=
            ARCHIVE_MAX_SESSIONS
    )
    {
        return;
    }

    if (
        archiveSessionCount <
        ARCHIVE_MAX_SESSIONS
    )
    {
        archiveSessionCount++;
    }

    for (
        size_t index =
            archiveSessionCount - 1;
        index > insertIndex;
        index--
    )
    {
        archiveSessions[index] =
            archiveSessions[index - 1];
    }

    archiveSessions[
        insertIndex
    ] = candidate;
}

size_t archivePageCount()
{
    if (archiveSessionCount == 0)
    {
        return 1;
    }

    return
        (
            archiveSessionCount +
            ARCHIVE_PAGE_SIZE -
            1
        ) /
        ARCHIVE_PAGE_SIZE;
}

void loadArchiveSessions()
{
    archiveSessionCount = 0;
    archiveCurrentPage = 0;
    selectedArchiveSessionIndex = -1;
    archiveStatusMessage[0] = '\0';

    if (!ensureSDReady(true))
    {
        snprintf(
            archiveStatusMessage,
            sizeof(archiveStatusMessage),
            "SD KARTA NIE JE DOSTUPNA"
        );

        return;
    }

    if (!ensureSessionDirectory())
    {
        snprintf(
            archiveStatusMessage,
            sizeof(archiveStatusMessage),
            "ADRESAR RELACII NIE JE DOSTUPNY"
        );

        return;
    }

    File directory = SD.open(
        SESSION_DIRECTORY
    );

    if (
        !directory ||
        !directory.isDirectory()
    )
    {
        snprintf(
            archiveStatusMessage,
            sizeof(archiveStatusMessage),
            "ADRESAR RELACII SA NEDA OTVORIT"
        );

        if (directory)
        {
            directory.close();
        }

        return;
    }

    size_t closedSessionFileCount = 0;

    while (true)
    {
        File entry =
            directory.openNextFile();

        if (!entry)
        {
            break;
        }

        if (entry.isDirectory())
        {
            entry.close();
            continue;
        }

        const char *entryName =
            entry.name();

        char candidatePath[
            ARCHIVE_PATH_LENGTH
        ];

        if (
            entryName != nullptr &&
            entryName[0] == '/'
        )
        {
            snprintf(
                candidatePath,
                sizeof(candidatePath),
                "%s",
                entryName
            );
        }
        else
        {
            snprintf(
                candidatePath,
                sizeof(candidatePath),
                "%s/%s",
                SESSION_DIRECTORY,
                entryName != nullptr
                    ? entryName
                    : ""
            );
        }

        entry.close();

        if (
            !hasCsvExtension(
                candidatePath
            )
        )
        {
            continue;
        }

        if (
            sessionActive &&
            strcmp(
                candidatePath,
                sessionFilePath
            ) == 0
        )
        {
            continue;
        }

        ArchiveSession candidate{};

        if (
            !loadArchiveSessionOverview(
                candidatePath,
                candidate
            )
        )
        {
            continue;
        }

        closedSessionFileCount++;

        insertArchiveSessionSorted(
            candidate
        );
    }

    directory.close();

    if (
        closedSessionFileCount >
        ARCHIVE_MAX_SESSIONS
    )
    {
        snprintf(
            archiveStatusMessage,
            sizeof(archiveStatusMessage),
            "ZOBRAZENYCH %u NAJNOVSICH RELACII",
            static_cast<unsigned int>(
                ARCHIVE_MAX_SESSIONS
            )
        );
    }
}

void formatArchiveDateTime(
    uint32_t unixTime,
    char *buffer,
    size_t bufferSize,
    bool includeSeconds
)
{
    if (
        buffer == nullptr ||
        bufferSize == 0
    )
    {
        return;
    }

    if (unixTime == 0)
    {
        snprintf(
            buffer,
            bufferSize,
            "--.--.---- --:--"
        );

        return;
    }

    const DateTime dateTime(
        unixTime
    );

    if (includeSeconds)
    {
        snprintf(
            buffer,
            bufferSize,
            "%02u.%02u.%04u %02u:%02u:%02u",
            dateTime.day(),
            dateTime.month(),
            dateTime.year(),
            dateTime.hour(),
            dateTime.minute(),
            dateTime.second()
        );
    }
    else
    {
        snprintf(
            buffer,
            bufferSize,
            "%02u.%02u.%04u %02u:%02u",
            dateTime.day(),
            dateTime.month(),
            dateTime.year(),
            dateTime.hour(),
            dateTime.minute()
        );
    }
}

void drawArchiveLoadingScreen(
    const char *message
)
{
    tft.fillScreen(COLOR_BACKGROUND);
    tft.fillRect(0, 0, 320, 40, COLOR_PANEL);
    tft.drawFastHLine(
        0,
        39,
        320,
        COLOR_PANEL_BORDER
    );

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(
        COLOR_TEXT,
        COLOR_PANEL
    );

    tft.drawString(
        "ARCHIV RELACII",
        160,
        20,
        2
    );

    tft.setTextColor(
        COLOR_PRIMARY,
        COLOR_BACKGROUND
    );

    tft.drawString(
        message != nullptr
            ? message
            : "NACITAVAM...",
        160,
        235,
        2
    );
}

void drawArchiveListScreen()
{
    currentScreen = Screen::ArchiveList;

    tft.fillScreen(COLOR_BACKGROUND);
    tft.fillRect(0, 0, 320, 40, COLOR_PANEL);
    tft.drawFastHLine(
        0,
        39,
        320,
        COLOR_PANEL_BORDER
    );

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(
        COLOR_TEXT,
        COLOR_PANEL
    );

    tft.drawString(
        "UKONCENE RELACIE",
        160,
        20,
        2
    );

    const size_t firstSessionIndex =
        archiveCurrentPage *
        ARCHIVE_PAGE_SIZE;

    for (
        size_t rowIndex = 0;
        rowIndex < ARCHIVE_PAGE_SIZE;
        rowIndex++
    )
    {
        const Rect &rowRect =
            ARCHIVE_ROW_RECTS[rowIndex];

        const size_t sessionIndex =
            firstSessionIndex +
            rowIndex;

        if (
            sessionIndex >=
            archiveSessionCount
        )
        {
            tft.fillRect(
                rowRect.x,
                rowRect.y,
                rowRect.width,
                rowRect.height,
                COLOR_BACKGROUND
            );

            continue;
        }

        const ArchiveSession &item =
            archiveSessions[
                sessionIndex
            ];

        drawPanel(
            rowRect,
            6
        );

        char stationLine[32];
        char dateLine[32];

        snprintf(
            stationLine,
            sizeof(stationLine),
            "ST: %s",
            item.station
        );

        formatArchiveDateTime(
            item.startUnix,
            dateLine,
            sizeof(dateLine)
        );

        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(
            COLOR_TEXT,
            COLOR_PANEL
        );

        tft.drawString(
            stationLine,
            rowRect.x + 12,
            rowRect.y + 18,
            2
        );

        tft.setTextColor(
            COLOR_SECONDARY_TEXT,
            COLOR_PANEL
        );

        tft.drawString(
            dateLine,
            rowRect.x + 12,
            rowRect.y + 42,
            1
        );

        char sizeLine[20];

        snprintf(
            sizeLine,
            sizeof(sizeLine),
            "%lu kB",
            static_cast<unsigned long>(
                (
                    item.fileSize +
                    1023
                ) /
                1024
            )
        );

        tft.setTextDatum(MR_DATUM);
        tft.setTextColor(
            COLOR_PRIMARY,
            COLOR_PANEL
        );

        tft.drawString(
            sizeLine,
            rowRect.x +
                rowRect.width -
                12,
            rowRect.y + 42,
            1
        );
    }

    if (archiveSessionCount == 0)
    {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(
            archiveStatusMessage[0] != '\0'
                ? COLOR_ERROR
                : COLOR_SECONDARY_TEXT,
            COLOR_BACKGROUND
        );

        tft.drawString(
            archiveStatusMessage[0] != '\0'
                ? archiveStatusMessage
                : "ZIADNE UKONCENE RELACIE",
            160,
            230,
            2
        );
    }

    char pageLabel[28];

    snprintf(
        pageLabel,
        sizeof(pageLabel),
        "STRANA %u/%u  RELACIE: %u",
        static_cast<unsigned int>(
            archiveCurrentPage + 1
        ),
        static_cast<unsigned int>(
            archivePageCount()
        ),
        static_cast<unsigned int>(
            archiveSessionCount
        )
    );

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(
        archiveStatusMessage[0] != '\0'
            ? COLOR_WARNING
            : COLOR_SECONDARY_TEXT,
        COLOR_BACKGROUND
    );

    tft.drawString(
        archiveStatusMessage[0] != '\0'
            ? archiveStatusMessage
            : pageLabel,
        160,
        393,
        1
    );

    const bool hasPrevious =
        archiveCurrentPage > 0;

    const bool hasNext =
        archiveCurrentPage + 1 <
        archivePageCount();

    drawButton(
        ARCHIVE_BACK_RECT,
        "SPAT",
        COLOR_PANEL,
        COLOR_PRIMARY,
        COLOR_TEXT,
        2
    );

    drawButton(
        ARCHIVE_PREVIOUS_RECT,
        "<",
        hasPrevious
            ? COLOR_PANEL
            : COLOR_BACKGROUND,
        hasPrevious
            ? COLOR_PRIMARY
            : COLOR_DISABLED,
        hasPrevious
            ? COLOR_TEXT
            : COLOR_DISABLED,
        2
    );

    drawButton(
        ARCHIVE_NEXT_RECT,
        ">",
        hasNext
            ? COLOR_PANEL
            : COLOR_BACKGROUND,
        hasNext
            ? COLOR_PRIMARY
            : COLOR_DISABLED,
        hasNext
            ? COLOR_TEXT
            : COLOR_DISABLED,
        2
    );

    drawButton(
        ARCHIVE_REFRESH_RECT,
        "OBNOV",
        COLOR_PANEL,
        COLOR_PRIMARY,
        COLOR_TEXT,
        1
    );
}

void openArchiveList()
{
    if (!ensureSDReady(true))
    {
        setStatusMessage(
            "SD KARTA NIE JE DOSTUPNA",
            COLOR_ERROR
        );

        updateMessageStrip();
        return;
    }

    drawArchiveLoadingScreen(
        "NACITAVAM SD..."
    );

    loadArchiveSessions();
    drawArchiveListScreen();
}

void resetArchiveDetailData()
{
    memset(
        &archiveDetailData,
        0,
        sizeof(archiveDetailData)
    );

    resetGraphStatistics(
        archiveDetailData.temperatureStats
    );

    resetGraphStatistics(
        archiveDetailData.humidityStats
    );

    resetGraphStatistics(
        archiveDetailData.pressureStats
    );
}

bool loadArchiveDetailData(
    size_t sessionIndex
)
{
    resetArchiveDetailData();

    if (
        sessionIndex >=
        archiveSessionCount
    )
    {
        return false;
    }

    const ArchiveSession &selected =
        archiveSessions[
            sessionIndex
        ];

    snprintf(
        archiveDetailData.filePath,
        sizeof(
            archiveDetailData.filePath
        ),
        "%s",
        selected.filePath
    );

    snprintf(
        archiveDetailData.sessionId,
        sizeof(
            archiveDetailData.sessionId
        ),
        "%s",
        selected.sessionId
    );

    snprintf(
        archiveDetailData.station,
        sizeof(
            archiveDetailData.station
        ),
        "%s",
        selected.station
    );

    archiveDetailData.startUnix =
        selected.startUnix;

    archiveDetailData.endUnix =
        selected.endUnix;

    if (!ensureSDReady(false))
    {
        return false;
    }

    File file = SD.open(
        archiveDetailData.filePath,
        FILE_READ
    );

    if (!file)
    {
        return false;
    }

    char line[
        ARCHIVE_CSV_LINE_LENGTH
    ];

    while (
        readCsvLine(
            file,
            line,
            sizeof(line)
        )
    )
    {
        GraphSample sample{};
        bool isAutoRecord = false;
        bool isManualRecord = false;
        bool hasTimestamp = false;

        const bool parsed =
            parseGraphCsvRecord(
                line,
                sample,
                isAutoRecord,
                isManualRecord,
                hasTimestamp
            );

        if (
            hasTimestamp &&
            archiveDetailData.startUnix == 0
        )
        {
            archiveDetailData.startUnix =
                sample.unixTime;
        }

        if (hasTimestamp)
        {
            archiveDetailData.endUnix =
                sample.unixTime;
        }

        if (isManualRecord)
        {
            archiveDetailData.manualRecordCount++;
        }

        if (
            !parsed ||
            !isAutoRecord
        )
        {
            continue;
        }

        archiveDetailData.autoRecordCount++;

        addSampleToStatistics(
            sample,
            archiveDetailData.temperatureStats,
            archiveDetailData.humidityStats,
            archiveDetailData.pressureStats
        );
    }

    file.close();

    archiveDetailData.loaded = true;

    return true;
}

void drawArchiveStatisticRow(
    int16_t y,
    const char *label,
    const GraphStatistics &statistics
)
{
    char minimumText[16] = "--";
    char averageText[16] = "--";
    char maximumText[16] = "--";

    if (statistics.count > 0)
    {
        snprintf(
            minimumText,
            sizeof(minimumText),
            "%.2f",
            statistics.minimum
        );

        snprintf(
            averageText,
            sizeof(averageText),
            "%.2f",
            graphAverage(statistics)
        );

        snprintf(
            maximumText,
            sizeof(maximumText),
            "%.2f",
            statistics.maximum
        );
    }

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(
        COLOR_SECONDARY_TEXT,
        COLOR_PANEL
    );

    tft.drawString(
        label,
        22,
        y,
        1
    );

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(
        COLOR_TEXT,
        COLOR_PANEL
    );

    tft.drawString(
        minimumText,
        92,
        y,
        1
    );

    tft.drawString(
        averageText,
        182,
        y,
        1
    );

    tft.drawString(
        maximumText,
        272,
        y,
        1
    );
}

void drawArchiveDetailScreen()
{
    currentScreen = Screen::ArchiveDetail;

    tft.fillScreen(COLOR_BACKGROUND);
    tft.fillRect(0, 0, 320, 40, COLOR_PANEL);
    tft.drawFastHLine(
        0,
        39,
        320,
        COLOR_PANEL_BORDER
    );

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(
        COLOR_TEXT,
        COLOR_PANEL
    );

    tft.drawString(
        "PREHLAD RELACIE",
        160,
        20,
        2
    );

    drawPanel(
        ARCHIVE_DETAIL_PANEL_RECT
    );

    if (!archiveDetailData.loaded)
    {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(
            COLOR_ERROR,
            COLOR_PANEL
        );

        tft.drawString(
            "RELACIU SA NEPODARILO NACITAT",
            160,
            220,
            2
        );
    }
    else
    {
        char startText[32];
        char endText[32];
        char durationText[20];
        char countText[40];

        formatArchiveDateTime(
            archiveDetailData.startUnix,
            startText,
            sizeof(startText),
            true
        );

        formatArchiveDateTime(
            archiveDetailData.endUnix,
            endText,
            sizeof(endText),
            true
        );

        uint32_t durationSeconds = 0;

        if (
            archiveDetailData.endUnix >=
            archiveDetailData.startUnix
        )
        {
            durationSeconds =
                archiveDetailData.endUnix -
                archiveDetailData.startUnix;
        }

        formatGraphDuration(
            durationSeconds,
            durationText,
            sizeof(durationText)
        );

        snprintf(
            countText,
            sizeof(countText),
            "AUTO: %lu   MANUAL: %lu",
            static_cast<unsigned long>(
                archiveDetailData.autoRecordCount
            ),
            static_cast<unsigned long>(
                archiveDetailData.manualRecordCount
            )
        );

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(
            COLOR_SECONDARY_TEXT,
            COLOR_PANEL
        );

        tft.drawString(
            "STANOVISKO",
            22,
            62,
            1
        );

        tft.setTextColor(
            COLOR_TEXT,
            COLOR_PANEL
        );

        tft.drawString(
            archiveDetailData.station,
            22,
            82,
            4
        );

        tft.setTextColor(
            COLOR_SECONDARY_TEXT,
            COLOR_PANEL
        );

        tft.drawString(
            "ZACIATOK",
            22,
            126,
            1
        );

        tft.setTextColor(
            COLOR_TEXT,
            COLOR_PANEL
        );

        tft.drawString(
            startText,
            100,
            126,
            1
        );

        tft.setTextColor(
            COLOR_SECONDARY_TEXT,
            COLOR_PANEL
        );

        tft.drawString(
            "KONIEC",
            22,
            150,
            1
        );

        tft.setTextColor(
            COLOR_TEXT,
            COLOR_PANEL
        );

        tft.drawString(
            endText,
            100,
            150,
            1
        );

        tft.setTextColor(
            COLOR_SECONDARY_TEXT,
            COLOR_PANEL
        );

        tft.drawString(
            "TRVANIE",
            22,
            174,
            1
        );

        tft.setTextColor(
            COLOR_PRIMARY,
            COLOR_PANEL
        );

        tft.drawString(
            durationText,
            100,
            174,
            1
        );

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(
            COLOR_TEXT,
            COLOR_PANEL
        );

        tft.drawString(
            countText,
            160,
            218,
            2
        );

        tft.drawFastHLine(
            22,
            248,
            276,
            COLOR_PANEL_BORDER
        );

        tft.setTextColor(
            COLOR_SECONDARY_TEXT,
            COLOR_PANEL
        );

        tft.drawString(
            "MIN",
            92,
            270,
            1
        );

        tft.drawString(
            "PRIEMER",
            182,
            270,
            1
        );

        tft.drawString(
            "MAX",
            272,
            270,
            1
        );

        drawArchiveStatisticRow(
            306,
            "T (C)",
            archiveDetailData.temperatureStats
        );

        drawArchiveStatisticRow(
            338,
            "RH (%)",
            archiveDetailData.humidityStats
        );

        drawArchiveStatisticRow(
            370,
            "P (hPa)",
            archiveDetailData.pressureStats
        );
    }

    drawButton(
        ARCHIVE_DETAIL_BACK_RECT,
        "SPAT",
        COLOR_PANEL,
        COLOR_PRIMARY,
        COLOR_TEXT,
        2
    );

    const bool graphsAvailable =
        archiveDetailData.loaded &&
        archiveDetailData.autoRecordCount > 0;

    drawButton(
        ARCHIVE_DETAIL_GRAPH_RECT,
        graphsAvailable
            ? "GRAFY"
            : "BEZ AUTO DAT",
        graphsAvailable
            ? COLOR_PANEL
            : COLOR_BACKGROUND,
        graphsAvailable
            ? COLOR_PRIMARY
            : COLOR_DISABLED,
        graphsAvailable
            ? COLOR_TEXT
            : COLOR_DISABLED,
        graphsAvailable
            ? 2
            : 1
    );
}

void openArchiveDetail(
    size_t sessionIndex
)
{
    if (
        sessionIndex >=
        archiveSessionCount
    )
    {
        return;
    }

    selectedArchiveSessionIndex =
        static_cast<int16_t>(
            sessionIndex
        );

    drawArchiveLoadingScreen(
        "SPRACUVAM CSV..."
    );

    loadArchiveDetailData(
        sessionIndex
    );

    drawArchiveDetailScreen();
}

void openArchiveGraphs()
{
    if (
        !archiveDetailData.loaded ||
        archiveDetailData.autoRecordCount == 0
    )
    {
        return;
    }

    graphViewingArchive = true;
    graphRange = GraphRange::Last120;
    graphParameter =
        GraphParameter::Temperature;

    reloadGraphScreen();
}

void processArchiveListTouch(
    int16_t x,
    int16_t y
)
{
    if (
        ARCHIVE_BACK_RECT.contains(
            x,
            y
        )
    )
    {
        drawMainScreen();
        return;
    }

    if (
        ARCHIVE_PREVIOUS_RECT.contains(
            x,
            y
        ) &&
        archiveCurrentPage > 0
    )
    {
        archiveCurrentPage--;
        drawArchiveListScreen();
        return;
    }

    if (
        ARCHIVE_NEXT_RECT.contains(
            x,
            y
        ) &&
        archiveCurrentPage + 1 <
            archivePageCount()
    )
    {
        archiveCurrentPage++;
        drawArchiveListScreen();
        return;
    }

    if (
        ARCHIVE_REFRESH_RECT.contains(
            x,
            y
        )
    )
    {
        drawArchiveLoadingScreen(
            "OBNOVUJEM ZOZNAM..."
        );

        loadArchiveSessions();
        drawArchiveListScreen();
        return;
    }

    const size_t firstSessionIndex =
        archiveCurrentPage *
        ARCHIVE_PAGE_SIZE;

    for (
        size_t rowIndex = 0;
        rowIndex < ARCHIVE_PAGE_SIZE;
        rowIndex++
    )
    {
        if (
            !ARCHIVE_ROW_RECTS[
                rowIndex
            ].contains(
                x,
                y
            )
        )
        {
            continue;
        }

        const size_t sessionIndex =
            firstSessionIndex +
            rowIndex;

        if (
            sessionIndex <
            archiveSessionCount
        )
        {
            openArchiveDetail(
                sessionIndex
            );
        }

        return;
    }
}

void processArchiveDetailTouch(
    int16_t x,
    int16_t y
)
{
    if (
        ARCHIVE_DETAIL_BACK_RECT.contains(
            x,
            y
        )
    )
    {
        drawArchiveListScreen();
        return;
    }

    if (
        ARCHIVE_DETAIL_GRAPH_RECT.contains(
            x,
            y
        )
    )
    {
        openArchiveGraphs();
    }
}

// ============================================================
// POTVRDENIE UKONCENIA RELACIE
// ============================================================

void drawEndSessionConfirmation()
{
    currentScreen = Screen::ConfirmEndSession;

    tft.fillScreen(COLOR_BACKGROUND);

    tft.fillRect(0, 0, 320, 48, COLOR_PANEL);
    tft.drawFastHLine(0, 47, 320, COLOR_PANEL_BORDER);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_WARNING, COLOR_PANEL);

    tft.drawString(
        "UKONCIT RELACIU?",
        160,
        24,
        2
    );

    drawPanel(Rect{10, 64, 300, 190});

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_SECONDARY_TEXT, COLOR_PANEL);
    tft.drawString("STANOVISKO", 24, 82, 1);

    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
    tft.drawString(
        identifierIsEmpty(station) ? "---" : station,
        24,
        104,
        4
    );

    tft.setTextColor(COLOR_SECONDARY_TEXT, COLOR_PANEL);
    tft.drawString("MANUALNE ZAZNAMY", 24, 150, 1);

    char countBuffer[24];

    snprintf(
        countBuffer,
        sizeof(countBuffer),
        "%lu",
        static_cast<unsigned long>(manualRecordCount)
    );

    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
    tft.drawString(countBuffer, 24, 172, 4);

    tft.setTextColor(COLOR_WARNING, COLOR_PANEL);
    tft.setTextDatum(MC_DATUM);

    tft.drawString(
        "Po ukonceni sa zastavi",
        160,
        218,
        2
    );

    tft.drawString(
        "automaticky zaznam.",
        160,
        238,
        2
    );

    drawButton(
        CONFIRM_BACK_BUTTON_RECT,
        "SPAT",
        COLOR_PANEL,
        COLOR_PRIMARY,
        COLOR_TEXT,
        2
    );

    drawButton(
        CONFIRM_END_BUTTON_RECT,
        "UKONCIT",
        COLOR_PANEL,
        COLOR_ERROR,
        COLOR_TEXT,
        2
    );
}

void openEndSessionConfirmation()
{
    if (!sessionActive)
    {
        setStatusMessage(
            "ZIADNA AKTIVNA RELACIA",
            COLOR_WARNING
        );

        updateMainScreen();
        return;
    }

    drawEndSessionConfirmation();
}

// ============================================================
// OBNOVA NEDOKONCENEJ RELACIE
// ============================================================

bool recoveryFileAvailable()
{
    return
        sdReady &&
        recoveredSessionFilePath[0] != '\0' &&
        SD.exists(recoveredSessionFilePath);
}

void drawRecoveryScreen()
{
    currentScreen = Screen::Recovery;

    tft.fillScreen(COLOR_BACKGROUND);

    tft.fillRect(0, 0, 320, 50, COLOR_PANEL);
    tft.drawFastHLine(0, 49, 320, COLOR_PANEL_BORDER);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_WARNING, COLOR_PANEL);

    tft.drawString(
        "NEDOKONCENA RELACIA",
        160,
        25,
        2
    );

    drawPanel(Rect{10, 64, 300, 214});

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_SECONDARY_TEXT, COLOR_PANEL);

    tft.drawString(
        "STANOVISKO",
        24,
        80,
        1
    );

    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);

    tft.drawString(
        recoveredStation,
        24,
        100,
        4
    );

    tft.setTextColor(COLOR_SECONDARY_TEXT, COLOR_PANEL);

    tft.drawString(
        "IDENTIFIKATOR RELACIE",
        24,
        145,
        1
    );

    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);

    tft.drawString(
        recoveredSessionId,
        24,
        164,
        2
    );

    tft.setTextColor(COLOR_SECONDARY_TEXT, COLOR_PANEL);

    tft.drawString(
        "MANUALNE ZAZNAMY",
        24,
        198,
        1
    );

    char countBuffer[20];

    snprintf(
        countBuffer,
        sizeof(countBuffer),
        "%lu",
        static_cast<unsigned long>(
            recoveredManualRecordCount
        )
    );

    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);

    tft.drawString(
        countBuffer,
        24,
        216,
        4
    );

    const bool fileAvailable =
        recoveryFileAvailable();

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(
        fileAvailable
            ? COLOR_OK
            : COLOR_ERROR,
        COLOR_PANEL
    );

    tft.drawString(
        fileAvailable
            ? "SD SUBOR JE DOSTUPNY"
            : "SD SUBOR NIE JE DOSTUPNY",
        160,
        260,
        1
    );

    drawButton(
        RECOVERY_CONTINUE_RECT,
        "POKRACOVAT",
        fileAvailable
            ? COLOR_PANEL
            : COLOR_BACKGROUND,
        fileAvailable
            ? COLOR_OK
            : COLOR_DISABLED,
        fileAvailable
            ? COLOR_TEXT
            : COLOR_DISABLED,
        2
    );

    drawButton(
        RECOVERY_END_RECT,
        "UKONCIT",
        fileAvailable
            ? COLOR_PANEL
            : COLOR_BACKGROUND,
        fileAvailable
            ? COLOR_WARNING
            : COLOR_DISABLED,
        fileAvailable
            ? COLOR_TEXT
            : COLOR_DISABLED,
        2
    );

    drawButton(
        RECOVERY_NEW_RECT,
        "NOVA",
        COLOR_PANEL,
        COLOR_ERROR,
        COLOR_TEXT,
        2
    );

    tft.setTextDatum(MC_DATUM);

    const bool hasRecoveryMessage =
        statusMessage[0] != '\0';

    tft.setTextColor(
        hasRecoveryMessage
            ? statusMessageColor
            : COLOR_SECONDARY_TEXT,
        COLOR_BACKGROUND
    );

    tft.drawString(
        hasRecoveryMessage
            ? statusMessage
            : "NOVA opusti povodnu relaciu.",
        160,
        440,
        1
    );
}

void resumePendingSession()
{
    if (!ensureSDReady(true))
    {
        setStatusMessage(
            "SD KARTA NIE JE DOSTUPNA",
            COLOR_ERROR
        );

        drawRecoveryScreen();
        return;
    }

    if (!recoveryFileAvailable())
    {
        setStatusMessage(
            "SUBOR RELACIE SA NENASIEL",
            COLOR_ERROR
        );

        drawRecoveryScreen();
        return;
    }

    restoreRecoveredSessionToRuntime();
    currentMeasurement = readMeasurement();

    const bool resumeWriteOk =
        appendMeasurementToSession(
            currentMeasurement,
            "SESSION_RESUME",
            station,
            targetPoint
        );

    if (!resumeWriteOk)
    {
        sessionActive = false;

        setStatusMessage(
            "RELACIU SA NEPODARILO OBNOVIT",
            COLOR_ERROR
        );

        drawRecoveryScreen();
        return;
    }

    pendingSessionRecovery = false;
    persistActiveSessionState();

    setStatusMessage(
        "RELACIA OBNOVENA",
        COLOR_OK
    );

    drawMainScreen();
}

void endPendingSession()
{
    if (
        !ensureSDReady(true) ||
        !recoveryFileAvailable()
    )
    {
        setStatusMessage(
            "RELACIU NIE JE MOZNE UKONCIT BEZ SUBORU",
            COLOR_ERROR
        );

        drawRecoveryScreen();
        return;
    }

    restoreRecoveredSessionToRuntime();
    currentMeasurement = readMeasurement();

    const bool endWriteOk =
        appendMeasurementToSession(
            currentMeasurement,
            "SESSION_END_RECOVERY",
            station,
            ""
        );

    if (!endWriteOk)
    {
        sessionActive = false;

        setStatusMessage(
            "CHYBA UKONCENIA OBNOVENEJ RELACIE",
            COLOR_ERROR
        );

        drawRecoveryScreen();
        return;
    }

    clearPersistentSessionState();
    resetRuntimeSessionState();

    pendingSessionRecovery = false;

    recoveredStation[0] = '\0';
    recoveredTargetPoint[0] = '\0';
    recoveredSessionId[0] = '\0';
    recoveredSessionFilePath[0] = '\0';
    recoveredManualRecordCount = 0;

    setStatusMessage(
        "NEDOKONCENA RELACIA BOLA UZAVRETA",
        COLOR_OK
    );

    drawMainScreen();
}

void abandonPendingSession()
{
    if (
        ensureSDReady(true) &&
        recoveryFileAvailable()
    )
    {
        restoreRecoveredSessionToRuntime();
        currentMeasurement = readMeasurement();

        appendMeasurementToSession(
            currentMeasurement,
            "SESSION_ABANDONED",
            station,
            ""
        );
    }

    clearPersistentSessionState();
    resetRuntimeSessionState();

    pendingSessionRecovery = false;

    recoveredStation[0] = '\0';
    recoveredTargetPoint[0] = '\0';
    recoveredSessionId[0] = '\0';
    recoveredSessionFilePath[0] = '\0';
    recoveredManualRecordCount = 0;

    setStatusMessage(
        "POVODNA RELACIA OPUSTENA",
        COLOR_WARNING
    );

    openKeyboard(EditField::Station);
}

void processRecoveryTouch(
    int16_t x,
    int16_t y
)
{
    if (RECOVERY_CONTINUE_RECT.contains(x, y))
    {
        resumePendingSession();
        return;
    }

    if (RECOVERY_END_RECT.contains(x, y))
    {
        endPendingSession();
        return;
    }

    if (RECOVERY_NEW_RECT.contains(x, y))
    {
        abandonPendingSession();
    }
}

// ============================================================
// OBRAZOVKA NASTAVENI
// ============================================================

void drawDateTimeEditor();

bool isLeapYear(int16_t year)
{
    return
        (year % 4 == 0 && year % 100 != 0) ||
        year % 400 == 0;
}

uint8_t daysInMonth(
    int16_t year,
    uint8_t month
)
{
    static const uint8_t DAYS[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    if (month < 1 || month > 12)
    {
        return 31;
    }

    if (month == 2 && isLeapYear(year))
    {
        return 29;
    }

    return DAYS[month - 1];
}

void fillDateTimeDigitsFromRtc()
{
    if (!rtcReady)
    {
        dateTimeDigits[0] = '\0';
        return;
    }

    const DateTime now = rtc.now();

    snprintf(
        dateTimeDigits,
        sizeof(dateTimeDigits),
        "%02u%02u%04u%02u%02u%02u",
        now.day(),
        now.month(),
        now.year(),
        now.hour(),
        now.minute(),
        now.second()
    );
}

void formatDateTimeDigits(
    char *destination,
    size_t destinationSize
)
{
    char padded[15] = "______________";

    const size_t length = strlen(dateTimeDigits);

    for (
        size_t index = 0;
        index < length && index < 14;
        index++
    )
    {
        padded[index] = dateTimeDigits[index];
    }

    snprintf(
        destination,
        destinationSize,
        "%c%c.%c%c.%c%c%c%c  %c%c:%c%c:%c%c",
        padded[0],
        padded[1],
        padded[2],
        padded[3],
        padded[4],
        padded[5],
        padded[6],
        padded[7],
        padded[8],
        padded[9],
        padded[10],
        padded[11],
        padded[12],
        padded[13]
    );
}

bool parseDateTimeDigits(DateTime &result)
{
    if (strlen(dateTimeDigits) != 14)
    {
        return false;
    }

    for (size_t index = 0; index < 14; index++)
    {
        if (
            dateTimeDigits[index] < '0' ||
            dateTimeDigits[index] > '9'
        )
        {
            return false;
        }
    }

    const uint8_t day =
        (dateTimeDigits[0] - '0') * 10 +
        (dateTimeDigits[1] - '0');

    const uint8_t month =
        (dateTimeDigits[2] - '0') * 10 +
        (dateTimeDigits[3] - '0');

    const int16_t year =
        (dateTimeDigits[4] - '0') * 1000 +
        (dateTimeDigits[5] - '0') * 100 +
        (dateTimeDigits[6] - '0') * 10 +
        (dateTimeDigits[7] - '0');

    const uint8_t hour =
        (dateTimeDigits[8] - '0') * 10 +
        (dateTimeDigits[9] - '0');

    const uint8_t minute =
        (dateTimeDigits[10] - '0') * 10 +
        (dateTimeDigits[11] - '0');

    const uint8_t second =
        (dateTimeDigits[12] - '0') * 10 +
        (dateTimeDigits[13] - '0');

    if (
        year < 2020 ||
        year > 2099 ||
        month < 1 ||
        month > 12 ||
        day < 1 ||
        day > daysInMonth(year, month) ||
        hour > 23 ||
        minute > 59 ||
        second > 59
    )
    {
        return false;
    }

    result = DateTime(
        year,
        month,
        day,
        hour,
        minute,
        second
    );

    return true;
}

void appendDateTimeDigit(char digit)
{
    const size_t length = strlen(dateTimeDigits);

    if (length >= 14)
    {
        return;
    }

    dateTimeDigits[length] = digit;
    dateTimeDigits[length + 1] = '\0';
}

void removeDateTimeDigit()
{
    const size_t length = strlen(dateTimeDigits);

    if (length == 0)
    {
        return;
    }

    dateTimeDigits[length - 1] = '\0';
}

void clearDateTimeDigits()
{
    dateTimeDigits[0] = '\0';
}

// ============================================================
// KALIBRACIA - POMOCNE FUNKCIE A OBRAZOVKY
// ============================================================

const char *calibrationParameterName(
    CalibrationParameter parameter
)
{
    switch (parameter)
    {
        case CalibrationParameter::Temperature:
            return "TEPLOTA";

        case CalibrationParameter::Humidity:
            return "VLHKOST";

        case CalibrationParameter::Pressure:
            return "TLAK";
    }

    return "PARAMETER";
}

const char *calibrationParameterUnit(
    CalibrationParameter parameter
)
{
    switch (parameter)
    {
        case CalibrationParameter::Temperature:
            return "C";

        case CalibrationParameter::Humidity:
            return "%";

        case CalibrationParameter::Pressure:
            return "hPa";
    }

    return "";
}

float selectedCalibrationScale(
    const CalibrationConfig &config
)
{
    switch (calibrationParameter)
    {
        case CalibrationParameter::Temperature:
            return config.temperatureScale;

        case CalibrationParameter::Humidity:
            return config.humidityScale;

        case CalibrationParameter::Pressure:
            return config.pressureScale;
    }

    return DEFAULT_CALIBRATION_SCALE;
}

float selectedCalibrationOffset(
    const CalibrationConfig &config
)
{
    switch (calibrationParameter)
    {
        case CalibrationParameter::Temperature:
            return config.temperatureOffsetC;

        case CalibrationParameter::Humidity:
            return config.humidityOffsetPercent;

        case CalibrationParameter::Pressure:
            return config.pressureOffsetHpa;
    }

    return DEFAULT_CALIBRATION_OFFSET;
}

float selectedRawMeasurementValue(
    const Measurement &measurement
)
{
    switch (calibrationParameter)
    {
        case CalibrationParameter::Temperature:
            return measurement.rawTemperatureC;

        case CalibrationParameter::Humidity:
            return measurement.rawHumidityPercent;

        case CalibrationParameter::Pressure:
            return measurement.rawPressureHpa;
    }

    return NAN;
}

void setSelectedPendingCalibrationValue(
    CalibrationEditField field,
    float value
)
{
    switch (calibrationParameter)
    {
        case CalibrationParameter::Temperature:
            if (field == CalibrationEditField::Scale)
            {
                pendingCalibration.temperatureScale = value;
            }
            else
            {
                pendingCalibration.temperatureOffsetC = value;
            }
            break;

        case CalibrationParameter::Humidity:
            if (field == CalibrationEditField::Scale)
            {
                pendingCalibration.humidityScale = value;
            }
            else
            {
                pendingCalibration.humidityOffsetPercent = value;
            }
            break;

        case CalibrationParameter::Pressure:
            if (field == CalibrationEditField::Scale)
            {
                pendingCalibration.pressureScale = value;
            }
            else
            {
                pendingCalibration.pressureOffsetHpa = value;
            }
            break;
    }
}

void selectedCalibrationLimits(
    CalibrationEditField field,
    float &minimum,
    float &maximum
)
{
    if (field == CalibrationEditField::Scale)
    {
        minimum = MIN_CALIBRATION_SCALE;
        maximum = MAX_CALIBRATION_SCALE;
        return;
    }

    switch (calibrationParameter)
    {
        case CalibrationParameter::Temperature:
            minimum = MIN_TEMPERATURE_OFFSET_C;
            maximum = MAX_TEMPERATURE_OFFSET_C;
            break;

        case CalibrationParameter::Humidity:
            minimum = MIN_HUMIDITY_OFFSET_PERCENT;
            maximum = MAX_HUMIDITY_OFFSET_PERCENT;
            break;

        case CalibrationParameter::Pressure:
            minimum = MIN_PRESSURE_OFFSET_HPA;
            maximum = MAX_PRESSURE_OFFSET_HPA;
            break;
    }
}

void drawCalibrationTabs()
{
    const CalibrationParameter parameters[] = {
        CalibrationParameter::Temperature,
        CalibrationParameter::Humidity,
        CalibrationParameter::Pressure
    };

    const Rect rectangles[] = {
        CALIBRATION_TEMPERATURE_RECT,
        CALIBRATION_HUMIDITY_RECT,
        CALIBRATION_PRESSURE_RECT
    };

    const char *labels[] = {
        "TEPLOTA",
        "VLHKOST",
        "TLAK"
    };

    for (uint8_t index = 0; index < 3; index++)
    {
        const bool selected =
            calibrationParameter == parameters[index];

        drawButton(
            rectangles[index],
            labels[index],
            selected ? COLOR_PRIMARY : COLOR_PANEL,
            selected ? COLOR_PRIMARY : COLOR_PANEL_BORDER,
            selected ? TFT_BLACK : COLOR_TEXT,
            1
        );
    }
}

void drawCalibrationLivePanel(
    bool drawStaticElements
)
{
    if (currentScreen != Screen::Calibration)
    {
        return;
    }

    if (drawStaticElements)
    {
        drawPanel(CALIBRATION_LIVE_RECT);

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(
            COLOR_SECONDARY_TEXT,
            COLOR_PANEL
        );

        tft.drawString(
            "ZIVE POROVNANIE",
            20,
            106,
            1
        );

        tft.drawString(
            "MERANA",
            20,
            134,
            1
        );

        tft.drawString(
            "KORIGOVANA",
            20,
            160,
            1
        );
    }

    const float rawValue =
        selectedRawMeasurementValue(
            currentMeasurement
        );

    const float correctedValue =
        isfinite(rawValue)
            ? applyLinearCalibration(
                rawValue,
                selectedCalibrationScale(
                    pendingCalibration
                ),
                selectedCalibrationOffset(
                    pendingCalibration
                ),
                pendingCalibration.enabled
            )
            : NAN;

    char rawBuffer[24];
    char correctedBuffer[24];

    if (isfinite(rawValue))
    {
        snprintf(
            rawBuffer,
            sizeof(rawBuffer),
            "%.3f %s",
            rawValue,
            calibrationParameterUnit(
                calibrationParameter
            )
        );
    }
    else
    {
        snprintf(
            rawBuffer,
            sizeof(rawBuffer),
            "---"
        );
    }

    if (isfinite(correctedValue))
    {
        snprintf(
            correctedBuffer,
            sizeof(correctedBuffer),
            "%.3f %s",
            correctedValue,
            calibrationParameterUnit(
                calibrationParameter
            )
        );
    }
    else
    {
        snprintf(
            correctedBuffer,
            sizeof(correctedBuffer),
            "---"
        );
    }

    // Prekreslia sa iba prave casti s hodnotami.
    tft.fillRect(150, 121, 152, 25, COLOR_PANEL);
    tft.fillRect(150, 147, 152, 25, COLOR_PANEL);

    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
    tft.drawString(rawBuffer, 300, 134, 2);

    tft.setTextColor(
        pendingCalibration.enabled
            ? COLOR_ACCENT
            : COLOR_SECONDARY_TEXT,
        COLOR_PANEL
    );

    tft.drawString(
        correctedBuffer,
        300,
        160,
        2
    );
}

void drawCalibrationCoefficientFields()
{
    char scaleBuffer[32];
    char offsetBuffer[32];

    snprintf(
        scaleBuffer,
        sizeof(scaleBuffer),
        "a = %.6f",
        selectedCalibrationScale(
            pendingCalibration
        )
    );

    snprintf(
        offsetBuffer,
        sizeof(offsetBuffer),
        "b = %+.3f %s",
        selectedCalibrationOffset(
            pendingCalibration
        ),
        calibrationParameterUnit(
            calibrationParameter
        )
    );

    drawButton(
        CALIBRATION_SCALE_RECT,
        scaleBuffer,
        COLOR_PANEL,
        COLOR_PRIMARY,
        COLOR_TEXT,
        2
    );

    drawButton(
        CALIBRATION_OFFSET_RECT,
        offsetBuffer,
        COLOR_PANEL,
        COLOR_PRIMARY,
        COLOR_TEXT,
        2
    );
}

void drawCalibrationScreen()
{
    currentScreen = Screen::Calibration;

    tft.fillScreen(COLOR_BACKGROUND);
    tft.fillRect(0, 0, 320, 42, COLOR_PANEL);
    tft.drawFastHLine(0, 41, 320, COLOR_PANEL_BORDER);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
    tft.drawString("KALIBRACIA", 160, 21, 2);

    drawCalibrationTabs();
    drawCalibrationLivePanel(true);
    drawCalibrationCoefficientFields();

    drawButton(
        CALIBRATION_ENABLED_RECT,
        pendingCalibration.enabled
            ? "KALIBRACIA: ZAPNUTA"
            : "KALIBRACIA: VYPNUTA",
        pendingCalibration.enabled
            ? COLOR_PRIMARY
            : COLOR_PANEL,
        pendingCalibration.enabled
            ? COLOR_PRIMARY
            : COLOR_PANEL_BORDER,
        pendingCalibration.enabled
            ? TFT_BLACK
            : COLOR_TEXT,
        1
    );

    drawPanel(CALIBRATION_INFO_RECT);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(
        COLOR_SECONDARY_TEXT,
        COLOR_PANEL
    );

    tft.drawString(
        "KOREKCIA: y = a*x + b",
        160,
        373,
        1
    );

    tft.drawString(
        "ZMENY SU MOZNE MIMO RELACIE",
        160,
        388,
        1
    );

    drawButton(
        CALIBRATION_BACK_RECT,
        "SPAT",
        COLOR_PANEL,
        COLOR_PANEL_BORDER,
        COLOR_TEXT,
        2
    );

    drawButton(
        CALIBRATION_SAVE_RECT,
        "ULOZIT",
        COLOR_PRIMARY,
        COLOR_PRIMARY,
        TFT_BLACK,
        2
    );
}

void openCalibration()
{
    if (sessionActive)
    {
        setStatusMessage(
            "KALIBRACIU ZMEN AZ PO UKONCENI RELACIE",
            COLOR_WARNING,
            4500
        );

        drawMainScreen();
        return;
    }

    pendingCalibration = calibration;
    calibrationParameter =
        CalibrationParameter::Temperature;
    calibrationEditField =
        CalibrationEditField::None;
    calibrationEditorError[0] = '\0';

    drawCalibrationScreen();
}

void saveCalibrationAndClose()
{
    if (
        sessionActive ||
        !calibrationConfigIsValid(
            pendingCalibration
        )
    )
    {
        return;
    }

    calibration = pendingCalibration;
    saveCalibrationPersistentSettings();

    currentMeasurement = readMeasurement();

    setStatusMessage(
        calibration.enabled
            ? "KALIBRACIA ULOZENA A ZAPNUTA"
            : "KALIBRACIA ULOZENA A VYPNUTA",
        COLOR_OK,
        3500
    );

    Serial.printf(
        "USPORA ENERGIE: %s | stlmenie=%u s | vypnutie=+%u s\n",
        powerSavingEnabled ? "ON" : "OFF",
        powerDimTimeoutSeconds,
        POWER_OFF_DELAY_SECONDS
    );

    Serial.printf(
        "KALIBRACIA: %s | "
        "T a=%.6f b=%+.3f | "
        "RH a=%.6f b=%+.3f | "
        "P a=%.6f b=%+.3f\n",
        calibration.enabled ? "ON" : "OFF",
        calibration.temperatureScale,
        calibration.temperatureOffsetC,
        calibration.humidityScale,
        calibration.humidityOffsetPercent,
        calibration.pressureScale,
        calibration.pressureOffsetHpa
    );

    drawMainScreen();
}

void cancelCalibration()
{
    pendingCalibration = calibration;
    drawSettingsScreen();
}

void openCalibrationEditor(
    CalibrationEditField field
)
{
    calibrationEditField = field;
    calibrationEditorError[0] = '\0';

    const float value =
        field == CalibrationEditField::Scale
            ? selectedCalibrationScale(
                pendingCalibration
            )
            : selectedCalibrationOffset(
                pendingCalibration
            );

    if (field == CalibrationEditField::Scale)
    {
        snprintf(
            calibrationEditBuffer,
            sizeof(calibrationEditBuffer),
            "%.6f",
            value
        );
    }
    else
    {
        snprintf(
            calibrationEditBuffer,
            sizeof(calibrationEditBuffer),
            "%.3f",
            value
        );
    }

    drawCalibrationEditor();
}

void drawCalibrationEditorKeypad()
{
    constexpr int16_t startX = 14;
    constexpr int16_t startY = 150;
    constexpr int16_t gap = 6;
    constexpr int16_t keyWidth = 92;
    constexpr int16_t keyHeight = 53;

    const char *labels[4][3] = {
        {"1", "2", "3"},
        {"4", "5", "6"},
        {"7", "8", "9"},
        {".", "0", "+/-"}
    };

    for (uint8_t row = 0; row < 4; row++)
    {
        for (uint8_t column = 0; column < 3; column++)
        {
            const Rect keyRect{
                static_cast<int16_t>(
                    startX +
                    column * (keyWidth + gap)
                ),
                static_cast<int16_t>(
                    startY +
                    row * (keyHeight + gap)
                ),
                keyWidth,
                keyHeight
            };

            drawButton(
                keyRect,
                labels[row][column],
                COLOR_PANEL,
                COLOR_PANEL_BORDER,
                COLOR_TEXT,
                2
            );
        }
    }
}

void drawCalibrationEditor()
{
    currentScreen = Screen::CalibrationEditor;

    tft.fillScreen(COLOR_BACKGROUND);
    tft.fillRect(0, 0, 320, 42, COLOR_PANEL);
    tft.drawFastHLine(0, 41, 320, COLOR_PANEL_BORDER);

    char title[40];

    snprintf(
        title,
        sizeof(title),
        "%s - %s",
        calibrationParameterName(
            calibrationParameter
        ),
        calibrationEditField ==
            CalibrationEditField::Scale
                ? "KOEFICIENT a"
                : "OFFSET b"
    );

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
    tft.drawString(title, 160, 21, 2);

    tft.fillRoundRect(
        CALIBRATION_EDITOR_DISPLAY_RECT.x,
        CALIBRATION_EDITOR_DISPLAY_RECT.y,
        CALIBRATION_EDITOR_DISPLAY_RECT.width,
        CALIBRATION_EDITOR_DISPLAY_RECT.height,
        7,
        COLOR_INPUT
    );

    tft.drawRoundRect(
        CALIBRATION_EDITOR_DISPLAY_RECT.x,
        CALIBRATION_EDITOR_DISPLAY_RECT.y,
        CALIBRATION_EDITOR_DISPLAY_RECT.width,
        CALIBRATION_EDITOR_DISPLAY_RECT.height,
        7,
        COLOR_PRIMARY
    );

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_INPUT);

    tft.drawString(
        calibrationEditBuffer[0] == '\0'
            ? "0"
            : calibrationEditBuffer,
        CALIBRATION_EDITOR_DISPLAY_RECT.x + 10,
        CALIBRATION_EDITOR_DISPLAY_RECT.y +
            CALIBRATION_EDITOR_DISPLAY_RECT.height / 2,
        2
    );

    drawButton(
        CALIBRATION_EDITOR_BACKSPACE_RECT,
        "<-",
        COLOR_PANEL,
        COLOR_WARNING,
        COLOR_TEXT,
        2
    );

    drawButton(
        CALIBRATION_EDITOR_CLEAR_RECT,
        "VYMAZAT HODNOTU",
        COLOR_BACKGROUND,
        COLOR_PANEL_BORDER,
        COLOR_SECONDARY_TEXT,
        1
    );

    drawCalibrationEditorKeypad();

    tft.fillRect(
        10,
        392,
        300,
        16,
        COLOR_BACKGROUND
    );

    if (calibrationEditorError[0] != '\0')
    {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(
            COLOR_ERROR,
            COLOR_BACKGROUND
        );

        tft.drawString(
            calibrationEditorError,
            160,
            400,
            1
        );
    }

    drawButton(
        CALIBRATION_EDITOR_BACK_RECT,
        "SPAT",
        COLOR_PANEL,
        COLOR_PANEL_BORDER,
        COLOR_TEXT,
        2
    );

    drawButton(
        CALIBRATION_EDITOR_SAVE_RECT,
        "POTVRDIT",
        COLOR_PRIMARY,
        COLOR_PRIMARY,
        TFT_BLACK,
        2
    );
}

void appendCalibrationEditorDigit(char digit)
{
    const size_t length =
        strlen(calibrationEditBuffer);

    if (length >= sizeof(calibrationEditBuffer) - 1)
    {
        return;
    }

    calibrationEditBuffer[length] = digit;
    calibrationEditBuffer[length + 1] = '\0';
}

void appendCalibrationEditorDecimalPoint()
{
    if (strchr(calibrationEditBuffer, '.') != nullptr)
    {
        return;
    }

    if (calibrationEditBuffer[0] == '\0')
    {
        snprintf(
            calibrationEditBuffer,
            sizeof(calibrationEditBuffer),
            "0."
        );
        return;
    }

    appendCalibrationEditorDigit('.');
}

void toggleCalibrationEditorSign()
{
    if (calibrationEditBuffer[0] == '-')
    {
        memmove(
            calibrationEditBuffer,
            calibrationEditBuffer + 1,
            strlen(calibrationEditBuffer)
        );
        return;
    }

    const size_t length =
        strlen(calibrationEditBuffer);

    if (length >= sizeof(calibrationEditBuffer) - 1)
    {
        return;
    }

    memmove(
        calibrationEditBuffer + 1,
        calibrationEditBuffer,
        length + 1
    );

    calibrationEditBuffer[0] = '-';
}

void removeCalibrationEditorCharacter()
{
    const size_t length =
        strlen(calibrationEditBuffer);

    if (length > 0)
    {
        calibrationEditBuffer[length - 1] = '\0';
    }
}

bool saveCalibrationEditorValue()
{
    if (
        calibrationEditBuffer[0] == '\0' ||
        strcmp(calibrationEditBuffer, "-") == 0 ||
        strcmp(calibrationEditBuffer, ".") == 0 ||
        strcmp(calibrationEditBuffer, "-.") == 0
    )
    {
        snprintf(
            calibrationEditorError,
            sizeof(calibrationEditorError),
            "ZADAJ PLATNU HODNOTU"
        );
        return false;
    }

    char *endPointer = nullptr;
    const float value = strtof(
        calibrationEditBuffer,
        &endPointer
    );

    if (
        endPointer == calibrationEditBuffer ||
        *endPointer != '\0' ||
        !isfinite(value)
    )
    {
        snprintf(
            calibrationEditorError,
            sizeof(calibrationEditorError),
            "NEPLATNE CISLO"
        );
        return false;
    }

    float minimum = 0.0F;
    float maximum = 0.0F;

    selectedCalibrationLimits(
        calibrationEditField,
        minimum,
        maximum
    );

    if (value < minimum || value > maximum)
    {
        snprintf(
            calibrationEditorError,
            sizeof(calibrationEditorError),
            "ROZSAH %.2f AZ %.2f",
            minimum,
            maximum
        );
        return false;
    }

    setSelectedPendingCalibrationValue(
        calibrationEditField,
        value
    );

    calibrationEditField =
        CalibrationEditField::None;
    calibrationEditorError[0] = '\0';

    drawCalibrationScreen();
    return true;
}

bool processCalibrationEditorKeypadTouch(
    int16_t x,
    int16_t y
)
{
    constexpr int16_t startX = 14;
    constexpr int16_t startY = 150;
    constexpr int16_t gap = 6;
    constexpr int16_t keyWidth = 92;
    constexpr int16_t keyHeight = 53;

    if (x < startX || y < startY)
    {
        return false;
    }

    const int16_t column =
        (x - startX) / (keyWidth + gap);
    const int16_t row =
        (y - startY) / (keyHeight + gap);

    if (
        column < 0 || column > 2 ||
        row < 0 || row > 3
    )
    {
        return false;
    }

    const Rect keyRect{
        static_cast<int16_t>(
            startX + column * (keyWidth + gap)
        ),
        static_cast<int16_t>(
            startY + row * (keyHeight + gap)
        ),
        keyWidth,
        keyHeight
    };

    if (!keyRect.contains(x, y))
    {
        return false;
    }

    const char *labels[4][3] = {
        {"1", "2", "3"},
        {"4", "5", "6"},
        {"7", "8", "9"},
        {".", "0", "+/-"}
    };

    const char *label = labels[row][column];

    if (strcmp(label, ".") == 0)
    {
        appendCalibrationEditorDecimalPoint();
    }
    else if (strcmp(label, "+/-") == 0)
    {
        toggleCalibrationEditorSign();
    }
    else
    {
        appendCalibrationEditorDigit(label[0]);
    }

    calibrationEditorError[0] = '\0';
    drawCalibrationEditor();
    return true;
}

void processCalibrationEditorTouch(
    int16_t x,
    int16_t y
)
{
    if (CALIBRATION_EDITOR_BACK_RECT.contains(x, y))
    {
        calibrationEditField =
            CalibrationEditField::None;
        drawCalibrationScreen();
        return;
    }

    if (CALIBRATION_EDITOR_SAVE_RECT.contains(x, y))
    {
        if (!saveCalibrationEditorValue())
        {
            drawCalibrationEditor();
        }
        return;
    }

    if (
        CALIBRATION_EDITOR_BACKSPACE_RECT.contains(
            x,
            y
        )
    )
    {
        removeCalibrationEditorCharacter();
        calibrationEditorError[0] = '\0';
        drawCalibrationEditor();
        return;
    }

    if (CALIBRATION_EDITOR_CLEAR_RECT.contains(x, y))
    {
        calibrationEditBuffer[0] = '\0';
        calibrationEditorError[0] = '\0';
        drawCalibrationEditor();
        return;
    }

    processCalibrationEditorKeypadTouch(x, y);
}

void processCalibrationTouch(
    int16_t x,
    int16_t y
)
{
    if (CALIBRATION_BACK_RECT.contains(x, y))
    {
        cancelCalibration();
        return;
    }

    if (CALIBRATION_SAVE_RECT.contains(x, y))
    {
        saveCalibrationAndClose();
        return;
    }

    if (CALIBRATION_TEMPERATURE_RECT.contains(x, y))
    {
        calibrationParameter =
            CalibrationParameter::Temperature;
        drawCalibrationScreen();
        return;
    }

    if (CALIBRATION_HUMIDITY_RECT.contains(x, y))
    {
        calibrationParameter =
            CalibrationParameter::Humidity;
        drawCalibrationScreen();
        return;
    }

    if (CALIBRATION_PRESSURE_RECT.contains(x, y))
    {
        calibrationParameter =
            CalibrationParameter::Pressure;
        drawCalibrationScreen();
        return;
    }

    if (CALIBRATION_SCALE_RECT.contains(x, y))
    {
        openCalibrationEditor(
            CalibrationEditField::Scale
        );
        return;
    }

    if (CALIBRATION_OFFSET_RECT.contains(x, y))
    {
        openCalibrationEditor(
            CalibrationEditField::Offset
        );
        return;
    }

    if (CALIBRATION_ENABLED_RECT.contains(x, y))
    {
        pendingCalibration.enabled =
            !pendingCalibration.enabled;

        drawCalibrationScreen();
    }
}

void drawSettingsDateTimePanel()
{
    drawPanel(SETTINGS_DATETIME_PANEL_RECT);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(
        COLOR_SECONDARY_TEXT,
        COLOR_PANEL
    );

    tft.drawString(
        "DATUM A CAS",
        20,
        62,
        1
    );

    char dateTimeBuffer[28];

    if (rtcReady)
    {
        const DateTime now = rtc.now();

        snprintf(
            dateTimeBuffer,
            sizeof(dateTimeBuffer),
            "%02u.%02u.%04u  %02u:%02u:%02u",
            now.day(),
            now.month(),
            now.year(),
            now.hour(),
            now.minute(),
            now.second()
        );
    }
    else
    {
        snprintf(
            dateTimeBuffer,
            sizeof(dateTimeBuffer),
            "RTC NEDOSTUPNE"
        );
    }

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(
        rtcReady
            ? COLOR_TEXT
            : COLOR_ERROR,
        COLOR_PANEL
    );

    tft.drawString(
        dateTimeBuffer,
        160,
        82,
        2
    );

    drawButton(
        SETTINGS_DATETIME_BUTTON_RECT,
        sessionActive
            ? "CAS ZAMKNUTY POCAS RELACIE"
            : "NASTAVIT DATUM A CAS",
        sessionActive || !rtcReady
            ? COLOR_BACKGROUND
            : COLOR_INPUT,
        sessionActive || !rtcReady
            ? COLOR_DISABLED
            : COLOR_PRIMARY,
        sessionActive || !rtcReady
            ? COLOR_DISABLED
            : COLOR_TEXT,
        1
    );
}

void drawIntervalButtons()
{
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(
        COLOR_SECONDARY_TEXT,
        COLOR_BACKGROUND
    );

    tft.drawString(
        "INTERVAL AUTOMATICKEHO ZAPISU",
        10,
        154,
        1
    );

    for (
        size_t index = 0;
        index < AUTO_LOG_INTERVAL_OPTION_COUNT;
        index++
    )
    {
        char label[12];

        snprintf(
            label,
            sizeof(label),
            "%u s",
            AUTO_LOG_INTERVAL_OPTIONS_SECONDS[index]
        );

        const bool selected =
            pendingAutoLogIntervalSeconds ==
            AUTO_LOG_INTERVAL_OPTIONS_SECONDS[index];

        drawButton(
            SETTINGS_INTERVAL_BUTTON_RECTS[index],
            label,
            selected
                ? COLOR_PRIMARY
                : COLOR_PANEL,
            selected
                ? COLOR_PRIMARY
                : COLOR_PANEL_BORDER,
            selected
                ? TFT_BLACK
                : COLOR_TEXT,
            2
        );
    }
}

uint8_t brightnessFromSliderX(int16_t x)
{
    const int16_t minimumX =
        SETTINGS_BRIGHTNESS_TRACK_RECT.x;

    const int16_t maximumX =
        SETTINGS_BRIGHTNESS_TRACK_RECT.x +
        SETTINGS_BRIGHTNESS_TRACK_RECT.width;

    if (x < minimumX)
    {
        x = minimumX;
    }

    if (x > maximumX)
    {
        x = maximumX;
    }

    const int32_t relative =
        x - minimumX;

    const int32_t percent =
        MIN_BRIGHTNESS_PERCENT +
        relative *
        (MAX_BRIGHTNESS_PERCENT - MIN_BRIGHTNESS_PERCENT) /
        SETTINGS_BRIGHTNESS_TRACK_RECT.width;

    return static_cast<uint8_t>(percent);
}

int16_t sliderXFromBrightness(uint8_t percent)
{
    if (percent < MIN_BRIGHTNESS_PERCENT)
    {
        percent = MIN_BRIGHTNESS_PERCENT;
    }

    if (percent > MAX_BRIGHTNESS_PERCENT)
    {
        percent = MAX_BRIGHTNESS_PERCENT;
    }

    return
        SETTINGS_BRIGHTNESS_TRACK_RECT.x +
        static_cast<int32_t>(
            percent - MIN_BRIGHTNESS_PERCENT
        ) *
        SETTINGS_BRIGHTNESS_TRACK_RECT.width /
        (MAX_BRIGHTNESS_PERCENT - MIN_BRIGHTNESS_PERCENT);
}

void drawBrightnessSlider()
{
    tft.fillRect(
        10,
        238,
        300,
        112,
        COLOR_BACKGROUND
    );

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(
        COLOR_SECONDARY_TEXT,
        COLOR_BACKGROUND
    );

    tft.drawString(
        "JAS DISPLEJA",
        10,
        244,
        1
    );

    char brightnessBuffer[16];

    snprintf(
        brightnessBuffer,
        sizeof(brightnessBuffer),
        "%u %%",
        pendingBrightnessPercent
    );

    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(
        COLOR_ACCENT,
        COLOR_BACKGROUND
    );

    tft.drawString(
        brightnessBuffer,
        310,
        244,
        2
    );

    const int16_t trackY =
        SETTINGS_BRIGHTNESS_TRACK_RECT.y +
        SETTINGS_BRIGHTNESS_TRACK_RECT.height / 2;

    tft.fillRoundRect(
        SETTINGS_BRIGHTNESS_TRACK_RECT.x,
        SETTINGS_BRIGHTNESS_TRACK_RECT.y,
        SETTINGS_BRIGHTNESS_TRACK_RECT.width,
        SETTINGS_BRIGHTNESS_TRACK_RECT.height,
        6,
        COLOR_DISABLED
    );

    const int16_t knobX =
        sliderXFromBrightness(
            pendingBrightnessPercent
        );

    const int16_t filledWidth =
        knobX -
        SETTINGS_BRIGHTNESS_TRACK_RECT.x;

    if (filledWidth > 0)
    {
        tft.fillRoundRect(
            SETTINGS_BRIGHTNESS_TRACK_RECT.x,
            SETTINGS_BRIGHTNESS_TRACK_RECT.y,
            filledWidth,
            SETTINGS_BRIGHTNESS_TRACK_RECT.height,
            6,
            COLOR_PRIMARY
        );
    }

    tft.fillCircle(
        knobX,
        trackY,
        12,
        COLOR_ACCENT
    );

    tft.drawCircle(
        knobX,
        trackY,
        12,
        COLOR_PRIMARY
    );

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(
        COLOR_SECONDARY_TEXT,
        COLOR_BACKGROUND
    );

    tft.drawString(
        "10 %",
        20,
        324,
        1
    );

    tft.setTextDatum(TR_DATUM);

    tft.drawString(
        "100 %",
        300,
        324,
        1
    );
}

void drawSettingsScreen()
{
    currentScreen = Screen::Settings;

    tft.fillScreen(COLOR_BACKGROUND);

    tft.fillRect(0, 0, 320, 44, COLOR_PANEL);
    tft.drawFastHLine(0, 43, 320, COLOR_PANEL_BORDER);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
    tft.drawString("NASTAVENIA", 160, 22, 2);

    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(COLOR_PRIMARY, COLOR_PANEL);
    tft.drawString(APP_VERSION, 312, 22, 1);

    drawSettingsDateTimePanel();
    drawIntervalButtons();
    drawBrightnessSlider();

    drawButton(
        SETTINGS_CALIBRATION_RECT,
        sessionActive
            ? "KALIBRACIA ZAMKNUTA"
            : calibration.enabled
                ? "KALIBRACIA: ON"
                : "KALIBRACIA: OFF",
        sessionActive
            ? COLOR_BACKGROUND
            : COLOR_PANEL,
        sessionActive
            ? COLOR_DISABLED
            : COLOR_PRIMARY,
        sessionActive
            ? COLOR_DISABLED
            : COLOR_TEXT,
        1
    );

    drawButton(
        SETTINGS_POWER_RECT,
        powerSavingEnabled
            ? "USPORA ENERGIE: ON"
            : "USPORA ENERGIE: OFF",
        COLOR_PANEL,
        COLOR_PRIMARY,
        COLOR_TEXT,
        1
    );

    drawButton(
        SETTINGS_BACK_RECT,
        "SPAT",
        COLOR_PANEL,
        COLOR_PANEL_BORDER,
        COLOR_TEXT,
        2
    );

    drawButton(
        SETTINGS_GENERAL_SAVE_RECT,
        "ULOZIT JAS/INT.",
        COLOR_PRIMARY,
        COLOR_PRIMARY,
        TFT_BLACK,
        1
    );
}

void openSettings()
{
    pendingAutoLogIntervalSeconds =
        autoLogIntervalSeconds;

    pendingBrightnessPercent =
        brightnessPercent;

    brightnessSliderDragging = false;

    drawSettingsScreen();
}

void cancelSettings()
{
    applyBrightness(brightnessPercent);
    brightnessSliderDragging = false;
    drawMainScreen();
}

void saveGeneralSettingsAndClose()
{
    autoLogIntervalSeconds =
        pendingAutoLogIntervalSeconds;

    brightnessPercent =
        pendingBrightnessPercent;

    applyBrightness(brightnessPercent);
    saveGeneralPersistentSettings();

    lastAutoLogMillis = millis();

    setStatusMessage(
        "INTERVAL A JAS ULOZENE",
        COLOR_OK
    );

    Serial.printf(
        "NASTAVENIA: interval=%u s, jas=%u %%\n",
        autoLogIntervalSeconds,
        brightnessPercent
    );

    drawMainScreen();
}

void setPendingBrightnessFromTouch(int16_t x)
{
    pendingBrightnessPercent =
        brightnessFromSliderX(x);

    applyBrightness(
        pendingBrightnessPercent
    );

    drawBrightnessSlider();
}

const char *powerTimeoutLabel(
    uint16_t seconds
)
{
    switch (seconds)
    {
        case 30:
            return "30 s";

        case 60:
            return "1 min";

        case 120:
            return "2 min";

        case 300:
            return "5 min";

        default:
            return "?";
    }
}

void drawPowerSettingsScreen()
{
    currentScreen = Screen::PowerSettings;

    tft.fillScreen(COLOR_BACKGROUND);

    tft.fillRect(0, 0, 320, 44, COLOR_PANEL);
    tft.drawFastHLine(
        0,
        43,
        320,
        COLOR_PANEL_BORDER
    );

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
    tft.drawString(
        "USPORA ENERGIE",
        160,
        22,
        2
    );

    drawButton(
        POWER_ENABLED_RECT,
        pendingPowerSavingEnabled
            ? "AUTOMATICKY REZIM: ZAPNUTY"
            : "AUTOMATICKY REZIM: VYPNUTY",
        pendingPowerSavingEnabled
            ? COLOR_PRIMARY
            : COLOR_PANEL,
        pendingPowerSavingEnabled
            ? COLOR_PRIMARY
            : COLOR_PANEL_BORDER,
        pendingPowerSavingEnabled
            ? TFT_BLACK
            : COLOR_TEXT,
        1
    );

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(
        COLOR_SECONDARY_TEXT,
        COLOR_BACKGROUND
    );

    tft.drawString(
        "STLMIT PO NECINNOSTI",
        10,
        132,
        1
    );

    for (
        size_t index = 0;
        index <
            POWER_DIM_TIMEOUT_OPTION_COUNT;
        index++
    )
    {
        const uint16_t option =
            POWER_DIM_TIMEOUT_OPTIONS_SECONDS[
                index
            ];

        const bool selected =
            pendingPowerDimTimeoutSeconds ==
            option;

        drawButton(
            POWER_TIMEOUT_RECTS[index],
            powerTimeoutLabel(option),
            selected
                ? COLOR_PRIMARY
                : COLOR_PANEL,
            selected
                ? COLOR_PRIMARY
                : COLOR_PANEL_BORDER,
            selected
                ? TFT_BLACK
                : COLOR_TEXT,
            1
        );
    }

    drawPanel(POWER_INFO_RECT);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(
        COLOR_ACCENT,
        COLOR_PANEL
    );

    tft.drawString(
        pendingPowerSavingEnabled
            ? "REZIM JE AKTIVNY"
            : "REZIM JE VYPNUTY",
        160,
        260,
        2
    );

    tft.setTextColor(
        COLOR_SECONDARY_TEXT,
        COLOR_PANEL
    );

    char dimInfo[48];

    snprintf(
        dimInfo,
        sizeof(dimInfo),
        "Stlmenie po: %s",
        powerTimeoutLabel(
            pendingPowerDimTimeoutSeconds
        )
    );

    tft.drawString(
        dimInfo,
        160,
        294,
        1
    );

    char offInfo[56];

    snprintf(
        offInfo,
        sizeof(offInfo),
        "Vypnutie o dalsich %u s",
        POWER_OFF_DELAY_SECONDS
    );

    tft.drawString(
        offInfo,
        160,
        318,
        1
    );

    tft.drawString(
        "Meranie a zapis na SD pokracuju",
        160,
        350,
        1
    );

    tft.drawString(
        "Prvy dotyk iba prebudi displej",
        160,
        374,
        1
    );

    drawButton(
        POWER_BACK_RECT,
        "SPAT",
        COLOR_PANEL,
        COLOR_PANEL_BORDER,
        COLOR_TEXT,
        2
    );

    drawButton(
        POWER_SAVE_RECT,
        "ULOZIT",
        COLOR_PRIMARY,
        COLOR_PRIMARY,
        TFT_BLACK,
        2
    );
}

void openPowerSettings()
{
    pendingPowerSavingEnabled =
        powerSavingEnabled;

    pendingPowerDimTimeoutSeconds =
        powerDimTimeoutSeconds;

    drawPowerSettingsScreen();
}

void cancelPowerSettings()
{
    pendingPowerSavingEnabled =
        powerSavingEnabled;

    pendingPowerDimTimeoutSeconds =
        powerDimTimeoutSeconds;

    drawSettingsScreen();
}

void savePowerSettingsAndReturn()
{
    powerSavingEnabled =
        pendingPowerSavingEnabled;

    powerDimTimeoutSeconds =
        pendingPowerDimTimeoutSeconds;

    savePowerPersistentSettings();

    lastUserActivityMillis = millis();
    setDisplayPowerState(
        DisplayPowerState::Awake
    );

    setStatusMessage(
        powerSavingEnabled
            ? "USPORA ENERGIE ZAPNUTA"
            : "USPORA ENERGIE VYPNUTA",
        COLOR_OK
    );

    Serial.printf(
        "USPORA ENERGIE: %s, stlmenie po %u s, vypnutie +%u s\n",
        powerSavingEnabled ? "ON" : "OFF",
        powerDimTimeoutSeconds,
        POWER_OFF_DELAY_SECONDS
    );

    drawSettingsScreen();
}

void processPowerSettingsTouch(
    int16_t x,
    int16_t y
)
{
    if (POWER_BACK_RECT.contains(x, y))
    {
        cancelPowerSettings();
        return;
    }

    if (POWER_SAVE_RECT.contains(x, y))
    {
        savePowerSettingsAndReturn();
        return;
    }

    if (POWER_ENABLED_RECT.contains(x, y))
    {
        pendingPowerSavingEnabled =
            !pendingPowerSavingEnabled;

        drawPowerSettingsScreen();
        return;
    }

    for (
        size_t index = 0;
        index <
            POWER_DIM_TIMEOUT_OPTION_COUNT;
        index++
    )
    {
        if (
            POWER_TIMEOUT_RECTS[index].contains(
                x,
                y
            )
        )
        {
            pendingPowerDimTimeoutSeconds =
                POWER_DIM_TIMEOUT_OPTIONS_SECONDS[
                    index
                ];

            drawPowerSettingsScreen();
            return;
        }
    }
}

void openDateTimeEditor()
{
    if (
        sessionActive ||
        !rtcReady
    )
    {
        return;
    }

    fillDateTimeDigitsFromRtc();
    drawDateTimeEditor();
}

void drawDateTimeEditorDisplay()
{
    drawPanel(DATETIME_DISPLAY_RECT);

    char formatted[32];

    formatDateTimeDigits(
        formatted,
        sizeof(formatted)
    );

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);

    tft.drawString(
        formatted,
        160,
        78,
        2
    );

    tft.setTextColor(
        COLOR_SECONDARY_TEXT,
        COLOR_PANEL
    );

    tft.drawString(
        "DD.MM.RRRR  HH:MM:SS",
        160,
        108,
        1
    );
}

void drawDateTimeEditorKeypad()
{
    constexpr int16_t startX = 14;
    constexpr int16_t startY = 142;
    constexpr int16_t gap = 7;
    constexpr int16_t keyWidth = 92;
    constexpr int16_t keyHeight = 58;

    const char *labels[4][3] = {
        {"1", "2", "3"},
        {"4", "5", "6"},
        {"7", "8", "9"},
        {"CLR", "0", "<-"}
    };

    for (uint8_t row = 0; row < 4; row++)
    {
        for (uint8_t column = 0; column < 3; column++)
        {
            const Rect keyRect{
                static_cast<int16_t>(
                    startX +
                    column *
                    (keyWidth + gap)
                ),
                static_cast<int16_t>(
                    startY +
                    row *
                    (keyHeight + gap)
                ),
                keyWidth,
                keyHeight
            };

            drawButton(
                keyRect,
                labels[row][column],
                COLOR_PANEL,
                labels[row][column][0] >= '0' &&
                labels[row][column][0] <= '9'
                    ? COLOR_PRIMARY
                    : COLOR_PANEL_BORDER,
                COLOR_TEXT,
                2
            );
        }
    }
}

void drawDateTimeEditor()
{
    currentScreen = Screen::DateTimeEditor;

    tft.fillScreen(COLOR_BACKGROUND);

    tft.fillRect(0, 0, 320, 42, COLOR_PANEL);
    tft.drawFastHLine(0, 41, 320, COLOR_PANEL_BORDER);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);
    tft.drawString("DATUM A CAS", 160, 21, 2);

    drawDateTimeEditorDisplay();
    drawDateTimeEditorKeypad();

    drawButton(
        DATETIME_BACK_RECT,
        "SPAT",
        COLOR_PANEL,
        COLOR_PANEL_BORDER,
        COLOR_TEXT,
        2
    );

    const bool validLength =
        strlen(dateTimeDigits) == 14;

    drawButton(
        DATETIME_SAVE_RECT,
        "ULOZIT CAS",
        validLength
            ? COLOR_PRIMARY
            : COLOR_BACKGROUND,
        validLength
            ? COLOR_PRIMARY
            : COLOR_DISABLED,
        validLength
            ? TFT_BLACK
            : COLOR_DISABLED,
        2
    );
}

void saveDateTimeAndReturn()
{
    if (
        sessionActive ||
        !rtcReady
    )
    {
        return;
    }

    DateTime parsedDateTime(
        2026,
        1,
        1,
        0,
        0,
        0
    );

    if (!parseDateTimeDigits(parsedDateTime))
    {
        drawDateTimeEditor();

        tft.fillRect(
            0,
            392,
            320,
            18,
            COLOR_BACKGROUND
        );

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(
            COLOR_ERROR,
            COLOR_BACKGROUND
        );

        tft.drawString(
            "NEPLATNY DATUM ALEBO CAS",
            160,
            401,
            1
        );

        return;
    }

    rtc.adjust(parsedDateTime);
    currentMeasurement = readMeasurement();

    setStatusMessage(
        "DATUM A CAS ULOZENY",
        COLOR_OK
    );

    Serial.printf(
        "RTC: %04u-%02u-%02u %02u:%02u:%02u\n",
        parsedDateTime.year(),
        parsedDateTime.month(),
        parsedDateTime.day(),
        parsedDateTime.hour(),
        parsedDateTime.minute(),
        parsedDateTime.second()
    );

    drawSettingsScreen();
}

bool processDateTimeKeypadTouch(
    int16_t x,
    int16_t y
)
{
    constexpr int16_t startX = 14;
    constexpr int16_t startY = 142;
    constexpr int16_t gap = 7;
    constexpr int16_t keyWidth = 92;
    constexpr int16_t keyHeight = 58;

    if (
        x < startX ||
        y < startY
    )
    {
        return false;
    }

    const int16_t column =
        (x - startX) /
        (keyWidth + gap);

    const int16_t row =
        (y - startY) /
        (keyHeight + gap);

    if (
        column < 0 ||
        column > 2 ||
        row < 0 ||
        row > 3
    )
    {
        return false;
    }

    const Rect keyRect{
        static_cast<int16_t>(
            startX +
            column *
            (keyWidth + gap)
        ),
        static_cast<int16_t>(
            startY +
            row *
            (keyHeight + gap)
        ),
        keyWidth,
        keyHeight
    };

    if (!keyRect.contains(x, y))
    {
        return false;
    }

    const char *labels[4][3] = {
        {"1", "2", "3"},
        {"4", "5", "6"},
        {"7", "8", "9"},
        {"CLR", "0", "<-"}
    };

    const char *label =
        labels[row][column];

    if (strcmp(label, "CLR") == 0)
    {
        clearDateTimeDigits();
    }
    else if (strcmp(label, "<-") == 0)
    {
        removeDateTimeDigit();
    }
    else
    {
        appendDateTimeDigit(
            label[0]
        );
    }

    drawDateTimeEditor();

    return true;
}

void processDateTimeEditorTouch(
    int16_t x,
    int16_t y
)
{
    if (DATETIME_BACK_RECT.contains(x, y))
    {
        drawSettingsScreen();
        return;
    }

    if (DATETIME_SAVE_RECT.contains(x, y))
    {
        saveDateTimeAndReturn();
        return;
    }

    processDateTimeKeypadTouch(x, y);
}

void processSettingsTouch(
    int16_t x,
    int16_t y
)
{
    if (SETTINGS_BACK_RECT.contains(x, y))
    {
        cancelSettings();
        return;
    }

    if (SETTINGS_GENERAL_SAVE_RECT.contains(x, y))
    {
        saveGeneralSettingsAndClose();
        return;
    }

    if (SETTINGS_DATETIME_BUTTON_RECT.contains(x, y))
    {
        openDateTimeEditor();
        return;
    }

    if (SETTINGS_POWER_RECT.contains(x, y))
    {
        openPowerSettings();
        return;
    }

    if (SETTINGS_CALIBRATION_RECT.contains(x, y))
    {
        if (sessionActive)
        {
            setStatusMessage(
                "KALIBRACIU ZMEN AZ PO UKONCENI RELACIE",
                COLOR_WARNING,
                4500
            );
            drawMainScreen();
            return;
        }

        openCalibration();
        return;
    }

    for (
        size_t index = 0;
        index < AUTO_LOG_INTERVAL_OPTION_COUNT;
        index++
    )
    {
        if (
            SETTINGS_INTERVAL_BUTTON_RECTS[index].contains(
                x,
                y
            )
        )
        {
            pendingAutoLogIntervalSeconds =
                AUTO_LOG_INTERVAL_OPTIONS_SECONDS[index];

            drawIntervalButtons();
            return;
        }
    }

    if (SETTINGS_BRIGHTNESS_TOUCH_RECT.contains(x, y))
    {
        brightnessSliderDragging = true;
        setPendingBrightnessFromTouch(x);
    }
}

// ============================================================
// KLAVESNICA - POMOCNE FUNKCIE
// ============================================================

const char *editedFieldTitle()
{
    switch (editedField)
    {
        case EditField::Station:
            return "STANOVISKO";

        case EditField::TargetPoint:
            return "MERANY BOD";

        case EditField::None:
        default:
            return "IDENTIFIKATOR";
    }
}

void appendEditCharacter(char character)
{
    const size_t currentLength = strlen(editBuffer);

    if (currentLength >= IDENTIFIER_MAX_LENGTH)
    {
        setStatusMessage(
            "MAXIMALNE 16 ZNAKOV",
            COLOR_WARNING
        );
        return;
    }

    editBuffer[currentLength] = character;
    editBuffer[currentLength + 1] = '\0';
}

void removeLastEditCharacter()
{
    const size_t currentLength = strlen(editBuffer);

    if (currentLength == 0)
    {
        return;
    }

    editBuffer[currentLength - 1] = '\0';
}

void clearEditBuffer()
{
    editBuffer[0] = '\0';
}

void drawKeyboardHeader()
{
    tft.fillRect(0, 0, 320, 40, COLOR_PANEL);
    tft.drawFastHLine(0, 39, 320, COLOR_PANEL_BORDER);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_PANEL);

    tft.drawString(
        editedFieldTitle(),
        160,
        20,
        2
    );
}

void drawKeyboardInput()
{
    tft.fillRoundRect(
        KEYBOARD_INPUT_RECT.x,
        KEYBOARD_INPUT_RECT.y,
        KEYBOARD_INPUT_RECT.width,
        KEYBOARD_INPUT_RECT.height,
        7,
        COLOR_INPUT
    );

    tft.drawRoundRect(
        KEYBOARD_INPUT_RECT.x,
        KEYBOARD_INPUT_RECT.y,
        KEYBOARD_INPUT_RECT.width,
        KEYBOARD_INPUT_RECT.height,
        7,
        COLOR_PRIMARY
    );

    tft.setTextDatum(ML_DATUM);

    if (identifierIsEmpty(editBuffer))
    {
        tft.setTextColor(COLOR_SECONDARY_TEXT, COLOR_INPUT);
        tft.drawString(
            "ZADAJ HODNOTU",
            KEYBOARD_INPUT_RECT.x + 10,
            KEYBOARD_INPUT_RECT.y + KEYBOARD_INPUT_RECT.height / 2,
            2
        );
    }
    else
    {
        tft.setTextColor(COLOR_TEXT, COLOR_INPUT);
        tft.drawString(
            editBuffer,
            KEYBOARD_INPUT_RECT.x + 10,
            KEYBOARD_INPUT_RECT.y + KEYBOARD_INPUT_RECT.height / 2,
            2
        );
    }

    drawButton(
        KEYBOARD_BACKSPACE_RECT,
        "<-",
        COLOR_PANEL,
        COLOR_WARNING,
        COLOR_TEXT,
        2
    );
}

void drawKey(
    const Rect &rect,
    const char *label,
    bool active = true
)
{
    drawButton(
        rect,
        label,
        active ? COLOR_PANEL : COLOR_BACKGROUND,
        active ? COLOR_PANEL_BORDER : COLOR_DISABLED,
        active ? COLOR_TEXT : COLOR_DISABLED,
        2
    );
}

void drawNumericKeyboard()
{
    constexpr int16_t startX = 10;
    constexpr int16_t startY = 106;
    constexpr int16_t gap = 6;
    constexpr int16_t keyWidth = 70;
    constexpr int16_t keyHeight = 68;

    const char *labels[4][4] = {
        {"1", "2", "3", "_"},
        {"4", "5", "6", "."},
        {"7", "8", "9", ","},
        {"-", "0", "CLR", ""}
    };

    for (uint8_t row = 0; row < 4; row++)
    {
        for (uint8_t column = 0; column < 4; column++)
        {
            const Rect keyRect{
                static_cast<int16_t>(
                    startX + column * (keyWidth + gap)
                ),
                static_cast<int16_t>(
                    startY + row * (keyHeight + gap)
                ),
                keyWidth,
                keyHeight
            };

            drawKey(
                keyRect,
                labels[row][column]
            );
        }
    }
}

void drawAlphabeticKeyboard()
{
    constexpr int16_t startX = 8;
    constexpr int16_t startY = 106;
    constexpr int16_t gap = 4;
    constexpr int16_t keyWidth = 47;
    constexpr int16_t keyHeight = 56;

    const char *labels[5][6] = {
        {"A", "B", "C", "D", "E", "F"},
        {"G", "H", "I", "J", "K", "L"},
        {"M", "N", "O", "P", "Q", "R"},
        {"S", "T", "U", "V", "W", "X"},
        {"Y", "Z", "_", ".", ",", "-"}
    };

    for (uint8_t row = 0; row < 5; row++)
    {
        for (uint8_t column = 0; column < 6; column++)
        {
            const Rect keyRect{
                static_cast<int16_t>(
                    startX + column * (keyWidth + gap)
                ),
                static_cast<int16_t>(
                    startY + row * (keyHeight + gap)
                ),
                keyWidth,
                keyHeight
            };

            drawKey(
                keyRect,
                labels[row][column]
            );
        }
    }
}

void drawKeyboardFooter()
{
    drawButton(
        KEYBOARD_MODE_RECT,
        keyboardMode == KeyboardMode::Numeric
            ? "ABC"
            : "123",
        COLOR_PANEL,
        COLOR_PRIMARY,
        COLOR_TEXT,
        2
    );

    drawButton(
        KEYBOARD_CANCEL_RECT,
        "ZRUSIT",
        COLOR_PANEL,
        COLOR_WARNING,
        COLOR_TEXT,
        2
    );

    drawButton(
        KEYBOARD_OK_RECT,
        "OK",
        identifierIsEmpty(editBuffer)
            ? COLOR_BACKGROUND
            : COLOR_PANEL,
        identifierIsEmpty(editBuffer)
            ? COLOR_DISABLED
            : COLOR_OK,
        identifierIsEmpty(editBuffer)
            ? COLOR_DISABLED
            : COLOR_TEXT,
        2
    );
}

void drawKeyboardScreen()
{
    currentScreen = Screen::Keyboard;

    tft.fillScreen(COLOR_BACKGROUND);

    drawKeyboardHeader();
    drawKeyboardInput();

    if (keyboardMode == KeyboardMode::Numeric)
    {
        drawNumericKeyboard();
    }
    else
    {
        drawAlphabeticKeyboard();
    }

    drawKeyboardFooter();
}

void openKeyboard(EditField field)
{
    editedField = field;
    keyboardMode = KeyboardMode::Numeric;

    if (field == EditField::Station)
    {
        copyIdentifier(
            editBuffer,
            sizeof(editBuffer),
            station
        );
    }
    else if (field == EditField::TargetPoint)
    {
        copyIdentifier(
            editBuffer,
            sizeof(editBuffer),
            targetPoint
        );
    }
    else
    {
        clearEditBuffer();
    }

    drawKeyboardScreen();
}

void closeKeyboardWithoutSaving()
{
    editedField = EditField::None;
    clearEditBuffer();
    drawMainScreen();
}

// ============================================================
// KLAVESNICA - DETEKCIA KLAVESOV
// ============================================================

bool handleNumericKey(int16_t x, int16_t y)
{
    constexpr int16_t startX = 10;
    constexpr int16_t startY = 106;
    constexpr int16_t gap = 6;
    constexpr int16_t keyWidth = 70;
    constexpr int16_t keyHeight = 68;

    if (
        x < startX ||
        y < startY
    )
    {
        return false;
    }

    const int16_t relativeX = x - startX;
    const int16_t relativeY = y - startY;

    const int16_t column =
        relativeX / (keyWidth + gap);

    const int16_t row =
        relativeY / (keyHeight + gap);

    if (
        column < 0 ||
        column > 3 ||
        row < 0 ||
        row > 3
    )
    {
        return false;
    }

    const int16_t keyX =
        startX + column * (keyWidth + gap);

    const int16_t keyY =
        startY + row * (keyHeight + gap);

    const Rect keyRect{
        keyX,
        keyY,
        keyWidth,
        keyHeight
    };

    if (!keyRect.contains(x, y))
    {
        return false;
    }

    const char *labels[4][4] = {
        {"1", "2", "3", "_"},
        {"4", "5", "6", "."},
        {"7", "8", "9", ","},
        {"-", "0", "CLR", ""}
    };

    const char *label = labels[row][column];

    if (label[0] == '\0')
    {
        return true;
    }

    if (strcmp(label, "CLR") == 0)
    {
        clearEditBuffer();
        drawKeyboardInput();
        drawKeyboardFooter();
        return true;
    }

    appendEditCharacter(label[0]);
    drawKeyboardInput();
    drawKeyboardFooter();

    return true;
}

bool handleAlphabeticKey(int16_t x, int16_t y)
{
    constexpr int16_t startX = 8;
    constexpr int16_t startY = 106;
    constexpr int16_t gap = 4;
    constexpr int16_t keyWidth = 47;
    constexpr int16_t keyHeight = 56;

    if (
        x < startX ||
        y < startY
    )
    {
        return false;
    }

    const int16_t relativeX = x - startX;
    const int16_t relativeY = y - startY;

    const int16_t column =
        relativeX / (keyWidth + gap);

    const int16_t row =
        relativeY / (keyHeight + gap);

    if (
        column < 0 ||
        column > 5 ||
        row < 0 ||
        row > 4
    )
    {
        return false;
    }

    const Rect keyRect{
        static_cast<int16_t>(
            startX + column * (keyWidth + gap)
        ),
        static_cast<int16_t>(
            startY + row * (keyHeight + gap)
        ),
        keyWidth,
        keyHeight
    };

    if (!keyRect.contains(x, y))
    {
        return false;
    }

    const char *labels[5][6] = {
        {"A", "B", "C", "D", "E", "F"},
        {"G", "H", "I", "J", "K", "L"},
        {"M", "N", "O", "P", "Q", "R"},
        {"S", "T", "U", "V", "W", "X"},
        {"Y", "Z", "_", ".", ",", "-"}
    };

    appendEditCharacter(
        labels[row][column][0]
    );

    drawKeyboardInput();
    drawKeyboardFooter();

    return true;
}

// ============================================================
// CSV - BEZPECNE ZAPISANIE TEXTOVEHO POLA
// Umoznuje pouzit ciarku v stanovisku alebo meranom bode.
// ============================================================

void writeCsvTextField(
    File &file,
    const char *value
)
{
    bool requiresQuotes = false;

    for (const char *character = value; *character != '\0'; character++)
    {
        if (
            *character == ',' ||
            *character == '"' ||
            *character == '\n' ||
            *character == '\r'
        )
        {
            requiresQuotes = true;
            break;
        }
    }

    if (!requiresQuotes)
    {
        file.print(value);
        return;
    }

    file.print('"');

    for (const char *character = value; *character != '\0'; character++)
    {
        if (*character == '"')
        {
            file.print("\"\"");
        }
        else
        {
            file.print(*character);
        }
    }

    file.print('"');
}

// ============================================================
// RELACIA - IDENTIFIKATOR A NAZOV SUBORU
// ============================================================

void sanitizeForFileName(
    const char *source,
    char *destination,
    size_t destinationSize
)
{
    if (destinationSize == 0)
    {
        return;
    }

    size_t writeIndex = 0;

    for (
        size_t readIndex = 0;
        source[readIndex] != '\0' &&
        writeIndex + 1 < destinationSize;
        readIndex++
    )
    {
        const char character = source[readIndex];

        if (
            (character >= 'A' && character <= 'Z') ||
            (character >= 'a' && character <= 'z') ||
            (character >= '0' && character <= '9')
        )
        {
            destination[writeIndex++] = character;
        }
        else
        {
            destination[writeIndex++] = '_';
        }
    }

    destination[writeIndex] = '\0';
}

bool ensureSessionDirectory()
{
    if (!sdReady)
    {
        return false;
    }

    if (SD.exists(SESSION_DIRECTORY))
    {
        return true;
    }

    return SD.mkdir(SESSION_DIRECTORY);
}

bool createSessionFile(
    const char *newStation
)
{
    if (
        !rtcReady ||
        identifierIsEmpty(newStation)
    )
    {
        return false;
    }

    if (!ensureSDReady(true))
    {
        return false;
    }

    if (!ensureSessionDirectory())
    {
        Serial.println(
            "RELACIA: nepodarilo sa vytvorit adresar"
        );
        return false;
    }

    const DateTime now = rtc.now();

    if (
        now.year() < MIN_VALID_RTC_YEAR ||
        now.year() > MAX_VALID_RTC_YEAR ||
        now.month() < 1 ||
        now.month() > 12 ||
        now.day() < 1 ||
        now.day() >
            daysInMonth(
                now.year(),
                now.month()
            )
    )
    {
        Serial.println(
            "RELACIA: NEPLATNY DATUM RTC"
        );

        return false;
    }

    char sanitizedStation[IDENTIFIER_MAX_LENGTH + 1];

    sanitizeForFileName(
        newStation,
        sanitizedStation,
        sizeof(sanitizedStation)
    );

    char baseSessionId[32];

    snprintf(
        baseSessionId,
        sizeof(baseSessionId),
        "%04u%02u%02u_%02u%02u%02u",
        now.year(),
        now.month(),
        now.day(),
        now.hour(),
        now.minute(),
        now.second()
    );

    char candidateSessionId[32] = "";
    char candidateFilePath[80] = "";
    bool uniquePathFound = false;

    for (uint8_t suffix = 0; suffix < 100; suffix++)
    {
        if (suffix == 0)
        {
            snprintf(
                candidateSessionId,
                sizeof(candidateSessionId),
                "%s",
                baseSessionId
            );
        }
        else
        {
            snprintf(
                candidateSessionId,
                sizeof(candidateSessionId),
                "%s_%02u",
                baseSessionId,
                suffix
            );
        }

        snprintf(
            candidateFilePath,
            sizeof(candidateFilePath),
            "%s/%s_%s.csv",
            SESSION_DIRECTORY,
            candidateSessionId,
            sanitizedStation
        );

        if (!SD.exists(candidateFilePath))
        {
            uniquePathFound = true;
            break;
        }
    }

    if (!uniquePathFound)
    {
        Serial.println(
            "RELACIA: nepodarilo sa vytvorit jedinecny nazov"
        );
        return false;
    }

    File logFile = SD.open(
        candidateFilePath,
        FILE_WRITE
    );

    if (!logFile)
    {
        Serial.println(
            "RELACIA: nepodarilo sa vytvorit CSV subor"
        );
        return false;
    }

    logFile.println(
        "session_id,timestamp,record_type,station,target_point,"
        "temperature_c,humidity_pct,pressure_hpa,"
        "battery_voltage_v,battery_percent,"
        "temperature_raw_c,humidity_raw_pct,pressure_raw_hpa,"
        "calibration_enabled,temperature_scale,temperature_offset_c,"
        "humidity_scale,humidity_offset_pct,"
        "pressure_scale,pressure_offset_hpa"
    );

    logFile.flush();

    const bool writeOk =
        logFile.getWriteError() == 0;

    logFile.close();

    if (!writeOk)
    {
        SD.remove(candidateFilePath);

        Serial.println(
            "RELACIA: nepodarilo sa zapisat hlavicku CSV"
        );
        return false;
    }

    copyIdentifier(
        sessionId,
        sizeof(sessionId),
        candidateSessionId
    );

    snprintf(
        sessionFilePath,
        sizeof(sessionFilePath),
        "%s",
        candidateFilePath
    );

    sessionActive = true;
    lastAutoWriteOk = false;
    lastManualWriteOk = false;
    manualRecordCount = 0;
    lastAutoLogMillis = millis();

    Serial.printf(
        "NOVA RELACIA: %s\n",
        sessionId
    );

    Serial.printf(
        "SUBOR: %s\n",
        sessionFilePath
    );

    persistActiveSessionState();

    return true;
}

// ============================================================
// CSV - ZAPIS MERANIA
// ============================================================

bool writeSessionRecordOnce(
    const Measurement &measurement,
    const char *recordType,
    const char *recordStation,
    const char *recordTargetPoint
)
{
    if (
        !sessionActive ||
        sessionFilePath[0] == '\0'
    )
    {
        return false;
    }

    const bool allowInvalidMeasurement =
        recordTypeAllowsInvalidMeasurement(
            recordType
        );

    if (
        !measurement.valid &&
        !allowInvalidMeasurement
    )
    {
        return false;
    }

    if (!SD.exists(sessionFilePath))
    {
        Serial.printf(
            "CSV %s: SUBOR RELACIE SA NENASIEL\n",
            recordType
        );

        return false;
    }

    File logFile = SD.open(
        sessionFilePath,
        FILE_APPEND
    );

    if (!logFile)
    {
        Serial.printf(
            "CSV %s: CHYBA OTVORENIA\n",
            recordType
        );

        return false;
    }

    logFile.print(sessionId);
    logFile.print(',');

    logFile.printf(
        "%04u-%02u-%02uT%02u:%02u:%02u,",
        measurement.timestamp.year(),
        measurement.timestamp.month(),
        measurement.timestamp.day(),
        measurement.timestamp.hour(),
        measurement.timestamp.minute(),
        measurement.timestamp.second()
    );

    logFile.print(recordType);
    logFile.print(',');

    writeCsvTextField(logFile, recordStation);
    logFile.print(',');

    writeCsvTextField(logFile, recordTargetPoint);
    logFile.print(',');

    if (measurement.valid)
    {
        logFile.printf(
            "%.2f,%.2f,%.2f,",
            measurement.temperatureC,
            measurement.humidityPercent,
            measurement.pressureHpa
        );
    }
    else
    {
        // Udalosti SESSION_* sa zapisu aj pri chybe senzora.
        logFile.print(",,,");
    }

    if (batteryAvailable)
    {
        logFile.printf(
            "%.2f,%u,",
            batteryVoltage,
            batteryPercent
        );
    }
    else
    {
        logFile.print(",,");
    }

    if (measurement.valid)
    {
        logFile.printf(
            "%.3f,%.3f,%.3f,",
            measurement.rawTemperatureC,
            measurement.rawHumidityPercent,
            measurement.rawPressureHpa
        );
    }
    else
    {
        logFile.print(",,,");
    }

    logFile.printf(
        "%u,%.6f,%.3f,%.6f,%.3f,%.6f,%.3f\n",
        calibration.enabled ? 1U : 0U,
        calibration.temperatureScale,
        calibration.temperatureOffsetC,
        calibration.humidityScale,
        calibration.humidityOffsetPercent,
        calibration.pressureScale,
        calibration.pressureOffsetHpa
    );

    logFile.flush();

    const bool writeOk =
        logFile.getWriteError() == 0;

    logFile.close();

    return writeOk;
}

bool appendMeasurementToSession(
    const Measurement &measurement,
    const char *recordType,
    const char *recordStation,
    const char *recordTargetPoint
)
{
    if (!sessionActive)
    {
        return false;
    }

    if (
        !measurement.valid &&
        !recordTypeAllowsInvalidMeasurement(
            recordType
        )
    )
    {
        Serial.printf(
            "CSV %s: %s\n",
            recordType,
            measurementErrorText(
                lastMeasurementError
            )
        );

        return false;
    }

    for (
        uint8_t attempt = 0;
        attempt < SD_WRITE_ATTEMPTS;
        attempt++
    )
    {
        const bool forceRecovery =
            attempt > 0;

        if (!ensureSDReady(forceRecovery))
        {
            continue;
        }

        if (
            writeSessionRecordOnce(
                measurement,
                recordType,
                recordStation,
                recordTargetPoint
            )
        )
        {
            Serial.printf(
                "CSV %s: OK\n",
                recordType
            );

            return true;
        }

        sdReady = false;

        Serial.printf(
            "CSV %s: POKUS %u ZLYHAL\n",
            recordType,
            static_cast<unsigned int>(
                attempt + 1
            )
        );
    }

    Serial.printf(
        "CSV %s: CHYBA PO OBNOVE SD\n",
        recordType
    );

    return false;
}


// ============================================================
// UKONCENIE MERACEJ RELACIE
// ============================================================

void endCurrentSession()
{
    if (!sessionActive)
    {
        drawMainScreen();
        return;
    }

    currentMeasurement = readMeasurement();

    const bool endWriteOk =
        appendMeasurementToSession(
            currentMeasurement,
            "SESSION_END",
            station,
            ""
        );

    if (!endWriteOk)
    {
        persistActiveSessionState();

        setStatusMessage(
            "RELACIU NIE JE MOZNE UKONCIT - SKONTROLUJ SD",
            COLOR_ERROR,
            5000
        );

        drawMainScreen();
        return;
    }

    Serial.printf(
        "RELACIA UKONCENA: %s | MANUALNE ZAZNAMY: %lu\n",
        sessionId,
        static_cast<unsigned long>(manualRecordCount)
    );

    clearPersistentSessionState();
    resetRuntimeSessionState();

    setStatusMessage(
        "RELACIA UKONCENA",
        COLOR_OK
    );

    drawMainScreen();
}

void processEndSessionConfirmationTouch(
    int16_t x,
    int16_t y
)
{
    if (CONFIRM_BACK_BUTTON_RECT.contains(x, y))
    {
        drawMainScreen();
        return;
    }

    if (CONFIRM_END_BUTTON_RECT.contains(x, y))
    {
        endCurrentSession();
    }
}

// ============================================================
// POTVRDENIE HODNOTY Z KLAVESNICE
// ============================================================

void confirmKeyboardValue()
{
    if (identifierIsEmpty(editBuffer))
    {
        return;
    }

    if (editedField == EditField::Station)
    {
        if (sessionActive)
        {
            setStatusMessage(
                "STANOVISKO JE POCAS RELACIE ZAMKNUTE",
                COLOR_WARNING
            );

            editedField = EditField::None;
            clearEditBuffer();
            drawMainScreen();
            return;
        }

        if (identifiersEqual(station, editBuffer))
        {
            editedField = EditField::None;
            clearEditBuffer();
            drawMainScreen();
            return;
        }

        char previousStation[IDENTIFIER_MAX_LENGTH + 1];

        copyIdentifier(
            previousStation,
            sizeof(previousStation),
            station
        );

        copyIdentifier(
            station,
            sizeof(station),
            editBuffer
        );

        targetPoint[0] = '\0';

        if (!createSessionFile(station))
        {
            copyIdentifier(
                station,
                sizeof(station),
                previousStation
            );

            sessionActive = false;

            setStatusMessage(
                "CHYBA VYTVORENIA RELACIE",
                COLOR_ERROR
            );
        }
        else
        {
            currentMeasurement = readMeasurement();

            const bool startWriteOk =
                appendMeasurementToSession(
                    currentMeasurement,
                    "SESSION_START",
                    station,
                    ""
                );

            lastAutoWriteOk = false;

            setStatusMessage(
                startWriteOk
                    ? "NOVA RELACIA SPUSTENA"
                    : "RELACIA BEZI - CHYBA START ZAZNAMU",
                startWriteOk
                    ? COLOR_OK
                    : COLOR_WARNING
            );
        }
    }
    else if (editedField == EditField::TargetPoint)
    {
        if (!sessionActive)
        {
            setStatusMessage(
                "NAJPRV SPUST NOVU RELACIU",
                COLOR_WARNING
            );

            editedField = EditField::None;
            clearEditBuffer();
            drawMainScreen();
            return;
        }

        copyIdentifier(
            targetPoint,
            sizeof(targetPoint),
            editBuffer
        );

        persistActiveSessionState();

        setStatusMessage(
            "MERANY BOD NASTAVENY",
            COLOR_OK
        );
    }

    editedField = EditField::None;
    clearEditBuffer();
    drawMainScreen();
}

// ============================================================
// KLAVESNICA - SPRACOVANIE DOTYKU
// ============================================================

void processKeyboardTouch(int16_t x, int16_t y)
{
    if (KEYBOARD_BACKSPACE_RECT.contains(x, y))
    {
        removeLastEditCharacter();
        drawKeyboardInput();
        drawKeyboardFooter();
        return;
    }

    if (KEYBOARD_MODE_RECT.contains(x, y))
    {
        keyboardMode =
            keyboardMode == KeyboardMode::Numeric
                ? KeyboardMode::Alphabetic
                : KeyboardMode::Numeric;

        drawKeyboardScreen();
        return;
    }

    if (KEYBOARD_CANCEL_RECT.contains(x, y))
    {
        closeKeyboardWithoutSaving();
        return;
    }

    if (KEYBOARD_OK_RECT.contains(x, y))
    {
        confirmKeyboardValue();
        return;
    }

    if (keyboardMode == KeyboardMode::Numeric)
    {
        handleNumericKey(x, y);
    }
    else
    {
        handleAlphabeticKey(x, y);
    }
}

// ============================================================
// INICIALIZACIA DOTYKU
// ============================================================

void initializeTouch()
{
    if (!internalWireReady)
    {
        touchReady = false;
        Serial.println(
            "DOTYK: INTERNA I2C NIE JE DOSTUPNA"
        );
        return;
    }

    touchReady = touch.begin(40);

    if (!touchReady)
    {
        Serial.println(
            "DOTYK: CHYBA INICIALIZACIE"
        );
        return;
    }

    /*
     * Pri portretnej orientacii displeja tft.setRotation(0)
     * poskytuje FT5436 na tejto doske uz spravne orientovane
     * surove suradnice. Rezim 2 ich ponecha bez zrkadlenia.
     */
    touch.setRotation(2);

    Serial.println("DOTYK: OK");
    Serial.println("DOTYK MAPOVANIE: PORTRET, ROTACIA 2");
}

// ============================================================
// INICIALIZACIA SENZOROV
// ============================================================

void initializeSHT40()
{
    if (!externalWireReady)
    {
        sht40Ready = false;
        return;
    }

    sht40Ready = sht40.begin(&Wire1);

    if (!sht40Ready)
    {
        Serial.println("SHT40: CHYBA");
        return;
    }

    sht40.setPrecision(SHT4X_HIGH_PRECISION);
    sht40.setHeater(SHT4X_NO_HEATER);

    Serial.println("SHT40: OK");

    Serial.printf(
        "SHT40 SERIAL: 0x%08lX\n",
        static_cast<unsigned long>(
            sht40.readSerial()
        )
    );
}

void initializeBMP280()
{
    if (!externalWireReady)
    {
        bmp280Ready = false;
        return;
    }

    bmp280Ready = bmp280.begin(
        BMP280_I2C_ADDRESS
    );

    if (!bmp280Ready)
    {
        Serial.println("BMP280: CHYBA");
        return;
    }

    bmp280.setSampling(
        Adafruit_BMP280::MODE_NORMAL,
        Adafruit_BMP280::SAMPLING_X2,
        Adafruit_BMP280::SAMPLING_X16,
        Adafruit_BMP280::FILTER_X16,
        Adafruit_BMP280::STANDBY_MS_500
    );

    Serial.println("BMP280: OK");
    Serial.printf(
        "BMP280 ID: 0x%02X\n",
        bmp280.sensorID()
    );
}

void initializeRTC()
{
    if (!externalWireReady)
    {
        rtcReady = false;
        return;
    }

    rtcReady = rtc.begin(&Wire1);

    if (!rtcReady)
    {
        Serial.println("DS3231: CHYBA");
        return;
    }

    Serial.println("DS3231: OK");

    if (rtc.lostPower())
    {
        Serial.println(
            "RTC STRATILO NAPAJANIE - NASTAVUJEM CAS KOMPILACIE"
        );

        rtc.adjust(
            DateTime(
                F(__DATE__),
                F(__TIME__)
            )
        );
    }
}

// ============================================================
// INICIALIZACIA SD KARTY
// ============================================================

bool initializeSD()
{
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);

    if (!sdSpiStarted)
    {
        sdSpi.begin(
            SD_SCK_PIN,
            SD_MISO_PIN,
            SD_MOSI_PIN,
            SD_CS_PIN
        );

        sdSpiStarted = true;
    }

    if (!SD.begin(
            SD_CS_PIN,
            sdSpi,
            SD_SPI_FREQUENCY
        ))
    {
        Serial.println("SD: CHYBA INICIALIZACIE");
        return false;
    }

    if (SD.cardType() == CARD_NONE)
    {
        Serial.println("SD: KARTA NEBOLA ROZPOZNANA");
        SD.end();
        return false;
    }

    Serial.println("SD: OK");
    return true;
}

bool ensureSDReady(bool forceRecovery)
{
    if (
        sdReady &&
        SD.cardType() != CARD_NONE
    )
    {
        return true;
    }

    const uint32_t currentMillis =
        millis();

    if (
        !forceRecovery &&
        lastSdRecoveryAttemptMillis != 0 &&
        currentMillis - lastSdRecoveryAttemptMillis <
            SD_RECOVERY_INTERVAL_MS
    )
    {
        return false;
    }

    lastSdRecoveryAttemptMillis =
        currentMillis;

    Serial.println(
        "SD: POKUS O OBNOVENIE"
    );

    SD.end();
    delay(50);

    sdReady = initializeSD();

    if (!sdReady)
    {
        Serial.println(
            "SD: OBNOVA ZLYHALA"
        );

        return false;
    }

    if (
        sessionActive &&
        sessionFilePath[0] != '\0' &&
        !SD.exists(sessionFilePath)
    )
    {
        Serial.println(
            "SD: SUBOR AKTIVNEJ RELACIE NA KARTE CHYBA"
        );

        sdReady = false;
        return false;
    }

    Serial.println(
        "SD: OBNOVA USPESNA"
    );

    return true;
}


// ============================================================
// CITANIE SENZOROV
// ============================================================

Measurement readMeasurement()
{
    Measurement measurement{
        rtcReady
            ? rtc.now()
            : DateTime(2000, 1, 1, 0, 0, 0),
        NAN,
        NAN,
        NAN,
        NAN,
        NAN,
        NAN,
        false
    };

    if (
        !sht40Ready ||
        !bmp280Ready ||
        !rtcReady
    )
    {
        lastMeasurementError =
            MeasurementError::DevicesNotReady;

        return measurement;
    }

    if (
        measurement.timestamp.year() <
            MIN_VALID_RTC_YEAR ||
        measurement.timestamp.year() >
            MAX_VALID_RTC_YEAR ||
        measurement.timestamp.month() < 1 ||
        measurement.timestamp.month() > 12 ||
        measurement.timestamp.day() < 1 ||
        measurement.timestamp.day() >
            daysInMonth(
                measurement.timestamp.year(),
                measurement.timestamp.month()
            ) ||
        measurement.timestamp.hour() > 23 ||
        measurement.timestamp.minute() > 59 ||
        measurement.timestamp.second() > 59
    )
    {
        lastMeasurementError =
            MeasurementError::RtcInvalid;

        return measurement;
    }

    sensors_event_t humidityEvent;
    sensors_event_t temperatureEvent;

    const bool shtReadOk = sht40.getEvent(
        &humidityEvent,
        &temperatureEvent
    );

    if (!shtReadOk)
    {
        lastMeasurementError =
            MeasurementError::ShtReadFailed;

        return measurement;
    }

    const float pressurePa =
        bmp280.readPressure();

    if (
        isnan(pressurePa) ||
        pressurePa <= 0.0F
    )
    {
        lastMeasurementError =
            MeasurementError::PressureReadFailed;

        return measurement;
    }

    measurement.rawTemperatureC =
        temperatureEvent.temperature;

    measurement.rawHumidityPercent =
        humidityEvent.relative_humidity;

    measurement.rawPressureHpa =
        pressurePa / 100.0F;

    if (
        isnan(measurement.rawTemperatureC) ||
        measurement.rawTemperatureC <
            MIN_VALID_TEMPERATURE_C ||
        measurement.rawTemperatureC >
            MAX_VALID_TEMPERATURE_C
    )
    {
        lastMeasurementError =
            MeasurementError::TemperatureOutOfRange;

        return measurement;
    }

    if (
        isnan(measurement.rawHumidityPercent) ||
        measurement.rawHumidityPercent <
            MIN_VALID_HUMIDITY_PERCENT ||
        measurement.rawHumidityPercent >
            MAX_VALID_HUMIDITY_PERCENT
    )
    {
        lastMeasurementError =
            MeasurementError::HumidityOutOfRange;

        return measurement;
    }

    if (
        isnan(measurement.rawPressureHpa) ||
        measurement.rawPressureHpa <
            MIN_VALID_PRESSURE_HPA ||
        measurement.rawPressureHpa >
            MAX_VALID_PRESSURE_HPA
    )
    {
        lastMeasurementError =
            MeasurementError::PressureOutOfRange;

        return measurement;
    }

    measurement.temperatureC =
        applyLinearCalibration(
            measurement.rawTemperatureC,
            calibration.temperatureScale,
            calibration.temperatureOffsetC,
            calibration.enabled
        );

    measurement.humidityPercent =
        applyLinearCalibration(
            measurement.rawHumidityPercent,
            calibration.humidityScale,
            calibration.humidityOffsetPercent,
            calibration.enabled
        );

    measurement.pressureHpa =
        applyLinearCalibration(
            measurement.rawPressureHpa,
            calibration.pressureScale,
            calibration.pressureOffsetHpa,
            calibration.enabled
        );

    if (
        !isfinite(measurement.temperatureC) ||
        measurement.temperatureC <
            MIN_VALID_TEMPERATURE_C ||
        measurement.temperatureC >
            MAX_VALID_TEMPERATURE_C
    )
    {
        lastMeasurementError =
            MeasurementError::TemperatureOutOfRange;
        return measurement;
    }

    if (
        !isfinite(measurement.humidityPercent) ||
        measurement.humidityPercent <
            MIN_VALID_HUMIDITY_PERCENT ||
        measurement.humidityPercent >
            MAX_VALID_HUMIDITY_PERCENT
    )
    {
        lastMeasurementError =
            MeasurementError::HumidityOutOfRange;
        return measurement;
    }

    if (
        !isfinite(measurement.pressureHpa) ||
        measurement.pressureHpa <
            MIN_VALID_PRESSURE_HPA ||
        measurement.pressureHpa >
            MAX_VALID_PRESSURE_HPA
    )
    {
        lastMeasurementError =
            MeasurementError::PressureOutOfRange;
        return measurement;
    }

    measurement.valid = true;

    lastMeasurementError =
        MeasurementError::None;

    return measurement;
}

void printMeasurement(
    const Measurement &measurement
)
{
    if (!measurement.valid)
    {
        Serial.printf(
            "MERANIE: %s\n",
            measurementErrorText(
                lastMeasurementError
            )
        );

        return;
    }

    Serial.printf(
        "%04u-%02u-%02u %02u:%02u:%02u | "
        "%.2f C | %.2f %% | %.2f hPa",
        measurement.timestamp.year(),
        measurement.timestamp.month(),
        measurement.timestamp.day(),
        measurement.timestamp.hour(),
        measurement.timestamp.minute(),
        measurement.timestamp.second(),
        measurement.temperatureC,
        measurement.humidityPercent,
        measurement.pressureHpa
    );

    if (calibration.enabled)
    {
        Serial.printf(
            " | RAW %.2f C, %.2f %%, %.2f hPa",
            measurement.rawTemperatureC,
            measurement.rawHumidityPercent,
            measurement.rawPressureHpa
        );
    }

    Serial.println();
}

// ============================================================
// MANUALNE ULOZENIE
// ============================================================

void saveManualMeasurement()
{
    if (!sdReady)
    {
        ensureSDReady(true);
    }

    currentMeasurement = readMeasurement();

    if (!currentMeasurement.valid)
    {
        lastManualWriteOk = false;
        saveButtonState = SaveButtonState::Error;
        saveFeedbackUntil =
            millis() + SAVE_FEEDBACK_MS;

        setStatusMessage(
            measurementErrorText(
                lastMeasurementError
            ),
            COLOR_ERROR,
            4000
        );

        updateMainScreen();
        return;
    }

    if (
        !sessionActive ||
        identifierIsEmpty(station) ||
        identifierIsEmpty(targetPoint)
    )
    {
        lastManualWriteOk = false;
        saveButtonState = SaveButtonState::Error;
        saveFeedbackUntil =
            millis() + SAVE_FEEDBACK_MS;

        setStatusMessage(
            "CHYBA RELACIE ALEBO MERANEHO BODU",
            COLOR_ERROR
        );

        updateMainScreen();
        return;
    }

    lastManualWriteOk =
        appendMeasurementToSession(
            currentMeasurement,
            "MANUAL",
            station,
            targetPoint
        );

    if (lastManualWriteOk)
    {
        manualRecordCount++;
        saveButtonState =
            SaveButtonState::Success;

        Serial.printf(
            "MANUAL #%lu | ST=%s | BOD=%s\n",
            static_cast<unsigned long>(
                manualRecordCount
            ),
            station,
            targetPoint
        );

        // Merany bod sa po uspesnom zapise vymaze.
        targetPoint[0] = '\0';

        persistActiveSessionState();

        setStatusMessage(
            "MERANIE ULOZENE - ZADAJ NOVY BOD",
            COLOR_OK,
            SAVE_FEEDBACK_MS
        );
    }
    else
    {
        saveButtonState =
            SaveButtonState::Error;

        persistActiveSessionState();

        setStatusMessage(
            "CHYBA SD - MANUALNY ZAZNAM NEBOL ULOZENY",
            COLOR_ERROR,
            5000
        );
    }

    saveFeedbackUntil =
        millis() + SAVE_FEEDBACK_MS;

    updateMainScreen();
}

// ============================================================
// HLAVNA OBRAZOVKA - SPRACOVANIE DOTYKU
// ============================================================

void processMainTouch(int16_t x, int16_t y)
{
    if (WIFI_SHORTCUT_RECT.contains(x, y))
    {
        openWifiExport();
        return;
    }

    if (ARCHIVE_SHORTCUT_RECT.contains(x, y))
    {
        openArchiveList();
        return;
    }

    if (GRAPH_SHORTCUT_RECT.contains(x, y))
    {
        openGraphs();
        return;
    }

    if (SETTINGS_SHORTCUT_RECT.contains(x, y))
    {
        openSettings();
        return;
    }

    if (STATION_FIELD_RECT.contains(x, y))
    {
        if (sessionActive)
        {
            setStatusMessage(
                "STANOVISKO JE ZAMKNUTE - UKONCI RELACIU",
                COLOR_WARNING
            );

            updateMessageStrip();
            return;
        }

        openKeyboard(EditField::Station);
        return;
    }

    if (TARGET_FIELD_RECT.contains(x, y))
    {
        if (!sessionActive)
        {
            setStatusMessage(
                "NAJPRV SPUST NOVU RELACIU",
                COLOR_WARNING
            );

            updateMessageStrip();
            return;
        }

        openKeyboard(EditField::TargetPoint);
        return;
    }

    if (SESSION_TOGGLE_BUTTON_RECT.contains(x, y))
    {
        if (sessionActive)
        {
            openEndSessionConfirmation();
        }
        else
        {
            openKeyboard(EditField::Station);
        }

        return;
    }

    if (
        SAVE_BUTTON_RECT.contains(x, y) &&
        saveButtonState == SaveButtonState::Idle
    )
    {
        saveManualMeasurement();
    }
}

// ============================================================
// GLOBALNE SPRACOVANIE DOTYKU
// ============================================================

void processTouch()
{
    if (!touchReady)
    {
        return;
    }

    const bool currentlyTouched =
        touch.touched() > 0;

    if (currentlyTouched)
    {
        const TS_Point point =
            touch.getPoint();

        if (wakeDisplayAndConsumeTouch())
        {
            touchWasPressed = true;
            return;
        }

        // Slider jasu reaguje aj pocas drzania a tahania prsta.
        if (
            currentScreen == Screen::Settings &&
            (
                brightnessSliderDragging ||
                SETTINGS_BRIGHTNESS_TOUCH_RECT.contains(
                    point.x,
                    point.y
                )
            )
        )
        {
            brightnessSliderDragging = true;
            setPendingBrightnessFromTouch(
                point.x
            );

            touchWasPressed = true;
            return;
        }

        if (!touchWasPressed)
        {
            Serial.printf(
                "DOTYK: x=%d, y=%d\n",
                point.x,
                point.y
            );

            if (currentScreen == Screen::Main)
            {
                processMainTouch(
                    point.x,
                    point.y
                );
            }
            else if (currentScreen == Screen::Keyboard)
            {
                processKeyboardTouch(
                    point.x,
                    point.y
                );
            }
            else if (currentScreen == Screen::Settings)
            {
                processSettingsTouch(
                    point.x,
                    point.y
                );
            }
            else if (currentScreen == Screen::DateTimeEditor)
            {
                processDateTimeEditorTouch(
                    point.x,
                    point.y
                );
            }
            else if (currentScreen == Screen::Recovery)
            {
                processRecoveryTouch(
                    point.x,
                    point.y
                );
            }
            else if (currentScreen == Screen::Graphs)
            {
                processGraphTouch(
                    point.x,
                    point.y
                );
            }
            else if (
                currentScreen ==
                Screen::ArchiveList
            )
            {
                processArchiveListTouch(
                    point.x,
                    point.y
                );
            }
            else if (
                currentScreen ==
                Screen::ArchiveDetail
            )
            {
                processArchiveDetailTouch(
                    point.x,
                    point.y
                );
            }
            else if (
                currentScreen ==
                Screen::Calibration
            )
            {
                processCalibrationTouch(
                    point.x,
                    point.y
                );
            }
            else if (
                currentScreen ==
                Screen::CalibrationEditor
            )
            {
                processCalibrationEditorTouch(
                    point.x,
                    point.y
                );
            }
            else if (
                currentScreen ==
                Screen::PowerSettings
            )
            {
                processPowerSettingsTouch(
                    point.x,
                    point.y
                );
            }
            else if (
                currentScreen ==
                Screen::WifiExport
            )
            {
                processWifiExportTouch(
                    point.x,
                    point.y
                );
            }
            else
            {
                processEndSessionConfirmationTouch(
                    point.x,
                    point.y
                );
            }
        }
    }
    else
    {
        brightnessSliderDragging = false;
    }

    touchWasPressed = currentlyTouched;
}

// ============================================================
// CASOVANE UI UDALOSTI
// ============================================================

void updateTimedUiEvents()
{
    bool mainScreenNeedsUpdate = false;

    if (
        saveButtonState != SaveButtonState::Idle &&
        !saveFeedbackActive()
    )
    {
        saveButtonState = SaveButtonState::Idle;
        saveFeedbackUntil = 0;
        mainScreenNeedsUpdate = true;
    }

    if (
        statusMessage[0] != '\0' &&
        statusMessageUntil != 0 &&
        !timedEventActive(statusMessageUntil)
    )
    {
        clearStatusMessage();
        mainScreenNeedsUpdate = true;
    }

    if (
        mainScreenNeedsUpdate &&
        currentScreen == Screen::Main
    )
    {
        drawSessionButtons();
        drawSaveButton();
        updateEditableFields();
        updateStatusPanel();
        updateMessageStrip();
    }
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);
    delay(1000);

    internalWireReady = Wire.begin(
        INTERNAL_I2C_SDA,
        INTERNAL_I2C_SCL,
        I2C_FREQUENCY
    );

    externalWireReady = Wire1.begin(
        EXTERNAL_I2C_SDA,
        EXTERNAL_I2C_SCL,
        I2C_FREQUENCY
    );

    loadPersistentSettings();
    loadPendingSessionRecovery();
    initializeDisplay();
    buildWifiExportSsid();

    Serial.println();
    Serial.println(
        "============================================"
    );
    Serial.printf(
        "%s %s | ESPD-35 v3.2\n",
        APP_NAME,
        APP_VERSION
    );
    Serial.println(
        "============================================"
    );

    Serial.printf(
        "INTERNA WIRE: %s\n",
        internalWireReady ? "OK" : "CHYBA"
    );

    Serial.printf(
        "EXTERNA WIRE1: %s\n",
        externalWireReady ? "OK" : "CHYBA"
    );

    Serial.printf(
        "WI-FI EXPORT: siet=%s | heslo=%s | limit=%lu s\n",
        wifiExportSsid,
        WIFI_EXPORT_PASSWORD,
        static_cast<unsigned long>(
            WIFI_EXPORT_IDLE_TIMEOUT_MS / 1000UL
        )
    );

    Serial.printf(
        "KALIBRACIA: %s | "
        "T a=%.6f b=%+.3f | "
        "RH a=%.6f b=%+.3f | "
        "P a=%.6f b=%+.3f\n",
        calibration.enabled ? "ON" : "OFF",
        calibration.temperatureScale,
        calibration.temperatureOffsetC,
        calibration.humidityScale,
        calibration.humidityOffsetPercent,
        calibration.pressureScale,
        calibration.pressureOffsetHpa
    );

    initializeTouch();
    initializeSHT40();
    initializeBMP280();
    initializeRTC();

    sdReady = initializeSD();
    initializeBatteryMonitor();
    configureWifiExportRoutes();

    currentMeasurement = readMeasurement();

    if (pendingSessionRecovery)
    {
        drawRecoveryScreen();
    }
    else
    {
        setStatusMessage(
            "STLAC NOVA RELACIA",
            COLOR_WARNING,
            0
        );

        drawMainScreen();
    }

    printMeasurement(currentMeasurement);

    lastSensorMillis = millis();
    lastAutoLogMillis = millis();
    lastBatteryMillis = millis();
    lastUserActivityMillis = millis();
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
    processTouch();
    updateTimedUiEvents();

    const uint32_t currentMillis = millis();

    if (
        currentScreen == Screen::Recovery &&
        !sdReady
    )
    {
        const bool recovered =
            ensureSDReady(false);

        if (recovered)
        {
            drawRecoveryScreen();
        }
    }

    if (
        currentMillis - lastBatteryMillis >=
        BATTERY_INTERVAL_MS
    )
    {
        lastBatteryMillis = currentMillis;
        readBatteryState();
        updateBatteryWarning();

        if (currentScreen == Screen::Main)
        {
            updateDateTimeDisplay(
                currentMeasurement.timestamp
            );

            updateMessageStrip();
        }
    }

    // Senzory sa citaju aj pocas otvorenej klavesnice.
    if (
        currentMillis - lastSensorMillis >=
        SENSOR_INTERVAL_MS
    )
    {
        lastSensorMillis = currentMillis;

        currentMeasurement = readMeasurement();
        printMeasurement(currentMeasurement);

        if (currentScreen == Screen::Main)
        {
            updateMainScreen();
        }
        else if (currentScreen == Screen::Calibration)
        {
            drawCalibrationLivePanel(false);
        }
    }

    // Automaticky zapis bezi iba po zadani stanoviska
    // a vytvoreni meracej relacie.
    if (
        sessionActive &&
        currentMillis - lastAutoLogMillis >=
        static_cast<uint32_t>(autoLogIntervalSeconds) * 1000UL
    )
    {
        lastAutoLogMillis = currentMillis;

        if (!currentMeasurement.valid)
        {
            lastAutoWriteOk = false;

            setStatusMessage(
                measurementErrorText(
                    lastMeasurementError
                ),
                COLOR_ERROR,
                4000
            );
        }
        else
        {
            lastAutoWriteOk =
                appendMeasurementToSession(
                    currentMeasurement,
                    "AUTO",
                    station,
                    ""
                );

            if (lastAutoWriteOk)
            {
                registerSuccessfulAutoRecordForGraph(
                    currentMeasurement
                );
            }

            if (!lastAutoWriteOk)
            {
                persistActiveSessionState();

                setStatusMessage(
                    "CHYBA AUTOMATICKEHO ZAPISU - KONTROLA SD",
                    COLOR_ERROR,
                    5000
                );
            }
        }

        if (currentScreen == Screen::Main)
        {
            updateStatusPanel();
            updateMessageStrip();
        }
    }

    updateLiveGraphScreen(
        currentMillis
    );

    updateWifiExport(
        currentMillis
    );

    updateDisplayPowerSaving(
        currentMillis
    );
}

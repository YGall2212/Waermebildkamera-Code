#include <Wire.h>
#include <Adafruit_MLX90640.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// Display-Pins für ESP32
#define TFT_CS   15 
#define TFT_DC    2
#define TFT_MOSI 23
#define TFT_CLK  18
#define TFT_RST  4
#define TFT_MISO 19
#define TFT_LED  27
#define BATTERY_PIN 34  // Pin für die Akku-Messung

// Spannungsteiler-Widerstände
#define R1 100000  // 100kOhm
#define R2 47000   // 47kOhm

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);
Adafruit_MLX90640 mlx;

float mlx90640To[768];  // Sensordaten
float T_max, T_min, T_center;  // Temperaturwerte
int R_colour, G_colour, B_colour; // Variablen für Farbberechnung

void setup() {
    Serial.begin(115200);
    Wire.begin();
    Wire.setClock(400000);
    
    if (!mlx.begin()) {
        Serial.println("Fehler beim Starten des MLX90640 Sensors");
        while (1);
    }
    
    mlx.setMode(MLX90640_INTERLEAVED);
    mlx.setResolution(MLX90640_ADC_16BIT);
    mlx.setRefreshRate(MLX90640_8_HZ);
    
    pinMode(TFT_LED, OUTPUT);
    digitalWrite(TFT_LED, HIGH);
    
    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(ILI9341_BLACK);
    drawColorScale();  
}

void loop() {
    if (mlx.getFrame(mlx90640To) != 0) {
        Serial.println("Fehler beim Abrufen der Sensordaten");
        return;
    }
    
    calculateMinMaxTemperature();
    drawThermalImage();
    displayTemperatureInfo();
    displayBatteryStatus();
}

// Funktion zur Berechnung der min./max. Temperatur
void calculateMinMaxTemperature() {
    T_min = 1000.0;
    T_max = -1000.0;
    for (int i = 0; i < 768; i++) {
        if (mlx90640To[i] > -40 && mlx90640To[i] < 300) {  // Temperaturbereich begrenzen
            if (mlx90640To[i] < T_min) T_min = mlx90640To[i];
            if (mlx90640To[i] > T_max) T_max = mlx90640To[i];
        }
    }
    T_center = mlx90640To[12 * 32 + 16]; // Temperatur in der Mitte des Bildes
}

// Funktion zur Darstellung des Wärmebilds
void drawThermalImage() {
    int pixelSize = 7;
    for (int i = 0; i < 24; i++) {
        for (int j = 0; j < 32; j++) {
            int index = i * 32 + j;
            int colorValue = 180.0 * (mlx90640To[index] - T_min) / (T_max - T_min);
            getColour(colorValue);
            tft.fillRect(217 - j * pixelSize, 35 + i * pixelSize, pixelSize, pixelSize, tft.color565(R_colour, G_colour, B_colour));
        }
    }
    drawCrosshair();
}

// Funktion für Fadenkreuz in der Mitte
void drawCrosshair() {
    int x = 217 - 15 * 7 + 3.5;
    int y = 35 + 11 * 7 + 3.5;
    tft.drawLine(x - 5, y, x + 5, y, ILI9341_WHITE);
    tft.drawLine(x, y - 5, x, y + 5, ILI9341_WHITE);


// Akku-Spannungsmessung mit Mittelwertbildung
float getSmoothedBatteryVoltage() {
    static float voltageSamples[10] = {0}; 
    static int index = 0;
    
    voltageSamples[index] = analogRead(BATTERY_PIN) * (3.3 / 4095.0) * ((R1 + R2) / R2);
    index = (index + 1) % 10;

    float sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += voltageSamples[i];
    }
    return sum / 10.0; 
}

// Akkuanzeige auf dem Display
void displayBatteryStatus() {
    float voltage = getSmoothedBatteryVoltage();
    int batteryPercentage = map(voltage * 100, 300, 420, 0, 100);
    batteryPercentage = constrain(batteryPercentage, 0, 100);

    tft.fillRect(5, 5, 80, 15, ILI9341_BLACK);
    tft.setCursor(5, 5);
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    tft.print("Akku: "); 
    tft.print(batteryPercentage);
    tft.print("%");
}

// Farbabstufung berechnen
void getColour(int j) {
    if (j < 30) { R_colour = 0; G_colour = 0; B_colour = 20 + (120.0 / 30.0) * j; }
    else if (j < 60) { R_colour = (120.0 / 30) * (j - 30.0); G_colour = 0; B_colour = 140 - (60.0 / 30.0) * (j - 30.0); }
    else if (j < 90) { R_colour = 120 + (135.0 / 30.0) * (j - 60.0); G_colour = 0; B_colour = 80 - (70.0 / 30.0) * (j - 60.0); }
    else if (j < 120) { R_colour = 255; G_colour = (60.0 / 30.0) * (j - 90.0); B_colour = 10 - (10.0 / 30.0) * (j - 90.0); }
    else if (j < 150) { R_colour = 255; G_colour = 60 + (175.0 / 30.0) * (j - 120.0); B_colour = 0; }
    else { R_colour = 255; G_colour = 235 + (20.0 / 30.0) * (j - 150.0); B_colour = 255.0 / 30.0 * (j - 150.0); }
}

// Temperaturwerte anzeigen
void displayTemperatureInfo() {
    tft.fillRect(260, 25, 37, 10, ILI9341_BLACK);
    tft.fillRect(260, 205, 37, 10, ILI9341_BLACK);
    tft.fillRect(115, 220, 50, 10, ILI9341_BLACK);  // Platz für Fadenkreuz-Temperatur

    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    tft.setCursor(265, 25);
    tft.print(T_max, 1); tft.print(" C");
    tft.setCursor(265, 205);
    tft.print(T_min, 1); tft.print(" C");
    
    // Fadenkreuz-Temperatur unter dem Wärmebild
    tft.setCursor(120, 220);
    tft.print("Temp: ");
    tft.print(T_center, 1);
    tft.print(" C");
}

// Farbbalken zeichnen 
void drawColorScale() {
    for (int i = 0; i < 181; i++) {
        getColour(i);
        tft.drawLine(240, 210 - i, 250, 210 - i, tft.color565(R_colour, G_colour, B_colour));
    }
}

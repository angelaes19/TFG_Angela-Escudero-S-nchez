#include <Arduino.h>
#include <LSM6DS3.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "cupra_logo_bitmaps.h"

LSM6DS3 myIMU(I2C_MODE, 0x6A);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// --- PANTALLA ---
const float CENTRO_X = 64.0f;
const float CENTRO_Y = 32.0f;
const int   ANCHO    = 128;
const int   ALTO     = 64;
const int   RADIO_PUNTO = 4;
const int   MARGEN      = 5;
const unsigned long SPLASH_MS = 1000;

// --- POSICION Y VELOCIDAD ---
float px = CENTRO_X;
float py = CENTRO_Y;
float vx = 0.0f;
float vy = 0.0f;

// --- FILTRO DE TRASLACION LINEAL ---
// Se usa acelerometro filtrado, no inclinacion. El giroscopio solo bloquea giros.
float prevRawH = 0.0f;
float prevRawV = 0.0f;
float hpH = 0.0f;
float hpV = 0.0f;

// --- AJUSTES DE SENSACION ---
const unsigned long INTERVALO_MS = 10;
const int   CALIB_MUESTRAS = 100;
const float HP_ALPHA       = 0.80f;
const float ZONA_MUERTA    = 0.04f;
const float GANANCIA_H     = 23500.0f;  // Compensa el ancho 128 frente al alto 64.
const float GANANCIA_V     = 14000.0f;
const float FRICCION       = 0.78f;
const float VEL_MINIMA     = 0.15f;
const float FRENO_CAMBIO   = 0.50f;
const float RESPUESTA_INICIAL = 0.0f;
const float VEL_OBJ_H      = 375.0f;
const float VEL_OBJ_V      = 500.0f;
const float SIGNO_H        = 1.0f;   // Placa por debajo: giro izquierda -> punto derecha.
const float SIGNO_V        = 1.0f;   // Avance hacia delante -> punto abajo.

// Si se gira la caja, el giroscopio sube: reducimos o anulamos la entrada.
const float ROT_UMBRAL_DPS = 35.0f;
const unsigned long GIRO_COOLDOWN_MS = 90;

// --- RETORNO AL CENTRO ---
const unsigned long TIEMPO_IDLE_MS = 100;
const float FUERZA_RETORNO = 16.0f;
const float FRENO_RETORNO  = 0.95f;

unsigned long lastUpdate = 0;
unsigned long lastMovimiento = 0;
unsigned long lastGiro = 0;

void dibujarLogoCupra(const uint8_t *bitmap, uint8_t ancho, uint8_t alto) {
  uint8_t x = (ANCHO - ancho) / 2;
  uint8_t y = (ALTO - alto) / 2;
  u8g2.drawXBMP(x, y, ancho, alto, bitmap);
}

void animarLogoCupra() {
  delay(150);
  unsigned long inicio = millis();

  while (millis() - inicio < SPLASH_MS) {
    unsigned long elapsed = millis() - inicio;
    const uint8_t *bitmap = CUPRA_LOGO_FULL;
    uint8_t ancho = CUPRA_LOGO_FULL_W;
    uint8_t alto = CUPRA_LOGO_FULL_H;

    if (elapsed < 250) {
      bitmap = CUPRA_LOGO_SMALL;
      ancho = CUPRA_LOGO_SMALL_W;
      alto = CUPRA_LOGO_SMALL_H;
    } else if (elapsed < 520) {
      bitmap = CUPRA_LOGO_MID;
      ancho = CUPRA_LOGO_MID_W;
      alto = CUPRA_LOGO_MID_H;
    } else if (elapsed < 780) {
      bitmap = CUPRA_LOGO_FULL;
      ancho = CUPRA_LOGO_FULL_W;
      alto = CUPRA_LOGO_FULL_H;
    } else if (elapsed < 910) {
      bitmap = CUPRA_LOGO_FADE_1;
      ancho = CUPRA_LOGO_FADE_1_W;
      alto = CUPRA_LOGO_FADE_1_H;
      delay(200);
    } else {
      bitmap = CUPRA_LOGO_FADE_2;
      ancho = CUPRA_LOGO_FADE_2_W;
      alto = CUPRA_LOGO_FADE_2_H;
    }

    u8g2.clearBuffer();
    dibujarLogoCupra(bitmap, ancho, alto);
    u8g2.sendBuffer();
    delay(50);
  }

  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

void dibujarErrorIMU() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(10, 35, "IMU ERROR");
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  animarLogoCupra();

  if (myIMU.begin() != 0) {
    dibujarErrorIMU();
    while (1);
  }

  // Calibracion inicial: deja la caja quieta en su posicion real de uso.
  float sumaH = 0.0f;
  float sumaV = 0.0f;
  for (int i = 0; i < CALIB_MUESTRAS; i++) {
    // Montaje actual: Y = izquierda/derecha, Z = arriba/abajo-delante/atras.
    // Si la placa queda girada en otra version, cambia solo estas dos lecturas.
    sumaH += myIMU.readFloatAccelY();
    sumaV += myIMU.readFloatAccelZ();
    delay(2);
  }

  prevRawH = sumaH / CALIB_MUESTRAS;
  prevRawV = sumaV / CALIB_MUESTRAS;

  unsigned long ahora = millis();
  lastUpdate = ahora;
  lastMovimiento = ahora;

  u8g2.clearBuffer();
  u8g2.drawDisc((int)CENTRO_X, (int)CENTRO_Y, RADIO_PUNTO);
  u8g2.sendBuffer();
}

void loop() {
  unsigned long ahora = millis();
  if (ahora - lastUpdate < INTERVALO_MS) return;

  float dt = (ahora - lastUpdate) * 0.001f;
  lastUpdate = ahora;

  // 1. LEER ACELERACION LINEAL EN LOS EJES USADOS POR LA PANTALLA
  float rawH = myIMU.readFloatAccelY();
  float rawV = myIMU.readFloatAccelZ();

  // 2. PASO ALTO: elimina gravedad, inclinacion mantenida y deriva lenta.
  hpH = HP_ALPHA * (hpH + rawH - prevRawH);
  hpV = HP_ALPHA * (hpV + rawV - prevRawV);
  prevRawH = rawH;
  prevRawV = rawV;

  // 3. DETECTAR ROTACION: se usa solo para bloquear, no para mover el punto.
  float gyroX = myIMU.readFloatGyroX();
  float gyroY = myIMU.readFloatGyroY();
  float gyroZ = myIMU.readFloatGyroZ();
  float rotDps = sqrtf(gyroX * gyroX + gyroY * gyroY + gyroZ * gyroZ);
  float factorTraslacion = 1.0f;

  if (rotDps > ROT_UMBRAL_DPS) {
    lastGiro = ahora;
  }

  if (ahora - lastGiro < GIRO_COOLDOWN_MS) {
    // Limpia el resto del paso alto para que un giro brusco no deje cola.
    factorTraslacion = 0.0f;
    hpH *= 0.45f;
    hpV *= 0.45f;
  }

  // 4. MAPEO INVERSO: ajustado para la placa montada por debajo de la PCB.
  float moveH = SIGNO_H * hpH * factorTraslacion;
  float moveV = SIGNO_V * hpV * factorTraslacion;

  if (fabsf(moveH) < ZONA_MUERTA) moveH = 0.0f;
  if (fabsf(moveV) < ZONA_MUERTA) moveV = 0.0f;

  bool hayTraslacion = (moveH != 0.0f || moveV != 0.0f);
  if (hayTraslacion) {
    lastMovimiento = ahora;
  }

  // 5. FISICA DEL PUNTO
  if ((moveH > 0.0f && vx < 0.0f) || (moveH < 0.0f && vx > 0.0f)) {
    vx *= FRENO_CAMBIO;
  }
  if ((moveV > 0.0f && vy < 0.0f) || (moveV < 0.0f && vy > 0.0f)) {
    vy *= FRENO_CAMBIO;
  }

  if (moveH != 0.0f) {
    vx += (moveH * VEL_OBJ_H - vx) * RESPUESTA_INICIAL;
  }
  if (moveV != 0.0f) {
    vy += (moveV * VEL_OBJ_V - vy) * RESPUESTA_INICIAL;
  }

  vx += moveH * GANANCIA_H * dt;
  vy += moveV * GANANCIA_V * dt;

  if (!hayTraslacion && (ahora - lastMovimiento > TIEMPO_IDLE_MS)) {
    vx += (CENTRO_X - px) * FUERZA_RETORNO * dt;
    vy += (CENTRO_Y - py) * FUERZA_RETORNO * dt;
    vx *= FRENO_RETORNO;
    vy *= FRENO_RETORNO;
  }

  vx *= FRICCION;
  vy *= FRICCION;

  if (fabsf(vx) < VEL_MINIMA) vx = 0.0f;
  if (fabsf(vy) < VEL_MINIMA) vy = 0.0f;

  px += vx * dt;
  py += vy * dt;

  // 6. LIMITES DE PANTALLA
  if (px < MARGEN)          { px = MARGEN;          vx = 0.0f; }
  if (px > ANCHO - MARGEN)  { px = ANCHO - MARGEN;  vx = 0.0f; }
  if (py < MARGEN)          { py = MARGEN;          vy = 0.0f; }
  if (py > ALTO - MARGEN)   { py = ALTO - MARGEN;   vy = 0.0f; }

  // 7. DIBUJAR
  u8g2.clearBuffer();
  u8g2.drawDisc((int)px, (int)py, RADIO_PUNTO);
  u8g2.sendBuffer();
}

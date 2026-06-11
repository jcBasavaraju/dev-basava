/*
 * 1CH Mic Tester Firmware  —  ICS43434 Edition
 * Board  : Glyph C6 (ESP32-C6)
 * Mic    : ICS43434  —  WS=20, BCK=21, SD=18
 *          LEFT-JUSTIFIED format, 24-bit data in top 24 bits of 32-bit word
 * Serial : 115200 baud over USB
 *
 * Protocol (PC sends → C6 responds):
 *   PC sends "TEST:2000\n" → C6 runs full test at 2000 Hz, replies JSON result
 *   PC sends "TEST\n"      → C6 runs full test at default TEST_FREQ
 *   PC sends "NOISE\n"     → C6 measures noise floor only
 *   PC sends "PING\n"      → C6 replies "PONG\n"
 *
 * Test flow:
 *   1. Flush stale buffer (1 sec)
 *   2. Measure noise floor  → sends {"status":"MEASURING_NOISE"}
 *   3. Signal PC to play tone → sends {"status":"PLAY_TONE"}
 *   4. Wait 800 ms for tone to stabilise at mic
 *   5. Record 7.5 sec of signal  → sends {"status":"RECORDING"}
 *   6. Run FFT, compute SNR  → sends PASS/FAIL JSON
 *
 * Result JSON:
 *   {"status":"PASS","snr":24.3,"peak_freq":2000,"signal_db":-18.2,"noise_db":-42.5}
 *   {"status":"FAIL","snr":3.1,"peak_freq":2000,"signal_db":-38.0,"noise_db":-41.1}
 *   {"status":"ERROR","msg":"reason string"}
 */

#include "AudioTools.h"
#include <arduinoFFT.h>

// ── Hardware config ──────────────────────────────────────────────
#define SAMPLE_RATE      44100
#define CHANNELS         1
#define BITS             32
#define I2S_WS_PIN       20
#define I2S_BCK_PIN      21
#define I2S_SD_PIN       18

// ── Test config ──────────────────────────────────────────────────
#define TEST_FREQ           200       // Hz — must match Python app
                                       // 2 kHz: laptop speakers louder here than 1 kHz
#define FFT_SIZE            4096       // ~10.7 Hz/bin at 44100 Hz
#define SNR_PASS_DB         20.0       // industry standard pass threshold
#define NOISE_DURATION_MS   600        // noise floor measurement window
#define SIGNAL_DURATION_MS  5000       // 5 sec recording window
#define POST_TONE_DELAY_MS  5000        // wait after PLAY_TONE before recording
                                       // gives PC time to start + tone to reach mic

// ── Globals ──────────────────────────────────────────────────────
AudioInfo          audioInfo(SAMPLE_RATE, CHANNELS, BITS);
I2SStream          i2sStream;
double             vReal[FFT_SIZE];
double             vImag[FFT_SIZE];
ArduinoFFT<double> FFT(vReal, vImag, FFT_SIZE, SAMPLE_RATE);

bool i2s_ready  = false;
int  g_test_freq = TEST_FREQ;   // runtime-settable; updated by TEST:<hz> command

// ── Setup ────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  auto cfg = i2sStream.defaultConfig(RX_MODE);
  cfg.copyFrom(audioInfo);

  // ICS43434: LEFT-JUSTIFIED format (NOT standard I2S)
  cfg.i2s_format   = I2S_LEFT_JUSTIFIED_FORMAT;
  cfg.pin_ws        = I2S_WS_PIN;
  cfg.pin_bck       = I2S_BCK_PIN;
  cfg.pin_data_rx   = I2S_SD_PIN;
  cfg.is_master     = true;
  cfg.use_apll      = false;

  if (i2sStream.begin(cfg)) {
    i2s_ready = true;
    Serial.println("{\"status\":\"READY\",\"msg\":\"ICS43434 mic tester ready\"}");
  } else {
    Serial.println("{\"status\":\"ERROR\",\"msg\":\"I2S init failed — check wiring\"}");
  }
}

// ── Collect FFT samples ──────────────────────────────────────────
bool collectSamples(int duration_ms) {
  memset(vReal, 0, sizeof(vReal));
  memset(vImag, 0, sizeof(vImag));

  int32_t  buf[64];
  int      collected = 0;
  uint32_t start     = millis();

  while (collected < FFT_SIZE &&
         (millis() - start) < (uint32_t)(duration_ms + 500)) {
    int bytes   = i2sStream.readBytes((uint8_t*)buf, sizeof(buf));
    int samples = bytes / sizeof(int32_t);

    for (int i = 0; i < samples && collected < FFT_SIZE; i++) {
      // ICS43434: 24-bit data sits in top 24 bits of the 32-bit word
      // Shift right 8, normalise to -1.0 .. +1.0
      vReal[collected] = (double)(buf[i] >> 8) / (double)0x7FFFFF;
      vImag[collected] = 0.0;
      collected++;
    }
  }
  return (collected >= FFT_SIZE);
}

// ── Run FFT, return dB at target frequency ───────────────────────
float getFreqLevelDB(int target_hz, int* peak_freq_out) {  FFT.windowing(FFTWindow::Hann, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();

  double bin_hz      = (double)SAMPLE_RATE / FFT_SIZE;  // ~10.7 Hz/bin
  int    target_bin  = (int)(target_hz / bin_hz);
  int    window_bins = (int)(200.0 / bin_hz);           // ±200 Hz search window

  int start_bin = max(1, target_bin - window_bins);
  int end_bin   = min(FFT_SIZE / 2 - 1, target_bin + window_bins);

  double peak_mag = 0;
  int    peak_bin = target_bin;
  for (int i = start_bin; i <= end_bin; i++) {
    if (vReal[i] > peak_mag) {
      peak_mag = vReal[i];
      peak_bin = i;
    }
  }

  if (peak_freq_out) *peak_freq_out = (int)(peak_bin * bin_hz);

  double normalised = peak_mag / (FFT_SIZE / 2.0);
  normalised = max(normalised, 1e-10);
  return (float)(20.0 * log10(normalised));
}

// ── Full test ────────────────────────────────────────────────────
void runTest() {
  if (!i2s_ready) {
    Serial.println("{\"status\":\"ERROR\",\"msg\":\"I2S not initialised\"}");
    return;
  }

  // Flush 1 second of stale data from buffer
  char flush[256];
  uint32_t flush_end = millis() + 1000;
  while (millis() < flush_end)
    i2sStream.readBytes((uint8_t*)flush, sizeof(flush));

  // Phase 1: noise floor
  Serial.println("{\"status\":\"MEASURING_NOISE\"}");
  if (!collectSamples(NOISE_DURATION_MS)) {
    Serial.println("{\"status\":\"ERROR\",\"msg\":\"Not enough samples for noise floor\"}");
    return;
  }
  int   dummy_freq;
  float noise_db = getFreqLevelDB(g_test_freq, &dummy_freq);

  // Phase 2: signal PC to start playing tone
  Serial.println("{\"status\":\"PLAY_TONE\"}");
  delay(POST_TONE_DELAY_MS);  // let tone reach stable amplitude at mic

  // Phase 3: record with tone playing
  Serial.println("{\"status\":\"RECORDING\"}");
  if (!collectSamples(SIGNAL_DURATION_MS)) {
    Serial.println("{\"status\":\"ERROR\",\"msg\":\"Not enough samples during recording\"}");
    return;
  }

  int   peak_freq = 0;
  float signal_db = getFreqLevelDB(g_test_freq, &peak_freq);
  float snr       = signal_db - noise_db;
  bool  pass      = (snr >= SNR_PASS_DB) && (abs(peak_freq - g_test_freq) <= 200);

  char result[200];
  snprintf(result, sizeof(result),
    "{\"status\":\"%s\",\"snr\":%.1f,\"peak_freq\":%d,\"signal_db\":%.1f,\"noise_db\":%.1f}",
    pass ? "PASS" : "FAIL",
    snr, peak_freq, signal_db, noise_db);
  Serial.println(result);
}

// ── Noise only ───────────────────────────────────────────────────
void measureNoise() {
  if (!i2s_ready) {
    Serial.println("{\"status\":\"ERROR\",\"msg\":\"I2S not initialised\"}");
    return;
  }
  if (!collectSamples(NOISE_DURATION_MS)) {
    Serial.println("{\"status\":\"ERROR\",\"msg\":\"Not enough samples\"}");
    return;
  }
  int   dummy;
  float noise_db = getFreqLevelDB(g_test_freq, &dummy);
  char  out[100];
  snprintf(out, sizeof(out),
    "{\"status\":\"NOISE_FLOOR\",\"noise_db\":%.1f}", noise_db);
  Serial.println(out);
}

// ── Main loop ────────────────────────────────────────────────────
void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if (cmd.startsWith("TEST")) {
      // Accept "TEST" (use default) or "TEST:2000" (use specified freq)
      int colon = cmd.indexOf(':');
      if (colon >= 0) {
        int freq = cmd.substring(colon + 1).toInt();
        if (freq >= 20 && freq <= 20000)
          g_test_freq = freq;
      } else {
        g_test_freq = TEST_FREQ;   // reset to compile-time default
      }
      runTest();
    }
    else if (cmd == "NOISE") measureNoise();
    else if (cmd == "PING")  Serial.println("PONG");
  }
}

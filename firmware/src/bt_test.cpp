#include <Arduino.h>
#include <BluetoothA2DPSource.h>
#include <cmath>
#include <cstring>

// Bench de Bluetooth: sem SD, OLED ou botões. Mede conexão e reconexão com fones reais.
// Sem BT_TARGET só lista os fones achados. Com -DBT_TARGET=\"nome\" conecta.
// -DBT_PIN=\"0000\" para fones legados (padrão da lib é 1234). -DBT_AMP=30000 para testar clipping.
// A lib guarda o último fone na NVS e reconecta nele sem escanear: apagar com `pio run -e bt_test -t erase`.
static BluetoothA2DPSource a2dp;
static uint32_t connects, disconnects;

static bool onDeviceFound(const char* name, esp_bd_addr_t, int rssi) {
    Serial.printf("[%lu] achado \"%s\" rssi=%d\n", millis(), name, rssi);
    return *BT_TARGET && strstr(name, BT_TARGET);
}

static void onState(esp_a2d_connection_state_t state, void*) {
    if (state == ESP_A2D_CONNECTION_STATE_CONNECTED)
        connects++;
    if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED)
        disconnects++;
    Serial.printf("[%lu] %s conn=%lu desc=%lu heap=%lu\n", millis(), a2dp.to_str(state), connects, disconnects,
                  ESP.getFreeHeap());
}

static void onAudio(esp_a2d_audio_state_t state, void*) {
    Serial.printf("[%lu] audio %s\n", millis(), a2dp.to_str(state));
}

// Esquerda alterna 440/880 Hz a cada 0,5 s (queda vira buraco audível); direita fixa em 660 Hz
// (canais diferentes provam estéreo). Fase acumulada evita erro de float e cliques.
static int32_t fillFrames(Frame* frames, int32_t count) {
    static float phaseL, phaseR;
    static uint32_t n;
    for (int i = 0; i < count; i++, n++) {
        float hzL = (n / 22050) % 2 ? 880.f : 440.f;
        phaseL = fmodf(phaseL + 2 * PI * hzL / 44100.f, 2 * PI);
        phaseR = fmodf(phaseR + 2 * PI * 660.f / 44100.f, 2 * PI);
        frames[i].channel1 = BT_AMP * sinf(phaseL);
        frames[i].channel2 = BT_AMP * sinf(phaseR);
    }
    return count;
}

void setup() {
    Serial.begin(115200);
    a2dp.set_ssid_callback(onDeviceFound);
    a2dp.set_on_connection_state_changed(onState);
    a2dp.set_on_audio_state_changed(onAudio);
    if (*BT_PIN)
        a2dp.set_pin_code(BT_PIN, ESP_BT_PIN_TYPE_FIXED);
    a2dp.set_auto_reconnect(true);
    a2dp.start(fillFrames);
}

void loop() {
    delay(1000); // lint: ok ferramenta de bancada, nao entra no firmware final
}

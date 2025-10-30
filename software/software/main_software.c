#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "lora_RFM95.h"

// ==========================================================
// ===           CONFIGURAÇÕES E DEFINIÇÕES GLOBAIS        ===
// ==========================================================

// Mapeamento dos pinos do módulo LoRa no Raspberry Pi Pico.
// Certifique-se de ajustar de acordo com sua ligação física.
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19
#define PIN_RST  20
#define PIN_DIO0 8  // Pino de interrupção (necessário para LoRa)

// Frequência de operação (em Hz)
// Use a frequência correta para sua região:
// - 915E6: América (US915)
// - 868E6: Europa (EU868)
// - 433E6: Ásia (AS433)
#define LORA_FREQUENCY 915E6

// Intervalo entre envios (ms)
#define SEND_INTERVAL_MS 10000 // 10 segundos

// Estrutura do pacote recebido via LoRa
typedef struct {
    int16_t temperatura;
    int16_t umidade;
} sensor_packet_t;

struct repeating_timer sync_timer;
int sync_dot_index = 0;

// ==========================================================
// ===                   FUNÇÕES DE DISPLAY               ===
// ==========================================================

// Callback usado pelo temporizador para exibir pontos animados
bool sync_screen_animation(struct repeating_timer *t) {
    if (sync_dot_index < 3) {
        ssd1306_SetCursor((sync_dot_index + 4) * 12, 6 + 15);
        ssd1306_WriteString(".", Font_16x15, White);
    }
    if (sync_dot_index < 6 && sync_dot_index > 2) {
        ssd1306_SetCursor((sync_dot_index + 1) * 10, 6 + 15);
        ssd1306_WriteString(" ", Font_16x15, White);
    }
    if (sync_dot_index > 5) {
        sync_dot_index = -1;
    }
    sync_dot_index++;
    ssd1306_UpdateScreen();
    return true;
}

// Mostra tela de sincronização ("Sincronizando...")
void show_sync_screen() {
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 6);
    ssd1306_WriteString("Sincronizando", Font_16x15, White);
    ssd1306_UpdateScreen();

	ssd1306_SetCursor(0, 6);
    add_repeating_timer_ms(300, sync_screen_animation, NULL, &sync_timer);
    sleep_ms(5000);
}

// Mostra dados de temperatura e umidade no display
void show_sensor_data(float temperatura, float umidade) {
    char temp_buf[10];
    char umid_buf[10];

    sprintf(temp_buf, "%.1fC", temperatura);
    sprintf(umid_buf, "%.1f%%", umidade);

    ssd1306_Fill(Black);
    ssd1306_DrawLineCustom(0, 31, 127, 31, White);

    ssd1306_SetCursor(15, 5);
    ssd1306_WriteString(temp_buf, Font_16x24, White);

    ssd1306_SetCursor(15, 38);
    ssd1306_WriteString(umid_buf, Font_16x24, White);

    ssd1306_UpdateScreen();
}

// ==========================================================
// ===                FUNÇÕES DE INICIALIZAÇÃO             ===
// ==========================================================

// Inicializa o sistema LoRa e o display OLED
void init_lora_system() {
    stdio_init_all();
    ssd1306_Init();

    ssd1306_Fill(Black);
    ssd1306_SetCursor(10, 10);
    ssd1306_WriteString("Iniciando LoRa", Font_16x15, White);
    ssd1306_UpdateScreen();

    lora_config_t lora_cfg = {
        .spi_instance = spi0,
        .pin_miso = PIN_MISO,
        .pin_cs   = PIN_CS,
        .pin_sck  = PIN_SCK,
        .pin_mosi = PIN_MOSI,
        .pin_rst  = PIN_RST,
        .pin_dio0 = PIN_DIO0,
        .frequency = LORA_FREQUENCY
    };

    if (!lora_init(lora_cfg)) {
        ssd1306_Fill(Black);
        ssd1306_SetCursor(20, 25);
        ssd1306_WriteString("Falha LoRa!", Font_16x15, White);
        ssd1306_UpdateScreen();
        while (1);
    }

    ssd1306_Fill(Black);
    ssd1306_SetCursor(15, 25);
    ssd1306_WriteString("LoRa Pronto!", Font_16x15, White);
    ssd1306_UpdateScreen();
    sleep_ms(1000);

    lora_start_rx_continuous();
}

// ==========================================================
// ===                     FUNÇÃO PRINCIPAL               ===
// ==========================================================

int main() {
    sensor_packet_t pacote_recebido;
    bool primeira_leitura = true;

    init_lora_system();
    show_sync_screen();

    while (1) {
        int len = lora_receive_bytes((uint8_t *)&pacote_recebido, sizeof(pacote_recebido));

        if (len == sizeof(pacote_recebido)) {
            if (primeira_leitura) {
                cancel_repeating_timer(&sync_timer);
                ssd1306_Fill(Black);
            }

            float temp = (float)pacote_recebido.temperatura / 100;
            float umid = (float)pacote_recebido.umidade / 100;

            show_sensor_data(temp, umid);
            primeira_leitura = false;
        }
    }

    return 0;
}

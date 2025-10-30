// ==========================================================
// === Firmware TX bare-metal (RISC-V)                     ===
// === Envia leituras simuladas de temperatura/umidade via LoRa (RFM9X)
// ==========================================================

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <irq.h>
#include <uart.h>
#include <console.h>
#include <generated/csr.h>
#include <generated/soc.h>
#include <system.h>

#include "aht10.h"        // Biblioteca do sensor AHT10
#include "lora_RFM95.h"   // Biblioteca do módulo LoRa

// ==========================================================
// ===                 DEFINIÇÕES GLOBAIS                 ===
// ==========================================================
#define LORA_FREQUENCY 915.0f   // Frequência (MHz) — US915 para Brasil
#define SENSOR_SEND_INTERVAL_MS 10000  // Intervalo entre envios (10s)

// ==========================================================
// ===                PROTÓTIPOS DE FUNÇÃO                ===
// ==========================================================
static void transmit_sensor_data(void);
static void delay_ms(unsigned int ms);

// ==========================================================
// ===               UTILITÁRIOS DE TEMPO                 ===
// ==========================================================
// Busy-wait simples em milissegundos
static void delay_ms(unsigned int ms) {
    for (unsigned int i = 0; i < ms; ++i) {
#ifdef CSR_TIMER0_BASE
        busy_wait_us(1000);
#else
        for (volatile int j = 0; j < 2000; j++);
#endif
    }
}

// ==========================================================
// ===              ROTINA DE TRANSMISSÃO LoRa            ===
// ==========================================================
// Lê os dados do sensor AHT10 e envia via LoRa
static void transmit_sensor_data(void) {
    sensor_data_t sensor_data; // Renomeado: antes 'dados'

    printf("Lendo dados do sensor AHT10...\n");

    if (aht10_get_data(&sensor_data)) {
        printf("  Temperatura: %d.%02d C\n",
               sensor_data.temperatura / 100,
               abs(sensor_data.temperatura) % 100);

        printf("  Umidade: %d.%02d %%\n",
               sensor_data.umidade / 100,
               abs(sensor_data.umidade) % 100);

        if (!lora_send_bytes((uint8_t*)&sensor_data, sizeof(sensor_data_t))) {
            printf("Erro durante o envio via LoRa (verificar log da biblioteca).\n");
        }
    } else {
        printf("Falha na leitura do sensor AHT10. Envio cancelado.\n");
    }
}

// ==========================================================
// ===                    FUNÇÃO MAIN                     ===
// ==========================================================
int main(void) {
#ifdef CONFIG_CPU_HAS_INTERRUPT
    irq_setmask(0);
    irq_setie(1);
#endif
    uart_init();

#ifdef CSR_TIMER0_BASE
    // Inicializa timer0 (contagem em microssegundos)
    timer0_en_write(0);
    timer0_reload_write(0);
    timer0_load_write(CONFIG_CLOCK_FREQUENCY / 1000000);
    timer0_en_write(1);
    timer0_update_value_write(1);
#endif

    delay_ms(500);

    printf("\n---------------- LiteX BIOS --------------\n");
    printf("Tarefa 05 – Transmissão de dados via LoRa\n");

    // Inicializa barramento I2C e periféricos
    i2c_init();
    aht10_init();

    // Inicializa módulo LoRa
    if (!lora_init()) {
        printf("FALHA CRÍTICA: Inicialização do módulo LoRa falhou.\n");
        while (1);
    }

    // Loop principal: lê sensor e transmite via LoRa
    while (1) {
        transmit_sensor_data();
        delay_ms(SENSOR_SEND_INTERVAL_MS);
    }

    return 0;
}

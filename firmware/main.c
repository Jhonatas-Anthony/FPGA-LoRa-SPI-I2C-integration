#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <irq.h>
#include <uart.h>
#include <console.h>
#include <generated/csr.h>

// --------------------------------------------------
// Configurações AHT10 / I2C
// --------------------------------------------------
#define AHT10_I2C_ADDR      0x38  // 7-bit
#define AHT10_CMD_MEAS0     0xAC
#define AHT10_CMD_MEAS1     0x33
#define AHT10_CMD_MEAS2     0x00

#define AHT10_MEASURE_DELAY_MS 80  // 80 ms para segurança
#define I2C_DELAY_US_MIN       10   // delay base em microsegundos (aprox)
#define I2C_OP_TIMEOUT         1000  // iterações de timeout para ack

// Offsets / máscara conforme seu generated/csr.h
#define I2C_W_SCL_BIT   0
#define I2C_W_OE_BIT    1
#define I2C_W_SDA_BIT   2

// Funções de Console/Utilidade
static char *readstr(void);
static char *get_token(char **str);

// Prototipos

int aht10_get(float *temperature, float *humidity);
void show_aht10(void);
void i2c_scan(void);
static void i2c_start(void);
static void i2c_stop(void);
static int  i2c_write_byte(uint8_t byte);

// --------------------------------------------------
// Utilitário: delays aproximados (busy-wait)
//   Ajuste os loops se quiser mais precisão dependendo do sys_clk.
// --------------------------------------------------
static void delay_short(void)
{
    // micro-delay aproximado. Calibrar se necessário.
    // Este loop foi escolhido para rodar razoavelmente em SoC ~100MHz.
    volatile int i;
    for (i = 0; i < 60; i++) {
        __asm__ volatile("nop");
    }
}

static void delay_us(unsigned us)
{
    for (unsigned j = 0; j < us; j++) {
        delay_short();
    }
}

static void delay_ms(unsigned ms)
{
    for (unsigned i = 0; i < ms; i++) {
        delay_us(1000 / 10); // cada delay_us aqui é aproximado; este é um aproximador grosso
    }
}

// --------------------------------------------------
// Low-level I2C bit-bang using CSRs
//   write register layout: [ bit2:SDA | bit1:OE | bit0:SCL ]
//   read register: bit0 = SDA (level)
// --------------------------------------------------
static inline void i2c_w_raw(uint8_t scl, uint8_t oe, uint8_t sda)
{
    uint32_t v = ((uint32_t)(scl & 0x1) << I2C_W_SCL_BIT) |
                 ((uint32_t)(oe  & 0x1) << I2C_W_OE_BIT)  |
                 ((uint32_t)(sda & 0x1) << I2C_W_SDA_BIT);
    i2c0_w_write(v);
}

static inline uint8_t i2c_r_sda(void)
{
    return (uint8_t)(i2c0_r_read() & 0x1);
}

// Drive SDA low: OE=1, SDA=0
// Release SDA (let pull-up take it high): OE=0 (SDA bit ignored)
static void i2c_drive_sda_low(void)  { i2c_w_raw(0, 1, 0); } // SCL currently 0
static void i2c_release_sda(void)    { i2c_w_raw(0, 0, 0); }
static void i2c_set_scl(uint8_t level)
{
    // Keep OE and SDA as released (OE=0), set SCL level
    // We'll keep SDA state as 0 in the value; if OE=1, SDA bit is considered.
    // Use current OE=0 for safe open-drain behavior unless driving SDA.
    i2c_w_raw(level ? 1 : 0, 0, 0);
}

void i2c_scan(void)
{
    printf("I2C Scan: procurando dispositivos...\n");
    int found = 0;

    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        i2c_start();
        int ack = i2c_write_byte((addr << 1) | 0); // escrever bit = 0
        i2c_stop();
        if (ack == 0) {
            printf("  Dispositivo encontrado no endereço 0x%02X\n", addr);
            found++;
        }
    }

    if (found == 0) {
        printf("  Nenhum dispositivo detectado!\n");
    } else {
        printf("  Total de dispositivos encontrados: %d\n", found);
    }
}

// --------------------------------------------------
// I2C primitives (start/stop, write/read bit/byte)
// --------------------------------------------------
__attribute__((unused))
static void i2c_bus_idle(void)
{
    // release SDA and set SCL high
    i2c_release_sda();
    delay_us(I2C_DELAY_US_MIN);
    i2c_set_scl(1);
    delay_us(I2C_DELAY_US_MIN);
}

static void i2c_start(void)
{
    // SDA: 1 -> 0 while SCL=1
    i2c_release_sda();
    delay_us(I2C_DELAY_US_MIN);
    i2c_set_scl(1);
    delay_us(I2C_DELAY_US_MIN);
    // pull SDA low
    i2c_drive_sda_low();
    delay_us(I2C_DELAY_US_MIN);
    // pull SCL low to start transfer
    i2c_set_scl(0);
    delay_us(I2C_DELAY_US_MIN);
}

static void i2c_stop(void)
{
    // SDA low -> SCL high -> SDA high
    i2c_drive_sda_low();
    delay_us(I2C_DELAY_US_MIN);
    i2c_set_scl(1);
    delay_us(I2C_DELAY_US_MIN);
    i2c_release_sda();
    delay_us(I2C_DELAY_US_MIN);
}

static void i2c_write_bit(uint8_t bit)
{
    if (bit) {
        // release SDA to let pull-up drive it high
        i2c_release_sda();
    } else {
        // drive SDA low
        i2c_drive_sda_low();
    }
    delay_us(I2C_DELAY_US_MIN);
    // clock high
    i2c_set_scl(1);
    delay_us(I2C_DELAY_US_MIN);
    // clock low
    i2c_set_scl(0);
    delay_us(I2C_DELAY_US_MIN);
}

static uint8_t i2c_read_bit(void)
{
    uint8_t bit;
    // release SDA (so slave can drive)
    i2c_release_sda();
    delay_us(I2C_DELAY_US_MIN);
    // clock high, then sample SDA
    i2c_set_scl(1);
    delay_us(I2C_DELAY_US_MIN);
    bit = i2c_r_sda();
    // clock low
    i2c_set_scl(0);
    delay_us(I2C_DELAY_US_MIN);
    return bit;
}

// static int i2c_write_byte(uint8_t byte)
// {
//     for (int i = 7; i >= 0; --i) {
//         i2c_write_bit((byte >> i) & 0x1);
//     }
//     // read ACK (0 = ACK)
//     uint8_t ack = i2c_read_bit();
//     return (ack == 0) ? 0 : -1;
// }

static void i2c_write_bit(uint8_t bit)
{
    if (bit)
        i2c_release_sda();
    else
        i2c_drive_sda_low();

    printf("SDA = %d, SCL baixo\n", bit);  // DEBUG

    i2c_set_scl(1);
    printf("SCL alto, lendo SDA: %d\n", i2c_r_sda());  // DEBUG
    delay_us(I2C_DELAY_US_MIN);

    i2c_set_scl(0);
    delay_us(I2C_DELAY_US_MIN);
}

static uint8_t i2c_read_byte(bool send_ack)
{
    uint8_t v = 0;
    for (int i = 7; i >= 0; --i) {
        uint8_t b = i2c_read_bit();
        v |= (b << i);
    }
    // send ack(0) or nack(1)
    i2c_write_bit(send_ack ? 0 : 1);
    return v;
}

// --------------------------------------------------
// Helper: write then stop, read with start
// --------------------------------------------------
static int i2c_write_bytes(uint8_t addr7, const uint8_t *data, unsigned len)
{
    i2c_start();
    if (i2c_write_byte((addr7 << 1) | 0) < 0) {
        i2c_stop();
        return -1;
    }
    for (unsigned i = 0; i < len; ++i) {
        if (i2c_write_byte(data[i]) < 0) {
            i2c_stop();
            return -1;
        }
    }
    i2c_stop();
    return 0;
}

static int i2c_read_bytes(uint8_t addr7, uint8_t *buf, unsigned len)
{
    i2c_start();
    if (i2c_write_byte((addr7 << 1) | 1) < 0) {
        i2c_stop();
        return -1;
    }
    for (unsigned i = 0; i < len; ++i) {
        bool ack = (i + 1) < len; // ACK every byte except last
        buf[i] = i2c_read_byte(ack);
    }
    i2c_stop();
    return 0;
}

// --------------------------------------------------
// AHT10 specific routines
// --------------------------------------------------
// static int aht10_trigger_and_read(uint8_t buf[6])
// {
//     uint8_t cmd[3] = { AHT10_CMD_MEAS0, AHT10_CMD_MEAS1, AHT10_CMD_MEAS2 };

//     // write command (start + addrW + 3 bytes + stop)
//     if (i2c_write_bytes(AHT10_I2C_ADDR, cmd, 3) < 0) {
//         printf("AHT10: erro no WRITE do comando\n");
//         return -1;
//     }

//     // Espera conversão (datasheet ~75ms)
//     delay_ms(AHT10_MEASURE_DELAY_MS);

//     // Ler 6 bytes com start + addrR
//     if (i2c_read_bytes(AHT10_I2C_ADDR, buf, 6) < 0) {
//         printf("AHT10: erro no READ dos dados\n");
//         return -1;
//     }

//     return 0;
// }

static int aht10_trigger_and_read(uint8_t buf[6])
{
    printf("I2C: tentando escrever no endereço 0x%02x...\n", AHT10_I2C_ADDR);

    uint8_t cmd[3] = { AHT10_CMD_MEAS0, AHT10_CMD_MEAS1, AHT10_CMD_MEAS2 };

    // Início: só teste o start + address
    i2c_start();
    int ack = i2c_write_byte((AHT10_I2C_ADDR << 1) | 0);
    i2c_stop();

    if (ack < 0) {
        printf("AHT10: sem ACK do endereço 0x%02x\n", AHT10_I2C_ADDR);
        return -1;
    }

    printf("AHT10: ACK recebido, endereço OK!\n");
    return 0;
}

static int aht10_parse(const uint8_t buf[6], float *temperature, float *humidity)
{
    // raw humidity: bits [19:4] from buf[1],buf[2],buf[3] (total 20 bits)
    uint32_t raw_h = ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
    raw_h >>= 4;
    // raw temp: lower 20 bits from buf[3] (4 LSB) + buf[4] + buf[5]
    uint32_t raw_t = (((uint32_t)buf[3] & 0x0F) << 16) | ((uint32_t)buf[4] << 8) | (uint32_t)buf[5];

    float h = (float)raw_h * 100.0f / 1048576.0f;   // 2^20 = 1048576
    float t = (float)raw_t * 200.0f / 1048576.0f - 50.0f;

    if (humidity) *humidity = h;
    if (temperature) *temperature = t;
    return 0;
}

int aht10_get(float *temperature, float *humidity)
{
    uint8_t buf[6];
    if (aht10_trigger_and_read(buf) < 0)
        return -1;
    aht10_parse(buf, temperature, humidity);
    return 0;
}

// --------------------------------------------------
// Função pública pedida: show_aht10 (apenas uma leitura)
// --------------------------------------------------
// void show_aht10(void)
// {
//     float t = 0.0f, h = 0.0f;
//     if (aht10_get(&t, &h) == 0) {
//         printf("AHT10 => Temperatura: %.2f C   Umidade: %.2f %%RH\n", t, h);
//     } else {
//         printf("AHT10 => Erro ao ler sensor\n");
//     }
// }

void show_aht10(void)
{
    float t = 0.0f, h = 0.0f;
    if (aht10_get(&t, &h) == 0) {
        // converte para centésimos (int) para evitar usar %f no printf
        int t100 = (int)(t * 100.0f + (t >= 0.0f ? 0.5f : -0.5f));
        int h100 = (int)(h * 100.0f + 0.5f);

        int t_int = t100 / 100;
        int t_frac = t100 % 100;
        if (t_frac < 0) t_frac = -t_frac;

        int h_int = h100 / 100;
        int h_frac = h100 % 100;
        if (h_frac < 0) h_frac = -h_frac;

        printf("AHT10 => Temperatura: %d.%02d C   Umidade: %d.%02d %%RH\n",
               t_int, t_frac, h_int, h_frac);
    } else {
        printf("AHT10 => Erro ao ler sensor\n");
    }
}

// ---------------------------------------------------
// Funções de Serviço do Console (LiteX Shell)
// ---------------------------------------------------

static char *readstr(void)
{
    char c[2];
    static char s[64];
    static int ptr = 0;

    if (readchar_nonblock())
    {
        c[0] = readchar();
        c[1] = 0;
        switch (c[0])
        {
        case 0x7f:
        case 0x08:
            if (ptr > 0)
            {
                ptr--;
                putsnonl("\x08 \x08");
            }
            break;
        case 0x07:
            break;
        case '\r':
        case '\n':
            s[ptr] = 0x00;
            putsnonl("\n");
            ptr = 0;
            return s;
        default:
            if (ptr >= (sizeof(s) - 1))
                break;
            putsnonl(c);
            s[ptr] = c[0];
            ptr++;
            break;
        }
    }
    return NULL;
}

static char *get_token(char **str)
{
    char *c, *d;

    c = (char *)strchr(*str, ' ');
    if (c == NULL)
    {
        d = *str;
        *str = *str + strlen(*str);
        return d;
    }
    *c = 0;
    d = *str;
    *str = c + 1;
    return d;
}

static void prompt(void)
{
    printf("RUNTIME>");
}

static void help(void)
{
    puts("Available commands:");
    puts("help                            - this command");
    puts("reboot                          - reboot CPU");
    puts("aht10                           - Read AHT10 once and print temperature/humidity");
}

static void reboot(void)
{
    ctrl_reset_write(0); 
}


static void console_service(void)
{
    char *str;
    char *token;

    str = readstr();
    if (str == NULL)
        return;
    token = get_token(&str);
    if (strcmp(token, "help") == 0)
        help();
    else if (strcmp(token, "reboot") == 0)
        reboot();
    else if (strcmp(token, "aht10") == 0)
        show_aht10();
    else if (strcmp(token, "i2cscan") == 0)
        i2c_scan();
    else if (strcmp(token, "") == 0)
        ;
    else
        printf("unknown command: %s\n", token);

    prompt();
}

int main(void)
{
#ifdef CONFIG_CPU_HAS_INTERRUPT
    irq_setmask(0);
    irq_setie(1);
#endif
    uart_init();

    printf("LiteX SoC (RV32) Initialized!\n");
    help();
    prompt();

    while (1)
    {
        console_service();
    }

    return 0;
}
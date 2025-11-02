# Transmissão de dados via LoRa — Projeto (FPGA ColorLight i9 ↔ BitDogLab)

## Resumo
SoC customizado (LiteX, VexRiscv) na ColorLight i9 lê AHT10 via I2C e transmite periodicamente (10 s) dados de temperatura/umidade via módulo LoRa RFM96 conectado por SPI. A BitDogLab recebe e exibe no OLED.

---

## Estrutura do repositório

```
hardware/ # Wrapper LiteX e scripts para gerar bitstream
hardware/firmware/ # Firmware C para o VexRiscv (FPGA)
software/software # Firmware do BitDogLab (receptor)
README.md
```

## Scripts

[scripts.md](./scripts.md)

## Diagrama de blocos:

# Diagrama de Blocos – Sistema LoRa FPGA ↔ BitDogLab

```mermaid
flowchart TB
    %% FPGA Side
    subgraph FPGA_ColorLight_i9 ["FPGA- SoC LiteX"]
        CPU["VexRiscv Core"]
        I2C["Controlador I2C"]
        SPI["Controlador SPI"]
        Timer["Timer (10s)"]
        Sensor["Sensor AHT10\n(Temperatura / Umidade)"]
        LoRa_Tx["Módulo LoRa RFM96\n(Transmissor)"]

        CPU <--> I2C
        I2C <--> Sensor
        CPU --> SPI

        SPI --> LoRa_Tx
        
        
        Sensor --> Timer
        Timer --> Sensor
    end

    %% Communication Channel
    LoRaChannel["Transmissão LoRa"]

    LoRa_Tx --> LoRaChannel

    %% BitDogLab Side
    subgraph BitDogLab ["BitDogLab - Nó Receptor"]
        LoRa_Rx["Módulo LoRa RFM96\n(Receptor)"]
        MCU["Microcontrolador BitDogLab"]
        OLED["Display OLED"]
    end

    LoRaChannel --> LoRa_Rx
    LoRa_Rx --> MCU
    MCU --> OLED
```

## **Descrição do Fluxo**

| Parte | Função |
|------|--------|
FPGA (SoC LiteX) | Coleta dados via I2C + envia via LoRa (SPI)  
Sensor AHT10 | Mede temperatura e umidade  
Módulo LoRa (FPGA) | Transmissor LoRa  
BitDogLab | Recebe e exibe dados  
OLED | Mostra temperatura e umidade em tempo real  

---

## **Resumo do Processo**

1. FPGA inicializa SPI, I2C e timer
2. A cada 10s:
   - lê sensor AHT10 (I2C)
   - envia dados via LoRa (SPI → RFM96)
3. BitDogLab recebe via LoRa
4. MCU decodifica e mostra no OLED

---

# Informações

## Pinos Mapeados

### SPI (LoRa)

| Função  | FPGA pin | PIN |
| ------- | -------: | --- |
| SPI CLK |      G20 | 14  |
| MOSI    |      L18 | 12  |
| MISO    |      M18 | 11  |
| CS_N    |      N17 | 9   |
| RESET   |      L20 | 10  |


### I2C (AHT10)

| Função | FPGA pin | PIN |
| ------ | -------: | --- |
| SCL    |      U17 | 1   |
| SDA    |      U18 | 2   |

## Dados sobre frequência

- ```sys_clk_freq do SoC:``` 60 MHz (padrão no wrapper). Ajustável via --sys-clk-freq.

- LoRa:
    - frequência central: 915 MHz (verifique legislação local).
    - Bandwidth: 125 kHz

- SPI para LoRa: 1–8 MHz.
- I2C: 100 kHz (bitbang I2C master)

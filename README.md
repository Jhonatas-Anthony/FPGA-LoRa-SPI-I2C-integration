# Transmissão de dados via LoRa

## Hardware

Deve estar dentro da pasta "hardware"

```bash
cd hardware
```

Deve ativar o ambiente do oss-cad-suite

```bash
source [SEU-PATH]/oss-cad-suite/environment
```

### Compilar o código em python e subir

#### Código completo

```bash
python3 litex/colorlight_i5.py --board i9 --revision 7.2 --build --load --ecppack-compress
```

#### Apenas compilar 

```bash
python3 litex/colorlight_i5.py --board i9 --revision 7.2 --build 
```

#### Apenas subir

```bash
python3 litex/colorlight_i5.py --load --ecppack-compress
```

### Compilar o código em c

```bash
cd firmware

make

cd..
```

### Subir o firmware em python para a FPGA

```bash
sudo [SEU-PATH]/oss-cad-suite/bin/openFPGALoader -b colorlight-i5 build/colorlight_i5/gateware/colorlight_i5.bit
```

### Subir o código em C e iniciar o Terminal Serial FPGA

```bash
litex_term /dev/ttyACM0 --kernel firmware/firmware.bin 
```

## BitDogLab

Primeiro, vamos precisar da extensão Raspberry Pi Pico, devemos abrir um projeto dentro da pasta software para realizar as configurações e rodar os comandos.

```bash
code software/software
```

Após a extensão identificar que esse diretório é um projeto raspberry pi pico, podemos seguir com os passos

### Gerar arquivo de build

```bash
cmake -G Ninja -S . -B build

cmake --build build
```

Após isso podemos usar a interface da extensão *Raspberry Pi Pico*

#!/usr/bin/env bash
# build.sh
# Automação para fluxo LiteX + Verilog
#
# Flags disponíveis:
#   --iverilog   → compila e roda simulação Icarus Verilog
#   --up-c       → compila firmware, grava FPGA e abre litex_term
#   --run-all    → gera SoC, grava FPGA e abre litex_term
#
# Uso:
#   ./build.sh --iverilog
#   ./build.sh --up-c
#   ./build.sh --run-all
#
# Autor: Jhonatas Anthony

set -euo pipefail

# === CONFIGURAÇÕES PADRÃO ===
FIRMWARE_DIR="./firmware"
OPENFPGALOADER="/home/jhonatas/Downloads/oss-cad-suite/bin/openFPGALoader"
OPENFPGALOADER_ARGS="-b colorlight-i5 build/colorlight_i5/gateware/colorlight_i5.bit"
LITEX_DEVICE="/dev/ttyACM0"
LITEX_ARGS="--kernel firmware/main.bin"
IVERILOG_OUT="./a.out"
LITEX_PY="litex/colorlight_i5.py"
LITEX_BUILD_ARGS="--board i9 --revision 7.2 --build --load --ecppack-compress"

# === FUNÇÕES ===

usage() {
  cat <<EOF
Uso: $0 [FLAG]

Flags disponíveis:
  --iverilog   Compila e executa simulação com Icarus Verilog
  --up-c       Build do firmware, upload via openFPGALoader e abre litex_term
  --run-all    Gera SoC (python3 litex/... --build --load), depois executa --up-c
  -h, --help   Mostra esta ajuda

EOF
  exit 1
}

run_iverilog() {
  echo "[iverilog] Compilando..."
  if ! iverilog -g2012 rtl/dotProduct.sv testbench/dot_tb.sv -o "$IVERILOG_OUT"; then
    echo "Erro: falha na compilação Icarus Verilog." >&2
    exit 2
  fi
  echo "[iverilog] Executando simulação..."
  vvp "$IVERILOG_OUT"
}

run_up_c() {
  echo "[up-c] Entrando em ${FIRMWARE_DIR} e executando make..."
  if [[ ! -d "$FIRMWARE_DIR" ]]; then
    echo "Erro: diretório de firmware não encontrado: $FIRMWARE_DIR" >&2
    exit 3
  fi

  pushd "$FIRMWARE_DIR" > /dev/null
  if ! make; then
    echo "Erro: make falhou em $FIRMWARE_DIR" >&2
    popd > /dev/null
    exit 4
  fi
  popd > /dev/null

  if [[ ! -x "$OPENFPGALOADER" ]]; then
    echo "Erro: openFPGALoader não encontrado em $OPENFPGALOADER" >&2
    exit 5
  fi

  # Solicita sudo e executa
  echo "[up-c] Executando openFPGALoader (sudo)..."
  sudo -v
  sudo "$OPENFPGALOADER" $OPENFPGALOADER_ARGS

  # Verifica dispositivo
  if [[ ! -e "$LITEX_DEVICE" ]]; then
    echo "Erro: dispositivo $LITEX_DEVICE não encontrado." >&2
    exit 6
  fi

  echo "[up-c] Abrindo litex_term..."
  exec litex_term "$LITEX_DEVICE" $LITEX_ARGS
}

run_all() {
  echo "[run-all] Gerando SoC..."
  if [[ ! -f "$LITEX_PY" ]]; then
    echo "Erro: arquivo Python do LiteX não encontrado: $LITEX_PY" >&2
    exit 7
  fi

  python3 "$LITEX_PY" $LITEX_BUILD_ARGS
  echo "[run-all] Build do SoC concluído."

  run_up_c
}

# === PARSE DE FLAGS ===
if [[ $# -lt 1 ]]; then
  usage
fi

case "$1" in
  --iverilog) run_iverilog ;;
  --up-c) run_up_c ;;
  --run-all) run_all ;;
  -h|--help) usage ;;
  *) echo "Flag inválida: $1" >&2; usage ;;
esac

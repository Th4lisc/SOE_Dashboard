#!/bin/bash

# --- Configuração ---
# O dispositivo serial criado pelo rfcomm connect
BT_DEVICE="/dev/rfcomm0"

# O atraso (em segundos) entre o envio de um comando e a leitura da resposta
SLEEP_TIME=0.5

# --- Função para Enviar e Receber Dados ---
# $1 é o comando a ser enviado (ex: "ATZ")
send_and_receive() {
    COMMAND="$1\r" # Adiciona o Carriage Return exigido pelo ELM327

    # 1. Envia o comando para a porta serial
    # Use 'echo -e' para interpretar o '\r' (Carriage Return)
    echo -e "$COMMAND" > "$BT_DEVICE"

    # 2. Espera pela resposta
    sleep "$SLEEP_TIME"

    # 3. Lê a resposta da porta serial (lê apenas uma linha ou o que estiver disponível)
    # Usa 'head -n 1' para ler uma linha, mas pode precisar de 'timeout' em alguns sistemas
    RESPONSE=$(cat "$BT_DEVICE" | head -n 1)

    echo "COMANDO: $1"
    echo "RESPOSTA: $RESPONSE"
    echo "---------------------------"
}

# --- 1. Inicialização do ELM327 ---

echo "Iniciando comunicação com ELM327 em $BT_DEVICE..."
echo "---------------------------"

# Reset do ELM327
send_and_receive "ATZ"

# Desliga o eco de caracteres (para não receber o comando de volta)
send_and_receive "ATE0"

# Seleciona o protocolo (0: Automático)
send_and_receive "AT SP 0"

# --- 2. Leitura dos Dados OBD-II (PID's) ---

echo "Iniciando leitura de dados OBD-II (PID's)..."
echo "---------------------------"

# Loop para leitura contínua
while true; do

    # Exemplo 1: RPM do Motor (PID: 01 0C)
    send_and_receive "01 0C"

    # Exemplo 2: Velocidade do Veículo (PID: 01 0D)
    send_and_receive "01 0D"

    # Opcional: Pausa maior entre as leituras para não sobrecarregar
    sleep 2
done

# Observação: A desconexão é feita interrompendo o processo 'rfcomm connect'
# (CTRL+C no terminal onde você rodou o 'rfcomm connect')
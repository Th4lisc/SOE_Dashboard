#  🚗 Dashboard OBD2 BLE

Este projeto implementa um **dashboard automotivo em Qt** que recebe dados OBD2 via **Bluetooth Low Energy (BLE)** através de um módulo ELM327 conectado a uma Raspberry Pi. O objetivo é integrar conceitos de **SO embarcados**, comunicação **BLE**, **multithreading**, e interface gráfica utilizando **Qt**.

##  Descrição Geral

O sistema é composto por dois módulos principais:

### 1. Backend (C) — `ble_stream.c`

* Conecta ao adaptador OBD2 via **BLE**
* Envia comandos OBD2 (PIDs)
* Processa as respostas
* Gera pacotes JSON
* Publica via **WebSocket** na porta `9090`
* Opera continuamente sem bloquear

### 2. Dashboard (C++/Qt) — `main.cpp`

* Conecta ao WebSocket `ws://localhost:9090`
* Exibe dados em tempo real
* Mostra indicadores de RPM, velocidade, MAP, TPS, temperatura, tensão e outros
* Atualiza a interface a cada ~33 ms
* Exibe indicador de **Conexão / Desconexão** (verde/vermelho)

## 🧩 Arquitetura do Sistema

```
[ Raspberry Pi ]                        [ PC/Linux com Qt ]
---------------------------------------------------------------
  ELM327 BLE  <---BLE--->  ble_stream.c   ---> WebSocket --->  dashboard (Qt)
---------------------------------------------------------------
                 JSON                             UI
```

## 📦 Arquivos do Projeto

```
/project
│── ble_stream.c        # Backend BLE + WebSocket
│── main.cpp            # Dashboard Qt
│── dashboard.pro       # Arquivo de build (qmake)
│── README.md           # Este documento
```

## 🔧 Compilação (qmake — opção utilizada)

### 1. Instalar Qt

Ubuntu/Debian:

```bash
sudo apt install qt6-base-dev qt6-websockets-dev
# ou Qt5:
# sudo apt install qtbase5-dev libqt5websockets5-dev
```

### 2. Entrar na pasta do projeto

```bash
cd project/
```

### 3. Gerar Makefile

```bash
qmake
```

### 4. Compilar

```bash
make
```

### 5. Executar

```bash
./dashboard
```

## 🛰️ Execução do Backend BLE

Na Raspberry Pi:

```bash
gcc ble_stream.c -o ble_stream -lbluetooth
sudo ./ble_stream
```

Backend abre automaticamente:

```
ws://localhost:9090
```

## 📡 Formato do JSON Recebido

```json
{
  "rpm": 2100,
  "speed": 45,
  "map": 87.2,
  "tps": 12.5,
  "battery": 13.7,
  "coolant_temp": 92
}
```

## 🎯 Objetivos de Aprendizagem (SO Embarcados)

* Comunicação com dispositivos embarcados
* BLE
* Processos e controle de fluxo
* WebSocket como IPC remoto
* Multithreading e temporização
* Interface gráfica reativa com Qt
* Integração C + C++
* Parsing de protocolos automotivos


## 👨‍🏫 Autores

**Thalis Cezar Ianzer** e **Jordano do Santos**
Engenharia Eletrônica — Universidade de Brasília (UnB)
Disciplina: *Sistemas Operacionais Embarcados*

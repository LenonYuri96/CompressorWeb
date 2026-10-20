# EnergizAR - Sistema de Automação e Monitoramento de Compressores via ESP32

O **EnergizAR** é uma solução de automação, proteção e telemetria industrial desenvolvida em torno do microcontrolador **ESP32**. O sistema realiza o acionamento de compressores de ar comprimido, motores de ventilação e secadores industriais por meio de **pulsos não-bloqueantes de 500 ms**, garantindo supervisão térmica contínua, monitoramento de pressão por pressostato de alívio, chave física de manutenção com religamento sequencial escalonado, sincronização de horário fabril via SNTP nativo e proteção estrita contra curto-ciclo com persistência em Flash (SPIFFS).

O firmware opera sob arquitetura de **tarefas cooperativas não-bloqueantes** sincronizadas por `millis()`, com servidor web HTTP assíncrono (**ESPAsyncWebServer**) e cão de guarda por hardware (**Task Watchdog Timer - TWDT** ativo em 10 segundos).

---

## 👥 Equipe de Desenvolvimento e Mentoria

Projeto concebido e implementado no âmbito do **Programa Trilhas de Futuro** (Curso Técnico em Informática para Internet — Minas Gerais):

### 💻 Alunos Desenvolvedores
- **Brenda Luiza**
- **Davi Afonso**
- **João Martins**
- **Pedro Lucas**
- **Sofia Tresse**

### 👨‍🏫 Instrutores e Arquitetura Técnica
- **Lenon Yuri** — *Instrutor / Arquiteto de Software e Firmware* ([GitHub](https://github.com/LYuri26))
- **Franco M. A. Caixeta** — *Instrutor / Co-autor de Infraestrutura* ([GitHub](https://github.com/RoCkHeLuCk))

---

## ⚡ Mapeamento Geral de Portas e Pinagem (Pinout Consolidado)

> **NOTA TÉCNICA DE ENGENHARIA:**  
> O circuito conta com isolamento estrito entre entradas e saídas. As saídas de relé geram pulsos de comando de 500 ms; as entradas de alarme térmico e retorno de contator utilizam resistores internos de **PULLDOWN**; a chave de manutenção utiliza resistor interno de **PULLUP** (fechamento em GND); e a porta **GPIO 12** deve iniciar obrigatoriamente em nível `LOW` (0V) durante o ciclo de energização para assegurar a tensão interna correta de 3.3V na memória Flash do ESP32.

### 📌 Tabela de Atribuição dos GPIOs do ESP32

| Módulo / Função | GPIO | Direção | Modo Elétrico | Nível Ativo | Descrição da Ligação Física |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Pulso Motor 1 (Compressor)** | **GPIO 25** | Saída | Digital OUTPUT | `HIGH` (3.3V) | Pulso transitório de 500 ms para bobina do contator/relé |
| **Pulso Motor 2 (Ventilador)** | **GPIO 27** | Saída | Digital OUTPUT | `HIGH` (3.3V) | Pulso transitório de 500 ms para bobina do contator/relé |
| **Pulso Motor 3 (Secador)**    | **GPIO 26** | Saída | Digital OUTPUT | `HIGH` (3.3V) | Pulso transitório de 500 ms para bobina do contator/relé |
| **Feedback Contator 1**        | **GPIO 14** | Entrada | `INPUT_PULLDOWN` | `HIGH` (3.3V) | Contato auxiliar NA (13-14) do contator do compressor |
| **Feedback Contator 2**        | **GPIO 33** | Entrada | `INPUT_PULLDOWN` | `HIGH` (3.3V) | Contato auxiliar NA (13-14) do contator do ventilador |
| **Feedback Contator 3**        | **GPIO 32** | Entrada | `INPUT_PULLDOWN` | `HIGH` (3.3V) | Contato auxiliar NA (13-14) do contator do secador |
| **Sobrecarga Motor 1**         | **GPIO 19** | Entrada | `INPUT_PULLDOWN` | `HIGH` (3.3V) | Contato NA (97-98) do relé térmico do compressor |
| **Sobrecarga Motor 2**         | **GPIO 12** | Entrada | `INPUT_PULLDOWN` | `HIGH` (3.3V) | Contato NA (97-98) do relé térmico do ventilador* |
| **Sobrecarga Motor 3**         | **GPIO 18** | Entrada | `INPUT_PULLDOWN` | `HIGH` (3.3V) | Contato NA (97-98) do relé térmico do secador |
| **Pressostato de Linha**       | **GPIO 17** | Entrada | `INPUT_PULLDOWN` | `HIGH` (3.3V) | Sensor de alívio e segurança de pressão de ar |
| **Chave de Manutenção**        | **GPIO 04** | Entrada | `INPUT_PULLUP`   | `LOW` (GND)   | Chave seletora comutada ao terra (GND) no painel |
| **Serial Debug (TX0 / RX0)**   | **GPIO 01 / 03**| UART | N/A | N/A | Log e depuração serial em 115200 bps |

*\* Importante: Manter o GPIO 12 despressurizado/em 0V durante o boot do ESP32.*

---

## 🛡️ Camadas de Segurança, Intertravamento e Temporização

### 1. Acionamento por Pulso Elétrico Não-Bloqueante (500 ms)
- As bobinas dos contatores não permanecem sob tensão permanente do microcontrolador.
- Ao comandar **LIGAR** ou **DESLIGAR**, o ESP32 eleva a respectiva porta para `HIGH` por **500 ms** (`DURACAO_PULSO_MS`) e desliga cooperativamente via `millis()`.
- Reduz aquecimento e estresse elétrico nos drivers de comutação e fontes auxiliares.

### 2. Proteção Anti-Curto-Ciclo com Persistência em Flash
- **Pós-Partida (60 minutos — 3.600 s):** Garante funcionamento ininterrupto para estabilização de temperatura de óleo e bobinamento.
- **Pós-Parada (10 minutos — 600 s):** Janela de descanso compulsório para despressurização dos cabeçotes antes de novo arranque.
- **Wear-Leveling por Cache em RAM:** Gravações redundantes de estado e timers em arquivos (`/motorX.txt`, `/timerX.txt`, `/statusX.txt`) são filtradas em memória RAM para prevenir desgaste precoce dos blocos Flash SPIFFS.

### 3. Proteção Térmica Bimetálica Multi-Estágio (`sobrecarga.cpp`)
- **Carência de Boot:** Ignora oscilações transitórias durante os primeiros 2.000 ms após energização.
- **Armação Segura de Entrada:** Cada pino precisa registrar nível `LOW` contínuo por no mínimo 800 ms antes de liberar o motor.
- **Debounce Temporal (250 ms):** Disparo de alarme térmico requer sinal estabilizado em nível alto por 250 ms.
- **Corte Imediato e Hard Lock:** Ao confirmar falha térmica, a saída é cortada e o motor permanece bloqueado contra religamentos.

### 4. Modo de Manutenção com Religamento Escalonado (`manutencao.cpp`)
- **Acionamento:** Pino **GPIO 04** com resistor de pull-up interno (`INPUT_PULLUP`). Ao comutar a chave para GND (`LOW`), o sistema entra em manutenção.
- **Memorização de Estado:** O estado de marcha dos motores é congelado na memória RAM antes do desligamento geral.
- **Religamento Sequencial Não-Bloqueante:** Ao desligar a chave de manutenção, os motores que estavam operando são reativados de forma escalonada com intervalo de **1.000 ms** entre cada partida, impedindo quedas severas de tensão causadas pela partida simultânea na rede elétrica fabril.

### 5. Disciplina Horária e Janela Fabril (`tempo.cpp`)
- **Sincronização em Background:** Utiliza o subsistema SNTP nativo do ESP-IDF sem bloquear o loop.
- **Servidores de Baixa Latência:** Primários em `a.st1.ntp.br`, `b.st1.ntp.br` e contingência em `pool.ntp.org`.
- **Fuso Horário:** UTC-3 (América/São Paulo, `-10800` segundos).
- **Janela de Operação Fabril:** Motores são desligados e bloqueados compulsoriamente fora do expediente industrial (**08:00 às 22:00**).

---

## 📡 Topologia de Rede e Conectividade Wi-Fi

O ESP32 opera em modo duplo concorrente (**`WIFI_AP_STA`**), mantendo um ponto de acesso local para manutenção mesmo que a infraestrutura de rede da fábrica fique offline:

```
                  +-----------------------------------+
                  |        ESP32 (WIFI_AP_STA)        |
                  +-----------------+-----------------+
                                    |
            +-----------------------+-----------------------+
            |                                               |
            v                                               v
   [ Interface SoftAP ]                            [ Interface Station ]
   SSID: EnergizAR                                 Conectado ao Roteador da Fábrica
   Senha: 12345678                                 IP dinâmico via DHCP (Ex: 192.168.1.105)
   IP Fixo: 192.168.26.7                           Acesso liberado pela intranet
```

---

## 🔌 Diagrama Elétrico e Conexões

```text
                   ESP32 DEVKIT V1
              +-----------------------+
(Pulso 500ms) | GPIO 25 ------> [IN1] | Módulo Relé 1 (Compressor Principal)
(Pulso 500ms) | GPIO 27 ------> [IN2] | Módulo Relé 2 (Motor Ventilador)
(Pulso 500ms) | GPIO 26 ------> [IN3] | Módulo Relé 3 (Motor Secador de Ar)
              |                       |
(Feedback NA) | GPIO 14 <------ [14]  | Contato Auxiliar Contator 1 (3.3V)
(Feedback NA) | GPIO 33 <------ [14]  | Contato Auxiliar Contator 2 (3.3V)
(Feedback NA) | GPIO 32 <------ [14]  | Contato Auxiliar Contator 3 (3.3V)
              |                       |
(Sobrecarga)  | GPIO 19 <------ [98]  | Contato Alarme Relé Térmico 1 (3.3V)
(Sobrecarga)  | GPIO 12 <------ [98]  | Contato Alarme Relé Térmico 2 (3.3V)*
(Sobrecarga)  | GPIO 18 <------ [98]  | Contato Alarme Relé Térmico 3 (3.3V)
              |                       |
(Pressostato) | GPIO 17 <------ [P1]  | Pressostato Pneumático de Linha (3.3V)
(Manutenção)  | GPIO 04 <------ [S1]  | Chave de Painel (Comutada para GND)
              |                       |
              | GND ----------> GND   | Barramento Comum de Terra (Comum)
              | 3V3 ----------> +3.3V | Barramento de Alimentação de Sinal
              +-----------------------+
* Nota: GPIO 12 deve permanecer despressurizado/em 0V durante o boot da placa.
```

---

## 🗂️ Estrutura de Diretórios do Projeto

```text
├── .pio/                       # Metadados de compilação do PlatformIO
├── include/                    # Cabeçalhos e protótipos de funções (.h)
│   ├── autenticador.h          # Gerenciamento de sessão e tokens de login
│   ├── creditos.h              # Rota e interface de autoria do projeto
│   ├── dashboard.h             # Painel operacional de controle dos motores
│   ├── index.h                 # Página de aterrissagem e formulário de login
│   ├── ligadesliga.h           # Definição dos pinos de potência, timers e pulsos
│   ├── manutencao.h            # Monitoramento da chave física de manutenção
│   ├── paginaserro.h           # Telas de erro HTTP (404, acesso negado)
│   ├── pressao.h               # Telemetria pneumática e nível de óleo
│   ├── pressostato.h           # Sensor mecânico de pressão (GPIO 17)
│   ├── sobrecarga.h            # Proteção térmica bimetálica (GPIOs 19, 12, 18)
│   ├── tempo.h                 # Sincronização horária com servidores NTP
│   ├── umidade.h               # Monitor de qualidade do ar comprimido e ponto de orvalho
│   ├── wificonexao.h           # Máquina de estados não-bloqueante do rádio Wi-Fi
│   ├── wifigerenciador.h       # Rotas REST de configuração de rede e IP
│   └── wifiinterface.h         # Interface gráfica do portal de Wi-Fi
├── src/                        # Implementação das rotinas em C++ (.cpp)
│   ├── autenticador.cpp        # Verificação de credenciais e cookies
│   ├── creditos.cpp            # Interface estilizada de desenvolvedores
│   ├── dashboard.cpp           # Dashboard responsivo com timers sincronizados
│   ├── index.cpp               # Interface inicial de login
│   ├── ligadesliga.cpp         # Lógica soberana de acionamento por pulso (25, 27, 26)
│   ├── main.cpp                # Orquestrador do firmware e loop cooperativo
│   ├── manutencao.cpp          # Debounce (150ms) e restauração escalonada (GPIO 04)
│   ├── paginaserro.cpp         # Respostas de exceção HTTP
│   ├── pressao.cpp             # Interface de pressão e pressostato
│   ├── pressostato.cpp         # Leitura de alívio pneumático (GPIO 17)
│   ├── sobrecarga.cpp          # Armação segura e desarme por sobrecarga (19, 12, 18)
│   ├── tempo.cpp               # Cliente SNTP para horário fabril (08h às 22h, NTP.br)
│   ├── umidade.cpp             # Interface de medição de saturação
│   ├── wificonexao.cpp         # Máquina de estados não-bloqueante Wi-Fi
│   ├── wifigerenciador.cpp     # Endpoints de varredura, persistência e IP
│   └── wifiinterface.cpp       # HTML/CSS/JS do gerenciador de Wi-Fi
├── platformio.ini              # Configurações de build e upload do microcontrolador
└── README.md                   # Documentação técnica do projeto
```

---

## 🚀 Como Compilar e Gravar

### Pré-requisitos
* [Visual Studio Code](https://code.visualstudio.com/)
* Extensão [PlatformIO IDE](https://platformio.org/)
* Cabo Micro-USB com suporte a dados

### Configuração do PlatformIO (`platformio.ini`)
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 115200
board_build.flash_mode = dio
board_build.f_flash = 40000000L
lib_deps =
    ESPAsyncWebServer
    AsyncTCP
```

### Comandos de Terminal
```bash
# 1. Clonar o repositório
git clone https://github.com/LYuri26/CompressorWeb.git
cd CompressorWeb

# 2. Compilar o projeto
pio run

# 3. Fazer o upload do firmware para a placa ESP32
pio run --target upload

# 4. Abrir o monitor serial para acompanhar os logs em tempo real
pio device monitor
```

---

## 🌐 Endpoints da API REST

| Rota | Método | Descrição | Resposta |
| :--- | :---: | :--- | :--- |
| `/login` | `POST` | Processa credenciais de acesso do operador | Redirecionamento com cookie de sessão |
| `/logout` | `GET` | Invalida a sessão ativa | Redirecionamento para tela de login |
| `/motor-state`| `GET` | Telemetria completa em tempo real | JSON com estados, alarmes e segundos restantes |
| `/toggle` | `GET/POST`| Manobra de ligar/desligar motor (`?motor=1&action=ligar`) | Confirmação em texto ou motivo do bloqueio |
| `/ipdarede` | `GET` | Consulta os endereços IP da rede externa e do AP | JSON contendo `ip_sta`, `ip_ap`, `ssid` e status |
| `/scanwifi` | `POST` | Dispara varredura assíncrona das redes locais | JSON `{"status":"scanning_started"}` |
| `/scanstatus`| `GET` | Retorna as redes encontradas no ar com RSSI | Array JSON contendo SSIDs e sinais (dBm) |
| `/salvarwifi`| `POST` | Grava novas credenciais no arquivo `/wifiredes.txt` | JSON confirmando início da conexão |
| `/excluirwifi`| `GET` | Remove uma rede gravada na Flash (`?ssid=Rede`) | Retorno de confirmação |

---

## 📄 Licença

Este projeto é disponibilizado sob a licença **MIT**. Consulte o arquivo [LICENSE](LICENSE) para mais informações.

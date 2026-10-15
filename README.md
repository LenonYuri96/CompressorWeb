# EnergizAR - Sistema de Automação e Monitoramento de Compressores via ESP32

O **EnergizAR** é uma solução de automação e telemetria industrial desenvolvida em torno do microcontrolador **ESP32**. O sistema realiza o acionamento inteligente de compressores de ar comprimido, motores de ventilação e secadores por refrigeração através de pulsos elétricos não-bloqueantes de 500 ms, com monitoramento térmico contínuo, supervisão de pressão por pressostato mecânico, horímetro digital acumulador e proteção anti-curto-ciclo com persistência em memória Flash (SPIFFS).

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

## ⚡ Mapeamento Geral de Portas e Pinagem (Pinout Definitivo)

> **NOTA TÉCNICA DE ENGENHARIA:**  
> O mapeamento de GPIOs foi completamente redistribuído para evitar colisões entre saídas de potência e entradas de proteção. Todas as 11 portas estão isoladas e desempenham funções exclusivas. As entradas operam com resistores de **PULLDOWN**, e o pino **GPIO 12** deve iniciar obrigatoriamente em `LOW` (0V) durante o boot para assegurar a tensão correta de 3.3V na memória Flash do ESP32.

### 📌 Tabela de Atribuição dos GPIOs do ESP32

| Módulo / Função | GPIO | Direção | Modo Elétrico | Nível Ativo | Descrição da Ligação Física |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Pulso Motor 1 (Compressor)** | **GPIO 25** | Saída | Digital OUTPUT | `HIGH` (3.3V) | Bobina do relé/contator do compressor principal (Pulso 500 ms) |
| **Pulso Motor 2 (Ventilador)** | **GPIO 27** | Saída | Digital OUTPUT | `HIGH` (3.3V) | Bobina do relé/contator do ventilador/exaustor (Pulso 500 ms) |
| **Pulso Motor 3 (Secador)**    | **GPIO 26** | Saída | Digital OUTPUT | `HIGH` (3.3V) | Bobina do relé/contator do secador de ar (Pulso 500 ms) |
| **Feedback Contator 1**        | **GPIO 14** | Entrada | `INPUT_PULLDOWN` | `HIGH` (3.3V) | Contato auxiliar NA (13-14) do contator do compressor |
| **Feedback Contator 2**        | **GPIO 33** | Entrada | `INPUT_PULLDOWN` | `HIGH` (3.3V) | Contato auxiliar NA (13-14) do contator do ventilador |
| **Feedback Contator 3**        | **GPIO 32** | Entrada | `INPUT_PULLDOWN` | `HIGH` (3.3V) | Contato auxiliar NA (13-14) do contator do secador |
| **Sobrecarga Motor 1**         | **GPIO 19** | Entrada | `INPUT_PULLDOWN` | `HIGH` (3.3V) | Contato de alarme NA (97-98) do relé térmico do compressor |
| **Sobrecarga Motor 2**         | **GPIO 12** | Entrada | `INPUT_PULLDOWN` | `HIGH` (3.3V) | Contato de alarme NA (97-98) do relé térmico do ventilador |
| **Sobrecarga Motor 3**         | **GPIO 18** | Entrada | `INPUT_PULLDOWN` | `HIGH` (3.3V) | Contato de alarme NA (97-98) do relé térmico do secador |
| **Pressostato de Linha**       | **GPIO 17** | Entrada | `INPUT_PULLDOWN` | `HIGH` (3.3V) | Sensor de pressão pneumático de segurança e alívio |
| **Chave de Manutenção**        | **GPIO 04** | Entrada | `INPUT_PULLDOWN` | `HIGH` (3.3V) | Chave física comutadora local de manutenção no painel |
| **Serial Debug (TX0 / RX0)**   | **GPIO 01 / 03**| UART | N/A | N/A | Comunicação e depuração serial a 115200 bps |

---

## 🛡️ Regras de Proteção e Temporizadores Industriais

Para evitar a queima precoce dos enrolamentos dos compressores provocada por chaveamentos contínuos no painel ou por tentativas de partida com contrapressão no cabeçote, o sistema implementa travas de tempo diretamente no hardware:

1. **Tempo Mínimo em Marcha (60 Minutos — 3600 segundos):**
   * Ao emitir a ordem de **LIGAR**, o motor liga imediatamente via pulso de 500 ms e entra em bloqueio de desligamento manual por 60 minutos.
   * Evita desligar o compressor antes que o circuito atinja a temperatura de trabalho e o ciclo de lubrificação adequados.
   * *Exceção:* Falhas elétricas graves (sobrecarga térmica ou pressostato) desarmam o motor no mesmo milissegundo, sobrepondo o timer.

2. **Tempo de Descanso Obrigatório (10 Minutos — 600 segundos):**
   * Ao emitir a ordem de **DESLIGAR**, o motor desliga via pulso de 500 ms e o botão fica bloqueado por 10 minutos com contagem regressiva exibida na tela.
   * Impede a partida com pressão residual acumulada nos pistões, protegendo contra picos destrutivos de corrente de rotor bloqueado.

3. **Sincronização Ativa Hardware-para-Browser:**
   * O saldo dos temporizadores é gerenciado pelo ESP32 e enviado via JSON (`/motor-state`).
   * A contagem regressiva (**`HH:MM:SS`** ou **`MM:SS`**) é real: se a página for recarregada ou aberta em múltiplos dispositivos, o tempo exibido é exatamente o do microcontrolador.

4. **Cache em RAM e Wear-Leveling da Memória Flash:**
   * Estados de relés e temporizadores são espelhados em RAM.
   * Escritas repetitivas de valores idênticos ao já armazenado são descartadas, protegendo os setores da memória Flash SPIFFS contra desgaste prematuro.

---

## 📡 Topologia de Rede e Conectividade Wi-Fi

O ESP32 opera em modo híbrido concorrente (**`WIFI_AP_STA`**), garantindo acesso local de emergência mesmo se o roteador externo falhar:

```
                  +-----------------------------------+
                  |        ESP32 (WIFI_AP_STA)        |
                  +-----------------+-----------------+
                                    |
            +-----------------------+-----------------------+
            |                                               |
            v                                               v
   [ Interface SoftAP ]                            [ Interface Station ]
   SSID: EnergizAR                                 Conectado ao Roteador Wi-Fi
   Senha: 12345678                                 IP dinâmico via DHCP (Ex: 192.168.1.105)
   IP Fixo: 192.168.26.7                           Acesso liberado pela intranet
```

### Configurações de Fábrica do Ponto de Acesso (AP)

```cpp
const char *ap_ssid     = "EnergizAR";
const char *ap_password = "12345678";

IPAddress local_ip(192, 168, 26, 7);
IPAddress gateway(192, 168, 26, 1);
IPAddress subnet(255, 255, 255, 0);
```

### Gerenciador de Redes no Ar
* **Varredura Assíncrona:** Varre redes Wi-Fi 2.4 GHz em segundo plano sem travar os motores nem o Task Watchdog.
* **Preenchimento por Clique:** Ao tocar no nome da rede na lista, o SSID é preenchido automaticamente no formulário.
* **Painel Dinâmico de Endereços IP:** Exibe em tempo real o IP obtido no roteador externo com link direto e mantém o IP do modo AP visível.

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
│   ├── ligadesliga.cpp         # Lógica soberana de acionamento por pulso
│   ├── main.cpp                # Orquestrador do firmware e loop cooperativo
│   ├── manutencao.cpp          # Debounce e bloqueio por chave de manutenção
│   ├── paginaserro.cpp         # Respostas de exceção HTTP
│   ├── pressao.cpp             # Interface de pressão e pressostato
│   ├── pressostato.cpp         # Leitura de alívio pneumático
│   ├── sobrecarga.cpp          # Armação segura e desarme por sobrecarga
│   ├── tempo.cpp               # Cliente SNTP para horário fabril (08h às 22h)
│   ├── umidade.cpp             # Interface de medição de saturação
│   ├── wificonexao.cpp         # Máquina de estados não-bloqueante Wi-Fi
│   ├── wifigerenciador.cpp     # Endpoints de varredura, persistência e IP
│   └── wifiinterface.cpp       # HTML/CSS/JS do gerenciador de Wi-Fi
├── platformio.ini              # Configurações de build e upload do microcontrolador
└── README.md                   # Documentação técnica do projeto
```

---

## 🔌 Diagrama Elétrico e Conexões

### Esquemático do Acionamento e Proteções Atualizado
```
                   ESP32 DEVKIT V1
              +-----------------------+
              |                       |
(Pulso 500ms) | GPIO 25 ------> [IN1] | Módulo Relé 1 (Compressor)
(Pulso 500ms) | GPIO 27 ------> [IN2] | Módulo Relé 2 (Ventilador)
(Pulso 500ms) | GPIO 26 ------> [IN3] | Módulo Relé 3 (Secador)
              |                       |
(Feedback NA) | GPIO 14 <------ [14]  | Contato Auxiliar Contator 1 (3.3V)
(Feedback NA) | GPIO 33 <------ [14]  | Contato Auxiliar Contator 2 (3.3V)
(Feedback NA) | GPIO 32 <------ [14]  | Contato Auxiliar Contator 3 (3.3V)
              |                       |
(Sobrecarga)  | GPIO 19 <------ [98]  | Contato Falha Relé Térmico 1 (3.3V)
(Sobrecarga)  | GPIO 12 <------ [98]  | Contato Falha Relé Térmico 2 (3.3V)*
(Sobrecarga)  | GPIO 18 <------ [98]  | Contato Falha Relé Térmico 3 (3.3V)
              |                       |
(Pressostato) | GPIO 17 <------ [P1]  | Pressostato de Linha Pneumática
(Manutenção)  | GPIO 04 <------ [S1]  | Chave Seletora de Painel
              |                       |
              | GND ----------> GND   | Barramento Comum de Terra
              | 3V3 ----------> +3.3V | Barramento de Alimentação de Sinal
              +-----------------------+
* Nota: GPIO 12 deve iniciar em 0V (LOW) durante a energização da placa.
```

---

## 🚀 Como Compilar e Gravar

### Pré-requisitos
* [Visual Studio Code](https://code.visualstudio.com/)
* Extensão [PlatformIO IDE](https://platformio.org/)
* Cabo Micro-USB de boa qualidade (com suporte a transferência de dados)

### Arquivo de Configuração (`platformio.ini`)
Recomenda-se utilizar a velocidade de upload de **115200** ou **460800** bps para estabilidade na gravação da Flash:

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

### Comandos de Terminal (PlatformIO CLI)
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
| `/logout` | `GET` | Invalida a sessão ativa | Redirecionamento para a tela de login |
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

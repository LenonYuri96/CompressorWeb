/**
 * ============================================================================
 * PROJETO: EnergizAR - Sistema de Automacao e Monitoramento ESP32
 * ARQUIVO: src/dashboard.cpp
 * ----------------------------------------------------------------------------
 * DESCRICAO DO SUBSISTEMA:
 *   Interface grafica operacional (Dashboard Web) e servicos REST assincronos
 *   destinados ao monitoramento e controle dos compressores industriais.
 *
 * ARQUITETURA DE SINCRONISMO DE TEMPO (HARDWARE-TO-BROWSER):
 *   Para erradicar qualquer risco de queima de motores por curto-ciclo e
 *   eliminar descompassos entre o navegador e o microcontrolador:
 *
 *   1. BASE DE TEMPO SOBERANA NO ESP32:
 *      O saldo do temporizador anti-curto-ciclo e calculado diretamente na
 *      memoria do ESP32 pela funcao getMotorRemainingTime(). Os segundos
 *      reais sao despachados via JSON no endpoint /motor-state.
 *
 *   2. REGRAS DE TEMPORIZACAO INDUSTRIAL:
 *      - Pos-Partida: 60 MINUTOS (3600 segundos) de funcionamento minimo
 *        obrigatorio para garantir regime termico estavel nos bobinamentos.
 *      - Pos-Parada: 10 MINUTOS (600 segundos) de descanso obrigatorio
 *        para permitir despressurizacao do cabecote antes de novo arranque.
 *
 *   3. ELIMINACAO DE TRAVAS DE FRONT-END FALSAS:
 *      A verificacao de horario fabril foi centralizada no backend C++,
 *      impedindo que relogios descalibrados de celulares ou computadores
 *      bloqueiem indevidamente a operacao da planta.
 *
 *   4. RESILIENCIA A RECARGA DE PAGINA (F5):
 *      Mesmo que a pagina seja recarregada ou aberta simultaneamente em
 *      multiplos dispositivos, todos exibem a mesma contagem regressiva
 *      sincronizada no segundo exato com o microcontrolador.
 * ============================================================================
 */

// ============================================================================
// SECAO 1: BIBLIOTECAS, CABECALHOS E INTERFACES EXTERNAS
// ============================================================================

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SPIFFS.h>

#include "dashboard.h"
#include "ligadesliga.h"
#include "autenticador.h"
#include "manutencao.h"
#include "sobrecarga.h"
#include "pressostato.h"

// ============================================================================
// SECAO 2: REGISTRO DE ROTAS E CONTROLADORES HTTP DO DASHBOARD
// ============================================================================

/**
 * setupDashboardPage()
 * ----------------------------------------------------------------------------
 * Configura o servidor assincrono para servir a pagina web do Dashboard e o
 * endpoint de telemetria /motor-state com atualizacao em tempo real.
 *
 * @param server Instancia central de referencia do ESPAsyncWebServer.
 */
void setupDashboardPage(AsyncWebServer &server)
{
    // ========================================================================
    // ROTA 1: Pagina Grafica Principal do Dashboard (HTML5 / CSS3 / ES6)
    // ========================================================================
    server.on("/dashboard", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // Valida sessao autenticada via cookie criptografico
        if (!isAuthenticated(request))
        {
            redirectToAccessDenied(request);
            return;
        }

        // String literal em Flash contendo a aplicacao Web completa
        String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>Painel de Controle | EnergizAR</title>
    <style>
        /* -------------------------------------------------------------
           ESTILOS GERAIS E RESET CSS
           ------------------------------------------------------------- */
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif;
            background-color: #f0f2f5;
            color: #212529;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: space-between;
            transition: background-color 0.3s ease, color 0.3s ease;
        }

        /* -------------------------------------------------------------
           CONTAINER CENTRALIZADO
           ------------------------------------------------------------- */
        .dashboard-container {
            background-color: #ffffff;
            padding: 24px;
            border-radius: 12px;
            box-shadow: 0 4px 16px rgba(0, 0, 0, 0.1);
            text-align: center;
            width: 92%;
            max-width: 440px;
            margin: 20px auto;
            transition: background-color 0.3s ease, box-shadow 0.3s ease;
        }

        .dashboard-title {
            font-size: 22px;
            font-weight: 700;
            margin-bottom: 20px;
            color: #e65100;
            letter-spacing: 0.5px;
        }

        /* -------------------------------------------------------------
           CARDS INDIVIDUAIS DOS MOTORES
           ------------------------------------------------------------- */
        .motor-card {
            background: #fafafa;
            border: 1px solid #e0e0e0;
            border-radius: 10px;
            padding: 16px;
            margin-bottom: 16px;
            box-shadow: 0 2px 4px rgba(0, 0, 0, 0.03);
            transition: border-color 0.2s ease, background 0.2s ease;
        }

        .motor-card p {
            margin-top: 10px;
            font-size: 15px;
            font-weight: 700;
            color: #333333;
        }

        /* -------------------------------------------------------------
           BOTOES CIRCULARES DE ACIONAMENTO
           ------------------------------------------------------------- */
        .round-button {
            width: 96px;
            height: 96px;
            border-radius: 50%;
            display: flex;
            align-items: center;
            justify-content: center;
            text-align: center;
            font-size: 13px;
            font-weight: 700;
            margin: 0 auto;
            cursor: pointer;
            transition: transform 0.15s ease, background-color 0.2s ease, box-shadow 0.2s ease;
            color: #ffffff;
            border: 3px solid rgba(0, 0, 0, 0.15);
            outline: none;
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2);
            text-transform: uppercase;
            letter-spacing: 0.5px;
            user-select: none;
        }

        .round-button:active {
            transform: scale(0.95);
        }

        /* Cores de Estado Operacional */
        .btn-ligar       { background-color: #2e7d32; } /* Verde escuro */
        .btn-desligar    { background-color: #c62828; } /* Vermelho */
        .btn-overload    { background-color: #d32f2f; } /* Falha termica */
        .btn-maintenance { background-color: #ef6c00; } /* Alarme de chave */
        .btn-disabled    { background-color: #757575; } /* Bloqueio / Timer */

        /* Estado Bloqueado por Temporizador de Seguranca */
        .btn-locked {
            cursor: not-allowed !important;
            opacity: 0.72;
            filter: grayscale(30%);
            box-shadow: 0 2px 4px rgba(0, 0, 0, 0.15);
        }

        /* -------------------------------------------------------------
           EXIBICAO DA CONTAGEM REGRESSIVA SINCRONIZADA
           ------------------------------------------------------------- */
        .countdown {
            margin-top: 8px;
            font-size: 13px;
            font-weight: 700;
            color: #c62828;
            min-height: 18px;
            letter-spacing: 0.5px;
            font-variant-numeric: tabular-nums;
        }

        /* -------------------------------------------------------------
           PAINEIS DE MENSAGENS E ALERTAS
           ------------------------------------------------------------- */
        .message-box {
            margin-top: 8px;
            padding: 8px 10px;
            border-radius: 6px;
            font-size: 12px;
            font-weight: 600;
            text-align: center;
            display: none;
        }

        .message-box.active { display: block; }
        .message-box.error   { background: #ffebee; color: #c62828; border: 1px solid #ef9a9a; }
        .message-box.warning { background: #fff3e0; color: #ef6c00; border: 1px solid #ffe0b2; }
        .message-box.info    { background: #e3f2fd; color: #0d47a1; border: 1px solid #bbdefb; }

        /* -------------------------------------------------------------
           BOTOES DE NAVEGACAO E RECURSOS
           ------------------------------------------------------------- */
        .btn-link {
            display: block;
            width: 100%;
            padding: 11px;
            font-size: 14px;
            font-weight: 600;
            margin: 8px auto;
            border-radius: 6px;
            color: #ffffff;
            text-decoration: none;
            text-align: center;
            border: none;
            cursor: pointer;
            transition: opacity 0.2s ease;
        }

        .btn-link:hover { opacity: 0.88; }

        .btn-link-wifi     { background-color: #0288d1; }
        .btn-link-umidade  { background-color: #00897b; }
        .btn-link-pressao  { background-color: #455a64; }
        .btn-link-theme    { background-color: #ff9800; }
        .btn-link-logout   { background-color: #c62828; }

        /* -------------------------------------------------------------
           TEMA NOTURNO INDUSTRIAL (NIGHT MODE)
           ------------------------------------------------------------- */
        body.night-mode {
            background-color: #121212;
            color: #e0e0e0;
        }

        body.night-mode .dashboard-container {
            background-color: #1e1e1e;
            box-shadow: 0 4px 16px rgba(0, 0, 0, 0.5);
        }

        body.night-mode .motor-card {
            background: #2a2a2a;
            border-color: #383838;
        }

        body.night-mode .motor-card p {
            color: #ffffff;
        }

        /* -------------------------------------------------------------
           RODAPE INSTITUCIONAL
           ------------------------------------------------------------- */
        .footer {
            width: 100%;
            background-color: #e65100;
            color: #ffffff;
            text-align: center;
            padding: 10px 0;
            font-size: 12px;
            margin-top: auto;
        }
    </style>
</head>
<body>
    <div class="dashboard-container">
        <h2 class="dashboard-title">Painel de Compressores</h2>

        <!-- Motor 1: Compressor Principal -->
        <div class="motor-card">
            <button class="round-button" id="toggleButtonMotor1" onclick="enviarComando(0)">
                <span>Carregando...</span>
            </button>
            <p>Compressor Principal</p>
            <div id="countdownMotor1" class="countdown"></div>
            <div id="messageBoxMotor1" class="message-box"></div>
        </div>

        <!-- Motor 2: Motor Ventilador -->
        <div class="motor-card">
            <button class="round-button" id="toggleButtonMotor2" onclick="enviarComando(1)">
                <span>Carregando...</span>
            </button>
            <p>Motor Ventilador</p>
            <div id="countdownMotor2" class="countdown"></div>
            <div id="messageBoxMotor2" class="message-box"></div>
        </div>

        <!-- Motor 3: Secador de Ar -->
        <div class="motor-card">
            <button class="round-button" id="toggleButtonMotor3" onclick="enviarComando(2)">
                <span>Carregando...</span>
            </button>
            <p>Motor Secador de Ar</p>
            <div id="countdownMotor3" class="countdown"></div>
            <div id="messageBoxMotor3" class="message-box"></div>
        </div>

        <!-- Links e Acoes Administrativas -->
        <a href="/wifigerenciamento" class="btn-link btn-link-wifi">Configurar Wi-Fi</a>
        <a href="/umidade" class="btn-link btn-link-umidade">Monitor de Umidade</a>
        <a href="/pressao" class="btn-link btn-link-pressao">Monitor de Pressão</a>
        <button type="button" class="btn-link btn-link-theme" onclick="toggleTema()">🌓 Alternar Tema</button>
        <a href="/logout" class="btn-link btn-link-logout">Encerrar Sessão</a>
    </div>

    <div class="footer">
        EnergizAR - Automação Industrial e Monitoramento
    </div>

    <script>
    /**
     * Variaveis de Estado em Memoria (Espelho Fiel do Backend em C++)
     */
    let estadoMotores = [false, false, false];
    let bloqueioAtivo = [false, false, false];
    let segundosRestantes = [0, 0, 0];

    // Colecoes de Elementos do DOM
    const toggleButtons = [
        document.getElementById('toggleButtonMotor1'),
        document.getElementById('toggleButtonMotor2'),
        document.getElementById('toggleButtonMotor3')
    ];
    const countdownElements = [
        document.getElementById('countdownMotor1'),
        document.getElementById('countdownMotor2'),
        document.getElementById('countdownMotor3')
    ];
    const messageBoxes = [
        document.getElementById('messageBoxMotor1'),
        document.getElementById('messageBoxMotor2'),
        document.getElementById('messageBoxMotor3')
    ];

    /**
     * formatarTempo()
     * -------------------------------------------------------------
     * Converte o saldo total de segundos oriundo do ESP32 nos formatos:
     *   - HH:MM:SS para duracoes de 60 minutos (3600 segundos)
     *   - MM:SS para descansos de 10 minutos (600 segundos)
     *
     * @param totalSegundos Segundos inteiros remanescentes no timer.
     * @return String devidamente preenchida com zeros a esquerda.
     */
    function formatarTempo(totalSegundos) {
        if (!totalSegundos || totalSegundos <= 0) return "";
        const h = Math.floor(totalSegundos / 3600);
        const m = Math.floor((totalSegundos % 3600) / 60);
        const s = totalSegundos % 60;
        if (h > 0) {
            return `${String(h).padStart(2,'0')}:${String(m).padStart(2,'0')}:${String(s).padStart(2,'0')}`;
        }
        return `${String(m).padStart(2,'0')}:${String(s).padStart(2,'0')}`;
    }

    /**
     * consultarEstadoESP32()
     * -------------------------------------------------------------
     * Consulta ciclica assincrona que recupera o estado operacional,
     * alarmes industriais e saldos temporais de cada motor.
     */
    function consultarEstadoESP32() {
        fetch('/motor-state')
            .then(res => {
                if (!res.ok) throw new Error("Erro de resposta HTTP");
                return res.json();
            })
            .then(data => {
                const nomes = ["Compressor", "Ventilador", "Secador"];

                for (let i = 0; i < 3; i++) {
                    // Estados logicos recebidos diretamente do hardware
                    estadoMotores[i]  = data[`compressorLigadoMotor${i + 1}`];
                    const manutencao  = data.sistemaEmManutencao;
                    const sobrecarga  = data[`sobrecargaMotor${i + 1}`];
                    const timerAtivo  = data[`motorTimerActive${i + 1}`];
                    const pressostato = (i === 0 && data.pressostatoAtivo);
                    const foraHorario = data.foraDoHorario;

                    // Atualiza os segundos remanescentes reais fornecidos pelo ESP32
                    segundosRestantes[i] = data[`motorRemainingTime${i + 1}`] || 0;

                    let btnTexto = estadoMotores[i] ? "Desligar" : "Ligar";
                    let btnClasse = estadoMotores[i] ? "btn-desligar" : "btn-ligar";
                    let msgTexto = "";
                    let msgTipo = "";
                    let desabilitar = false;

                    // Hierarquia de Seguranca: 1. Falha Termica de Sobrecarga
                    if (sobrecarga) {
                        btnTexto = "Sobrecarga";
                        btnClasse = "btn-overload";
                        msgTexto = `Alarme térmico acionado em ${nomes[i]}!`;
                        msgTipo = "error";
                        desabilitar = true;
                    }
                    // Hierarquia de Seguranca: 2. Chave Fisica de Manutencao
                    else if (manutencao) {
                        btnTexto = "Manutenção";
                        btnClasse = "btn-maintenance";
                        msgTexto = "Sistema bloqueado: Manutenção ativa.";
                        msgTipo = "warning";
                        desabilitar = true;
                    }
                    // Hierarquia de Seguranca: 3. Sensor de Pressostato de Linha
                    else if (pressostato) {
                        btnTexto = "Pressão";
                        btnClasse = "btn-overload";
                        msgTexto = "Alarme de pressão crítica ativo!";
                        msgTipo = "error";
                        desabilitar = true;
                    }
                    // Hierarquia de Seguranca: 4. Violacao de Janela Operacional
                    else if (foraHorario) {
                        btnTexto = "Horário";
                        btnClasse = "btn-disabled";
                        msgTexto = "Fora do horário fabril (08:00 às 22:00).";
                        msgTipo = "warning";
                        desabilitar = true;
                    }
                    // Hierarquia de Seguranca: 5. Temporizador Anti-Curto-Ciclo Ativo
                    else if (timerAtivo && segundosRestantes[i] > 0) {
                        desabilitar = true;
                        // Define rotulo conforme o motor esteja em marcha ou repouso
                        const motivo = estadoMotores[i] ? "Mínimo ligado" : "Descanso";
                        countdownElements[i].innerText = `${motivo}: ${formatarTempo(segundosRestantes[i])}`;
                    }
                    else {
                        countdownElements[i].innerText = "";
                    }

                    // Aplica estados e classes visuais atualizadas
                    bloqueioAtivo[i] = desabilitar;
                    toggleButtons[i].className = `round-button ${btnClasse} ${desabilitar ? 'btn-locked' : ''}`;
                    toggleButtons[i].disabled = desabilitar;
                    toggleButtons[i].innerHTML = `<span>${btnTexto}</span>`;

                    // Renderiza mensagens informativas
                    if (msgTexto !== "") {
                        messageBoxes[i].className = `message-box active ${msgTipo}`;
                        messageBoxes[i].innerText = msgTexto;
                    } else {
                        messageBoxes[i].className = "message-box";
                    }
                }
            })
            .catch(err => {
                console.error("[TELEMETRIA] Erro ao sincronizar com ESP32:", err);
            });
    }

    /**
     * enviarComando()
     * -------------------------------------------------------------
     * Envia ordem de alternancia (Toggle) via chamada assincrona ao
     * endpoint /toggle do servidor ESP32.
     *
     * @param motorIdx Indice do motor selecionado (0 a 2).
     */
    function enviarComando(motorIdx) {
        // Bloqueia tentativas caso o motor esteja travado por timer ou alarme
        if (bloqueioAtivo[motorIdx]) {
            return;
        }

        // Se o motor estiver ligado, a acao solicitada e desligar; senao, ligar
        const acao = estadoMotores[motorIdx] ? "desligar" : "ligar";

        // Trava visual imediata para prevenir duplo clique acidental
        bloqueioAtivo[motorIdx] = true;
        toggleButtons[motorIdx].classList.add('btn-locked');

        // Despacha a requisicao HTTP com parâmetros de query string
        fetch(`/toggle?action=${acao}&motor=${motorIdx + 1}`)
            .then(res => res.text())
            .then(resposta => {
                // Consulta o estado oficial atualizado no ESP32 imediatamente
                consultarEstadoESP32();
            })
            .catch(err => {
                console.error("[COMANDO] Falha no envio da ordem:", err);
                bloqueioAtivo[motorIdx] = false;
                toggleButtons[motorIdx].classList.remove('btn-locked');
            });
    }

    /**
     * Inicializacao do Ciclo de Vida da Pagina
     */
    document.addEventListener('DOMContentLoaded', () => {
        // Primeira sincronizacao com o hardware
        consultarEstadoESP32();

        // Polling de sincronizacao continua a cada 1000 milissegundos
        setInterval(consultarEstadoESP32, 1000);

        // Decremento local a cada 1 segundo para garantir fluidez visual
        setInterval(() => {
            for (let i = 0; i < 3; i++) {
                if (bloqueioAtivo[i] && segundosRestantes[i] > 0) {
                    segundosRestantes[i]--;
                    const motivo = estadoMotores[i] ? "Mínimo ligado" : "Descanso";
                    countdownElements[i].innerText = `${motivo}: ${formatarTempo(segundosRestantes[i])}`;
                }
            }
        }, 1000);

        // Restaura preferencia de tema escuro memorizada
        if (localStorage.getItem('energizar_theme') === 'night') {
            document.body.classList.add('night-mode');
        }
    });

    /**
     * toggleTema()
     * -------------------------------------------------------------
     * Alterna a classe de modo escuro e salva no armazenamento local.
     */
    function toggleTema() {
        document.body.classList.toggle('night-mode');
        const modoEscuro = document.body.classList.contains('night-mode');
        localStorage.setItem('energizar_theme', modoEscuro ? 'night' : 'light');
    }
    </script>
</body>
</html>
        )rawliteral";

        // Despacha o documento HTML consolidado para o navegador
        request->send(200, "text/html", html); });

    // ========================================================================
    // ROTA 2: Endpoint REST /motor-state (Telemetria e Segundos em Tempo Real)
    // ========================================================================
    server.on("/motor-state", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // Avaliacao de hardware: leitura do pino fisico do pressostato
        bool pressostato = (digitalRead(17) == HIGH);

        // Avaliacao de integridade da janela operacional de horario
        bool foraDeHorario = (isAfterClosingTime() || isBeforeOpeningTime());

        // Serializacao JSON estruturada contendo todos os dados operacionais
        String json = "{";

        // 1. Estados soberanos de intencao operacional em memoria RAM
        json += "\"compressorLigadoMotor1\":" + String(motoresLigados[0] ? "true" : "false") + ",";
        json += "\"compressorLigadoMotor2\":" + String(motoresLigados[1] ? "true" : "false") + ",";
        json += "\"compressorLigadoMotor3\":" + String(motoresLigados[2] ? "true" : "false") + ",";

        // 2. Sensores globais de protecao e chaves fisicas
        json += "\"sistemaEmManutencao\":" + String(sistemaEmManutencao ? "true" : "false") + ",";
        json += "\"pressostatoAtivo\":" + String(pressostato ? "true" : "false") + ",";
        json += "\"foraDoHorario\":" + String(foraDeHorario ? "true" : "false") + ",";

        // 3. Alarmes individuais de sobrecarga termica bimetalica
        json += "\"sobrecargaMotor1\":" + String(sobrecargaDetectada[0] ? "true" : "false") + ",";
        json += "\"sobrecargaMotor2\":" + String(sobrecargaDetectada[1] ? "true" : "false") + ",";
        json += "\"sobrecargaMotor3\":" + String(sobrecargaDetectada[2] ? "true" : "false") + ",";

        // 4. Sinalizadores booleanos de vigencia do temporizador de descanso
        json += "\"motorTimerActive1\":" + String(isMotorTimerActive(0) ? "true" : "false") + ",";
        json += "\"motorTimerActive2\":" + String(isMotorTimerActive(1) ? "true" : "false") + ",";
        json += "\"motorTimerActive3\":" + String(isMotorTimerActive(2) ? "true" : "false") + ",";

        // 5. Saldo EXATO de segundos restantes calculado diretamente pelo ESP32
        json += "\"motorRemainingTime1\":" + String(getMotorRemainingTime(0)) + ",";
        json += "\"motorRemainingTime2\":" + String(getMotorRemainingTime(1)) + ",";
        json += "\"motorRemainingTime3\":" + String(getMotorRemainingTime(2)) + "}";

        // Envia resposta em formato application/json com codigo 200 (OK)
        request->send(200, "application/json", json); });
}

// ============================================================================
// FIM DO ARQUIVO: src/dashboard.cpp
// SISTEMA ENERGIZAR - DASHBOARD INDUSTRIAL COM SINCRONISMO DE BASE DE TEMPO
// ============================================================================
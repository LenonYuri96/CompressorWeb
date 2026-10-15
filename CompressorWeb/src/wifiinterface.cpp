/**
 * ============================================================================
 * PROJETO: EnergizAR - Sistema de Automação e Monitoramento ESP32
 * ARQUIVO: src/wifiinterface.cpp
 * DESCRIÇÃO: Gera a página HTML/CSS/JavaScript responsiva para configuração e
 *            gerenciamento das redes Wi-Fi. Exibe o IP da rede externa (STA)
 *            e do modo AP em tempo real, varredura no ar com seleção por clique,
 *            persistência no SPIFFS e modos de alto contraste e noturno.
 * ============================================================================
 */

#include "wifiinterface.h"

// -------------------------------------------------------------------------
// Função: getWiFiManagementPage
// Retorno: Ponteiro para string literal em Flash (PROGMEM) contendo todo o HTML.
// Finalidade: Servir o portal de rede sem alocar memória RAM extra.
// -------------------------------------------------------------------------
const char *getWiFiManagementPage()
{
    return R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Gerenciamento Wi-Fi | EnergizAR</title>
    
    <style>
        /* -----------------------------------------------------------------
           1. ESTILOS GERAIS E RESET CSS
           ----------------------------------------------------------------- */
        * { 
            box-sizing: border-box; 
            margin: 0; 
            padding: 0; 
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            background-color: #f0f2f5;
            color: #212529;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: space-between;
        }

        /* -----------------------------------------------------------------
           2. TEMAS DE ACESSIBILIDADE (MODO NOTURNO E ALTO CONTRASTE)
           ----------------------------------------------------------------- */
        body.night-mode { 
            background-color: #121212; 
            color: #e0e0e0; 
        }

        body.high-contrast { 
            background-color: #000000; 
            color: #ffffff; 
        }

        .container {
            background-color: #ffffff;
            width: 100%;
            max-width: 440px;
            padding: 24px;
            margin: 20px auto;
            border-radius: 12px;
            box-shadow: 0 4px 16px rgba(0,0,0,0.1);
        }

        body.night-mode .container { 
            background-color: #1e1e1e; 
            box-shadow: 0 4px 16px rgba(0,0,0,0.4); 
        }

        body.high-contrast .container { 
            background-color: #000000; 
            border: 2px solid #ffffff; 
        }

        .title {
            font-size: 22px;
            font-weight: 700;
            text-align: center;
            margin-bottom: 12px;
            color: #e65100;
        }

        /* -----------------------------------------------------------------
           PAINEL DE ENDEREÇAMENTO IP (REDE EXTERNA E AP)
           ----------------------------------------------------------------- */
        .ip-panel {
            background: #fafafa;
            border: 1px solid #e0e0e0;
            border-radius: 8px;
            padding: 10px 14px;
            margin-bottom: 16px;
            text-align: left;
            font-size: 13px;
        }

        body.night-mode .ip-panel {
            background: #262626;
            border-color: #404040;
        }

        .ip-row {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 4px 0;
        }

        .ip-row strong {
            color: #555;
        }

        body.night-mode .ip-row strong {
            color: #bbb;
        }

        .ip-value {
            font-weight: 700;
            font-variant-numeric: tabular-nums;
        }

        .ip-sta-online {
            color: #2e7d32;
        }

        .ip-sta-offline {
            color: #d32f2f;
            font-weight: 600;
        }

        .ip-ap-val {
            color: #0288d1;
        }

        .section-title {
            font-size: 14px;
            font-weight: 700;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            margin: 16px 0 8px 0;
            color: #616161;
        }

        body.night-mode .section-title { 
            color: #aaaaaa; 
        }

        /* -----------------------------------------------------------------
           3. LISTA DE ESCANEAMENTO DE REDES NO AR
           ----------------------------------------------------------------- */
        .scan-box {
            border: 1px solid #ddd;
            border-radius: 8px;
            max-height: 180px;
            overflow-y: auto;
            background: #fafafa;
            margin-bottom: 15px;
        }

        body.night-mode .scan-box { 
            background: #2a2a2a; 
            border-color: #444444; 
        }

        .network-item {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 10px 14px;
            border-bottom: 1px solid #eee;
            cursor: pointer;
            transition: background-color 0.2s;
        }

        body.night-mode .network-item { 
            border-color: #383838; 
        }

        .network-item:hover { 
            background-color: #ffe0b2; 
        }

        body.night-mode .network-item:hover { 
            background-color: #424242; 
        }

        .network-name {
            font-size: 14px;
            font-weight: 600;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
            max-width: 250px;
        }

        .network-signal {
            font-size: 12px;
            color: #757575;
            font-weight: 500;
        }

        .empty-hint {
            padding: 16px;
            text-align: center;
            font-size: 13px;
            color: #888888;
        }

        /* -----------------------------------------------------------------
           4. CAMPOS DO FORMULÁRIO DE ENTRADA
           ----------------------------------------------------------------- */
        .form-group {
            margin-bottom: 12px;
            text-align: left;
        }

        .form-group label {
            display: block;
            font-size: 13px;
            font-weight: 600;
            margin-bottom: 4px;
        }

        .form-group input {
            width: 100%;
            padding: 10px 12px;
            font-size: 14px;
            border: 1px solid #ccc;
            border-radius: 6px;
            background: #fff;
            color: inherit;
        }

        body.night-mode .form-group input { 
            background: #2d2d2d; 
            border-color: #555555; 
        }

        .pwd-group {
            position: relative;
            display: flex;
        }

        .pwd-group input { 
            padding-right: 70px; 
        }

        .pwd-btn {
            position: absolute;
            right: 4px;
            top: 4px;
            bottom: 4px;
            padding: 0 10px;
            background: #eee;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 12px;
            color: #333;
        }

        body.night-mode .pwd-btn { 
            background: #444444; 
            color: #eeeeee; 
        }

        /* -----------------------------------------------------------------
           5. BOTÕES DE AÇÃO DO SISTEMA
           ----------------------------------------------------------------- */
        .btn {
            width: 100%;
            padding: 11px;
            font-size: 14px;
            font-weight: 600;
            border: none;
            border-radius: 6px;
            cursor: pointer;
            margin-top: 8px;
            transition: opacity 0.2s;
        }

        .btn:active { 
            opacity: 0.8; 
        }

        .btn-scan    { background-color: #0288d1; color: #fff; }
        .btn-connect { background-color: #2e7d32; color: #fff; }
        .btn-back    { background-color: #607d8b; color: #fff; }
        .btn-theme   { background-color: #ff9800; color: #fff; }

        .alert {
            padding: 10px;
            border-radius: 6px;
            font-size: 13px;
            text-align: center;
            margin-top: 10px;
            display: none;
        }

        .alert-success { background: #d4edda; color: #155724; display: block; }
        .alert-error   { background: #f8d7da; color: #721c24; display: block; }
        .alert-info    { background: #e3f2fd; color: #0d47a1; display: block; }

        #saved-list {
            margin-top: 10px;
            font-size: 13px;
        }

        .saved-row {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 8px 0;
            border-bottom: 1px dotted #ccc;
        }

        .del-link {
            color: #d32f2f;
            text-decoration: none;
            font-weight: 600;
            font-size: 12px;
            padding: 4px 8px;
            border: 1px solid #d32f2f;
            border-radius: 4px;
        }

        .footer {
            width: 100%;
            background: #e65100;
            color: #fff;
            text-align: center;
            font-size: 12px;
            padding: 10px 0;
        }
    </style>
</head>
<body>
    <div class="container">
        <h2 class="title">Gerenciador Wi-Fi</h2>

        <!-- PAINEL DE ENDEREÇAMENTO EM TEMPO REAL -->
        <div class="ip-panel">
            <div class="ip-row">
                <strong>Wi-Fi da Empresa/Oficina:</strong>
                <span id="sta-ip" class="ip-value ip-sta-offline">Desconectado</span>
            </div>
            <div class="ip-row" id="sta-ssid-row" style="display:none;">
                <strong>Rede Conectada:</strong>
                <span id="sta-ssid" class="ip-value" style="color:#2e7d32;">-</span>
            </div>
            <div class="ip-row">
                <strong>Ponto de Acesso (AP):</strong>
                <span id="ap-ip" class="ip-value ip-ap-val">192.168.26.7</span>
            </div>
        </div>

        <!-- SEÇÃO: Varredura de redes no ar -->
        <div class="section-title">Redes Próximas Detectadas</div>
        <button type="button" id="btn-scan" class="btn btn-scan" onclick="iniciarVarredura()">🔍 Escanear Redes Wi-Fi</button>
        <div id="scan-box" class="scan-box" style="margin-top: 8px;">
            <div id="scan-hint" class="empty-hint">Clique em "Escanear Redes Wi-Fi" para buscar.</div>
        </div>

        <!-- FORMULÁRIO: Conexão e armazenamento das credenciais -->
        <form id="form-wifi" onsubmit="salvarConectar(event)">
            <div class="form-group">
                <label for="ssid">Nome da Rede (SSID):</label>
                <input type="text" id="ssid" name="ssid" placeholder="Selecione acima ou digite" required>
            </div>
            <div class="form-group">
                <label for="password">Senha:</label>
                <div class="pwd-group">
                    <input type="password" id="password" name="password" placeholder="Digite a senha da rede">
                    <button type="button" class="pwd-btn" onclick="toggleSenha()">Ver</button>
                </div>
            </div>
            <button type="submit" id="btn-submit" class="btn btn-connect">💾 Salvar e Conectar</button>
        </form>

        <!-- Mensagens de confirmação ou erro de conexão -->
        <div id="msg" class="alert"></div>

        <!-- SEÇÃO: Redes salvas na memória interna SPIFFS -->
        <div class="section-title">Redes Salvas na Memória</div>
        <div id="saved-list">Carregando redes salvas...</div>

        <!-- Botões de navegação e acessibilidade -->
        <button type="button" onclick="window.history.back()" class="btn btn-back">⬅ Voltar ao Painel</button>
        <button type="button" onclick="toggleTema()" class="btn btn-theme">🌓 Alternar Tema</button>
    </div>

    <div class="footer">
        EnergizAR - Automação Industrial e Monitoramento
    </div>

    <script>
        document.addEventListener('DOMContentLoaded', () => {
            atualizarIP();
            listarRedesSalvas();
            // Polling continuo a cada 3 segundos para detectar quando o roteador atribuir o IP
            setInterval(atualizarIP, 3000);
        });

        /**
         * Consulta o IP atual (STA e AP) diretamente do ESP32
         */
        function atualizarIP() {
            fetch('/ipdarede')
                .then(r => r.json())
                .then(d => {
                    const staIpEl = document.getElementById('sta-ip');
                    const staSsidRow = document.getElementById('sta-ssid-row');
                    const staSsidEl = document.getElementById('sta-ssid');
                    const apIpEl = document.getElementById('ap-ip');

                    // Atualiza IP do Modo AP
                    if (d.ip_ap) {
                        apIpEl.innerText = d.ip_ap;
                    }

                    // Determina se a Estação (STA) obteve IP na rede externa
                    const ipExterno = d.ip_sta || (d.ip !== "192.168.26.7" && d.ip !== "0.0.0.0" ? d.ip : "");
                    const conectado = (d.conectado === true) || (ipExterno && ipExterno.length > 7);

                    if (conectado && ipExterno) {
                        staIpEl.className = "ip-value ip-sta-online";
                        staIpEl.innerHTML = `<a href="http://${ipExterno}" target="_blank" style="color:#2e7d32; text-decoration:underline;">${ipExterno}</a>`;
                        
                        if (d.ssid && d.ssid.length > 0) {
                            staSsidRow.style.display = "flex";
                            staSsidEl.innerText = d.ssid;
                        }
                    } else {
                        staIpEl.className = "ip-value ip-sta-offline";
                        staIpEl.innerText = "Desconectado";
                        staSsidRow.style.display = "none";
                    }
                })
                .catch(() => {});
        }

        /**
         * Dispara a varredura assíncrona no rádio do ESP32
         */
        function iniciarVarredura() {
            const btn = document.getElementById('btn-scan');
            const box = document.getElementById('scan-box');
            btn.disabled = true;
            btn.innerText = 'Buscando redes... aguarde';
            box.innerHTML = '<div class="empty-hint">Varrendo canais de rádio 2.4GHz...</div>';

            fetch('/scanwifi', { method: 'POST' })
                .then(() => {
                    setTimeout(consultarResultadoScan, 3500);
                })
                .catch(() => {
                    btn.disabled = false;
                    btn.innerText = '🔍 Escanear Redes Wi-Fi';
                    box.innerHTML = '<div class="empty-hint">Falha ao iniciar varredura.</div>';
                });
        }

        /**
         * Obtém o resultado da varredura
         */
        function consultarResultadoScan() {
            const btn = document.getElementById('btn-scan');
            const box = document.getElementById('scan-box');

            fetch('/scanstatus')
                .then(r => r.json())
                .then(redes => {
                    btn.disabled = false;
                    btn.innerText = '🔍 Escanear Redes Wi-Fi';
                    box.innerHTML = '';

                    if (!redes || redes.length === 0) {
                        box.innerHTML = '<div class="empty-hint">Nenhuma rede encontrada. Tente novamente.</div>';
                        return;
                    }

                    redes.forEach(rede => {
                        const div = document.createElement('div');
                        div.className = 'network-item';
                        div.onclick = () => selecionarRede(rede.ssid);
                        div.innerHTML = `
                            <span class="network-name">📶 ${rede.ssid}</span>
                            <span class="network-signal">${rede.rssi} dBm</span>
                        `;
                        box.appendChild(div);
                    });
                })
                .catch(() => {
                    btn.disabled = false;
                    btn.innerText = '🔍 Escanear Redes Wi-Fi';
                    box.innerHTML = '<div class="empty-hint">Erro ao obter lista de redes.</div>';
                });
        }

        function selecionarRede(nomeSSID) {
            document.getElementById('ssid').value = nomeSSID;
            const pwd = document.getElementById('password');
            pwd.value = '';
            pwd.focus();
        }

        function toggleSenha() {
            const f = document.getElementById('password');
            f.type = (f.type === 'password') ? 'text' : 'password';
        }

        /**
         * Salva as credenciais e acompanha a conexão em tempo real
         */
        function salvarConectar(e) {
            e.preventDefault();
            const msg = document.getElementById('msg');
            const btnSubmit = document.getElementById('btn-submit');
            
            btnSubmit.disabled = true;
            msg.className = 'alert alert-info';
            msg.innerText = 'Salvando e conectando à rede externa... Aguarde o IP.';

            const form = document.getElementById('form-wifi');
            const dados = new FormData(form);

            fetch('/salvarwifi', { method: 'POST', body: dados })
                .then(r => {
                    if (!r.ok) throw new Error();
                    form.reset();
                    listarRedesSalvas();

                    // Realiza polling rápido para detectar o momento que o IP é gerado pelo roteador
                    let tentativas = 0;
                    const checarConexao = setInterval(() => {
                        tentativas++;
                        fetch('/ipdarede')
                            .then(res => res.json())
                            .then(d => {
                                const ipExterno = d.ip_sta || (d.ip !== "192.168.26.7" && d.ip !== "0.0.0.0" ? d.ip : "");
                                if (d.conectado || (ipExterno && ipExterno.length > 7)) {
                                    clearInterval(checarConexao);
                                    btnSubmit.disabled = false;
                                    msg.className = 'alert alert-success';
                                    msg.innerHTML = `Conectado com sucesso!<br>IP na rede externa: <a href="http://${ipExterno}" target="_blank" style="font-weight:bold; color:#155724;">http://${ipExterno}</a>`;
                                    atualizarIP();
                                } else if (tentativas >= 12) { // 12 x 1.5s = 18 segundos
                                    clearInterval(checarConexao);
                                    btnSubmit.disabled = false;
                                    msg.className = 'alert alert-error';
                                    msg.innerText = 'Tempo limite esgotado. Verifique a senha da rede ou o sinal do roteador.';
                                    atualizarIP();
                                }
                            });
                    }, 1500);
                })
                .catch(() => {
                    btnSubmit.disabled = false;
                    msg.className = 'alert alert-error';
                    msg.innerText = 'Falha ao enviar credencial ao ESP32.';
                });
        }

        function listarRedesSalvas() {
            fetch('/listadewifi')
                .then(r => r.text())
                .then(txt => {
                    const box = document.getElementById('saved-list');
                    box.innerHTML = '';
                    const linhas = txt.trim().split('\n');
                    let encontrou = false;

                    linhas.forEach(linha => {
                        linha = linha.trim();
                        if (linha.length > 0) {
                            encontrou = true;
                            const partes = linha.split(',');
                            const ssid = partes[0];
                            box.innerHTML += `
                                <div class="saved-row">
                                    <span><strong>${ssid}</strong></span>
                                    <a class="del-link" href="#" onclick="excluirRede('${encodeURIComponent(ssid)}'); return false;">Excluir</a>
                                </div>
                            `;
                        }
                    });

                    if (!encontrou) {
                        box.innerHTML = '<span style="color:#888;">Nenhuma rede salva no sistema.</span>';
                    }
                });
        }

        function excluirRede(ssid) {
            if (!confirm('Deseja excluir a rede salva?')) return;
            fetch('/excluirwifi?ssid=' + ssid)
                .then(() => {
                    listarRedesSalvas();
                    atualizarIP();
                });
        }

        function toggleTema() {
            document.body.classList.toggle('night-mode');
        }
    </script>
</body>
</html>
    )rawliteral";
}
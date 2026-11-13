/**
 * ============================================================================
 * PROJETO: EnergizAR - Sistema de Automação e Monitoramento ESP32
 * ARQUIVO: src/wifiinterface.cpp
 * DESCRIÇÃO: Gera a página HTML/CSS/JavaScript responsiva para configuração e
 *            gerenciamento das redes Wi-Fi. Exibe o IP da rede externa (STA)
 *            e do modo AP em tempo real, varredura no ar com seleção por clique,
 *            persistência no SPIFFS e tema escuro industrial.
 * ============================================================================
 */

#include "wifiinterface.h"
#include "rodape.h"

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
           1. ESTILOS GERAIS E RESET CSS (IHM INDUSTRIAL)
           ----------------------------------------------------------------- */
        * { 
            box-sizing: border-box; 
            margin: 0; 
            padding: 0; 
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            background-color: #d6dde4;
            background-image: linear-gradient(rgba(30, 39, 46, 0.05) 1px, transparent 1px),
                              linear-gradient(90deg, rgba(30, 39, 46, 0.05) 1px, transparent 1px);
            background-size: 20px 20px;
            color: #1e272e;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: space-between;
            transition: background-color 0.3s ease, color 0.3s ease;
        }

        /* -----------------------------------------------------------------
           2. TEMAS DE ACESSIBILIDADE (MODO NOTURNO E ALTO CONTRASTE)
           ----------------------------------------------------------------- */
        body.night-mode { 
            background-color: #0d1117; 
            background-image: linear-gradient(rgba(255, 255, 255, 0.03) 1px, transparent 1px),
                              linear-gradient(90deg, rgba(255, 255, 255, 0.03) 1px, transparent 1px);
            color: #e6edf3; 
        }

        body.high-contrast { 
            background-color: #000000; 
            color: #ffffff; 
        }

        .container {
            background-color: #eceff1;
            width: 92%;
            max-width: 450px;
            padding: 22px 20px;
            margin: 20px auto;
            border-radius: 8px;
            border: 2px solid #90a4ae;
            border-top: 6px solid #e65100;
            box-shadow: 0 10px 28px rgba(15, 23, 30, 0.22), inset 0 1px 0 #ffffff;
        }

        body.night-mode .container { 
            background-color: #161b22; 
            border-color: #30363d;
            border-top-color: #e65100;
            box-shadow: 0 12px 32px rgba(0, 0, 0, 0.7); 
        }

        body.high-contrast .container { 
            background-color: #000000; 
            border: 2px solid #ffffff; 
        }

        .title {
            font-size: 19px;
            font-weight: 800;
            text-align: center;
            margin-bottom: 14px;
            padding: 10px 12px;
            background-color: #1e272e;
            color: #ff9800;
            border-radius: 5px;
            border-left: 5px solid #e65100;
            border-right: 5px solid #e65100;
            text-transform: uppercase;
            letter-spacing: 1.2px;
            box-shadow: inset 0 2px 4px rgba(0, 0, 0, 0.4);
        }

        body.night-mode .title {
            background-color: #0d1117;
            color: #ffa726;
        }

        /* -----------------------------------------------------------------
           PAINEL DE ENDEREÇAMENTO IP (REDE EXTERNA E AP)
           ----------------------------------------------------------------- */
        .ip-panel {
            background: #f7f9fa;
            border: 1px solid #b0bec5;
            border-left: 5px solid #0277bd;
            border-radius: 6px;
            padding: 10px 14px;
            margin-bottom: 16px;
            text-align: left;
            font-size: 13px;
            box-shadow: 0 2px 5px rgba(0, 0, 0, 0.06);
        }

        body.night-mode .ip-panel {
            background: #1f2630;
            border-color: #30363d;
            border-left-color: #0288d1;
        }

        .ip-row {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 4px 0;
        }

        .ip-row strong {
            color: #455a64;
        }

        body.night-mode .ip-row strong {
            color: #b0bec5;
        }

        .ip-value {
            font-family: "Consolas", "Courier New", monospace;
            font-weight: 700;
            font-variant-numeric: tabular-nums;
        }

        .ip-sta-online {
            color: #2e7d32;
        }

        .ip-sta-offline {
            color: #c62828;
            font-weight: 700;
        }

        .ip-ap-val {
            color: #0277bd;
        }

        .section-title {
            font-size: 12px;
            font-weight: 800;
            text-transform: uppercase;
            letter-spacing: 0.8px;
            margin: 16px 0 8px 0;
            color: #37474f;
            text-align: left;
            border-bottom: 1px solid #b0bec5;
            padding-bottom: 4px;
        }

        body.night-mode .section-title { 
            color: #b0bec5;
            border-bottom-color: #30363d;
        }

        /* -----------------------------------------------------------------
           3. LISTA DE ESCANEAMENTO DE REDES NO AR
           ----------------------------------------------------------------- */
        .scan-box {
            border: 1px solid #b0bec5;
            border-radius: 6px;
            max-height: 180px;
            overflow-y: auto;
            background: #f7f9fa;
            margin-bottom: 15px;
        }

        body.night-mode .scan-box { 
            background: #1f2630; 
            border-color: #30363d; 
        }

        .network-item {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 10px 14px;
            border-bottom: 1px solid #cfd8dc;
            cursor: pointer;
            transition: background-color 0.2s;
        }

        body.night-mode .network-item { 
            border-color: #30363d; 
        }

        .network-item:hover { 
            background-color: #ffe0b2; 
        }

        body.night-mode .network-item:hover { 
            background-color: #2b3542; 
        }

        .network-name {
            font-size: 14px;
            font-weight: 700;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
            max-width: 250px;
        }

        .network-signal {
            font-family: "Consolas", "Courier New", monospace;
            font-size: 12px;
            color: #546e7a;
            font-weight: 700;
        }

        .empty-hint {
            padding: 16px;
            text-align: center;
            font-size: 13px;
            color: #607d8b;
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
            font-size: 12px;
            font-weight: 700;
            text-transform: uppercase;
            letter-spacing: 0.4px;
            margin-bottom: 4px;
            color: #37474f;
        }

        body.night-mode .form-group label {
            color: #cfd8dc;
        }

        .form-group input {
            width: 100%;
            padding: 10px 12px;
            font-size: 14px;
            border: 1px solid #90a4ae;
            border-radius: 5px;
            background: #ffffff;
            color: inherit;
        }

        body.night-mode .form-group input { 
            background: #1f2630; 
            border-color: #30363d; 
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
            background: #cfd8dc;
            border: 1px solid #90a4ae;
            border-radius: 4px;
            cursor: pointer;
            font-size: 11px;
            font-weight: 700;
            text-transform: uppercase;
            color: #263238;
        }

        body.night-mode .pwd-btn { 
            background: #30363d;
            border-color: #455a64;
            color: #eeeeee; 
        }

        /* -----------------------------------------------------------------
           5. BOTÕES DE AÇÃO DO SISTEMA (TECLAS DE IHM)
           ----------------------------------------------------------------- */
        .btn {
            width: 100%;
            padding: 11px;
            font-size: 13px;
            font-weight: 700;
            text-transform: uppercase;
            letter-spacing: 0.6px;
            border: none;
            border-bottom: 3px solid rgba(0, 0, 0, 0.28);
            border-radius: 5px;
            cursor: pointer;
            margin-top: 8px;
            transition: opacity 0.2s, transform 0.1s;
        }

        .btn:active { 
            opacity: 0.85;
            transform: translateY(1px);
        }

        .btn-scan    { background-color: #0277bd; color: #fff; }
        .btn-connect { background-color: #2e7d32; color: #fff; }
        .btn-back    { background-color: #455a64; color: #fff; }
        .btn-theme   { background-color: #ef6c00; color: #fff; }

        .alert {
            padding: 10px;
            border-radius: 4px;
            font-size: 13px;
            font-weight: 700;
            text-align: center;
            margin-top: 10px;
            display: none;
        }

        .alert-success { background: #e8f5e9; color: #1b5e20; border: 1px solid #a5d6a7; border-left: 4px solid #2e7d32; display: block; }
        .alert-error   { background: #ffebee; color: #b71c1c; border: 1px solid #ef9a9a; border-left: 4px solid #c62828; display: block; }
        .alert-info    { background: #e3f2fd; color: #0d47a1; border: 1px solid #90caf9; border-left: 4px solid #1565c0; display: block; }

        #saved-list {
            margin-top: 10px;
            font-size: 13px;
        }

        .saved-row {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 8px 6px;
            border-bottom: 1px solid #cfd8dc;
        }

        body.night-mode .saved-row {
            border-bottom-color: #30363d;
        }

        .del-link {
            color: #c62828;
            text-decoration: none;
            font-weight: 700;
            font-size: 11px;
            text-transform: uppercase;
            padding: 4px 8px;
            border: 1px solid #c62828;
            border-radius: 4px;
        }

)rawliteral" ENERGIZAR_FOOTER_CSS R"rawliteral(
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

)rawliteral" ENERGIZAR_FOOTER_HTML R"rawliteral(

    <script>
        document.addEventListener('DOMContentLoaded', () => {
            if (localStorage.getItem('energizar_theme') === 'night') {
                document.body.classList.add('night-mode');
            }
            atualizarIP();
            listarRedesSalvas();
            // Polling continuo a cada 3 segundos para detectar quando o roteador atribuir o IP
            setInterval(atualizarIP, 3000);
        });

        /**
         * Verifica se a sessao expirou (401/403) e redireciona com seguranca
         */
        function tratarNaoAutorizado(res) {
            if (res.status === 401 || res.status === 403) {
                window.location.href = '/';
                return true;
            }
            return false;
        }

        /**
         * Consulta o IP atual (STA e AP) diretamente do ESP32
         */
        function atualizarIP() {
            fetch('/ipdarede')
                .then(r => {
                    if (tratarNaoAutorizado(r)) return null;
                    return r.json();
                })
                .then(d => {
                    if (!d) return;

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
                        staIpEl.innerHTML = '';
                        const link = document.createElement('a');
                        link.href = 'http://' + ipExterno;
                        link.target = '_blank';
                        link.style.color = '#2e7d32';
                        link.style.textDecoration = 'underline';
                        link.textContent = ipExterno;
                        staIpEl.appendChild(link);
                        
                        if (d.ssid && d.ssid.length > 0) {
                            staSsidRow.style.display = "flex";
                            staSsidEl.textContent = d.ssid;
                        }
                    } else {
                        staIpEl.className = "ip-value ip-sta-offline";
                        staIpEl.textContent = "Desconectado";
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
                .then(r => {
                    if (tratarNaoAutorizado(r)) return;
                    setTimeout(consultarResultadoScan, 3500);
                })
                .catch(() => {
                    btn.disabled = false;
                    btn.innerText = '🔍 Escanear Redes Wi-Fi';
                    box.innerHTML = '<div class="empty-hint">Falha ao iniciar varredura.</div>';
                });
        }

        /**
         * Obtém o resultado da varredura usando criação segura de DOM (anti-XSS)
         */
        function consultarResultadoScan() {
            const btn = document.getElementById('btn-scan');
            const box = document.getElementById('scan-box');

            fetch('/scanstatus')
                .then(r => {
                    if (tratarNaoAutorizado(r)) return null;
                    return r.json();
                })
                .then(redes => {
                    if (!redes) return;

                    btn.disabled = false;
                    btn.innerText = '🔍 Escanear Redes Wi-Fi';
                    box.innerHTML = '';

                    if (redes.length === 0) {
                        box.innerHTML = '<div class="empty-hint">Nenhuma rede encontrada. Tente novamente.</div>';
                        return;
                    }

                    redes.forEach(rede => {
                        const div = document.createElement('div');
                        div.className = 'network-item';
                        div.addEventListener('click', () => selecionarRede(rede.ssid));

                        const spanNome = document.createElement('span');
                        spanNome.className = 'network-name';
                        spanNome.textContent = '📶 ' + rede.ssid;

                        const spanSinal = document.createElement('span');
                        spanSinal.className = 'network-signal';
                        spanSinal.textContent = rede.rssi + ' dBm';

                        div.appendChild(spanNome);
                        div.appendChild(spanSinal);
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
                    if (tratarNaoAutorizado(r)) return;
                    if (!r.ok) throw new Error();
                    form.reset();
                    listarRedesSalvas();

                    // Realiza polling rápido para detectar o momento que o IP é gerado pelo roteador
                    let tentativas = 0;
                    const checarConexao = setInterval(() => {
                        tentativas++;
                        fetch('/ipdarede')
                            .then(res => {
                                if (tratarNaoAutorizado(res)) {
                                    clearInterval(checarConexao);
                                    return null;
                                }
                                return res.json();
                            })
                            .then(d => {
                                if (!d) return;

                                const ipExterno = d.ip_sta || (d.ip !== "192.168.26.7" && d.ip !== "0.0.0.0" ? d.ip : "");
                                if (d.conectado || (ipExterno && ipExterno.length > 7)) {
                                    clearInterval(checarConexao);
                                    btnSubmit.disabled = false;
                                    msg.className = 'alert alert-success';
                                    msg.innerHTML = '';
                                    msg.appendChild(document.createTextNode('Conectado com sucesso!'));
                                    msg.appendChild(document.createElement('br'));
                                    msg.appendChild(document.createTextNode('IP na rede externa: '));
                                    const a = document.createElement('a');
                                    a.href = 'http://' + ipExterno;
                                    a.target = '_blank';
                                    a.style.fontWeight = 'bold';
                                    a.style.color = '#155724';
                                    a.textContent = 'http://' + ipExterno;
                                    msg.appendChild(a);
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

        /**
         * Lista apenas os SSIDs salvos na Flash via criação segura de DOM (anti-XSS)
         */
        function listarRedesSalvas() {
            fetch('/listadewifi')
                .then(r => {
                    if (tratarNaoAutorizado(r)) return null;
                    return r.text();
                })
                .then(txt => {
                    if (txt === null) return;

                    const box = document.getElementById('saved-list');
                    box.innerHTML = '';
                    const linhas = txt.trim().split('\n');
                    let encontrou = false;

                    linhas.forEach(linha => {
                        const ssid = linha.trim();
                        if (ssid.length > 0) {
                            encontrou = true;

                            const row = document.createElement('div');
                            row.className = 'saved-row';

                            const spanWrap = document.createElement('span');
                            const strong = document.createElement('strong');
                            strong.textContent = ssid;
                            spanWrap.appendChild(strong);

                            const delLink = document.createElement('a');
                            delLink.className = 'del-link';
                            delLink.href = '#';
                            delLink.textContent = 'Excluir';
                            delLink.addEventListener('click', (ev) => {
                                ev.preventDefault();
                                excluirRede(ssid);
                            });

                            row.appendChild(spanWrap);
                            row.appendChild(delLink);
                            box.appendChild(row);
                        }
                    });

                    if (!encontrou) {
                        box.innerHTML = '<span style="color:#888;">Nenhuma rede salva no sistema.</span>';
                    }
                });
        }

        /**
         * Remove rede salva enviando POST (proteção anti-CSRF)
         */
        function excluirRede(ssid) {
            if (!confirm('Deseja excluir a rede salva?')) return;

            const body = new URLSearchParams();
            body.append('ssid', ssid);

            fetch('/excluirwifi', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: body.toString()
            })
                .then(r => {
                    if (tratarNaoAutorizado(r)) return;
                    listarRedesSalvas();
                    atualizarIP();
                });
        }

        function toggleTema() {
            document.body.classList.toggle('night-mode');
            const modoEscuro = document.body.classList.contains('night-mode');
            localStorage.setItem('energizar_theme', modoEscuro ? 'night' : 'light');
        }
    </script>
</body>
</html>
    )rawliteral";
}
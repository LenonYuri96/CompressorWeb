/**
 * ============================================================================
 * PROJETO: EnergizAR - Sistema de Automacao e Monitoramento ESP32
 * ARQUIVO: src/ia_monitor.cpp
 * DESCRICAO: Painel de telemetria em tempo real (1 s) da IA Local Embarcada.
 *            Exibe modo adaptativo, indice de saude, latencia aprendida,
 *            Z-Score, duracao media de ciclo, demanda horaria e permite
 *            recalibrar o modelo apos manutencao de contatores.
 * ============================================================================
 */

#include <ESPAsyncWebServer.h>
#include "ia_monitor.h"
#include "autenticador.h"
#include "rodape.h"

void setupIAMonitorPage(AsyncWebServer &server)
{
    server.on("/ia", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (!isAuthenticated(request))
        {
            redirectToAccessDenied(request);
            return;
        }

        const char *html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Monitor IA Local | EnergizAR</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif;
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
            transition: background-color 0.3s, color 0.3s;
        }

        .container {
            background-color: #eceff1;
            padding: 22px 20px;
            border-radius: 8px;
            border: 2px solid #90a4ae;
            border-top: 6px solid #e65100;
            box-shadow: 0 10px 28px rgba(15, 23, 30, 0.22), inset 0 1px 0 #ffffff;
            text-align: center;
            width: 92%;
            max-width: 460px;
            margin: 20px auto;
            transition: background-color 0.3s;
        }

        .title {
            font-size: 19px;
            font-weight: 800;
            margin-bottom: 6px;
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
        .subtitle { font-size: 12px; font-weight: 700; text-transform: uppercase; letter-spacing: 0.4px; color: #546e7a; margin-bottom: 16px; }

        .metric-card {
            background: #f7f9fa;
            border: 1px solid #b0bec5;
            border-left: 5px solid #0277bd;
            border-radius: 6px;
            padding: 14px;
            margin-bottom: 12px;
            text-align: left;
            box-shadow: 0 2px 5px rgba(0, 0, 0, 0.06);
        }

        .metric-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 8px;
            gap: 8px;
        }

        .metric-title { font-size: 13px; font-weight: 800; text-transform: uppercase; letter-spacing: 0.5px; color: #263238; }

        .badge {
            display: inline-block;
            padding: 3px 9px;
            border-radius: 4px;
            font-size: 11px;
            font-weight: 800;
            text-transform: uppercase;
        }
        .badge-ok   { background: #e8f5e9; color: #1b5e20; border: 1px solid #a5d6a7; }
        .badge-cal  { background: #fff3e0; color: #e65100; border: 1px solid #ffcc80; }
        .badge-info { background: #e3f2fd; color: #0d47a1; border: 1px solid #90caf9; }
        .badge-crit { background: #ffebee; color: #b71c1c; border: 1px solid #ef9a9a; }

        .stats-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 6px 12px;
            font-size: 12px;
            color: #455a64;
            margin-top: 8px;
        }
        .stats-grid strong {
            font-family: "Consolas", "Courier New", monospace;
            color: #1e272e;
            font-variant-numeric: tabular-nums;
        }

        .health-bar-bg {
            background: #cfd8dc;
            border: 1px solid #b0bec5;
            border-radius: 4px;
            height: 10px;
            width: 100%;
            overflow: hidden;
            margin-top: 6px;
        }
        .health-bar-fill {
            height: 100%;
            width: 100%;
            background: #2e7d32;
            border-radius: 3px;
            transition: width 0.4s ease, background-color 0.4s ease;
        }

        .ia-alert-line {
            margin-top: 8px;
            padding: 7px 9px;
            border-radius: 4px;
            background: #e3f2fd;
            color: #0d47a1;
            border: 1px solid #90caf9;
            border-left: 4px solid #1565c0;
            font-size: 12px;
            font-weight: 700;
            display: none;
        }
        .ia-alert-line.active { display: block; }

        .sys-strip {
            display: flex;
            justify-content: space-between;
            font-family: "Consolas", "Courier New", monospace;
            font-size: 12px;
            font-weight: 700;
            color: #cfd8dc;
            background: #1e272e;
            border: 1px solid #37474f;
            border-radius: 5px;
            padding: 9px 12px;
            margin-bottom: 14px;
            font-variant-numeric: tabular-nums;
        }
        .sys-strip strong { color: #ff9800; }

        .btn {
            display: block;
            width: 100%;
            padding: 11px;
            font-size: 13px;
            font-weight: 700;
            text-transform: uppercase;
            letter-spacing: 0.6px;
            margin: 8px auto;
            border-radius: 5px;
            color: #ffffff;
            text-decoration: none;
            text-align: center;
            border: none;
            border-bottom: 3px solid rgba(0, 0, 0, 0.28);
            cursor: pointer;
            transition: opacity 0.2s, transform 0.1s;
        }
        .btn:hover { opacity: 0.92; }
        .btn:active { transform: translateY(1px); }
        .btn-reset { background-color: #b71c1c; }
        .btn-back  { background-color: #455a64; }
        .btn-theme { background-color: #ef6c00; }

        body.night-mode {
            background-color: #0d1117;
            background-image: linear-gradient(rgba(255, 255, 255, 0.03) 1px, transparent 1px),
                              linear-gradient(90deg, rgba(255, 255, 255, 0.03) 1px, transparent 1px);
            color: #e6edf3;
        }
        body.night-mode .container {
            background-color: #161b22;
            border-color: #30363d;
            border-top-color: #e65100;
            box-shadow: 0 12px 32px rgba(0,0,0,0.7);
        }
        body.night-mode .title { background-color: #0d1117; color: #ffa726; }
        body.night-mode .metric-card { background: #1f2630; border-color: #30363d; border-left-color: #0288d1; }
        body.night-mode .metric-title { color: #f0f6fc; }
        body.night-mode .subtitle, body.night-mode .stats-grid { color: #b0bec5; }
        body.night-mode .stats-grid strong { color: #ffffff; }
        body.night-mode .sys-strip { background: #0d1117; border-color: #30363d; color: #b0bec5; }
        body.night-mode .health-bar-bg { background: #2b3542; border-color: #30363d; }

)rawliteral" ENERGIZAR_FOOTER_CSS R"rawliteral(
    </style>
</head>
<body>
    <div class="container">
        <h2 class="title">IA Local em Tempo Real</h2>
        <p class="subtitle">Edge AI Embarcada • Atualização a cada 1 segundo</p>

        <div class="sys-strip">
            <span>Loop CPU: <strong id="sys-hz">--</strong> Hz</span>
            <span>Heap: <strong id="sys-heap">--</strong> KB</span>
            <span>Demanda Hora: <strong id="sys-dem">--</strong>%</span>
        </div>

        <div id="cards-ia"></div>

        <button type="button" class="btn btn-reset" onclick="recalibrarIA()">🔄 Recalibrar IA (Zerar Modelos)</button>
        <a href="/dashboard" class="btn btn-back">⬅ Voltar ao Painel</a>
        <button type="button" class="btn btn-theme" onclick="toggleTema()">🌓 Alternar Tema</button>
    </div>

)rawliteral" ENERGIZAR_FOOTER_HTML R"rawliteral(

    <script>
        const nomesMotores = ["Compressor Principal", "Motor Ventilador", "Secador de Ar"];

        function criarCards() {
            const root = document.getElementById('cards-ia');
            root.innerHTML = '';
            for (let i = 0; i < 3; i++) {
                const card = document.createElement('div');
                card.className = 'metric-card';
                card.innerHTML = `
                    <div class="metric-header">
                        <span class="metric-title">${nomesMotores[i]}</span>
                        <span class="badge badge-info" id="ia-modo-${i}">Carregando</span>
                    </div>
                    <div style="display:flex; justify-content:space-between; font-size:12px;">
                        <span>Índice de Saúde</span>
                        <strong id="ia-saude-txt-${i}">100%</strong>
                    </div>
                    <div class="health-bar-bg">
                        <div class="health-bar-fill" id="ia-saude-bar-${i}"></div>
                    </div>
                    <div class="stats-grid">
                        <span>Amostras FB: <strong id="ia-amostras-${i}">0</strong></span>
                        <span>Latência Média: <strong id="ia-lat-${i}">0.0 ms</strong></span>
                        <span>Z-Score Atual: <strong id="ia-score-${i}">0.00</strong></span>
                        <span>Ciclo Médio: <strong id="ia-dur-${i}">0 s</strong></span>
                    </div>
                    <div class="ia-alert-line" id="ia-alerta-${i}"></div>
                `;
                root.appendChild(card);
            }
        }

        function atualizarTelemetria() {
            fetch('/api/telemetria')
                .then(r => {
                    if (r.status === 401 || r.status === 403) {
                        window.location.href = '/';
                        return null;
                    }
                    return r.json();
                })
                .then(d => {
                    if (!d) return;

                    document.getElementById('sys-hz').textContent = (d.loop_frequencia_hz || 0).toFixed(0);
                    document.getElementById('sys-heap').textContent = Math.round((d.heap_livre || 0) / 1024);
                    document.getElementById('sys-dem').textContent = d.ia_demanda_hora_atual || 0;

                    (d.motores || []).forEach((m, i) => {
                        const badge = document.getElementById(`ia-modo-${i}`);
                        const modo = m.ia_modo || 'ADAPTATIVO_SEM_FEEDBACK';
                        if (modo === 'PREDITIVO_ATIVO') {
                            badge.className = 'badge badge-ok';
                            badge.textContent = 'Preditivo Ativo';
                        } else if (modo === 'CALIBRANDO_FEEDBACK') {
                            badge.className = 'badge badge-cal';
                            badge.textContent = `Calibrando (${m.ia_amostras}/12)`;
                        } else {
                            badge.className = 'badge badge-info';
                            badge.textContent = 'Sem Feedback (Auto)';
                        }

                        const saude = (m.ia_saude !== undefined) ? m.ia_saude : 100;
                        document.getElementById(`ia-saude-txt-${i}`).textContent = `${saude}%`;
                        const bar = document.getElementById(`ia-saude-bar-${i}`);
                        bar.style.width = `${saude}%`;
                        bar.style.backgroundColor = saude >= 80 ? '#2e7d32' : (saude >= 50 ? '#ff9800' : '#c62828');

                        document.getElementById(`ia-amostras-${i}`).textContent = m.ia_amostras || 0;
                        document.getElementById(`ia-lat-${i}`).textContent = `${(m.ia_latencia_ms || 0).toFixed(1)} ms`;
                        document.getElementById(`ia-score-${i}`).textContent = (m.ia_score || 0).toFixed(2);
                        document.getElementById(`ia-dur-${i}`).textContent = `${Math.round(m.ia_duracao_media_s || 0)} s`;

                        const al = document.getElementById(`ia-alerta-${i}`);
                        if (m.ia_alerta && m.ia_alerta !== "") {
                            al.className = 'ia-alert-line active';
                            al.textContent = m.ia_alerta;
                        } else {
                            al.className = 'ia-alert-line';
                        }
                    });
                })
                .catch(() => {});
        }

        function recalibrarIA() {
            if (!confirm('Deseja zerar o aprendizado da IA para todos os motores?')) return;
            fetch('/api/ia/reset', { method: 'POST' })
                .then(r => {
                    if (r.status === 401 || r.status === 403) {
                        window.location.href = '/';
                        return;
                    }
                    atualizarTelemetria();
                });
        }

        function toggleTema() {
            document.body.classList.toggle('night-mode');
            const modoEscuro = document.body.classList.contains('night-mode');
            localStorage.setItem('energizar_theme', modoEscuro ? 'night' : 'light');
        }

        document.addEventListener('DOMContentLoaded', () => {
            if (localStorage.getItem('energizar_theme') === 'night') {
                document.body.classList.add('night-mode');
            }
            criarCards();
            atualizarTelemetria();
            setInterval(atualizarTelemetria, 1000);
        });
    </script>
</body>
</html>
        )rawliteral";

        request->send(200, "text/html", html); });
}
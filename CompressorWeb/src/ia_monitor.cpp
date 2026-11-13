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
            background-color: #f0f2f5;
            color: #212529;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: space-between;
            transition: background-color 0.3s, color 0.3s;
        }

        .container {
            background-color: #ffffff;
            padding: 24px;
            border-radius: 12px;
            box-shadow: 0 4px 16px rgba(0, 0, 0, 0.1);
            text-align: center;
            width: 92%;
            max-width: 460px;
            margin: 20px auto;
            transition: background-color 0.3s;
        }

        .title { font-size: 22px; font-weight: 700; margin-bottom: 4px; color: #e65100; }
        .subtitle { font-size: 13px; color: #666; margin-bottom: 18px; }

        .metric-card {
            background: #fafafa;
            border: 1px solid #e0e0e0;
            border-radius: 10px;
            padding: 14px;
            margin-bottom: 12px;
            text-align: left;
        }

        .metric-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 8px;
            gap: 8px;
        }

        .metric-title { font-size: 14px; font-weight: 700; color: #333; }

        .badge {
            display: inline-block;
            padding: 3px 9px;
            border-radius: 20px;
            font-size: 11px;
            font-weight: 700;
            text-transform: uppercase;
        }
        .badge-ok   { background: #e8f5e9; color: #2e7d32; border: 1px solid #c8e6c9; }
        .badge-cal  { background: #fff3e0; color: #ef6c00; border: 1px solid #ffe0b2; }
        .badge-info { background: #e3f2fd; color: #0d47a1; border: 1px solid #bbdefb; }
        .badge-crit { background: #ffebee; color: #c62828; border: 1px solid #ef9a9a; }

        .stats-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 6px 12px;
            font-size: 12px;
            color: #555;
            margin-top: 8px;
        }
        .stats-grid strong { color: #222; font-variant-numeric: tabular-nums; }

        .health-bar-bg {
            background: #e0e0e0;
            border-radius: 6px;
            height: 8px;
            width: 100%;
            overflow: hidden;
            margin-top: 6px;
        }
        .health-bar-fill {
            height: 100%;
            width: 100%;
            background: #2e7d32;
            border-radius: 6px;
            transition: width 0.4s ease, background-color 0.4s ease;
        }

        .ia-alert-line {
            margin-top: 8px;
            padding: 6px 8px;
            border-radius: 6px;
            background: #e3f2fd;
            color: #0d47a1;
            border: 1px solid #bbdefb;
            font-size: 12px;
            font-weight: 600;
            display: none;
        }
        .ia-alert-line.active { display: block; }

        .sys-strip {
            display: flex;
            justify-content: space-between;
            font-size: 12px;
            color: #555;
            background: #f8f9fa;
            border: 1px solid #e0e0e0;
            border-radius: 8px;
            padding: 8px 12px;
            margin-bottom: 14px;
            font-variant-numeric: tabular-nums;
        }

        .btn {
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
            transition: opacity 0.2s;
        }
        .btn:hover { opacity: 0.88; }
        .btn-reset { background-color: #c62828; }
        .btn-back  { background-color: #607d8b; }
        .btn-theme { background-color: #ff9800; }

        body.night-mode { background-color: #121212; color: #e0e0e0; }
        body.night-mode .container { background-color: #1e1e1e; box-shadow: 0 4px 16px rgba(0,0,0,0.5); }
        body.night-mode .metric-card { background: #2a2a2a; border-color: #383838; }
        body.night-mode .metric-title { color: #f0f0f0; }
        body.night-mode .subtitle, body.night-mode .stats-grid { color: #bbb; }
        body.night-mode .stats-grid strong { color: #fff; }
        body.night-mode .sys-strip { background: #262626; border-color: #383838; color: #ccc; }
        body.night-mode .health-bar-bg { background: #444; }

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

    <div class="footer">
        EnergizAR - Automação Industrial e Monitoramento
    </div>

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
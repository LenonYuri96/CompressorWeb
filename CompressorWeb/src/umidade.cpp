/**
 * ============================================================================
 * PROJETO: EnergizAR - Sistema de Automação e Monitoramento ESP32
 * ARQUIVO: src/umidade.cpp
 * DESCRIÇÃO: Página de monitoramento da qualidade do ar comprimido e umidade.
 *            Exibe saturação relativa, ponto de orvalho (dew point), status
 *            do secador de refrigeração e purga de condensados.
 * ============================================================================
 */

#include <ESPAsyncWebServer.h>
#include "umidade.h"
#include "autenticador.h"

void setupUmidadePage(AsyncWebServer &server)
{
    server.on("/umidade", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // Validação de sessão autenticada
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
    <title>Monitor de Umidade | EnergizAR</title>
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
            transition: background-color 0.3s ease, color 0.3s ease;
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
            max-width: 450px;
            margin: 20px auto;
            transition: background-color 0.3s ease, border-color 0.3s ease, box-shadow 0.3s ease;
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

        .subtitle {
            font-size: 12px;
            font-weight: 700;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            color: #546e7a;
            margin-bottom: 18px;
        }

        /* Cartões de Medição */
        .metric-card {
            background: #f7f9fa;
            border: 1px solid #b0bec5;
            border-left: 5px solid #00695c;
            border-radius: 6px;
            padding: 15px;
            margin-bottom: 14px;
            text-align: left;
            box-shadow: 0 2px 5px rgba(0, 0, 0, 0.08), inset 0 1px 0 #ffffff;
            transition: border-color 0.2s ease, background 0.2s ease;
        }

        .metric-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 8px;
            gap: 8px;
        }

        .metric-title {
            font-size: 13px;
            font-weight: 800;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            color: #263238;
        }

        .metric-value {
            font-family: "Consolas", "Courier New", monospace;
            font-size: 32px;
            font-weight: 800;
            color: #1e272e;
            font-variant-numeric: tabular-nums;
        }

        .metric-unit {
            font-family: "Consolas", "Courier New", monospace;
            font-size: 15px;
            font-weight: 700;
            color: #546e7a;
        }

        /* Barra Gráfica de Saturação */
        .progress-bar-bg {
            background: #cfd8dc;
            border: 1px solid #b0bec5;
            border-radius: 4px;
            height: 12px;
            width: 100%;
            overflow: hidden;
            margin-top: 10px;
            box-shadow: inset 0 1px 2px rgba(0, 0, 0, 0.15);
        }

        .progress-bar-fill {
            background: linear-gradient(90deg, #2e7d32, #ff9800, #c62828);
            height: 100%;
            width: 28%; /* Valor de demonstração */
            border-radius: 3px;
            transition: width 0.5s ease;
        }

        /* Badges de Status */
        .badge {
            display: inline-block;
            padding: 3px 9px;
            border-radius: 4px;
            font-size: 11px;
            font-weight: 800;
            text-transform: uppercase;
            letter-spacing: 0.3px;
        }

        .badge-good { background: #e8f5e9; color: #1b5e20; border: 1px solid #a5d6a7; }
        .badge-warn { background: #fff3e0; color: #e65100; border: 1px solid #ffcc80; }

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
        .btn-back  { background-color: #455a64; }
        .btn-theme { background-color: #ef6c00; }

        /* Modo Noturno */
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
            box-shadow: 0 12px 32px rgba(0, 0, 0, 0.7);
        }
        body.night-mode .title { background-color: #0d1117; color: #ffa726; }
        body.night-mode .metric-card {
            background: #1f2630;
            border-color: #30363d;
            border-left-color: #00897b;
            box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.04);
        }
        body.night-mode .metric-title { color: #f0f6fc; }
        body.night-mode .metric-value { color: #ffffff; }
        body.night-mode .metric-unit { color: #90a4ae; }
        body.night-mode .metric-card p { color: #b0bec5 !important; }
        body.night-mode .metric-card p strong { color: #f0f6fc; }
        body.night-mode .subtitle { color: #b0bec5; }
        body.night-mode .progress-bar-bg { background: #2b3542; border-color: #30363d; }

        .footer {
            width: 100%;
            background-color: #1e272e;
            border-top: 3px solid #e65100;
            color: #cfd8dc;
            text-align: center;
            padding: 10px 0;
            font-size: 12px;
            font-weight: 600;
            letter-spacing: 0.4px;
            margin-top: auto;
        }
    </style>
</head>
<body>
    <div class="container">
        <h2 class="title">Monitor de Umidade</h2>
        <p class="subtitle">Qualidade e Ponto de Orvalho do Ar Comprimido</p>

        <!-- Medição 1: Umidade Relativa -->
        <div class="metric-card">
            <div class="metric-header">
                <span class="metric-title">Umidade Relativa (Linha)</span>
                <span class="badge badge-good" id="badge-umidade">Seco / Normal</span>
            </div>
            <div>
                <span class="metric-value" id="val-umidade">28.4</span>
                <span class="metric-unit">% RH</span>
            </div>
            <div class="progress-bar-bg">
                <div class="progress-bar-fill" id="bar-umidade" style="width: 28%;"></div>
            </div>
        </div>

        <!-- Medição 2: Ponto de Orvalho -->
        <div class="metric-card">
            <div class="metric-header">
                <span class="metric-title">Ponto de Orvalho (Dew Point)</span>
                <span class="badge badge-good">Norma ISO 8573</span>
            </div>
            <div>
                <span class="metric-value" id="val-dew">+3.2</span>
                <span class="metric-unit">°C</span>
            </div>
        </div>

        <!-- Medição 3: Secador & Purga -->
        <div class="metric-card">
            <div class="metric-header">
                <span class="metric-title">Secador & Drenagem</span>
                <span class="badge badge-good">Ativo</span>
            </div>
            <p style="font-size: 13px; color: #555; margin-top: 4px;">
                Secador por Refrigeração: <strong>Operando</strong><br>
                Válvula de Purga Automática: <strong>Cíclica OK</strong>
            </p>
        </div>

        <a href="/dashboard" class="btn btn-back">⬅ Voltar ao Painel</a>
        <button type="button" class="btn btn-theme" onclick="toggleTema()">🌓 Alternar Tema</button>
    </div>

    <div class="footer">
        EnergizAR - Automação Industrial e Monitoramento
    </div>

    <script>
        document.addEventListener('DOMContentLoaded', () => {
            if (localStorage.getItem('energizar_theme') === 'night') {
                document.body.classList.add('night-mode');
            }
        });

        function toggleTema() {
            document.body.classList.toggle('night-mode');
            const modoEscuro = document.body.classList.contains('night-mode');
            localStorage.setItem('energizar_theme', modoEscuro ? 'night' : 'light');
        }
    </script>
</body>
</html>
        )rawliteral";

        request->send(200, "text/html", html); });
}
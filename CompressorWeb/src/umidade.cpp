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
            max-width: 440px;
            margin: 20px auto;
            transition: background-color 0.3s;
        }

        .title {
            font-size: 22px;
            font-weight: 700;
            margin-bottom: 6px;
            color: #00897b;
        }

        .subtitle {
            font-size: 13px;
            color: #666;
            margin-bottom: 20px;
        }

        /* Cartões de Medição */
        .metric-card {
            background: #fafafa;
            border: 1px solid #e0e0e0;
            border-radius: 10px;
            padding: 16px;
            margin-bottom: 14px;
            text-align: left;
        }

        .metric-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 8px;
        }

        .metric-title {
            font-size: 14px;
            font-weight: 700;
            color: #444;
        }

        .metric-value {
            font-size: 32px;
            font-weight: 800;
            color: #00897b;
            font-variant-numeric: tabular-nums;
        }

        .metric-unit {
            font-size: 16px;
            font-weight: 600;
            color: #777;
        }

        /* Barra Gráfica de Saturação */
        .progress-bar-bg {
            background: #e0e0e0;
            border-radius: 6px;
            height: 10px;
            width: 100%;
            overflow: hidden;
            margin-top: 10px;
        }

        .progress-bar-fill {
            background: linear-gradient(90deg, #2e7d32, #ff9800, #c62828);
            height: 100%;
            width: 28%; /* Valor de demonstração */
            border-radius: 6px;
            transition: width 0.5s ease;
        }

        /* Badges de Status */
        .badge {
            display: inline-block;
            padding: 4px 10px;
            border-radius: 20px;
            font-size: 12px;
            font-weight: 700;
            text-transform: uppercase;
        }

        .badge-good { background: #e8f5e9; color: #2e7d32; border: 1px solid #c8e6c9; }
        .badge-warn { background: #fff3e0; color: #ef6c00; border: 1px solid #ffe0b2; }

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
        .btn-back  { background-color: #607d8b; }
        .btn-theme { background-color: #ff9800; }

        /* Modo Noturno */
        body.night-mode { background-color: #121212; color: #e0e0e0; }
        body.night-mode .container { background-color: #1e1e1e; box-shadow: 0 4px 16px rgba(0,0,0,0.5); }
        body.night-mode .metric-card { background: #2a2a2a; border-color: #383838; }
        body.night-mode .metric-title { color: #ccc; }
        body.night-mode .subtitle { color: #aaa; }
        body.night-mode .progress-bar-bg { background: #444; }

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
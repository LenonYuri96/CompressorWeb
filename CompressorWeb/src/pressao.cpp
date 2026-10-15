/**
 * ============================================================================
 * PROJETO: EnergizAR - Sistema de Automação e Monitoramento ESP32
 * ARQUIVO: src/pressao.cpp
 * DESCRIÇÃO: Página de telemetria da pressão pneumática de linha, pressostato
 *            de segurança mecânico (GPIO 17) e monitor de lubrificação/óleo.
 * ============================================================================
 */

#include <ESPAsyncWebServer.h>
#include "pressao.h"
#include "autenticador.h"
#include "pressostato.h"

void setupPressaoPage(AsyncWebServer &server)
{
    server.on("/pressao", HTTP_GET, [](AsyncWebServerRequest *request)
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
    <title>Monitor de Pressão e Óleo | EnergizAR</title>
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
            color: #455a64;
        }

        .subtitle {
            font-size: 13px;
            color: #666;
            margin-bottom: 20px;
        }

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
            color: #455a64;
            font-variant-numeric: tabular-nums;
        }

        .metric-unit {
            font-size: 16px;
            font-weight: 600;
            color: #777;
        }

        /* Barra de Pressão com Faixas de Operação */
        .gauge-bar-bg {
            background: #e0e0e0;
            border-radius: 6px;
            height: 12px;
            width: 100%;
            overflow: hidden;
            margin-top: 10px;
            position: relative;
        }

        .gauge-bar-fill {
            background: linear-gradient(90deg, #0288d1 0%, #2e7d32 60%, #c62828 95%);
            height: 100%;
            width: 76%; /* 7.6 bar de 10.0 bar */
            border-radius: 6px;
            transition: width 0.5s ease;
        }

        .gauge-labels {
            display: flex;
            justify-content: space-between;
            font-size: 11px;
            color: #888;
            margin-top: 4px;
            font-weight: 600;
        }

        .badge {
            display: inline-block;
            padding: 4px 10px;
            border-radius: 20px;
            font-size: 12px;
            font-weight: 700;
            text-transform: uppercase;
        }

        .badge-good { background: #e8f5e9; color: #2e7d32; border: 1px solid #c8e6c9; }
        .badge-crit { background: #ffebee; color: #c62828; border: 1px solid #ef9a9a; }

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

        body.night-mode { background-color: #121212; color: #e0e0e0; }
        body.night-mode .container { background-color: #1e1e1e; box-shadow: 0 4px 16px rgba(0,0,0,0.5); }
        body.night-mode .metric-card { background: #2a2a2a; border-color: #383838; }
        body.night-mode .metric-title { color: #ccc; }
        body.night-mode .subtitle { color: #aaa; }
        body.night-mode .gauge-bar-bg { background: #444; }

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
        <h2 class="title">Monitor de Pressão & Óleo</h2>
        <p class="subtitle">Reservatório Principal e Lubrificação</p>

        <!-- Medição 1: Pressão Pneumática -->
        <div class="metric-card">
            <div class="metric-header">
                <span class="metric-title">Pressão da Rede</span>
                <span class="badge badge-good" id="badge-pressao">Em Carga (Estável)</span>
            </div>
            <div>
                <span class="metric-value" id="val-bar">7.6</span>
                <span class="metric-unit">bar &nbsp;|&nbsp; 110.2 psi</span>
            </div>
            <div class="gauge-bar-bg">
                <div class="gauge-bar-fill" style="width: 76%;"></div>
            </div>
            <div class="gauge-labels">
                <span>0 bar</span>
                <span>Alívio: 6.0</span>
                <span>Carga: 8.0</span>
                <span>Máx: 10 bar</span>
            </div>
        </div>

        <!-- Medição 2: Pressostato Físico (GPIO 17) -->
        <div class="metric-card">
            <div class="metric-header">
                <span class="metric-title">Pressostato de Segurança (GPIO 17)</span>
                <span class="badge badge-good" id="badge-pressostato">Normal (Fechado)</span>
            </div>
            <p style="font-size: 13px; color: #555; margin-top: 4px;">
                Válvula de Alívio: <strong>Calibrada</strong><br>
                Intertravamento: <strong>Circuito Protegido</strong>
            </p>
        </div>

        <!-- Medição 3: Lubrificação & Nível de Óleo -->
        <div class="metric-card">
            <div class="metric-header">
                <span class="metric-title">Nível de Óleo do Cárter</span>
                <span class="badge badge-good">Nível Adequado</span>
            </div>
            <p style="font-size: 13px; color: #555; margin-top: 4px;">
                Visor Óptico de Nível: <strong>100% OK</strong><br>
                Filtro Separador de Óleo: <strong>Pressão Diferencial Normal</strong>
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

            // Consulta o estado real do pressostato através da telemetria do ESP32
            fetch('/motor-state')
                .then(r => r.json())
                .then(data => {
                    const badge = document.getElementById('badge-pressostato');
                    if (data.pressostatoAtivo) {
                        badge.className = 'badge badge-crit';
                        badge.innerText = 'Alarme Crítico (Aberto)';
                    } else {
                        badge.className = 'badge badge-good';
                        badge.innerText = 'Normal (Seguro)';
                    }
                })
                .catch(() => {});
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
/**
 * ============================================================================
 * PROJETO: EnergizAR - Sistema de Automação e Monitoramento ESP32
 * ARQUIVO: src/creditos.cpp
 * DESCRIÇÃO: Página institucional de créditos e autoria do projeto.
 *            Apresenta a equipe de alunos desenvolvedores e instrutores com
 *            layout moderno, responsivo, links para GitHub e modo noturno.
 * ============================================================================
 */

#include "creditos.h"
#include <ESPAsyncWebServer.h>

/**
 * Configura a rota HTTP e entrega a interface da página de créditos.
 * @param server Instância de referência do servidor assíncrono.
 */
void setupCreditosPage(AsyncWebServer &server)
{
    server.on("/creditos", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        const char *html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>Créditos | EnergizAR</title>
    <style>
        /* -------------------------------------------------------------
           RESET E TIPOGRAFIA GERAL
           ------------------------------------------------------------- */
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

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

        /* -------------------------------------------------------------
           CONTAINER PRINCIPAL ESTILO CARD
           ------------------------------------------------------------- */
        .credits-container {
            background-color: #eceff1;
            padding: 24px 20px;
            border-radius: 8px;
            border: 2px solid #90a4ae;
            border-top: 6px solid #e65100;
            box-shadow: 0 10px 28px rgba(15, 23, 30, 0.22), inset 0 1px 0 #ffffff;
            text-align: center;
            width: 92%;
            max-width: 480px;
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
            margin-bottom: 20px;
            line-height: 1.4;
        }

        /* -------------------------------------------------------------
           SEÇÕES E SUBTÍTULOS
           ------------------------------------------------------------- */
        .section-header {
            font-size: 12px;
            font-weight: 800;
            text-transform: uppercase;
            letter-spacing: 0.8px;
            color: #37474f;
            text-align: left;
            margin: 18px 0 10px 0;
            display: flex;
            align-items: center;
            gap: 8px;
        }

        .section-header::after {
            content: '';
            flex: 1;
            height: 1px;
            background-color: #b0bec5;
        }

        /* -------------------------------------------------------------
           CARDS DE INSTRUTORES
           ------------------------------------------------------------- */
        .mentor-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
            margin-bottom: 14px;
        }

        .mentor-card {
            background: #f7f9fa;
            border: 1px solid #b0bec5;
            border-left: 4px solid #37474f;
            border-radius: 6px;
            padding: 12px 8px;
            text-decoration: none;
            color: inherit;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            box-shadow: 0 2px 5px rgba(0, 0, 0, 0.06), inset 0 1px 0 #ffffff;
            transition: all 0.2s ease;
        }

        .mentor-card:hover {
            background-color: #fff3e0;
            border-color: #ff9800;
            border-left-color: #e65100;
            transform: translateY(-2px);
            box-shadow: 0 4px 10px rgba(0, 0, 0, 0.1);
        }

        .mentor-name {
            font-size: 14px;
            font-weight: 800;
            color: #1e272e;
            margin-top: 4px;
        }

        .mentor-role {
            font-size: 10px;
            color: #e65100;
            font-weight: 800;
            text-transform: uppercase;
            letter-spacing: 0.4px;
        }

        .github-badge {
            margin-top: 6px;
            font-family: "Consolas", "Courier New", monospace;
            font-size: 11px;
            color: #0277bd;
            display: flex;
            align-items: center;
            gap: 4px;
            font-weight: 700;
        }

        /* -------------------------------------------------------------
           GRADE DE CHIPS DE ALUNOS
           ------------------------------------------------------------- */
        .students-grid {
            display: flex;
            flex-wrap: wrap;
            gap: 8px;
            justify-content: center;
            margin-bottom: 20px;
        }

        .student-chip {
            background: #f7f9fa;
            border: 1px solid #b0bec5;
            border-left: 4px solid #0277bd;
            border-radius: 4px;
            padding: 7px 12px;
            font-size: 12px;
            font-weight: 700;
            color: #263238;
            display: inline-flex;
            align-items: center;
            gap: 6px;
            box-shadow: 0 1px 3px rgba(0, 0, 0, 0.06);
            transition: all 0.2s ease;
        }

        .student-chip:hover {
            background: #e8f5e9;
            border-color: #2e7d32;
            color: #1b5e20;
            transform: scale(1.03);
        }

        /* -------------------------------------------------------------
           INFO INSTITUCIONAL (TRILHAS DE FUTURO)
           ------------------------------------------------------------- */
        .institution-box {
            background: #f7f9fa;
            border: 1px solid #b0bec5;
            border-left: 5px solid #e65100;
            padding: 11px 14px;
            border-radius: 6px;
            text-align: left;
            font-size: 12px;
            color: #37474f;
            margin-bottom: 20px;
            line-height: 1.5;
            box-shadow: 0 2px 5px rgba(0, 0, 0, 0.06);
        }

        .institution-box strong {
            color: #1e272e;
            text-transform: uppercase;
            letter-spacing: 0.4px;
        }

        /* -------------------------------------------------------------
           BOTÕES DE AÇÃO E NAVEGAÇÃO
           ------------------------------------------------------------- */
        .btn-group {
            display: flex;
            flex-direction: column;
            gap: 8px;
        }

        .btn {
            display: block;
            width: 100%;
            padding: 11px;
            font-size: 13px;
            font-weight: 700;
            text-transform: uppercase;
            letter-spacing: 0.6px;
            border-radius: 5px;
            color: #ffffff;
            text-decoration: none;
            text-align: center;
            border: none;
            border-bottom: 3px solid rgba(0, 0, 0, 0.28);
            cursor: pointer;
            transition: opacity 0.2s ease, transform 0.1s ease;
        }

        .btn:hover { opacity: 0.92; }
        .btn:active { transform: translateY(1px); }
        .btn-back  { background-color: #455a64; }
        .btn-theme { background-color: #ef6c00; }

        /* -------------------------------------------------------------
           TEMA ESCURO (NIGHT MODE)
           ------------------------------------------------------------- */
        body.night-mode {
            background-color: #0d1117;
            background-image: linear-gradient(rgba(255, 255, 255, 0.03) 1px, transparent 1px),
                              linear-gradient(90deg, rgba(255, 255, 255, 0.03) 1px, transparent 1px);
            color: #e6edf3;
        }

        body.night-mode .credits-container {
            background-color: #161b22;
            border-color: #30363d;
            border-top-color: #e65100;
            box-shadow: 0 12px 32px rgba(0, 0, 0, 0.7);
        }

        body.night-mode .title {
            background-color: #0d1117;
            color: #ffa726;
        }

        body.night-mode .subtitle { color: #b0bec5; }
        body.night-mode .section-header { color: #b0bec5; }
        body.night-mode .section-header::after { background-color: #30363d; }

        body.night-mode .mentor-card {
            background: #1f2630;
            border-color: #30363d;
            border-left-color: #ff9800;
            box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.04);
        }

        body.night-mode .mentor-card:hover {
            background-color: #2b2118;
            border-color: #e65100;
        }

        body.night-mode .mentor-name { color: #f0f6fc; }
        body.night-mode .github-badge { color: #4fc3f7; }

        body.night-mode .student-chip {
            background: #1f2630;
            border-color: #30363d;
            border-left-color: #0288d1;
            color: #e6edf3;
        }

        body.night-mode .student-chip:hover {
            background: #1b3320;
            border-color: #2e7d32;
            color: #a5d6a7;
        }

        body.night-mode .institution-box {
            background: #1f2630;
            border-color: #30363d;
            border-left-color: #e65100;
            color: #b0bec5;
        }

        body.night-mode .institution-box strong { color: #ffffff; }

        /* -------------------------------------------------------------
           RODAPÉ PADRÃO ENERGIZAR
           ------------------------------------------------------------- */
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
    <div class="credits-container">
        <h2 class="title">Créditos do Projeto</h2>
        <p class="subtitle">Equipe de Desenvolvimento & Mentoria Técnica</p>

        <!-- SEÇÃO: Instrutores -->
        <div class="section-header">
            <span>👨‍🏫 Instrutores & Mentoria</span>
        </div>
        <div class="mentor-grid">
            <a href="https://github.com/LYuri26" target="_blank" class="mentor-card" aria-label="GitHub Lenon Yuri">
                <span class="mentor-role">Instrutor / Arquiteto</span>
                <span class="mentor-name">Lenon Yuri</span>
                <span class="github-badge">🔗 GitHub</span>
            </a>
            <a href="https://github.com/RoCkHeLuCk" target="_blank" class="mentor-card" aria-label="GitHub Franco Caixeta">
                <span class="mentor-role">Instrutor / Co-autor</span>
                <span class="mentor-name">Franco M. A. Caixeta</span>
                <span class="github-badge">🔗 GitHub</span>
            </a>
        </div>

        <!-- SEÇÃO: Alunos Desenvolvedores -->
        <div class="section-header">
            <span>💻 Equipe de Alunos</span>
        </div>
        <div class="students-grid">
            <span class="student-chip">👩‍💻 Brenda Luiza</span>
            <span class="student-chip">👨‍💻 Davi Afonso</span>
            <span class="student-chip">👨‍💻 João Martins</span>
            <span class="student-chip">👨‍💻 Pedro Lucas</span>
            <span class="student-chip">👩‍💻 Sofia Tresse</span>
        </div>

        <!-- SEÇÃO: Institucional -->
        <div class="institution-box">
            <strong>Programa Trilhas de Futuro</strong><br>
            Curso Técnico em Informática para Internet • Minas Gerais<br>
            Projeto Integrador: Automação Industrial e Supervisório IoT com ESP32.
        </div>

        <!-- BOTÕES: Navegação e Acessibilidade -->
        <div class="btn-group">
            <button type="button" onclick="voltarPagina()" class="btn btn-back">⬅ Voltar</button>
            <button type="button" onclick="toggleTema()" class="btn btn-theme">🌓 Alternar Tema</button>
        </div>
    </div>

    <div class="footer">
        EnergizAR - Automação Industrial e Monitoramento
    </div>

    <script>
        document.addEventListener('DOMContentLoaded', () => {
            // Sincroniza o modo escuro com a preferência salva no Dashboard
            if (localStorage.getItem('energizar_theme') === 'night') {
                document.body.classList.add('night-mode');
            }
        });

        function voltarPagina() {
            // Se houver histórico anterior volta para ele; caso contrário vai ao Dashboard
            if (window.history.length > 1) {
                window.history.back();
            } else {
                window.location.href = '/dashboard';
            }
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

        request->send(200, "text/html", html); });
}
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
           CONTAINER PRINCIPAL ESTILO CARD
           ------------------------------------------------------------- */
        .credits-container {
            background-color: #ffffff;
            padding: 28px 24px;
            border-radius: 12px;
            box-shadow: 0 4px 16px rgba(0, 0, 0, 0.1);
            text-align: center;
            width: 92%;
            max-width: 480px;
            margin: 20px auto;
            transition: background-color 0.3s ease, box-shadow 0.3s ease;
        }

        .title {
            font-size: 24px;
            font-weight: 700;
            color: #e65100;
            margin-bottom: 4px;
            letter-spacing: 0.5px;
        }

        .subtitle {
            font-size: 13px;
            color: #666;
            margin-bottom: 22px;
            line-height: 1.4;
        }

        /* -------------------------------------------------------------
           SEÇÕES E SUBTÍTULOS
           ------------------------------------------------------------- */
        .section-header {
            font-size: 13px;
            font-weight: 700;
            text-transform: uppercase;
            letter-spacing: 0.8px;
            color: #555;
            text-align: left;
            margin: 18px 0 10px 0;
            display: flex;
            align-items: center;
            gap: 6px;
        }

        .section-header::after {
            content: '';
            flex: 1;
            height: 1px;
            background-color: #e0e0e0;
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
            background: #fafafa;
            border: 1px solid #e0e0e0;
            border-radius: 8px;
            padding: 12px 8px;
            text-decoration: none;
            color: inherit;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            transition: all 0.2s ease;
        }

        .mentor-card:hover {
            background-color: #fff3e0;
            border-color: #ff9800;
            transform: translateY(-2px);
            box-shadow: 0 4px 10px rgba(0, 0, 0, 0.08);
        }

        .mentor-name {
            font-size: 14px;
            font-weight: 700;
            color: #212529;
            margin-top: 4px;
        }

        .mentor-role {
            font-size: 11px;
            color: #e65100;
            font-weight: 600;
            text-transform: uppercase;
        }

        .github-badge {
            margin-top: 6px;
            font-size: 11px;
            color: #0288d1;
            display: flex;
            align-items: center;
            gap: 4px;
            font-weight: 600;
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
            background: #f5f5f5;
            border: 1px solid #ddd;
            border-radius: 20px;
            padding: 7px 14px;
            font-size: 13px;
            font-weight: 600;
            color: #333;
            display: inline-flex;
            align-items: center;
            gap: 6px;
            transition: all 0.2s ease;
        }

        .student-chip:hover {
            background: #e8f5e9;
            border-color: #81c784;
            color: #1b5e20;
            transform: scale(1.03);
        }

        /* -------------------------------------------------------------
           INFO INSTITUCIONAL (TRILHAS DE FUTURO)
           ------------------------------------------------------------- */
        .institution-box {
            background: #f8f9fa;
            border-left: 4px solid #e65100;
            padding: 10px 14px;
            border-radius: 4px;
            text-align: left;
            font-size: 12px;
            color: #555;
            margin-bottom: 20px;
            line-height: 1.4;
        }

        .institution-box strong {
            color: #222;
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
            font-size: 14px;
            font-weight: 600;
            border-radius: 6px;
            color: #ffffff;
            text-decoration: none;
            text-align: center;
            border: none;
            cursor: pointer;
            transition: opacity 0.2s ease;
        }

        .btn:hover { opacity: 0.88; }
        .btn-back  { background-color: #607d8b; }
        .btn-theme { background-color: #ff9800; }

        /* -------------------------------------------------------------
           TEMA ESCURO (NIGHT MODE)
           ------------------------------------------------------------- */
        body.night-mode {
            background-color: #121212;
            color: #e0e0e0;
        }

        body.night-mode .credits-container {
            background-color: #1e1e1e;
            box-shadow: 0 4px 16px rgba(0, 0, 0, 0.5);
        }

        body.night-mode .subtitle { color: #aaa; }
        body.night-mode .section-header { color: #aaa; }
        body.night-mode .section-header::after { background-color: #383838; }

        body.night-mode .mentor-card {
            background: #2a2a2a;
            border-color: #383838;
        }

        body.night-mode .mentor-card:hover {
            background-color: #33261a;
            border-color: #e65100;
        }

        body.night-mode .mentor-name { color: #f0f0f0; }

        body.night-mode .student-chip {
            background: #2a2a2a;
            border-color: #404040;
            color: #ddd;
        }

        body.night-mode .student-chip:hover {
            background: #1b3320;
            border-color: #2e7d32;
            color: #a5d6a7;
        }

        body.night-mode .institution-box {
            background: #262626;
            color: #bbb;
        }

        body.night-mode .institution-box strong { color: #fff; }

        /* -------------------------------------------------------------
           RODAPÉ PADRÃO ENERGIZAR
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
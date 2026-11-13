#include <ESPAsyncWebServer.h> // Inclui a biblioteca para criar um servidor web assíncrono
#include "index.h"             // Inclui o cabeçalho que pode definir variáveis ou funções específicas para a página de login

// -------------------------------------------------------------------------
// Função para configurar a página de login
// -------------------------------------------------------------------------

/**
 * Configura a rota para a página de login no servidor Web.
 *
 * Otimização de Memória (Flash/RODATA):
 *   O documento HTML é armazenado como `const char *` diretamente na memória
 *   Flash, evitando a alocação de ~6 KB contíguos na Heap a cada requisição
 *   GET "/". O tratamento do parâmetro `?login_failed=true` é feito no
 *   navegador via `URLSearchParams`, dispensando `String::replace()`.
 *
 * @param server A instância do servidor web assíncrono.
 */
void setupIndexPage(AsyncWebServer &server)
{
    // Define a rota para a URL raiz ("/") do servidor web
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // Define o conteúdo HTML da página de login usando um literal RAW direto na Flash
        const char *html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <link rel="preconnect" href="https://fonts.googleapis.com">
        <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
        <link
            href="https://fonts.googleapis.com/css2?family=Playfair+Display:ital,wght@0,400..900;1,400..900&display=swap"
            rel="stylesheet">

        <title>Login | EnergizAR</title>
        <style>
        /* Estilos gerais da página (Estética de Painel Industrial) */
/* Estilos gerais para o corpo da página */
body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif;
    background-color: #d6dde4;
    background-image: linear-gradient(rgba(30, 39, 46, 0.05) 1px, transparent 1px),
                      linear-gradient(90deg, rgba(30, 39, 46, 0.05) 1px, transparent 1px);
    background-size: 20px 20px;
    color: #1e272e;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: space-between;
    min-height: 100vh; /* Garante que o conteúdo ocupe pelo menos 100% da altura da viewport */
    margin: 0;
    padding: 0;
    transition: background-color 0.3s ease, color 0.3s ease;
}
        .dark-mode {
            background-color: #0d1117;
            background-image: linear-gradient(rgba(255, 255, 255, 0.03) 1px, transparent 1px),
                              linear-gradient(90deg, rgba(255, 255, 255, 0.03) 1px, transparent 1px);
            color: #e6edf3;
        }
        .login-container {
    background-color: #eceff1;
    padding: 24px 22px;
    border-radius: 8px;
    border: 2px solid #90a4ae;
    border-top: 6px solid #e65100;
    box-shadow: 0 10px 28px rgba(15, 23, 30, 0.22), inset 0 1px 0 #ffffff;
    text-align: center;
    width: 92%;
    max-width: 410px;
    margin: auto; /* Garante que o conteúdo fique centralizado */
    transition: background-color 0.3s ease, border-color 0.3s ease;
            }
        .dark-mode .login-container {
            background-color: #161b22;
            border-color: #30363d;
            border-top-color: #e65100;
            box-shadow: 0 12px 32px rgba(0, 0, 0, 0.7);
        }
        .login-title {
            font-size: 22px;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif;
            margin-bottom: 20px;
            padding: 10px 12px;
            background-color: #1e272e;
            color: #ff9800;
            border-radius: 5px;
            border-left: 5px solid #e65100;
            border-right: 5px solid #e65100;
            text-transform: uppercase;
            letter-spacing: 1.2px;
            font-weight: 800;
            box-shadow: inset 0 2px 4px rgba(0, 0, 0, 0.4);
        }
        .dark-mode .login-title {
            background-color: #0d1117;
            color: #ffa726;
        }
        .form-group {
            margin-bottom: 14px;
        }
        .form-control {
            width: 100%;
            padding: 11px 12px;
            font-size: 14px;
            border: 1px solid #90a4ae;
            border-radius: 5px;
            background-color: #ffffff;
            color: #1e272e;
            box-sizing: border-box;
            box-shadow: inset 0 1px 3px rgba(0, 0, 0, 0.08);
        }
        .form-control:focus {
            outline: none;
            border-color: #e65100;
        }
        .dark-mode .form-control {
            background-color: #1f2630;
            color: #f0f6fc;
            border-color: #30363d;
        }
        .btn {
            display: inline-block;
            padding: 11px 20px;
            font-size: 13px;
            font-weight: 700;
            text-transform: uppercase;
            letter-spacing: 0.7px;
            border-radius: 5px;
            cursor: pointer;
            border: none;
            border-bottom: 3px solid rgba(0, 0, 0, 0.28);
            color: #ffffff;
            text-align: center;
            transition: opacity 0.2s, transform 0.1s;
            width: 100%;
            margin: 5px 0;
        }
        
        .btn-primary {
            width: 100%;
            height: 42px;
            border: none;
            border-bottom: 3px solid #1b5e20;
            outline: none;
            color: #fff;
            background: #2e7d32;
            cursor: pointer;
            position: relative;
            z-index: 0;
            border-radius: 5px;
        }
        
        .btn-primary:hover {
            background: #1b5e20;
        }
        
        .btn-primary:active {
            color: #ffffff;
            font-weight: bold;
            transform: translateY(1px);
        }
        
        .btn-secondary {
            background-color: #455a64;
        }
        .btn-secondary:hover {
            background-color: #37474f;
        }
        .btn-warning {
            background-color: #ef6c00;
            color: #ffffff;
        }
        .btn-warning:hover {
            background-color: #e65100;
        }
        /* Cores industriais dedicadas por função */
        .btn-creditos {
            background-color: #37474f; /* Grafite industrial */
        }
        .btn-creditos:hover {
            background-color: #263238;
        }
        .btn-dark-mode {
            background-color: #ef6c00; /* Âmbar segurança */
        }
        .btn-dark-mode:hover {
            background-color: #e65100;
        }
        .text-danger {
            color: #c62828;
            font-size: 13px;
            font-weight: 700;
            margin-top: 10px;
            min-height: 18px;
        }
/* Estilos para o rodapé */
.footer {
    width: 100%;
    background-color: #1e272e;
    border-top: 3px solid #e65100;
    color: #cfd8dc;
    text-align: center;
    padding: 10px 0;
    font-size: 12px;
    font-weight: 600;
    margin-top: auto; /* Mantém o rodapé no final da página sem fixá-lo */
}
            .theme-toggle {
            margin: 4px 0 0 0;
        }
    </style>
    </head>
    <body>
        <div class="login-container" role="form" aria-labelledby="loginTitle">
            <h2 id="loginTitle" class="login-title">Acesso Supervisório</h2>
            <form action="/login" method="post"
                onsubmit="storeLoginData(event)">
                <div class="form-group">
                    <input type="text" name="username" class="form-control"
                        placeholder="Usuário" aria-label="Usuário" required>
                </div>
                <div class="form-group">
                    <input type="password" name="password" class="form-control"
                        placeholder="Senha" aria-label="Senha" required>
                </div>
                <button type="submit" class="btn btn-primary"
                    aria-label="Entrar">Entrar</button>
                <div id="errorMessage" class="text-danger"></div>
            </form>
            <button onclick="window.location.href='/creditos'"
                class="btn btn-creditos"
                aria-label="Ir para créditos">Créditos</button>

            <div class="theme-toggle">
                <button onclick="toggleDarkMode()" class="btn btn-dark-mode"
                    aria-label="Ativar modo noturno">🌓 Modo Noturno</button>
            </div>
        </div>
        <div class="footer" role="contentinfo">
            <p>Aplicação desenvolvida pela Turma de Informática Para Internet
                Trilhas de Futuro 2024</p>
            <p>Instrutor: Lenon Yuri</p>
        </div>

        <script>
        document.addEventListener('DOMContentLoaded', function () {
            // Verifica o parâmetro ?login_failed=true diretamente no navegador (dispensa String::replace no ESP32)
            var params = new URLSearchParams(window.location.search);
            if (params.has('login_failed')) {
                document.getElementById('errorMessage').textContent = 'Usuário ou senha incorretos. Tente novamente.';
            }

            // Sincroniza o modo noturno salvo no navegador entre as telas
            if (localStorage.getItem('energizar_theme') === 'night') {
                document.body.classList.add('dark-mode');
            }
        });

        function storeLoginData(event) {
            event.preventDefault();
            var username = document.querySelector('input[name="username"]').value;
            // Não registra a senha no console do navegador por segurança
            console.log('Enviando autenticação para usuário: ' + username);
            event.target.submit();
        }

        function toggleDarkMode() {
            document.body.classList.toggle('dark-mode');
            var modoEscuro = document.body.classList.contains('dark-mode');
            localStorage.setItem('energizar_theme', modoEscuro ? 'night' : 'light');
        }
    </script>
    </body>
</html>
        )rawliteral";

        // Envia a resposta ao cliente diretamente da memória Flash (zero alocação de String na Heap)
        request->send(200, "text/html", html); }); // Responde ao pedido HTTP GET com o conteúdo HTML da página de login
}
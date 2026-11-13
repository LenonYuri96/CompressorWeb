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

        <title>Login</title>
        <style>
        /* Estilos gerais da página */
/* Estilos gerais para o corpo da página */
body {
    font-family: Arial, sans-serif;
    background-color: #f0f2f5;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: space-between;
    min-height: 100vh; /* Garante que o conteúdo ocupe pelo menos 100% da altura da viewport */
    margin: 0;
    padding: 0;
}
        .dark-mode {
            background-color: #181818;
            color: #f0f0f0;
        }
        .login-container {
    background-color: #ffffff;
    padding: 20px;
    border-radius: 8px;
    box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
    text-align: center;
    width: 100%;
    max-width: 400px;
    margin: auto; /* Garante que o conteúdo fique centralizado */
            }
        .dark-mode .login-container {
            background-color: #2a2a2a;
        }
        .login-title {
            font-size: 24px;
            font-family: "Playfair Display", serif;
            margin-bottom: 20px;
            background-image: linear-gradient(to bottom, rgb(255, 221, 0), rgba(246, 15, 15, 0.975), rgb(249, 216, 28));
            background-clip: text;
            -webkit-background-clip: text; 
            -webkit-text-fill-color: transparent;
            color: rgb(242, 95, 22);
            font-weight: bold;
        }
        .dark-mode .login-title {
            color: #1e90ff;
        }
        .form-group {
            margin-bottom: 15px;
        }
        .form-control {
            width: 100%;
            padding: 10px;
            border: 1px solid #ced4da;
            border-radius: 5px;
            box-sizing: border-box;
        }
        .dark-mode .form-control {
            background-color: #333;
            color: #fff;
            border-color: #555;
        }
        .btn {
            display: inline-block;
            padding: 10px 20px;
            font-size: 16px;
            border-radius: 5px;
            cursor: pointer;
            border: none;
            color: #ffffff;
            text-align: center;
            transition: background-color 0.3s, transform 0.3s;
            width: 100%;
            margin: 5px 0;
        }
        
        .btn-primary {
            width: 100%;
            height: 38px;
            border: none;
            outline: none;
            color: #fff;
            background: #111;
            cursor: pointer;
            position: relative;
            z-index: 0;
            border-radius: 5px;
        }
        
        .btn-primary:before {
            content: '';
            background: linear-gradient(45deg, #ff0000, #ff7300, #fffb00, #48ff00, #00ffd5, #002bff, #7a00ff, #ff00c8, #ff0000);
            position: absolute;
            top: -2px;
            left:-2px;
            background-size: 600%;
            z-index: -1;
            filter: blur(7px);
            width: calc(100% + 4px);
            height: calc(100% + 4px);
            animation: glowing 20s linear infinite;
            opacity: 0;
            transition: opacity .3s ease-in-out;
            border-radius: 10px;
        }
        
        .btn-primary:active {
            color: #ffffff;
            font-weight: bold;
        }
        
        .btn-primary:active:after {
            background: transparent;
        }
        
        .btn-primary:hover:before {
            opacity: 1;
        }
        
        .btn-primary:after {
            z-index: -1;
            content: '';
            position: absolute;
            width: 100%;
            height: 100%;
            background: #111;
            left: 0;
            top: 0;
            border-radius: 10px;
        }
        
        @keyframes glowing {
            0% { background-position: 0 0; }
            50% { background-position: 400% 0; }
            100% { background-position: 0 0; }
        }
        
        .btn-secondary {
            background-color: #6c757d;
        }
        .btn-secondary:hover {
            background-color: #5a6268;
            transform: scale(1.05);
        }
        .btn-warning {
            background-color: #ffc107;
            color: #212529;
        }
        .btn-warning:hover {
            background-color: #e0a800;
            transform: scale(1.05);
        }
        /* Cores adicionais para garantir que cada botão tenha uma cor diferente */
        .btn-creditos {
            background-color: #ea0b0b; /* Vermelho */
        }
        .btn-creditos:hover {
            background-color: #7d0909;
            transform: scale(1.05);
        }
        .btn-dark-mode {
            background-color: rgb(234, 161, 15); /* Laranja/Dourado */
        }
        .btn-dark-mode:hover {
            background-color: #8e5204;
            transform: scale(1.05);
        }
        .text-danger {
            color: #dc3545;
            margin-top: 10px;
        }
/* Estilos para o rodapé */
.footer {
    width: 100%;
    background-color: #ee641a;
    color: white;
    text-align: center;
    padding: 10px 0;
    font-size: 14px;
    margin-top: auto; /* Mantém o rodapé no final da página sem fixá-lo */
}
            .theme-toggle {
            margin: 10px 0;
        }
    </style>
    </head>
    <body>
        <div class="login-container" role="form" aria-labelledby="loginTitle">
            <h2 id="loginTitle" class="login-title">Faça o Login</h2>
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
                    aria-label="Ativar modo noturno">Modo Noturno</button>
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
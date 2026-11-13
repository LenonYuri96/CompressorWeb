#include <ESPAsyncWebServer.h> // Biblioteca para criar e gerenciar um servidor web assíncrono no ESP32
#include "paginaserro.h"       // Cabeçalho para a configuração específica das páginas de erro

/**
 * Gera uma página de erro em HTML com base nos parâmetros fornecidos.
 *
 * @param title O título da página de erro.
 * @param message A mensagem de erro a ser exibida.
 * @param linkText O texto do link de navegação.
 * @param linkHref O URL para o qual o link deve redirecionar.
 * @return O código HTML da página de erro.
 */
String generateErrorPage(const String &title, const String &message, const String &linkText, const String &linkHref)
{
    // Retorna o código HTML da página de erro formatado com os parâmetros fornecidos
    return R"rawliteral(
<!DOCTYPE html> <!-- Declaração do tipo de documento HTML5, que informa ao navegador que o conteúdo é HTML5 -->
<html lang="pt-br"> <!-- Define o idioma da página como Português Brasileiro -->
<head>
    <meta charset="UTF-8"> <!-- Define a codificação de caracteres como UTF-8 para suportar caracteres especiais e internacionais -->
    <meta name="viewport" content="width=device-width, initial-scale=1.0"> <!-- Configura o viewport para dispositivos móveis, ajustando o layout ao tamanho da tela -->
    <title>)rawliteral" +
           title + R"rawliteral(</title> <!-- Define o título da página que aparece na aba do navegador -->
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif; /* Define a fonte padrão industrial */
            display: flex; /* Usa o modelo de layout flexbox para centralizar o conteúdo */
            justify-content: center; /* Alinha o conteúdo horizontalmente no centro da página */
            align-items: center; /* Alinha o conteúdo verticalmente no centro da página */
            min-height: 100vh; /* Define a altura da página como 100% da altura da viewport (tela visível) */
            margin: 0; /* Remove as margens padrão do navegador */
            background-color: #d6dde4; /* Fundo chapa aço industrial */
            background-image: linear-gradient(rgba(30, 39, 46, 0.05) 1px, transparent 1px),
                              linear-gradient(90deg, rgba(30, 39, 46, 0.05) 1px, transparent 1px);
            background-size: 20px 20px;
            color: #1e272e;
        }
        body.night-mode {
            background-color: #0d1117;
            background-image: linear-gradient(rgba(255, 255, 255, 0.03) 1px, transparent 1px),
                              linear-gradient(90deg, rgba(255, 255, 255, 0.03) 1px, transparent 1px);
            color: #e6edf3;
        }
        .container {
            text-align: center; /* Centraliza o texto dentro do container */
            background-color: #eceff1; /* Chapa metálica clara */
            padding: 24px 22px; /* Adiciona espaçamento interno ao redor do conteúdo do container */
            border-radius: 8px; /* Arredonda os cantos do container */
            border: 2px solid #90a4ae; /* Borda metálica do gabinete */
            border-top: 6px solid #c62828; /* Faixa superior de alarme/bloqueio */
            box-shadow: 0 10px 28px rgba(15, 23, 30, 0.22), inset 0 1px 0 #ffffff; /* Profundidade industrial */
            max-width: 460px; /* Define a largura máxima do container */
            width: 92%; /* Responsivo para telas móveis */
        }
        body.night-mode .container {
            background-color: #161b22;
            border-color: #30363d;
            border-top-color: #c62828;
            box-shadow: 0 12px 32px rgba(0, 0, 0, 0.7);
        }
        h1 {
            color: #ff5252; /* Destaque de alarme sobre barra escura */
            background-color: #1e272e;
            padding: 10px 12px;
            border-radius: 5px;
            border-left: 5px solid #c62828;
            border-right: 5px solid #c62828;
            font-size: 20px; /* Tamanho proporcional ao painel */
            font-weight: 800;
            text-transform: uppercase;
            letter-spacing: 1px;
            margin-bottom: 16px; /* Adiciona espaço abaixo do título */
        }
        p {
            color: #37474f; /* Texto técnico escuro */
            font-size: 15px; /* Legibilidade em campo */
            font-weight: 600;
            line-height: 1.5;
            margin-bottom: 22px; /* Adiciona espaço abaixo do parágrafo */
        }
        body.night-mode p {
            color: #cfd8dc;
        }
        a {
            text-decoration: none; /* Remove o sublinhado dos links para estilizar como botões */
            color: #ffffff; /* Texto branco */
            background-color: #0277bd; /* Botão azul petróleo industrial */
            font-weight: 700; /* Negrito */
            font-size: 13px;
            text-transform: uppercase;
            letter-spacing: 0.6px;
            padding: 11px 22px; /* Espaçamento interno do botão */
            border: none;
            border-bottom: 3px solid rgba(0, 0, 0, 0.28); /* Efeito tecla de IHM */
            border-radius: 5px; /* Cantos técnicos */
            display: inline-block; /* Comportamento de bloco inline */
            transition: opacity 0.2s, transform 0.1s; /* Transição suave */
        }
        a:hover {
            opacity: 0.9;
        }
        a:active {
            transform: translateY(1px);
        }
    </style>
</head>
<body>
    <div class="container"> <!-- Contêiner centralizado que contém o conteúdo da página de erro -->
        <h1>)rawliteral" +
           title + R"rawliteral(</h1> <!-- Exibe o título da página de erro -->
        <p>)rawliteral" +
           message + R"rawliteral(</p> <!-- Exibe a mensagem de erro fornecida -->
        <a href=")rawliteral" +
           linkHref + R"rawliteral(">)rawliteral" + linkText + R"rawliteral(</a> <!-- Link para navegação para a URL fornecida -->
    </div>
    <script>
        if (localStorage.getItem('energizar_theme') === 'night') {
            document.body.classList.add('night-mode');
        }
    </script>
</body>
</html>
)rawliteral";
}

/**
 * Configura a rota para a página de erro "Usuário Já Logado".
 *
 * @param server A instância do servidor web assíncrono.
 */
void setupUsuarioJaLogadoPage(AsyncWebServer &server)
{
    // Define a rota "/usuario-ja-logado" para responder com a página de erro "Usuário Já Logado"
    server.on("/usuario-ja-logado", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // Envia uma resposta HTTP com o código de status 403 (Forbidden) e o HTML gerado pela função generateErrorPage
        request->send(403, "text/html", generateErrorPage(
            "Usuário Já Logado", // Título da página de erro
            "Há um usuário atualmente logado. Por favor, tente novamente mais tarde.", // Mensagem de erro a ser exibida
            "Acessar Tela Inicial", // Texto do link
            "/" // URL para o link
        )); });
}

/**
 * Configura a rota para a página de erro "Credenciais Inválidas".
 *
 * @param server A instância do servidor web assíncrono.
 */
void setupCredenciaisInvalidasPage(AsyncWebServer &server)
{
    // Define a rota "/credenciais-invalidas" para responder com a página de erro "Credenciais Inválidas"
    server.on("/credenciais-invalidas", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // Envia uma resposta HTTP com o código de status 401 (Unauthorized) e o HTML gerado pela função generateErrorPage
        request->send(401, "text/html", generateErrorPage(
            "Credenciais Inválidas", // Título da página de erro
            "Credenciais inválidas. Por favor, verifique seu nome de usuário e senha.", // Mensagem de erro a ser exibida
            "Voltar ao Login", // Texto do link
            "/" // URL para o link
        )); });
}

/**
 * Configura a rota para a página de erro "Acesso Inválido".
 *
 * @param server A instância do servidor web assíncrono.
 */
void setupAcessoInvalidoPage(AsyncWebServer &server)
{
    // Define a rota "/acesso-invalido" para responder com a página de erro "Acesso Inválido"
    server.on("/acesso-invalido", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // Envia uma resposta HTTP com o código de status 403 (Forbidden) e o HTML gerado pela função generateErrorPage
        request->send(403, "text/html", generateErrorPage(
            "Acesso Inválido", // Título da página de erro
            "Você não tem permissão para acessar esta página. Por favor, faça login para continuar.", // Mensagem de erro a ser exibida
            "Voltar à Página Inicial", // Texto do link
            "/" // URL para o link
        )); });
}

/**
 * Configura a rota para a página de erro "Página Não Encontrada".
 *
 * @param server A instância do servidor web assíncrono.
 */
void setupNotFoundPage(AsyncWebServer &server)
{
    // Define a rota padrão para responder com a página de erro "Página Não Encontrada"
    server.onNotFound([](AsyncWebServerRequest *request)
                      {
        // Envia uma resposta HTTP com o código de status 404 (Not Found) e o HTML gerado pela função generateErrorPage
        request->send(404, "text/html", generateErrorPage(
            "Página Não Encontrada", // Título da página de erro
            "Essa página não existe. Verifique o URL ou volte à página inicial.", // Mensagem de erro a ser exibida
            "Voltar à Página Inicial", // Texto do link
            "/" // URL para o link
        )); });
}
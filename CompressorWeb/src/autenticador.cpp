/**
 * ============================================================================
 * PROJETO: EnergizAR - Sistema de Automacao e Monitoramento ESP32
 * ARQUIVO: src/autenticador.cpp
 * ----------------------------------------------------------------------------
 * DESCRICAO DO SUBSISTEMA:
 *   Controlador de autenticacao, gerenciamento de sessoes HTTP e protecao
 *   de acesso as rotas criticas do sistema de compressores industriais.
 *
 * ARQUITETURA DE SEGURANCA (v2.0.0 - HARDENING):
 *   1. CREDENCIAIS COM HASH SHA-256 + SALT UNICO EM NVS (Preferences):
 *      Substitui credenciais em texto puro por resumo criptografico SHA-256
 *      com aceleracao por hardware via mbedTLS (mbedtls/md.h) e salt unico
 *      gerado via TRNG no primeiro boot. Zero exposicao de senhas no Serial.
 *
 *   2. GERACAO DE SESSION ID POR TRNG DE HARDWARE (128 BITS):
 *      Utiliza esp_random() para gerar identificadores de 32 caracteres HEX
 *      imprevisiveis, eliminando a vulnerabilidade do antigo String(millis()).
 *
 *   3. PARSER ROBUSTO DE COOKIES HTTP:
 *      Isola corretamente o campo "session_id=" em cabecalhos com multiplos
 *      cookies separados por ";", evitando falhas de autenticacao e colisoes
 *      de prefixo de nome de cookie.
 *
 *   4. GESTAO INTELIGENTE DE INATIVIDADE E FIM DO BLOQUEIO ETERNO:
 *      - Verifica expiracao antes de rejeitar logins (/usuario-ja-logado),
 *        encerrando o travamento permanente quando o usuario fecha a aba.
 *      - Renova o tempo de atividade em rotas interativas (/dashboard, /toggle)
 *        e ignora rotas de polling em segundo plano (/motor-state, /status),
 *        garantindo que operadores ativos nao sejam desconectados e que
 *        terminais ociosos bloqueiem automaticamente.
 *      - Corrige o bug de dupla resposta HTTP ao expirar dentro de
 *        isAuthenticated(), usando limpeza silenciosa de estado.
 *
 *   5. PROTECAO CONTRA FORCA BRUTA E TIMING ATTACKS:
 *      Implementa comparacao em tempo constante de tokens/hashes e bloqueio
 *      temporario progressivo apos falhas consecutivas de login.
 *
 * HISTORICO DE REVISOES:
 *   v1.0.0 - Autenticacao basica com millis() e credenciais fixas.
 *   v2.0.0 - Hardening completo: SHA-256 + Salt NVS, esp_random(),
 *            parser seguro de cookies, rate-limiting e fim do double-send.
 * ============================================================================
 */

// ============================================================================
// SECAO 1: BIBLIOTECAS, CABECALHOS E DEPENDENCIAS CRIPTOGRAFICAS
// ============================================================================

#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <mbedtls/md.h>
#include <string.h>

#include "autenticador.h"

// ============================================================================
// SECAO 2: CONSTANTES DE POLITICA DE SEGURANCA E ARMAZENAMENTO NVS
// ============================================================================

/** Tempo maximo de inatividade da sessao em milissegundos (2 minutos) */
const unsigned long SESSION_TIMEOUT = 2UL * 60UL * 1000UL;

/** Numero maximo de tentativas falhas antes do bloqueio anti-forca-bruta */
static const uint8_t MAX_TENTATIVAS_FALHAS = 5;

/** Duracao do bloqueio temporario apos exceder tentativas falhas (30 s) */
static const unsigned long BLOQUEIO_FORCA_BRUTA_MS = 30000UL;

/** Namespace NVS dedicado ao cofre de credenciais do sistema */
static const char *NVS_NAMESPACE_AUTH = "energizar_auth";

/** Chaves de armazenamento seguro dentro da particao NVS */
static const char *NVS_KEY_USER = "admin_user";
static const char *NVS_KEY_SALT = "admin_salt";
static const char *NVS_KEY_HASH = "admin_hash";

/** Credenciais iniciais gravadas apenas no primeiro boot (NVS virgem) */
static const char *DEFAULT_ADMIN_USER = "admin";
static const char *DEFAULT_ADMIN_PASS = "admin123";

// ============================================================================
// SECAO 3: VARIAVEIS GLOBAIS EXPORTADAS E CONTADORES DE AUDITORIA
// ============================================================================

/** Identificador criptografico da sessao ativa (32 chars HEX = 128 bits) */
String sessionId = "";

/** Indica se ha uma sessao logicamente ativa no servidor */
bool userLoggedIn = false;

/** Nome do usuario atualmente autenticado */
String loggedInUser = "";

/** Timestamp (millis) da ultima interacao valida do usuario */
unsigned long lastActivityTime = 0UL;

// ----------------------------------------------------------------------------
// Estado Interno de Seguranca, Rate-Limiting e Telemetria
// ----------------------------------------------------------------------------

/** Cache em RAM das credenciais carregadas da NVS (evita leituras repetidas) */
static bool authInicializado = false;
static String cachedAdminUser = "";
static String cachedSaltHex = "";
static String cachedPasswordHashHex = "";

/** Contadores de protecao contra ataque de forca bruta */
static uint8_t tentativasFalhasConsecutivas = 0;
static unsigned long bloqueioLoginAte = 0UL;

/** Metricas de auditoria de seguranca */
static unsigned long totalLoginsSucesso = 0UL;
static unsigned long totalLoginsFalhos = 0UL;
static unsigned long totalSessoesExpiradas = 0UL;
static unsigned long totalAcessosNegados = 0UL;

// ============================================================================
// SECAO 4: UTILITARIOS CRIPTOGRAFICOS E COMPARACAO EM TEMPO CONSTANTE
// ============================================================================

/**
 * compararTempoConstante()
 * ----------------------------------------------------------------------------
 * Compara duas Strings em tempo constante para mitigar ataques de canal
 * lateral (timing attack) na verificacao de tokens de sessao e hashes.
 *
 * @param a Primeira string a comparar.
 * @param b Segunda string a comparar.
 * @return true se possuirem exatamente o mesmo comprimento e conteudo.
 */
static bool compararTempoConstante(const String &a, const String &b)
{
    size_t lenA = a.length();
    size_t lenB = b.length();

    if (lenA == 0 || lenA != lenB)
    {
        return false;
    }

    volatile uint8_t diff = 0;
    for (size_t i = 0; i < lenA; i++)
    {
        diff |= (uint8_t)(a[i] ^ b[i]);
    }

    return (diff == 0);
}

/**
 * gerarHexAleatorio128Bits()
 * ----------------------------------------------------------------------------
 * Utiliza o gerador de numeros aleatorios por hardware (TRNG) do ESP32 para
 * produzir uma string hexadecimal de 128 bits (32 caracteres).
 */
static String gerarHexAleatorio128Bits()
{
    uint32_t r0 = esp_random();
    uint32_t r1 = esp_random();
    uint32_t r2 = esp_random();
    uint32_t r3 = esp_random();

    char buffer[33];
    snprintf(buffer, sizeof(buffer), "%08lx%08lx%08lx%08lx",
             (unsigned long)r0,
             (unsigned long)r1,
             (unsigned long)r2,
             (unsigned long)r3);

    return String(buffer);
}

/**
 * calcularHashSaltedSHA256()
 * ----------------------------------------------------------------------------
 * Calcula o resumo SHA-256 da combinacao "salt:senha" utilizando a camada
 * generica mbedtls/md.h (compativel com ESP-IDF 4.x e 5.x).
 *
 * @param salt  Salt aleatorio armazenado na NVS.
 * @param senha Senha em texto plano recebida para validacao ou gravacao.
 * @return String hexadecimal minuscula de 64 caracteres (256 bits).
 */
static String calcularHashSaltedSHA256(const String &salt, const String &senha)
{
    String payload = salt + ":" + senha;
    uint8_t shaResult[32];
    memset(shaResult, 0, sizeof(shaResult));

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);

    const mbedtls_md_info_t *mdInfo = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (mdInfo == nullptr)
    {
        mbedtls_md_free(&ctx);
        Serial.println("[AUTH] ERRO CRITICO: Algoritmo SHA-256 indisponivel.");
        return "";
    }

    if (mbedtls_md_setup(&ctx, mdInfo, 0) != 0)
    {
        mbedtls_md_free(&ctx);
        Serial.println("[AUTH] ERRO CRITICO: Falha ao configurar contexto mbedTLS.");
        return "";
    }

    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx,
                      (const unsigned char *)payload.c_str(),
                      payload.length());
    mbedtls_md_finish(&ctx, shaResult);
    mbedtls_md_free(&ctx);

    // Converte os 32 bytes binarios para representacao HEX de 64 caracteres
    char hexOut[65];
    for (int i = 0; i < 32; i++)
    {
        snprintf(&hexOut[i * 2], 3, "%02x", shaResult[i]);
    }
    hexOut[64] = '\0';

    return String(hexOut);
}

// ============================================================================
// SECAO 5: COFRE DE CREDENCIAIS EM MEMORIA NVS (PREFERENCES)
// ============================================================================

/**
 * garantirInicializacaoAuth()
 * ----------------------------------------------------------------------------
 * Inicializa de forma automatica e idempotente (lazy-init) as credenciais
 * armazenadas na particao NVS do ESP32. Caso seja o primeiro boot do modulo,
 * gera um salt unico por hardware e grava o hash SHA-256 da senha inicial.
 */
static void garantirInicializacaoAuth()
{
    if (authInicializado)
    {
        return;
    }

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE_AUTH, false))
    {
        Serial.println("[AUTH] AVISO: Falha ao abrir NVS. Usando fallback em RAM.");
        cachedAdminUser = DEFAULT_ADMIN_USER;
        cachedSaltHex = gerarHexAleatorio128Bits();
        cachedPasswordHashHex = calcularHashSaltedSHA256(cachedSaltHex, DEFAULT_ADMIN_PASS);
        authInicializado = true;
        return;
    }

    bool temUsuario = prefs.isKey(NVS_KEY_USER);
    bool temSalt = prefs.isKey(NVS_KEY_SALT);
    bool temHash = prefs.isKey(NVS_KEY_HASH);

    if (!temUsuario || !temSalt || !temHash)
    {
        cachedAdminUser = DEFAULT_ADMIN_USER;
        cachedSaltHex = gerarHexAleatorio128Bits();
        cachedPasswordHashHex = calcularHashSaltedSHA256(cachedSaltHex, DEFAULT_ADMIN_PASS);

        prefs.putString(NVS_KEY_USER, cachedAdminUser);
        prefs.putString(NVS_KEY_SALT, cachedSaltHex);
        prefs.putString(NVS_KEY_HASH, cachedPasswordHashHex);

        Serial.println("==================================================");
        Serial.println("[AUTH] Cofre NVS inicializado com hash SHA-256 + Salt.");
        Serial.println("[AUTH] RECOMENDACAO: Altere a senha padrao em producao.");
        Serial.println("==================================================");
    }
    else
    {
        cachedAdminUser = prefs.getString(NVS_KEY_USER, DEFAULT_ADMIN_USER);
        cachedSaltHex = prefs.getString(NVS_KEY_SALT, "");
        cachedPasswordHashHex = prefs.getString(NVS_KEY_HASH, "");

        Serial.println("[AUTH] Credenciais seguras carregadas da NVS com sucesso.");
    }

    prefs.end();
    authInicializado = true;
}

/**
 * setupAutenticador()
 * ----------------------------------------------------------------------------
 * Ponto de entrada opcional para inicializacao explicita no setup() do
 * firmware. Tambem eh chamada automaticamente na primeira requisicao web.
 */
void setupAutenticador()
{
    garantirInicializacaoAuth();
}

/**
 * alterarCredenciaisAdmin()
 * ----------------------------------------------------------------------------
 * Atualiza usuario e/ou senha de acesso gerando um novo salt aleatorio e
 * novo resumo SHA-256 diretamente na particao NVS.
 *
 * @param novoUsuario Novo nome de usuario (minimo 3 caracteres).
 * @param novaSenha   Nova senha em texto plano (minimo 6 caracteres).
 * @return true se gravado com sucesso na NVS; false se parametros invalidos.
 */
bool alterarCredenciaisAdmin(const String &novoUsuario, const String &novaSenha)
{
    garantirInicializacaoAuth();

    String userLimpo = novoUsuario;
    userLimpo.trim();

    if (userLimpo.length() < 3 || novaSenha.length() < 6)
    {
        Serial.println("[AUTH] Recusada troca de credenciais: usuario (<3) ou senha (<6) curtos.");
        return false;
    }

    String novoSalt = gerarHexAleatorio128Bits();
    String novoHash = calcularHashSaltedSHA256(novoSalt, novaSenha);

    if (novoHash.length() != 64)
    {
        return false;
    }

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE_AUTH, false))
    {
        return false;
    }

    prefs.putString(NVS_KEY_USER, userLimpo);
    prefs.putString(NVS_KEY_SALT, novoSalt);
    prefs.putString(NVS_KEY_HASH, novoHash);
    prefs.end();

    cachedAdminUser = userLimpo;
    cachedSaltHex = novoSalt;
    cachedPasswordHashHex = novoHash;

    Serial.println("[AUTH] Credenciais atualizadas e protegidas com novo Salt SHA-256.");
    return true;
}

// ============================================================================
// SECAO 6: GERENCIAMENTO DE SESSAO, EXPIRACAO E PARSING DE COOKIES
// ============================================================================

/**
 * limparSessaoInterna()
 * ----------------------------------------------------------------------------
 * Encerra a sessao em memoria RAM de maneira silenciosa (sem enviar resposta
 * HTTP). Fundamental para evitar dupla chamada de request->send() quando a
 * expiracao eh detectada dentro de isAuthenticated().
 */
static void limparSessaoInterna()
{
    userLoggedIn = false;
    loggedInUser = "";
    sessionId = "";
    lastActivityTime = 0UL;
}

/**
 * verificarEExpirarSessaoSeNecessario()
 * ----------------------------------------------------------------------------
 * Avalia se a sessao marcada como ativa ultrapassou SESSION_TIMEOUT.
 * Utiliza aritmetica modular de unsigned long segura contra rollover de millis().
 *
 * @return true se a sessao estava expirada e foi encerrada; false caso contrario.
 */
static bool verificarEExpirarSessaoSeNecessario()
{
    if (!userLoggedIn)
    {
        return false;
    }

    unsigned long agora = millis();
    if ((agora - lastActivityTime) >= SESSION_TIMEOUT)
    {
        Serial.printf("[AUTH] Sessao do usuario '%s' expirou por inatividade (%lu s).\n",
                      loggedInUser.c_str(), SESSION_TIMEOUT / 1000UL);
        limparSessaoInterna();
        totalSessoesExpiradas++;
        return true;
    }

    return false;
}

/**
 * rotaDePollingAutomatico()
 * ----------------------------------------------------------------------------
 * Identifica endpoints consultados automaticamente por setInterval() no
 * navegador. Essas rotas NAO devem renovar lastActivityTime, garantindo que
 * sessoes abandonadas expirem de fato sem interferencia do polling em tela.
 */
static bool rotaDePollingAutomatico(const String &url)
{
    return (url == "/motor-state" ||
            url == "/status" ||
            url == "/api/telemetria" ||
            url == "/check-auth");
}

/**
 * extrairSessionIdDoCookie()
 * ----------------------------------------------------------------------------
 * Percorre o cabecalho Cookie separando pares "chave=valor" delimitados por
 * ";" para extrair exclusivamente o valor exato de "session_id".
 *
 * @param cookieHeader Conteudo integral do cabecalho HTTP "Cookie".
 * @return Valor limpo do session_id, ou string vazia se ausente.
 */
static String extrairSessionIdDoCookie(const String &cookieHeader)
{
    int start = 0;
    int totalLen = cookieHeader.length();

    while (start < totalLen)
    {
        int sep = cookieHeader.indexOf(';', start);
        if (sep == -1)
        {
            sep = totalLen;
        }

        String par = cookieHeader.substring(start, sep);
        par.trim();

        if (par.startsWith("session_id="))
        {
            String valor = par.substring(11); // comprimento de "session_id="
            valor.trim();
            return valor;
        }

        start = sep + 1;
    }

    return "";
}

/**
 * generateSessionId()
 * ----------------------------------------------------------------------------
 * Gera um novo identificador de sessao criptograficamente aleatorio (128 bits)
 * usando o gerador de entropia de hardware do ESP32.
 *
 * @return String hexadecimal de 32 caracteres.
 */
String generateSessionId()
{
    String newSessionId = gerarHexAleatorio128Bits();
    Serial.println("[AUTH] Novo token de sessao de 128 bits gerado via TRNG.");
    return newSessionId;
}

/**
 * updateLastActivityTime()
 * ----------------------------------------------------------------------------
 * Atualiza o timestamp de ultima atividade do usuario para estender a sessao.
 */
void updateLastActivityTime()
{
    lastActivityTime = millis();
}

/**
 * isAuthenticated()
 * ----------------------------------------------------------------------------
 * Verifica de forma segura se a requisicao HTTP pertence a uma sessao ativa:
 *   1. Garante inicializacao da camada de autenticacao;
 *   2. Checa se ha sessao ativa e se nao expirou por inatividade;
 *   3. Extrai o token do cookie "session_id" ignorando demais cookies;
 *   4. Compara o token em tempo constante;
 *   5. Renova lastActivityTime se nao for endpoint de polling passivo.
 *
 * @param request Ponteiro para a requisicao AsyncWebServerRequest.
 * @return true se autenticada e vigente; false caso contrario.
 */
bool isAuthenticated(AsyncWebServerRequest *request)
{
    garantirInicializacaoAuth();

    if (request == nullptr)
    {
        return false;
    }

    // Se nao ha sessao ativa ou se expirou por inatividade, rejeita sem enviar resposta
    if (!userLoggedIn || sessionId.length() == 0)
    {
        totalAcessosNegados++;
        return false;
    }

    if (verificarEExpirarSessaoSeNecessario())
    {
        totalAcessosNegados++;
        return false;
    }

    if (!request->hasHeader("Cookie"))
    {
        totalAcessosNegados++;
        return false;
    }

    String cookieHeader = request->header("Cookie");
    String sessionValue = extrairSessionIdDoCookie(cookieHeader);

    if (sessionValue.length() == 0)
    {
        totalAcessosNegados++;
        return false;
    }

    if (compararTempoConstante(sessionValue, sessionId))
    {
        // Renova o prazo de inatividade apenas em acoes reais do operador
        if (!rotaDePollingAutomatico(request->url()))
        {
            updateLastActivityTime();
        }
        return true;
    }

    Serial.println("[AUTH] Token de sessao do cookie invalido ou divergente.");
    totalAcessosNegados++;
    return false;
}

// ============================================================================
// SECAO 7: CONTROLADORES HTTP DE LOGIN, LOGOUT E REDIRECIONAMENTO
// ============================================================================

/**
 * handleLogin()
 * ----------------------------------------------------------------------------
 * Processa a solicitacao POST /login validando:
 *   - Janela de bloqueio anti-forca-bruta;
 *   - Expiracao previa de sessoes abandonadas (fim do bloqueio eterno);
 *   - Verificacao de usuario e senha via hash SHA-256 com salt;
 *   - Emissao de cookie HttpOnly + SameSite=Lax com token de 128 bits.
 */
void handleLogin(AsyncWebServerRequest *request)
{
    garantirInicializacaoAuth();

    unsigned long agora = millis();

    // ----------------------------------------------------------------------
    // 1. Verifica bloqueio temporario por excesso de falhas (Rate Limiting)
    // ----------------------------------------------------------------------
    if (bloqueioLoginAte != 0UL)
    {
        if ((long)(agora - bloqueioLoginAte) < 0)
        {
            unsigned long restanteSeg = (bloqueioLoginAte - agora) / 1000UL + 1UL;
            Serial.printf("[AUTH] Tentativa bloqueada por rate-limit (%lu s restantes).\n",
                          restanteSeg);
            request->redirect("/?login_failed=true");
            return;
        }
        else
        {
            // Janela de penalidade encerrada
            bloqueioLoginAte = 0UL;
            tentativasFalhasConsecutivas = 0;
        }
    }

    // ----------------------------------------------------------------------
    // 2. Extrai parametros do formulario POST sem imprimir a senha no Serial
    // ----------------------------------------------------------------------
    if (!request->hasParam("username", true) || !request->hasParam("password", true))
    {
        Serial.println("[AUTH] Requisicao de login incompleta (parametros ausentes).");
        request->redirect("/?login_failed=true");
        return;
    }

    String username = request->getParam("username", true)->value();
    String password = request->getParam("password", true)->value();
    username.trim();

    if (username.length() == 0 || password.length() == 0)
    {
        request->redirect("/?login_failed=true");
        return;
    }

    Serial.printf("[AUTH] Tentativa de autenticacao recebida para usuario: '%s'.\n",
                  username.c_str());

    // ----------------------------------------------------------------------
    // 3. Validacao criptografica: Usuario + Hash(Salt + ":" + Senha)
    // ----------------------------------------------------------------------
    String hashRecebido = calcularHashSaltedSHA256(cachedSaltHex, password);
    bool usuarioValido = compararTempoConstante(username, cachedAdminUser);
    bool senhaValida = compararTempoConstante(hashRecebido, cachedPasswordHashHex);

    if (usuarioValido && senhaValida)
    {
        // Zera contador de falhas ao comprovar credencial legitima
        tentativasFalhasConsecutivas = 0;
        bloqueioLoginAte = 0UL;

        // CORRECAO DO BLOQUEIO ETERNO: limpa sessao anterior caso ja tenha expirado
        verificarEExpirarSessaoSeNecessario();

        // Verifica se o mesmo navegador ja possui o cookie da sessao em vigor
        bool mesmoClienteAtivo = false;
        if (userLoggedIn && request->hasHeader("Cookie"))
        {
            String cookieVal = extrairSessionIdDoCookie(request->header("Cookie"));
            mesmoClienteAtivo = compararTempoConstante(cookieVal, sessionId);
        }

        if (!userLoggedIn || mesmoClienteAtivo)
        {
            userLoggedIn = true;
            loggedInUser = username;
            sessionId = generateSessionId();
            updateLastActivityTime();
            totalLoginsSucesso++;

            Serial.printf("[AUTH] Login autorizado para '%s' (total logins: %lu).\n",
                          loggedInUser.c_str(), totalLoginsSucesso);

            AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
            response->addHeader("Set-Cookie",
                                "session_id=" + sessionId + "; Path=/; HttpOnly; SameSite=Lax");
            response->addHeader("Cache-Control", "no-store");
            response->addHeader("Location", "/dashboard");
            request->send(response);
        }
        else
        {
            Serial.printf("[AUTH] Login recusado para '%s': ha sessao ativa vigente.\n",
                          username.c_str());
            request->redirect("/usuario-ja-logado");
        }
    }
    else
    {
        // Registra falha e aplica penalidade apos limite configurado
        totalLoginsFalhos++;
        tentativasFalhasConsecutivas++;

        Serial.printf("[AUTH] Falha de autenticacao para '%s' (%u/%u).\n",
                      username.c_str(),
                      tentativasFalhasConsecutivas,
                      MAX_TENTATIVAS_FALHAS);

        if (tentativasFalhasConsecutivas >= MAX_TENTATIVAS_FALHAS)
        {
            bloqueioLoginAte = agora + BLOQUEIO_FORCA_BRUTA_MS;
            Serial.printf("[AUTH] ALERTA: Limite de falhas atingido. "
                          "Login bloqueado por %lu segundos.\n",
                          BLOQUEIO_FORCA_BRUTA_MS / 1000UL);
        }

        request->redirect("/?login_failed=true");
    }
}

/**
 * handleLogout()
 * ----------------------------------------------------------------------------
 * Encerra explicitamente a sessao do usuario, invalida o cookie no navegador
 * e redireciona para a pagina inicial de login.
 */
void handleLogout(AsyncWebServerRequest *request)
{
    if (userLoggedIn)
    {
        Serial.printf("[AUTH] Logout executado para o usuario: '%s'.\n",
                      loggedInUser.c_str());
        limparSessaoInterna();

        AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
        response->addHeader("Set-Cookie",
                            "session_id=; expires=Thu, 01 Jan 1970 00:00:00 GMT; Path=/; HttpOnly; SameSite=Lax");
        response->addHeader("Cache-Control", "no-store");
        response->addHeader("Location", "/");
        request->send(response);
    }
    else
    {
        Serial.println("[AUTH] Solicitacao de logout sem sessao ativa.");
        AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
        response->addHeader("Set-Cookie",
                            "session_id=; expires=Thu, 01 Jan 1970 00:00:00 GMT; Path=/; HttpOnly; SameSite=Lax");
        response->addHeader("Location", "/");
        request->send(response);
    }
}

/**
 * notAuthenticated()
 * ----------------------------------------------------------------------------
 * Redireciona o usuario nao autenticado para a pagina inicial de login.
 */
void notAuthenticated(AsyncWebServerRequest *request)
{
    Serial.println("[AUTH] Redirecionando para login: usuario nao autenticado.");
    request->redirect("/");
}

/**
 * handleDashboard()
 * ----------------------------------------------------------------------------
 * Funcao legada mantida para compatibilidade de ligacao.
 */
void handleDashboard(AsyncWebServerRequest *request)
{
    if (isAuthenticated(request))
    {
        updateLastActivityTime();
        request->send(200, "text/plain", "Bem-vindo ao Dashboard!");
    }
    else
    {
        notAuthenticated(request);
    }
}

// ============================================================================
// SECAO 8: DIAGNOSTICO DE SEGURANCA E TELEMETRIA DE SESSAO
// ============================================================================

/**
 * dumpEstadoAutenticacao()
 * ----------------------------------------------------------------------------
 * Imprime relatorio de auditoria de seguranca no console serial sem expor
 * tokens de sessao, salts ou hashes criptograficos.
 */
void dumpEstadoAutenticacao()
{
    garantirInicializacaoAuth();
    verificarEExpirarSessaoSeNecessario();

    unsigned long tempoRestanteSeg = 0UL;
    if (userLoggedIn)
    {
        unsigned long decorrido = millis() - lastActivityTime;
        if (decorrido < SESSION_TIMEOUT)
        {
            tempoRestanteSeg = (SESSION_TIMEOUT - decorrido) / 1000UL;
        }
    }

    Serial.println("==============================================");
    Serial.println("[DIAGNOSTICO] Subsistema de Autenticacao");
    Serial.println("==============================================");
    Serial.printf("  Sessao ativa      : %s\n", userLoggedIn ? "SIM" : "NAO");
    Serial.printf("  Usuario logado    : %s\n",
                  userLoggedIn ? loggedInUser.c_str() : "(nenhum)");
    Serial.printf("  Expira em         : %lu s\n", tempoRestanteSeg);
    Serial.printf("  Logins sucesso    : %lu\n", totalLoginsSucesso);
    Serial.printf("  Logins falhos     : %lu\n", totalLoginsFalhos);
    Serial.printf("  Sessoes expiradas : %lu\n", totalSessoesExpiradas);
    Serial.printf("  Acessos negados   : %lu\n", totalAcessosNegados);
    Serial.printf("  Rate-limit ativo  : %s\n",
                  (bloqueioLoginAte != 0UL && (long)(millis() - bloqueioLoginAte) < 0) ? "SIM" : "NAO");
    Serial.println("==============================================");
}

// ============================================================================
// FIM DO ARQUIVO: src/autenticador.cpp
// ENERGIZAR - SUBSISTEMA DE AUTENTICACAO CRIPTOGRAFICA E CONTROLE DE SESSAO
// ============================================================================
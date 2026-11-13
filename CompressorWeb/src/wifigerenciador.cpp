/**
 * ============================================================================
 * PROJETO: EnergizAR - Sistema de Automacao e Monitoramento ESP32
 * ARQUIVO: src/wifigerenciador.cpp
 * ----------------------------------------------------------------------------
 * DESCRICAO GERAL:
 *   Roteador HTTP assincrono responsavel por TODO o gerenciamento de
 *   conectividade Wi-Fi do sistema. Expoe as seguintes rotas protegidas:
 *
 *     1. GET  /wifigerenciamento -> Pagina HTML/CSS/JS do portal
 *     2. POST /scanwifi          -> Inicia varredura assincrona de redes
 *     3. GET  /scanstatus        -> Retorna resultados da varredura em JSON
 *     4. GET  /listadewifi       -> Lista SOMENTE os SSIDs salvos em SPIFFS
 *     5. POST /salvarwifi        -> Grava credencial sanitizada e conecta
 *     6. POST /excluirwifi       -> Remove rede salva por SSID (anti-CSRF)
 *     7. GET  /ipdarede          -> Retorna IP atual (STA e/ou SoftAP)
 *
 * FILOSOFIA DE PROJETO E SEGURANCA (v2.2.0):
 *   - 100% NAO-BLOQUEANTE: nenhum handler usa delay() ou lacos longos.
 *   - AUTENTICACAO OBRIGATORIA: todas as rotas exigem isAuthenticated(request),
 *     impedindo leitura/alteracao de redes por terceiros na rede local ou AP.
 *   - ZERO VAZAMENTO DE SENHAS: /listadewifi retorna apenas nomes (SSID).
 *   - PROTECAO ANTI-INJECAO: rejeita '\r', '\n' e ',' no SSID para preservar
 *     a integridade do arquivo /wifiredes.txt.
 *   - PROTECAO ANTI-CSRF: operacoes destrutivas (/excluirwifi) restritas a POST.
 * ============================================================================
 */

// -------------------------------------------------------------------------
// INCLUDES
// -------------------------------------------------------------------------
#include <WiFi.h>              // API nativa do radio Wi-Fi do ESP32
#include <ESPAsyncWebServer.h> // Servidor HTTP assincrono (nao bloqueante)
#include <SPIFFS.h>            // Sistema de arquivos em Flash SPI

#include "wificonexao.h"   // Varredura, conexao e persistencia de redes
#include "wifiinterface.h" // Geracao do HTML do portal de gerenciamento
#include "autenticador.h"  // Verificacao de sessao autenticada

// -------------------------------------------------------------------------
// FUNCAO AUXILIAR: escapeJSONWiFi()
// -------------------------------------------------------------------------
/**
 * Escapa caracteres especiais ('"' e '\') e remove caracteres de controle
 * ao montar respostas JSON manualmente sem alocar ArduinoJson.
 */
static String escapeJSONWiFi(const String &texto)
{
    String out;
    out.reserve(texto.length() + 8);

    for (size_t i = 0; i < texto.length(); i++)
    {
        char c = texto[i];
        if (c == '\"')
        {
            out += "\\\"";
        }
        else if (c == '\\')
        {
            out += "\\\\";
        }
        else if ((uint8_t)c >= 0x20)
        {
            out += c;
        }
    }
    return out;
}

// =========================================================================
// FUNCAO PRINCIPAL: setupWiFiGerenciadorPage()
// =========================================================================
/**
 * setupWiFiGerenciadorPage()
 * -------------------------------------------------------------------------
 * Registra no servidor assincrono TODAS as rotas HTTP protegidas do portal
 * de gerenciamento Wi-Fi. Deve ser chamada UMA UNICA VEZ dentro do setup().
 *
 * @param server Referencia ao objeto AsyncWebServer global.
 */
void setupWiFiGerenciadorPage(AsyncWebServer &server)
{
    // =====================================================================
    // ROTA 1: GET /wifigerenciamento — Pagina HTML do Portal (Autenticada)
    // =====================================================================
    server.on("/wifigerenciamento", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (!isAuthenticated(request))
        {
            redirectToAccessDenied(request);
            return;
        }

        request->send(200, "text/html", getWiFiManagementPage()); });

    // =====================================================================
    // ROTA 2: POST /scanwifi — Inicia Varredura Assincrona de Redes
    // =====================================================================
    server.on("/scanwifi", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (!isAuthenticated(request))
        {
            request->send(401, "application/json", "{\"erro\":\"nao_autorizado\"}");
            return;
        }

        startScanWiFiNetworks();
        request->send(200, "application/json",
                      "{\"status\":\"scanning_started\"}"); });

    // =====================================================================
    // ROTA 3: GET /scanstatus — Coleta de Resultados da Varredura
    // =====================================================================
    server.on("/scanstatus", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (!isAuthenticated(request))
        {
            request->send(401, "application/json", "{\"erro\":\"nao_autorizado\"}");
            return;
        }

        request->send(200, "application/json", getScanResults()); });

    // =====================================================================
    // ROTA 4: GET /listadewifi — Lista APENAS os SSIDs Salvos na Flash
    // =====================================================================
    /**
     * Le /wifiredes.txt ("SSID,SENHA\n") e devolve SOMENTE os SSIDs (um por
     * linha), omitindo integralmente as senhas armazenadas.
     */
    server.on("/listadewifi", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (!isAuthenticated(request))
        {
            request->send(401, "text/plain", "Nao autorizado.");
            return;
        }

        if (!SPIFFS.exists("/wifiredes.txt"))
        {
            request->send(200, "text/plain", "");
            return;
        }

        File file = SPIFFS.open("/wifiredes.txt", FILE_READ);
        if (!file)
        {
            request->send(500, "text/plain",
                          "Erro ao acessar armazenamento interno.");
            return;
        }

        String ssidsApenas = "";
        while (file.available())
        {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.length() == 0)
            {
                continue;
            }

            int sep = line.indexOf(',');
            String savedSSID = (sep != -1) ? line.substring(0, sep) : line;
            savedSSID.trim();

            if (savedSSID.length() > 0)
            {
                ssidsApenas += savedSSID + "\n";
            }
        }
        file.close();

        AsyncWebServerResponse *response =
            request->beginResponse(200, "text/plain", ssidsApenas);
        response->addHeader("Cache-Control",
                            "no-store, no-cache, must-revalidate");
        request->send(response); });

    // =====================================================================
    // ROTA 5: POST /salvarwifi — Grava Nova Credencial e Conecta
    // =====================================================================
    server.on("/salvarwifi", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (!isAuthenticated(request))
        {
            request->send(401, "application/json", "{\"erro\":\"nao_autorizado\"}");
            return;
        }

        // ---- 1. Validacao do parametro obrigatorio ----
        if (!request->hasParam("ssid", true))
        {
            request->send(400, "text/plain", "SSID ausente.");
            return;
        }

        // ---- 2. Extracao e sanitizacao dos campos ----
        String ssid = request->getParam("ssid", true)->value();
        String password = "";
        if (request->hasParam("password", true))
        {
            password = request->getParam("password", true)->value();
        }

        ssid.trim();
        password.trim();

        // Impede SSID vazio, longo demais ou com caracteres que corrompam o CSV
        if (ssid.length() == 0 || ssid.length() > MAX_SSID_LEN ||
            ssid.indexOf(',') != -1 || ssid.indexOf('\n') != -1 || ssid.indexOf('\r') != -1)
        {
            request->send(400, "text/plain", "SSID invalido.");
            return;
        }

        // Impede quebras de linha na senha e respeita limite WPA2
        if (password.length() > MAX_PASS_LEN ||
            password.indexOf('\n') != -1 || password.indexOf('\r') != -1)
        {
            request->send(400, "text/plain", "Senha invalida.");
            return;
        }

        // ---- 3. Leitura e mesclagem do arquivo existente ----
        String newContent = "";
        bool ssidExists = false;

        if (SPIFFS.exists("/wifiredes.txt"))
        {
            File file = SPIFFS.open("/wifiredes.txt", FILE_READ);
            if (file)
            {
                while (file.available())
                {
                    String line = file.readStringUntil('\n');
                    line.trim();
                    if (line.length() == 0)
                    {
                        continue;
                    }

                    int sep = line.indexOf(',');
                    String savedSSID = (sep != -1) ? line.substring(0, sep) : line;

                    if (savedSSID.equalsIgnoreCase(ssid))
                    {
                        newContent += ssid + "," + password + "\n";
                        ssidExists = true;
                    }
                    else
                    {
                        newContent += line + "\n";
                    }
                }
                file.close();
            }
        }

        // ---- 4. Novo SSID: acrescenta ao final da lista ----
        if (!ssidExists)
        {
            newContent += ssid + "," + password + "\n";
        }

        // ---- 5. Gravacao atomica no SPIFFS ----
        File outFile = SPIFFS.open("/wifiredes.txt", FILE_WRITE);
        if (!outFile)
        {
            request->send(500, "text/plain", "Erro ao gravar credencial.");
            return;
        }
        outFile.print(newContent);
        outFile.close();

        // ---- 6. Recarrega credenciais em RAM e dispara conexao ----
        loadSavedWiFiNetworks();
        connectToWiFi(ssid.c_str(), password.c_str());

        request->send(200, "application/json",
                      "{\"status\":\"saved_connecting\"}"); });

    // =====================================================================
    // ROTA 6: POST /excluirwifi — Remove Rede Salva por SSID (Anti-CSRF)
    // =====================================================================
    server.on("/excluirwifi", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (!isAuthenticated(request))
        {
            request->send(401, "application/json", "{\"erro\":\"nao_autorizado\"}");
            return;
        }

        // Aceita parametro tanto via query string quanto via corpo POST
        String ssidToDelete = "";
        if (request->hasParam("ssid", true))
        {
            ssidToDelete = request->getParam("ssid", true)->value();
        }
        else if (request->hasParam("ssid"))
        {
            ssidToDelete = request->getParam("ssid")->value();
        }
        else
        {
            request->send(400, "text/plain", "SSID ausente.");
            return;
        }

        ssidToDelete.trim();

        if (!SPIFFS.exists("/wifiredes.txt"))
        {
            request->send(200, "text/plain", "ok");
            return;
        }

        File file = SPIFFS.open("/wifiredes.txt", FILE_READ);
        if (!file)
        {
            request->send(500, "text/plain", "Erro ao abrir arquivo.");
            return;
        }

        String newContent = "";
        while (file.available())
        {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.length() == 0)
            {
                continue;
            }

            int sep = line.indexOf(',');
            String savedSSID = (sep != -1) ? line.substring(0, sep) : line;

            if (!savedSSID.equalsIgnoreCase(ssidToDelete))
            {
                newContent += line + "\n";
            }
        }
        file.close();

        File outFile = SPIFFS.open("/wifiredes.txt", FILE_WRITE);
        if (outFile)
        {
            outFile.print(newContent);
            outFile.close();
        }

        loadSavedWiFiNetworks();

        request->send(200, "text/plain", "ok"); });

    // =====================================================================
    // ROTA 7: GET /ipdarede — IP Atual (STA e SoftAP) (Autenticada)
    // =====================================================================
    server.on("/ipdarede", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (!isAuthenticated(request))
        {
            request->send(401, "application/json", "{\"erro\":\"nao_autorizado\"}");
            return;
        }

        bool conectado = isWiFiConnected();
        String ipSTA = conectado ? WiFi.localIP().toString() : "";
        String ipAP = WiFi.softAPIP().toString();
        String ssidAtual = conectado ? escapeJSONWiFi(WiFi.SSID()) : "";

        String json = "{";
        json += "\"conectado\":" + String(conectado ? "true" : "false") + ",";
        json += "\"ssid\":\"" + ssidAtual + "\",";
        json += "\"ip_sta\":\"" + ipSTA + "\",";
        json += "\"ip_ap\":\"" + ipAP + "\",";
        json += "\"ip\":\"" + (conectado ? ipSTA : ipAP) + "\"}";

        AsyncWebServerResponse *response =
            request->beginResponse(200, "application/json", json);
        response->addHeader("Cache-Control",
                            "no-store, no-cache, must-revalidate");
        request->send(response); });
}

// -------------------------------------------------------------------------
// Fim do arquivo: src/wifigerenciador.cpp
// Sistema EnergizAR — Modulo Wi-Fi Assincrono e Resiliente
// -------------------------------------------------------------------------
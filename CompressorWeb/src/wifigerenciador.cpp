/**
 * ============================================================================
 * PROJETO: EnergizAR - Sistema de Automação e Monitoramento ESP32
 * ARQUIVO: src/wifigerenciador.cpp
 * ----------------------------------------------------------------------------
 * DESCRIÇÃO GERAL:
 *   Roteador HTTP assíncrono responsável por TODO o gerenciamento de
 *   conectividade Wi-Fi do sistema. Expõe as seguintes rotas ao cliente:
 *
 *     1. GET  /wifigerenciamento -> Página HTML/CSS/JS do portal
 *     2. POST /scanwifi          -> Inicia varredura assíncrona de redes
 *     3. GET  /scanstatus        -> Retorna resultados da varredura em JSON
 *     4. GET  /listadewifi       -> Lista redes salvas em SPIFFS (texto)
 *     5. POST /salvarwifi        -> Grava credencial e conecta
 *     6. GET  /excluirwifi       -> Remove rede salva por SSID
 *     7. GET  /ipdarede          -> Retorna IP atual (STA e/ou SoftAP)
 *
 * FILOSOFIA DE PROJETO:
 *   - 100% NÃO-BLOQUEANTE: nenhum handler usa delay() ou laços longos.
 *     Todas as operações de I/O (varredura, conexão, SPIFFS) são delegadas
 *     aos módulos wificonexao.h / wifiinterface.h.
 *   - TASK WATCHDOG TIMER (TWDT) preservado — handlers respondem em < 100 ms.
 *   - Persistência atômica em SPIFFS no formato "SSID,SENHA\n" (uma por linha).
 *   - Comparação case-insensitive de SSID em todas as operações CRUD.
 *
 * DEPENDÊNCIAS EXTERNAS (implementadas em outros módulos):
 *   - wificonexao.h   : startScanWiFiNetworks(), getScanResults(),
 *                       loadSavedWiFiNetworks(), connectToWiFi(),
 *                       isWiFiConnected()
 *   - wifiinterface.h : getWiFiManagementPage() -> HTML embarcado
 *
 * AUTOR: Equipe de Firmware EnergizAR
 * VERSÃO: 2.1.0 (Produção Industrial Estável)
 * ============================================================================
 */

// -------------------------------------------------------------------------
// INCLUDES
// -------------------------------------------------------------------------
#include <WiFi.h>              // API nativa do rádio Wi-Fi do ESP32
#include <ESPAsyncWebServer.h> // Servidor HTTP assíncrono (não bloqueante)
#include <SPIFFS.h>            // Sistema de arquivos em Flash SPI

#include "wificonexao.h"   // Varredura, conexão e persistência de redes
#include "wifiinterface.h" // Geração do HTML do portal de gerenciamento

// =========================================================================
// FUNÇÃO PRINCIPAL: setupWiFiGerenciadorPage()
// =========================================================================
/**
 * setupWiFiGerenciadorPage()
 * -------------------------------------------------------------------------
 * Registra no servidor assíncrono TODAS as rotas HTTP do portal de
 * gerenciamento Wi-Fi. Deve ser chamada UMA ÚNICA VEZ dentro do setup()
 * do sketch principal — chamadas repetidas criariam handlers duplicados.
 *
 * @param server  Referência ao objeto AsyncWebServer global.
 */
void setupWiFiGerenciadorPage(AsyncWebServer &server)
{
    // =====================================================================
    // ROTA 1: /wifigerenciamento — Página HTML do Portal
    // =====================================================================
    /**
     * Serve o documento HTML completo (CSS e JS embarcados) do portal de
     * gerenciamento. O conteúdo é gerado dinamicamente por
     * getWiFiManagementPage(), permitindo injeção de estado atual
     * (ex.: IP, redes salvas) sem requisições adicionais.
     */
    server.on("/wifigerenciamento", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/html", getWiFiManagementPage()); });

    // =====================================================================
    // ROTA 2: /scanwifi — Inicia Varredura Assíncrona de Redes
    // =====================================================================
    /**
     * Dispara a varredura Wi-Fi sem travar a thread do servidor web.
     * A varredura real é executada em background pelo módulo wificonexao.h.
     * O cliente deve consultar /scanstatus periodicamente para obter
     * os resultados quando estiverem prontos.
     */
    server.on("/scanwifi", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        startScanWiFiNetworks();
        request->send(200, "application/json",
                      "{\"status\":\"scanning_started\"}"); });

    // =====================================================================
    // ROTA 3: /scanstatus — Coleta de Resultados da Varredura
    // =====================================================================
    /**
     * Retorna o JSON com a lista de redes detectadas (SSID + RSSI +
     * criptografia). Se a varredura ainda estiver em andamento, o
     * módulo wificonexao.h devolve a lista parcial ou um marcador.
     */
    server.on("/scanstatus", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "application/json", getScanResults()); });

    // =====================================================================
    // ROTA 4: /listadewifi — Lista Redes Salvas na Memória Flash
    // =====================================================================
    /**
     * Lê e devolve o conteúdo bruto do arquivo /wifiredes.txt, que contém
     * as credenciais persistidas no formato "SSID,SENHA\n" (uma por linha).
     * Se o arquivo não existir, responde com conteúdo vazio (sem erro) —
     * isso simplifica o tratamento no front-end.
     */
    server.on("/listadewifi", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // Caso ainda não exista arquivo, responde vazio
        if (!SPIFFS.exists("/wifiredes.txt"))
        {
            request->send(200, "text/plain", "");
            return;
        }

        // Abre o arquivo em modo leitura
        File file = SPIFFS.open("/wifiredes.txt", FILE_READ);
        if (!file)
        {
            // Falha de I/O real — devolve erro ao cliente
            request->send(500, "text/plain",
                          "Erro ao acessar armazenamento interno.");
            return;
        }

        // Lê o conteúdo integral e fecha o descritor imediatamente
        String networks = file.readString();
        file.close();

        request->send(200, "text/plain", networks); });

    // =====================================================================
    // ROTA 5: /salvarwifi — Grava Nova Credencial e Conecta
    // =====================================================================
    /**
     * Recebe via POST (application/x-www-form-urlencoded) os campos:
     *   - ssid      (obrigatório)
     *   - password  (opcional — redes abertas permitidas)
     *
     * Comportamento:
     *   1. Valida presença e formato do SSID.
     *   2. Lê o arquivo existente e reescreve-o com a nova entrada,
     *      substituindo a senha caso o SSID já exista.
     *   3. Persiste no SPIFFS.
     *   4. Recarrega a tabela de redes em RAM.
     *   5. Dispara a conexão não-bloqueante com o novo SSID.
     */
    server.on("/salvarwifi", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        // ---- 1. Validação do parâmetro obrigatório ----
        if (!request->hasParam("ssid", true))
        {
            request->send(400, "text/plain", "SSID ausente.");
            return;
        }

        // ---- 2. Extração e sanitização dos campos ----
        String ssid     = request->getParam("ssid", true)->value();
        String password = "";
        if (request->hasParam("password", true))
        {
            password = request->getParam("password", true)->value();
        }

        ssid.trim();
        password.trim();

        if (ssid.length() == 0)
        {
            request->send(400, "text/plain", "SSID invalido.");
            return;
        }

        // ---- 3. Leitura e mesclagem do arquivo existente ----
        String newContent = "";
        bool   ssidExists = false;

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
                        continue; // Ignora linhas em branco
                    }

                    // Extrai o SSID da linha (antes da vírgula)
                    int    sep       = line.indexOf(',');
                    String savedSSID = (sep != -1) ? line.substring(0, sep) : line;

                    // Se for o mesmo SSID, atualiza a senha
                    if (savedSSID.equalsIgnoreCase(ssid))
                    {
                        newContent += ssid + "," + password + "\n";
                        ssidExists  = true;
                    }
                    else
                    {
                        newContent += line + "\n"; // Preserva a linha original
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

        // ---- 5. Gravação atômica no SPIFFS ----
        File outFile = SPIFFS.open("/wifiredes.txt", FILE_WRITE);
        if (!outFile)
        {
            request->send(500, "text/plain", "Erro ao gravar credencial.");
            return;
        }
        outFile.print(newContent);
        outFile.close();

        // ---- 6. Recarrega credenciais em RAM e dispara conexão ----
        loadSavedWiFiNetworks();
        connectToWiFi(ssid.c_str(), password.c_str());

        request->send(200, "application/json",
                      "{\"status\":\"saved_connecting\"}"); });

    // =====================================================================
    // ROTA 6: /excluirwifi — Remove Rede Salva por SSID
    // =====================================================================
    /**
     * Remove uma rede persistida cujo SSID coincida (case-insensitive)
     * com o parâmetro de query "ssid". Se o arquivo não existir, devolve
     * "ok" para idempotência. Após a remoção, recarrega a tabela em RAM.
     */
    server.on("/excluirwifi", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // ---- 1. Validação do parâmetro obrigatório ----
        if (!request->hasParam("ssid"))
        {
            request->send(400, "text/plain", "SSID ausente.");
            return;
        }

        String ssidToDelete = request->getParam("ssid")->value();
        ssidToDelete.trim();

        // ---- 2. Arquivo inexistente: nada a remover ----
        if (!SPIFFS.exists("/wifiredes.txt"))
        {
            request->send(200, "text/plain", "ok");
            return;
        }

        // ---- 3. Leitura e filtro do arquivo existente ----
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

            int    sep       = line.indexOf(',');
            String savedSSID = (sep != -1) ? line.substring(0, sep) : line;

            // Preserva todas as linhas exceto a que será excluída
            if (!savedSSID.equalsIgnoreCase(ssidToDelete))
            {
                newContent += line + "\n";
            }
        }
        file.close();

        // ---- 4. Regravação do arquivo filtrado ----
        File outFile = SPIFFS.open("/wifiredes.txt", FILE_WRITE);
        if (outFile)
        {
            outFile.print(newContent);
            outFile.close();
        }

        // ---- 5. Recarrega a tabela em RAM ----
        loadSavedWiFiNetworks();

        request->send(200, "text/plain", "ok"); });

    // =====================================================================
    // ROTA 7: /ipdarede — IP Atual (STA e SoftAP)
    // =====================================================================
    /**
     * Retorna um JSON completo com informações de conectividade:
     *   - conectado  (bool)   : há conexão STA ativa?
     *   - ssid       (string) : SSID da rede conectada (STA)
     *   - ip_sta     (string) : IP obtido como cliente (STA)
     *   - ip_ap      (string) : IP do ponto de acesso (SoftAP)
     *   - ip         (string) : IP "principal" (STA se conectado, senão AP)
     *
     * O campo "ip" é mantido para compatibilidade com clientes legados que
     * esperam apenas a string simples em vez do objeto expandido.
     */
    server.on("/ipdarede", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        bool   conectado = isWiFiConnected();
        String ipSTA     = conectado ? WiFi.localIP().toString() : "";
        String ipAP      = WiFi.softAPIP().toString();
        String ssidAtual = conectado ? WiFi.SSID() : "";

        // Montagem do JSON com escape manual (evita alocação de ArduinoJson)
        String json = "{";
        json += "\"conectado\":" + String(conectado ? "true" : "false") + ",";
        json += "\"ssid\":\""    + ssidAtual + "\",";
        json += "\"ip_sta\":\""  + ipSTA     + "\",";
        json += "\"ip_ap\":\""   + ipAP      + "\",";
        json += "\"ip\":\""      + (conectado ? ipSTA : ipAP) + "\"}";

        // Cabeçalho anti-cache: garante leitura sempre atualizada
        AsyncWebServerResponse *response =
            request->beginResponse(200, "application/json", json);
        response->addHeader("Cache-Control",
                            "no-store, no-cache, must-revalidate");
        request->send(response); });
}

// -------------------------------------------------------------------------
// Fim do arquivo: src/wifigerenciador.cpp
// Sistema EnergizAR — Módulo Wi-Fi Assíncrono e Resiliente
// -------------------------------------------------------------------------
#include "wificonexao.h"
#include <SPIFFS.h>
#include <vector>

// -------------------------------------------------------------------------
// 1. Definições Globais do Ponto de Acesso (AP) e Endereçamento
// -------------------------------------------------------------------------
const char *ap_ssid = "EnergizAR";
const char *ap_password = "12345678";

IPAddress local_ip(192, 168, 26, 7);
IPAddress gateway(192, 168, 26, 1);
IPAddress subnet(255, 255, 255, 0);

bool isAPMode = false;
bool scanning = false;
int scanResults = 0;
unsigned long lastScan = 0;
const unsigned long scanInterval = WIFI_SCAN_INTERVAL_MS;

// -------------------------------------------------------------------------
// 2. Variáveis Internas da Máquina de Estados e Buffer de Credenciais
// -------------------------------------------------------------------------
static std::vector<CredencialWiFi> redesSalvas;
static int indiceRedeAtual = 0;
static wifi_system_state_t estadoAtual = WIFI_STATE_IDLE;
static unsigned long inicioTentativaConexao = 0;
static unsigned long ultimoCheckReconexao = 0;

// -------------------------------------------------------------------------
// 3. Configuração do Ponto de Acesso Industrial (Modo Dual WIFI_AP_STA)
// -------------------------------------------------------------------------
void setupAP()
{
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(local_ip, gateway, subnet);

    if (WiFi.softAP(ap_ssid, ap_password))
    {
        Serial.printf("[WIFI-AP] Ativo: '%s' | IP: %s\n", ap_ssid, WiFi.softAPIP().toString().c_str());
        isAPMode = true;
    }
    else
    {
        Serial.println("[WIFI-AP] Falha ao inicializar o ponto de acesso.");
        isAPMode = false;
    }
}

void enterAPMode()
{
    setupAP();
}

// -------------------------------------------------------------------------
// 4. Conexão Assíncrona e Não-Bloqueante
// -------------------------------------------------------------------------
void connectToWiFi(const char *targetSSID, const char *targetPass)
{
    if (!targetSSID || strlen(targetSSID) == 0)
    {
        Serial.println("[WIFI] SSID informado e invalido.");
        return;
    }

    Serial.printf("[WIFI] Conectando de forma nao-bloqueante a: %s\n", targetSSID);
    WiFi.disconnect(false);
    WiFi.begin(targetSSID, (targetPass ? targetPass : ""));

    inicioTentativaConexao = millis();
    estadoAtual = WIFI_STATE_CONNECTING;
}

void desconectarWiFi()
{
    WiFi.disconnect(true);
    estadoAtual = WIFI_STATE_DISCONNECTED;
    Serial.println("[WIFI] Interface de estacao desconectada manualmente.");
}

// -------------------------------------------------------------------------
// 5. Carregamento de Redes Salvas a partir do SPIFFS
// -------------------------------------------------------------------------
void loadSavedWiFiNetworks()
{
    redesSalvas.clear();
    indiceRedeAtual = 0;

    if (!SPIFFS.exists("/wifiredes.txt"))
    {
        Serial.println("[WIFI] Nenhum arquivo /wifiredes.txt detectado na flash.");
        return;
    }

    File file = SPIFFS.open("/wifiredes.txt", FILE_READ);
    if (!file)
    {
        Serial.println("[WIFI] Erro ao abrir /wifiredes.txt para leitura.");
        return;
    }

    while (file.available())
    {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0)
            continue;

        int sep = line.indexOf(',');
        if (sep != -1)
        {
            CredencialWiFi cred;
            cred.ssid = line.substring(0, sep);
            cred.pass = line.substring(sep + 1);
            redesSalvas.push_back(cred);
        }
    }
    file.close();

    Serial.printf("[WIFI] Carregadas %d credenciais de rede da memoria flash.\n", redesSalvas.size());

    if (!redesSalvas.empty())
    {
        indiceRedeAtual = 0;
        connectToWiFi(redesSalvas[0].ssid.c_str(), redesSalvas[0].pass.c_str());
    }
}

// -------------------------------------------------------------------------
// 6. Máquina de Estados Executada no Loop Principal
// -------------------------------------------------------------------------
void processarConexaoWiFi()
{
    unsigned long agora = millis();

    switch (estadoAtual)
    {
    case WIFI_STATE_CONNECTING:
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.printf("[WIFI] Conexao estabelecida com exito! IP: %s\n", WiFi.localIP().toString().c_str());
            estadoAtual = WIFI_STATE_CONNECTED;
        }
        else if (agora - inicioTentativaConexao >= WIFI_CONNECT_TIMEOUT_MS)
        {
            Serial.println("[WIFI] Tempo esgotado para a rede atual.");
            if (!redesSalvas.empty() && indiceRedeAtual + 1 < (int)redesSalvas.size())
            {
                indiceRedeAtual++;
                connectToWiFi(redesSalvas[indiceRedeAtual].ssid.c_str(), redesSalvas[indiceRedeAtual].pass.c_str());
            }
            else
            {
                Serial.println("[WIFI] Ciclo de redes finalizado. Aguardando intervalo de retry...");
                estadoAtual = WIFI_STATE_RETRY_WAIT;
                ultimoCheckReconexao = agora;
            }
        }
        break;

    case WIFI_STATE_CONNECTED:
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("[WIFI] Queda de enlace detectada. Entrando em modo recuperacao...");
            estadoAtual = WIFI_STATE_RETRY_WAIT;
            ultimoCheckReconexao = agora;
        }
        break;

    case WIFI_STATE_RETRY_WAIT:
        if (agora - ultimoCheckReconexao >= WIFI_RECONNECT_INTERVAL_MS)
        {
            if (!redesSalvas.empty())
            {
                indiceRedeAtual = 0;
                connectToWiFi(redesSalvas[0].ssid.c_str(), redesSalvas[0].pass.c_str());
            }
            else
            {
                ultimoCheckReconexao = agora;
            }
        }
        break;

    case WIFI_STATE_IDLE:
    case WIFI_STATE_DISCONNECTED:
    default:
        break;
    }
}

// -------------------------------------------------------------------------
// 7. Varredura Assíncrona de Redes
// -------------------------------------------------------------------------
void startScanWiFiNetworks()
{
    if (!scanning && (millis() - lastScan > scanInterval))
    {
        scanning = true;
        WiFi.scanNetworks(true);
        lastScan = millis();
    }
}

String getScanResults()
{
    int n = WiFi.scanComplete();
    if (n < 0)
        return "[]";

    scanResults = n;
    String json = "[";
    for (int i = 0; i < n; i++)
    {
        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
        if (i < n - 1)
            json += ",";
    }
    json += "]";

    WiFi.scanDelete();
    scanning = false;
    return json;
}

// -------------------------------------------------------------------------
// 8. Diagnóstico, Qualidade do Sinal e Telemetria
// -------------------------------------------------------------------------
bool isWiFiConnected()
{
    return (WiFi.status() == WL_CONNECTED);
}

int getWiFiSignalQuality()
{
    if (!isWiFiConnected())
        return 0;
    int rssi = WiFi.RSSI();
    if (rssi <= -100)
        return 0;
    if (rssi >= -50)
        return 100;
    return 2 * (rssi + 100);
}

const char *getWiFiStateString()
{
    switch (estadoAtual)
    {
    case WIFI_STATE_CONNECTING:
        return "CONECTANDO";
    case WIFI_STATE_CONNECTED:
        return "CONECTADO";
    case WIFI_STATE_RETRY_WAIT:
        return "AGUARDANDO_RECONEXAO";
    case WIFI_STATE_DISCONNECTED:
        return "DESCONECTADO";
    default:
        return "OCIOSO";
    }
}

String getWiFiStatusJSON()
{
    String json = "{";
    json += "\"conectado\":" + String(isWiFiConnected() ? "true" : "false") + ",";
    json += "\"modo_ap\":" + String(isAPMode ? "true" : "false") + ",";
    json += "\"ip_local\":\"" + (isWiFiConnected() ? WiFi.localIP().toString() : "") + "\",";
    json += "\"ip_ap\":\"" + WiFi.softAPIP().toString() + "\",";
    json += "\"qualidade_sinal\":" + String(getWiFiSignalQuality()) + ",";
    json += "\"estado\":\"" + String(getWiFiStateString()) + "\"}";
    return json;
}
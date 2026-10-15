#ifndef WIFICONEXAO_H
#define WIFICONEXAO_H

#include <Arduino.h>
#include <WiFi.h>

// -------------------------------------------------------------------------
// 1. Constantes e Configurações de Temporização
// -------------------------------------------------------------------------
#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64
#define WIFI_CONNECT_TIMEOUT_MS 10000
#define WIFI_RECONNECT_INTERVAL_MS 30000
#define WIFI_SCAN_INTERVAL_MS 5000

// -------------------------------------------------------------------------
// 2. Máquina de Estados da Conectividade Wi-Fi
// -------------------------------------------------------------------------
typedef enum
{
    WIFI_STATE_IDLE,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_RETRY_WAIT
} wifi_system_state_t;

// -------------------------------------------------------------------------
// 3. Estrutura de Credenciais de Redes Armazenadas
// -------------------------------------------------------------------------
struct CredencialWiFi
{
    String ssid;
    String pass;
};

// -------------------------------------------------------------------------
// 4. Declaração de Variáveis Globais de Estado e Rede (Exportadas)
// -------------------------------------------------------------------------
extern const char *ap_ssid;
extern const char *ap_password;
extern bool isAPMode;
extern bool scanning;
extern unsigned long lastScan;
extern const unsigned long scanInterval;
extern int scanResults;

extern IPAddress local_ip;
extern IPAddress gateway;
extern IPAddress subnet;

// -------------------------------------------------------------------------
// 5. Inicialização e Ciclo de Vida do Subsistema Wi-Fi
// -------------------------------------------------------------------------
void setupAP();
void enterAPMode();
void loadSavedWiFiNetworks();
void processarConexaoWiFi();
void connectToWiFi(const char *ssid, const char *password);
void desconectarWiFi();

// -------------------------------------------------------------------------
// 6. Varredura e Métricas de Sinal
// -------------------------------------------------------------------------
void startScanWiFiNetworks();
String getScanResults();
bool isWiFiConnected();
int getWiFiSignalQuality();
const char *getWiFiStateString();
String getWiFiStatusJSON();

#endif // WIFICONEXAO_H
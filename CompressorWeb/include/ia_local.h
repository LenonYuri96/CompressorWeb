/**
 * ============================================================================
 * PROJETO: EnergizAR - Sistema de Automacao e Monitoramento ESP32
 * ARQUIVO: include/ia_local.h
 * DESCRICAO: Motor de Inteligencia Artificial Embarcada (Edge AI / Online
 *            Machine Learning) 100% local e ADAPTATIVA no ESP32.
 *            Detecta automaticamente quais pinos/sensores ja estao ligados
 *            fisicamente no campo e opera em modo comportamental enquanto os
 *            pinos futuros ainda estiverem desconectados.
 * ============================================================================
 */
#ifndef IA_LOCAL_H
#define IA_LOCAL_H

#include <Arduino.h>
#include <Preferences.h>
#include "ligadesliga.h"

// -------------------------------------------------------------------------
// Modos Adaptativos Independentes por Canal (Motor)
// -------------------------------------------------------------------------
enum ModoIAMotor : uint8_t
{
    IA_MODO_SEM_FEEDBACK = 0, // Pino de feedback ainda nao ligado (sem falsos alarmes)
    IA_MODO_CALIBRANDO = 1,   // Feedback detectado! Aprendendo padrao normal (1..11 amostras)
    IA_MODO_ATIVO = 2         // Calibrado (>=12 amostras): diagnostico preditivo completo
};

// -------------------------------------------------------------------------
// Estruturas de Dados Persistidas na NVS (Zero Alocacao Dinamica)
// -------------------------------------------------------------------------

/** Modelo estatistico incremental (Algoritmo de Welford) */
struct ModeloWelford
{
    uint32_t amostras;
    float media;
    float m2; // Soma dos quadrados das diferencas para variancia exata
};

/** Estado persistido de adaptacao e aprendizado por canal */
struct EstadoAdaptativoCanal
{
    uint8_t modo;             // ModoIAMotor atual
    uint8_t falhasSemRetorno; // Contador de ciclos sem sinal durante calibracao
    ModeloWelford latenciaMs; // Padrao de tempo de atracamento (com feedback)
    ModeloWelford duracaoSeg; // Padrao de tempo ligado (funciona SEM feedback!)
};

/** Estado de medicao em tempo real durante transicao de comando */
struct EstadoMedicaoIA
{
    bool pendente;
    bool esperadoLigado;
    unsigned long inicioComandoMs;
    unsigned long primeiraBordaMs;
    bool bordaDetectada;
    unsigned long inicioConfirmacaoMs;
    bool monitorandoRepique;
    unsigned long fimJanelaRepiqueMs;
    bool ultimoNivelRepique;
    uint16_t contadorRepiques;

    // Controle de duracao de ciclo (ativo mesmo sem pino de feedback ligado)
    bool cicloLigadoAtivo;
    unsigned long inicioCicloLigadoMs;

    // Controle de deteccao passiva de conexao futura de feedback
    unsigned long inicioSinalPassivoMs;
};

// -------------------------------------------------------------------------
// Classe Principal: IALocal
// -------------------------------------------------------------------------

class IALocal
{
private:
    EstadoAdaptativoCanal canais[NUM_MOTORES];
    EstadoMedicaoIA medicao[NUM_MOTORES];

    // Matriz 7 dias x 24 horas (Media Movel Exponencial de acionamentos)
    uint8_t perfilDemanda[7][24];

    // Estado de saude e alertas pre-alocados (sem fragmentacao de Heap)
    char alerta[NUM_MOTORES][96];
    float ultimoZScore[NUM_MOTORES];
    uint8_t saudePercentual[NUM_MOTORES];
    uint8_t falhasRecentes[NUM_MOTORES];
    unsigned long timestampUltimaFalha[NUM_MOTORES];

    Preferences prefs;
    bool inicializada;
    unsigned long ultimoDecayMs;

    void carregarDaNVS();
    void salvarCanalNVS(int motorIdx);
    void salvarDemandaNVS();
    void atualizarWelford(ModeloWelford &w, float valor);
    void registrarAmostraLatencia(int motorIdx, float latenciaMs, uint16_t repiques);
    void registrarDuracaoCiclo(int motorIdx, float duracaoSeg);
    void verificarConexaoPassivaFeedback(int motorIdx, bool nivelStatusAtual, unsigned long agora);
    void atualizarSaude(int motorIdx);
    void atualizarPerfilDemanda(int motorIdx);

public:
    IALocal();

    // Ciclo de vida no setup() e loop()
    void begin();
    void processar();

    // Gatilhos chamados pelos modulos de potencia e protecao
    void iniciarMedicaoComando(int motorIdx, bool ligando);
    void notificarEventoSeguranca(int motorIdx, const char *origem);

    // Consultas seguras para Dashboard, API REST e Serial
    const char *getAlerta(int motorIdx) const;
    const char *getModoString(int motorIdx) const;
    ModoIAMotor getModo(int motorIdx) const;
    uint8_t getSaudePercentual(int motorIdx) const;
    float getMediaLatenciaMs(int motorIdx) const;
    float getDesvioLatenciaMs(int motorIdx) const;
    float getMediaDuracaoSeg(int motorIdx) const;
    float getScore(int motorIdx) const;
    uint32_t getAmostras(int motorIdx) const;
    uint8_t getDemandaHoraAtual() const;

    // Manutencao e diagnostico
    void limparAlerta(int motorIdx);
    void resetarModelo(int motorIdx = -1);
    void dumpDiagnostico() const;
};

// Instancia global exportada
extern IALocal iaLocal;

#endif // IA_LOCAL_H
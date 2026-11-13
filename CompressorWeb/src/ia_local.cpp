/**
 * ============================================================================
 * PROJETO: EnergizAR - Sistema de Automacao e Monitoramento ESP32
 * ARQUIVO: src/ia_local.cpp
 * ----------------------------------------------------------------------------
 * DESCRICAO DO SUBSISTEMA:
 *   Motor de IA embarcada 100% local e ADAPTATIVA:
 *     - Enquanto o pino de feedback (pinosStatus[i]) NAO estiver ligado
 *       fisicamente (modo SEM_FEEDBACK), a IA nao gera falsos alarmes de
 *       hardware ausente e aprende o perfil comportamental (duracao de ciclos,
 *       matriz de demanda 7x24h e eventos de seguranca).
 *     - Assim que um pino de feedback futuro for conectado e apresentar nivel
 *       HIGH estavel, o canal se autopromove para CALIBRANDO -> ATIVO,
 *       habilitando diagnostico preditivo completo (Z-Score de latencia,
 *       repique nos contatos, bobina aberta e contator colado).
 *     - Se um teste de bancada com jumper for removido antes da calibracao
 *       concluir (3 ciclos sem resposta durante CALIBRANDO), o canal retorna
 *       automaticamente para SEM_FEEDBACK.
 * ============================================================================
 */

#include <math.h>
#include <string.h>
#include "ia_local.h"
#include "tempo.h"

// Instancia global unica
IALocal iaLocal;

// Constantes de calibracao do motor de IA
static const char *NVS_NS_IA = "energizar_ia2";
static const uint32_t MIN_AMOSTRAS_CALIBRACAO = 12;     // Ciclos de feedback p/ ativar Z-Score
static const uint8_t MAX_FALHAS_REBAIXAMENTO = 3;       // Perda de sinal em bancada volta p/ SEM_FEEDBACK
static const float LIMIAR_ZSCORE_ALERTA = 3.5f;         // 3.5 desvios padrao
static const float PISO_DESVIO_MS = 3.0f;               // Piso contra variancia quase nula
static const unsigned long CONFIRM_ESTAVEL_MS = 8UL;    // Micro-debounce na captura de borda
static const unsigned long CONFIRM_PASSIVA_MS = 25UL;   // Deteccao passiva de novo sensor ligado
static const unsigned long JANELA_REPIQUE_MS = 400UL;   // Auditoria de arco/chattering
static const uint16_t LIMIAR_REPIQUES_ANOMALIA = 4;     // Limite de oscilacoes anormais
static const unsigned long DECAY_FALHAS_MS = 3600000UL; // 1 hora para atenuar falhas antigas
static const float DURACAO_MIN_CICLO_VALIDO_S = 10.0f;  // Ignora toques muito curtos na media

// -------------------------------------------------------------------------
// Construtor
// -------------------------------------------------------------------------
IALocal::IALocal()
{
    inicializada = false;
    ultimoDecayMs = 0UL;
    memset(canais, 0, sizeof(canais));
    memset(medicao, 0, sizeof(medicao));
    memset(perfilDemanda, 0, sizeof(perfilDemanda));
    memset(alerta, 0, sizeof(alerta));
    memset(ultimoZScore, 0, sizeof(ultimoZScore));
    memset(falhasRecentes, 0, sizeof(falhasRecentes));
    memset(timestampUltimaFalha, 0, sizeof(timestampUltimaFalha));

    for (int i = 0; i < NUM_MOTORES; i++)
    {
        canais[i].modo = IA_MODO_SEM_FEEDBACK;
        saudePercentual[i] = 100;
    }
}

// -------------------------------------------------------------------------
// Inicializacao e Carregamento da Memoria NVS
// -------------------------------------------------------------------------
void IALocal::begin()
{
    carregarDaNVS();
    ultimoDecayMs = millis();
    inicializada = true;

    Serial.println("==================================================");
    Serial.println("[IA_LOCAL] Motor de Edge AI Adaptativa inicializado.");
    for (int i = 0; i < NUM_MOTORES; i++)
    {
        Serial.printf("[IA_LOCAL] Motor %d | Modo=%s | AmostrasFB=%lu | MediaFB=%.1fms | CiclosUso=%lu | Saude=%u%%\n",
                      i + 1,
                      getModoString(i),
                      (unsigned long)canais[i].latenciaMs.amostras,
                      canais[i].latenciaMs.media,
                      (unsigned long)canais[i].duracaoSeg.amostras,
                      saudePercentual[i]);
    }
    Serial.println("==================================================");
}

void IALocal::carregarDaNVS()
{
    if (!prefs.begin(NVS_NS_IA, false))
    {
        Serial.println("[IA_LOCAL] AVISO: Falha ao abrir NVS. Operando em RAM.");
        return;
    }

    for (int i = 0; i < NUM_MOTORES; i++)
    {
        char key[8];
        snprintf(key, sizeof(key), "ch%d", i);
        if (prefs.getBytesLength(key) == sizeof(EstadoAdaptativoCanal))
        {
            prefs.getBytes(key, &canais[i], sizeof(EstadoAdaptativoCanal));
            if (canais[i].modo > IA_MODO_ATIVO)
            {
                canais[i].modo = IA_MODO_SEM_FEEDBACK;
            }
        }
        else
        {
            memset(&canais[i], 0, sizeof(EstadoAdaptativoCanal));
            canais[i].modo = IA_MODO_SEM_FEEDBACK;
        }

        alerta[i][0] = '\0';
        ultimoZScore[i] = 0.0f;
        atualizarSaude(i);
    }

    if (prefs.getBytesLength("dem7x24") == sizeof(perfilDemanda))
    {
        prefs.getBytes("dem7x24", perfilDemanda, sizeof(perfilDemanda));
    }

    prefs.end();
}

void IALocal::salvarCanalNVS(int motorIdx)
{
    if (motorIdx < 0 || motorIdx >= NUM_MOTORES)
        return;

    if (!prefs.begin(NVS_NS_IA, false))
        return;

    char key[8];
    snprintf(key, sizeof(key), "ch%d", motorIdx);
    prefs.putBytes(key, &canais[motorIdx], sizeof(EstadoAdaptativoCanal));
    prefs.end();
}

void IALocal::salvarDemandaNVS()
{
    if (!prefs.begin(NVS_NS_IA, false))
        return;
    prefs.putBytes("dem7x24", perfilDemanda, sizeof(perfilDemanda));
    prefs.end();
}

// -------------------------------------------------------------------------
// Registro de Inicio de Comando (Chamado por ligadesliga.cpp)
// -------------------------------------------------------------------------
void IALocal::iniciarMedicaoComando(int motorIdx, bool ligando)
{
    if (!inicializada || motorIdx < 0 || motorIdx >= NUM_MOTORES)
        return;

    unsigned long agora = millis();
    EstadoMedicaoIA &m = medicao[motorIdx];

    // Sempre inicia medicao de resposta do pino de feedback:
    // - Se estiver em SEM_FEEDBACK, serve para auto-descobrir se o pino foi ligado no futuro!
    // - Se estiver em CALIBRANDO ou ATIVO, mede latencia e valida resposta.
    m.pendente = true;
    m.esperadoLigado = ligando;
    m.inicioComandoMs = agora;
    m.bordaDetectada = false;
    m.primeiraBordaMs = 0UL;
    m.inicioConfirmacaoMs = 0UL;
    m.monitorandoRepique = false;
    m.fimJanelaRepiqueMs = 0UL;
    m.contadorRepiques = 0;

    if (ligando)
    {
        m.cicloLigadoAtivo = true;
        m.inicioCicloLigadoMs = agora;
        atualizarPerfilDemanda(motorIdx);
    }
    else
    {
        // Ao desligar, registra a duracao real do ciclo (funciona mesmo SEM sensores extras!)
        if (m.cicloLigadoAtivo)
        {
            float duracaoSeg = (float)(agora - m.inicioCicloLigadoMs) / 1000.0f;
            m.cicloLigadoAtivo = false;
            registrarDuracaoCiclo(motorIdx, duracaoSeg);
        }
    }
}

// -------------------------------------------------------------------------
// Atualizacao do Perfil Semanal de Demanda (7 dias x 24 horas)
// -------------------------------------------------------------------------
void IALocal::atualizarPerfilDemanda(int motorIdx)
{
    if (!isTimeSynchronized())
        return;

    int dia = getDiaSemana();
    int hora = getHoraAtual();
    if (dia < 0 || dia > 6 || hora < 0 || hora > 23)
        return;

    uint16_t novo = (uint16_t)perfilDemanda[dia][hora] + 5;
    perfilDemanda[dia][hora] = (novo > 100) ? 100 : (uint8_t)novo;

    if ((canais[motorIdx].duracaoSeg.amostras % 5) == 0)
    {
        salvarDemandaNVS();
    }
}

// -------------------------------------------------------------------------
// Deteccao Passiva de Sensor Ligado no Futuro (Hot-Plug em Campo)
// -------------------------------------------------------------------------
void IALocal::verificarConexaoPassivaFeedback(int motorIdx, bool nivelStatusAtual, unsigned long agora)
{
    if (canais[motorIdx].modo != IA_MODO_SEM_FEEDBACK)
        return;

    // Se o motor consta como ligado e o pino de feedback passar a HIGH por 25ms,
    // significa que o hardware auxiliar passou a estar conectado fisicamente!
    if (motoresLigados[motorIdx] && nivelStatusAtual)
    {
        if (medicao[motorIdx].inicioSinalPassivoMs == 0UL)
        {
            medicao[motorIdx].inicioSinalPassivoMs = agora;
        }
        else if ((agora - medicao[motorIdx].inicioSinalPassivoMs) >= CONFIRM_PASSIVA_MS)
        {
            canais[motorIdx].modo = IA_MODO_CALIBRANDO;
            canais[motorIdx].falhasSemRetorno = 0;
            medicao[motorIdx].inicioSinalPassivoMs = 0UL;
            salvarCanalNVS(motorIdx);

            Serial.printf("[IA_LOCAL] AUTO-DESCOBERTA: Feedback fisico detectado no Motor %d! "
                          "Promovido para modo CALIBRANDO.\n",
                          motorIdx + 1);
        }
    }
    else
    {
        medicao[motorIdx].inicioSinalPassivoMs = 0UL;
    }
}

// -------------------------------------------------------------------------
// Loop Cooperativo Sub-Milissegundo (Chamado no loop() principal)
// -------------------------------------------------------------------------
void IALocal::processar()
{
    if (!inicializada)
        return;

    unsigned long agora = millis();

    // Atenua gradativamente falhas antigas a cada 1 hora de operacao estavel
    if ((agora - ultimoDecayMs) >= DECAY_FALHAS_MS)
    {
        ultimoDecayMs = agora;
        for (int i = 0; i < NUM_MOTORES; i++)
        {
            if (falhasRecentes[i] > 0)
            {
                falhasRecentes[i]--;
                atualizarSaude(i);
            }
        }
    }

    for (int i = 0; i < NUM_MOTORES; i++)
    {
        bool nivelAtual = (digitalRead(pinosStatus[i]) == HIGH);
        EstadoMedicaoIA &m = medicao[i];

        // Se ainda nao ha feedback ligado, vigia passivamente caso seja conectado no futuro
        verificarConexaoPassivaFeedback(i, nivelAtual, agora);

        if (!m.pendente && !m.monitorandoRepique)
            continue;

        // -----------------------------------------------------------------
        // FASE 1: Aguarda fechamento (LIGAR) ou abertura (DESLIGAR)
        // -----------------------------------------------------------------
        if (m.pendente)
        {
            if (nivelAtual == m.esperadoLigado)
            {
                if (!m.bordaDetectada)
                {
                    m.bordaDetectada = true;
                    m.primeiraBordaMs = agora;
                    m.inicioConfirmacaoMs = agora;
                }
                else if ((agora - m.inicioConfirmacaoMs) >= CONFIRM_ESTAVEL_MS)
                {
                    unsigned long latenciaMs = m.primeiraBordaMs - m.inicioComandoMs;
                    m.pendente = false;

                    if (m.esperadoLigado)
                    {
                        // Se estava SEM_FEEDBACK e recebeu HIGH ao ligar, autopromove!
                        if (canais[i].modo == IA_MODO_SEM_FEEDBACK)
                        {
                            canais[i].modo = IA_MODO_CALIBRANDO;
                            canais[i].falhasSemRetorno = 0;
                            Serial.printf("[IA_LOCAL] AUTO-DESCOBERTA: Feedback detectado na partida do Motor %d! "
                                          "Iniciando calibracao.\n",
                                          i + 1);
                        }

                        // Inicia janela de 400 ms para auditar repiques (chattering)
                        m.monitorandoRepique = true;
                        m.fimJanelaRepiqueMs = agora + JANELA_REPIQUE_MS;
                        m.ultimoNivelRepique = nivelAtual;
                        m.contadorRepiques = 0;
                        m.primeiraBordaMs = latenciaMs;
                    }
                    else
                    {
                        // Contator abriu normalmente no desligamento
                        if (strstr(alerta[i], "colado") != nullptr)
                        {
                            alerta[i][0] = '\0';
                        }
                        if (canais[i].modo != IA_MODO_SEM_FEEDBACK)
                        {
                            Serial.printf("[IA_LOCAL] Motor %d abriu em %lu ms.\n", i + 1, latenciaMs);
                        }
                    }
                }
            }
            else
            {
                m.inicioConfirmacaoMs = agora;

                // Timeout (4000 ms sem atingir o nivel esperado)
                if ((agora - m.inicioComandoMs) >= TOLERANCIA_COMANDO_MS)
                {
                    m.pendente = false;

                    if (!m.esperadoLigado)
                    {
                        // Se comandou DESLIGAR e o pino continua HIGH, existe sinal fisico preso!
                        // (Mesmo antes de calibrar, um pino HIGH permanente com motor OFF eh contator colado!)
                        snprintf(alerta[i], sizeof(alerta[i]),
                                 "IA: Contator nao abriu apos desligar (possivel contato colado)");
                        falhasRecentes[i]++;
                        atualizarSaude(i);
                        Serial.printf("[IA_LOCAL] ALERTA Motor %d: %s\n", i + 1, alerta[i]);
                    }
                    else
                    {
                        // Timeout ao LIGAR (pino permaneceu LOW por 4000 ms)
                        if (canais[i].modo == IA_MODO_ATIVO)
                        {
                            // Em producao ja calibrada, perda de retorno eh falha real!
                            snprintf(alerta[i], sizeof(alerta[i]),
                                     "IA: Contator nao respondeu ao comando ligar (bobina ou auxiliar)");
                            falhasRecentes[i]++;
                            atualizarSaude(i);
                            Serial.printf("[IA_LOCAL] ALERTA Motor %d: %s\n", i + 1, alerta[i]);
                        }
                        else if (canais[i].modo == IA_MODO_CALIBRANDO)
                        {
                            // Se estava apenas em calibracao e perdeu sinal 3 vezes, rebaixa p/ SEM_FEEDBACK
                            canais[i].falhasSemRetorno++;
                            if (canais[i].falhasSemRetorno >= MAX_FALHAS_REBAIXAMENTO)
                            {
                                canais[i].modo = IA_MODO_SEM_FEEDBACK;
                                canais[i].falhasSemRetorno = 0;
                                canais[i].latenciaMs = {0, 0.0f, 0.0f};
                                alerta[i][0] = '\0';
                                salvarCanalNVS(i);
                                Serial.printf("[IA_LOCAL] Motor %d: sinal de feedback ausente na bancada. "
                                              "Adaptado para modo SEM_FEEDBACK.\n",
                                              i + 1);
                            }
                        }
                        // Em IA_MODO_SEM_FEEDBACK: comportamento totalmente normal enquanto fio nao esta ligado!
                    }
                }
            }
            continue;
        }

        // -----------------------------------------------------------------
        // FASE 2: Auditoria de Repiques (Chattering) durante 400 ms
        // -----------------------------------------------------------------
        if (m.monitorandoRepique)
        {
            if (nivelAtual != m.ultimoNivelRepique)
            {
                m.ultimoNivelRepique = nivelAtual;
                m.contadorRepiques++;
            }

            if ((long)(agora - m.fimJanelaRepiqueMs) >= 0)
            {
                m.monitorandoRepique = false;
                float latencia = (float)m.primeiraBordaMs;
                registrarAmostraLatencia(i, latencia, m.contadorRepiques);
            }
        }
    }
}

// -------------------------------------------------------------------------
// Utilitario Welford Generico
// -------------------------------------------------------------------------
void IALocal::atualizarWelford(ModeloWelford &w, float valor)
{
    w.amostras++;
    float delta = valor - w.media;
    w.media += delta / (float)w.amostras;
    float delta2 = valor - w.media;
    w.m2 += delta * delta2;
}

// -------------------------------------------------------------------------
// Aprendizado de Duracao de Ciclo (Ativo mesmo SEM sensores extras!)
// -------------------------------------------------------------------------
void IALocal::registrarDuracaoCiclo(int motorIdx, float duracaoSeg)
{
    if (duracaoSeg < DURACAO_MIN_CICLO_VALIDO_S)
        return;

    ModeloWelford &d = canais[motorIdx].duracaoSeg;
    atualizarWelford(d, duracaoSeg);

    if ((d.amostras % 5) == 0)
    {
        salvarCanalNVS(motorIdx);
    }
}

// -------------------------------------------------------------------------
// Inferencia Estatistica de Latencia do Contator (Modo CALIBRANDO / ATIVO)
// -------------------------------------------------------------------------
void IALocal::registrarAmostraLatencia(int motorIdx, float latenciaMs, uint16_t repiques)
{
    EstadoAdaptativoCanal &ch = canais[motorIdx];
    ch.falhasSemRetorno = 0;

    // 1. Verifica repiques anormais (arco/oxidacao nos contatos)
    if (repiques >= LIMIAR_REPIQUES_ANOMALIA)
    {
        snprintf(alerta[motorIdx], sizeof(alerta[motorIdx]),
                 "IA: Repique excessivo no contator (%u oscilacoes) - inspecionar contatos",
                 (unsigned int)repiques);
        falhasRecentes[motorIdx]++;
        atualizarSaude(motorIdx);
        Serial.printf("[IA_LOCAL] ALERTA Motor %d: %s\n", motorIdx + 1, alerta[motorIdx]);
        return; // Nao contamina o modelo base
    }

    // 2. Se ja estiver no modo ATIVO, aplica Z-Score em relacao ao padrao aprendido
    if (ch.modo == IA_MODO_ATIVO && ch.latenciaMs.amostras >= MIN_AMOSTRAS_CALIBRACAO)
    {
        float desvio = getDesvioLatenciaMs(motorIdx);
        float zScore = fabsf(latenciaMs - ch.latenciaMs.media) / desvio;
        ultimoZScore[motorIdx] = zScore;

        if (zScore > LIMIAR_ZSCORE_ALERTA)
        {
            snprintf(alerta[motorIdx], sizeof(alerta[motorIdx]),
                     "IA: Resposta lenta do contator (%dms vs padrao %dms) - checar bobina",
                     (int)latenciaMs, (int)ch.latenciaMs.media);
            atualizarSaude(motorIdx);
            Serial.printf("[IA_LOCAL] ANOMALIA Motor %d: Z=%.2f | %s\n",
                          motorIdx + 1, zScore, alerta[motorIdx]);
            return; // Protege a media contra outliers
        }
        else
        {
            alerta[motorIdx][0] = '\0';
        }
    }
    else
    {
        ultimoZScore[motorIdx] = 0.0f;
        alerta[motorIdx][0] = '\0';
    }

    // 3. Atualiza o modelo Welford e promove para ATIVO ao atingir 12 amostras
    atualizarWelford(ch.latenciaMs, latenciaMs);

    if (ch.modo == IA_MODO_CALIBRANDO && ch.latenciaMs.amostras >= MIN_AMOSTRAS_CALIBRACAO)
    {
        ch.modo = IA_MODO_ATIVO;
        salvarCanalNVS(motorIdx);
        Serial.printf("[IA_LOCAL] Motor %d CALIBRADO (%lu amostras)! Modo preditivo ATIVO.\n",
                      motorIdx + 1, (unsigned long)ch.latenciaMs.amostras);
    }
    else if ((ch.latenciaMs.amostras % 5) == 0)
    {
        salvarCanalNVS(motorIdx);
    }

    atualizarSaude(motorIdx);

    Serial.printf("[IA_LOCAL] Motor %d [%s] amostra #%lu: %.0f ms (padrao=%.1f ms +- %.1f ms | saude=%u%%)\n",
                  motorIdx + 1,
                  getModoString(motorIdx),
                  (unsigned long)ch.latenciaMs.amostras,
                  latenciaMs,
                  ch.latenciaMs.media,
                  getDesvioLatenciaMs(motorIdx),
                  saudePercentual[motorIdx]);
}

// -------------------------------------------------------------------------
// Notificacao de Eventos de Seguranca (Sobrecarga / Pressostato)
// -------------------------------------------------------------------------
void IALocal::notificarEventoSeguranca(int motorIdx, const char *origem)
{
    if (!inicializada || motorIdx < 0 || motorIdx >= NUM_MOTORES)
        return;

    unsigned long agora = millis();
    if (timestampUltimaFalha[motorIdx] != 0UL && (agora - timestampUltimaFalha[motorIdx]) < 10000UL)
        return;

    timestampUltimaFalha[motorIdx] = agora;
    if (falhasRecentes[motorIdx] < 10)
    {
        falhasRecentes[motorIdx]++;
    }

    if (falhasRecentes[motorIdx] >= 2)
    {
        snprintf(alerta[motorIdx], sizeof(alerta[motorIdx]),
                 "IA: %u desarmes recentes (%s) - manutencao preventiva recomendada",
                 (unsigned int)falhasRecentes[motorIdx],
                 (origem != nullptr) ? origem : "protecao");
    }

    atualizarSaude(motorIdx);
}

// -------------------------------------------------------------------------
// Calculo do Indice de Saude Composto (0 a 100%)
// -------------------------------------------------------------------------
void IALocal::atualizarSaude(int motorIdx)
{
    int saude = 100;

    if (canais[motorIdx].modo == IA_MODO_ATIVO && ultimoZScore[motorIdx] > 2.0f)
    {
        int penZ = (int)((ultimoZScore[motorIdx] - 2.0f) * 12.0f);
        saude -= penZ;
    }

    saude -= (int)(falhasRecentes[motorIdx] * 15);

    if (saude < 0)
        saude = 0;
    if (saude > 100)
        saude = 100;

    saudePercentual[motorIdx] = (uint8_t)saude;
}

// -------------------------------------------------------------------------
// Getters Publicos e Utilitarios
// -------------------------------------------------------------------------
const char *IALocal::getAlerta(int motorIdx) const
{
    if (motorIdx < 0 || motorIdx >= NUM_MOTORES)
        return "";
    return alerta[motorIdx];
}

ModoIAMotor IALocal::getModo(int motorIdx) const
{
    if (motorIdx < 0 || motorIdx >= NUM_MOTORES)
        return IA_MODO_SEM_FEEDBACK;
    return (ModoIAMotor)canais[motorIdx].modo;
}

const char *IALocal::getModoString(int motorIdx) const
{
    switch (getModo(motorIdx))
    {
    case IA_MODO_ATIVO:
        return "PREDITIVO_ATIVO";
    case IA_MODO_CALIBRANDO:
        return "CALIBRANDO_FEEDBACK";
    case IA_MODO_SEM_FEEDBACK:
    default:
        return "ADAPTATIVO_SEM_FEEDBACK";
    }
}

uint8_t IALocal::getSaudePercentual(int motorIdx) const
{
    if (motorIdx < 0 || motorIdx >= NUM_MOTORES)
        return 100;
    return saudePercentual[motorIdx];
}

float IALocal::getMediaLatenciaMs(int motorIdx) const
{
    if (motorIdx < 0 || motorIdx >= NUM_MOTORES)
        return 0.0f;
    return canais[motorIdx].latenciaMs.media;
}

float IALocal::getDesvioLatenciaMs(int motorIdx) const
{
    if (motorIdx < 0 || motorIdx >= NUM_MOTORES || canais[motorIdx].latenciaMs.amostras < 2)
        return PISO_DESVIO_MS;

    const ModeloWelford &w = canais[motorIdx].latenciaMs;
    float variancia = w.m2 / (float)(w.amostras - 1);
    float desvio = (variancia > 0.0f) ? sqrtf(variancia) : 0.0f;
    return (desvio < PISO_DESVIO_MS) ? PISO_DESVIO_MS : desvio;
}

float IALocal::getMediaDuracaoSeg(int motorIdx) const
{
    if (motorIdx < 0 || motorIdx >= NUM_MOTORES)
        return 0.0f;
    return canais[motorIdx].duracaoSeg.media;
}

float IALocal::getScore(int motorIdx) const
{
    if (motorIdx < 0 || motorIdx >= NUM_MOTORES)
        return 0.0f;
    return ultimoZScore[motorIdx];
}

uint32_t IALocal::getAmostras(int motorIdx) const
{
    if (motorIdx < 0 || motorIdx >= NUM_MOTORES)
        return 0;
    return canais[motorIdx].latenciaMs.amostras;
}

uint8_t IALocal::getDemandaHoraAtual() const
{
    if (!isTimeSynchronized())
        return 0;
    int d = getDiaSemana();
    int h = getHoraAtual();
    if (d < 0 || d > 6 || h < 0 || h > 23)
        return 0;
    return perfilDemanda[d][h];
}

void IALocal::limparAlerta(int motorIdx)
{
    if (motorIdx >= 0 && motorIdx < NUM_MOTORES)
    {
        alerta[motorIdx][0] = '\0';
        ultimoZScore[motorIdx] = 0.0f;
        atualizarSaude(motorIdx);
    }
}

void IALocal::resetarModelo(int motorIdx)
{
    int inicio = (motorIdx < 0) ? 0 : motorIdx;
    int fim = (motorIdx < 0) ? (NUM_MOTORES - 1) : motorIdx;

    if (inicio >= NUM_MOTORES || fim < 0)
        return;

    for (int i = inicio; i <= fim; i++)
    {
        memset(&canais[i], 0, sizeof(EstadoAdaptativoCanal));
        canais[i].modo = IA_MODO_SEM_FEEDBACK;
        alerta[i][0] = '\0';
        ultimoZScore[i] = 0.0f;
        falhasRecentes[i] = 0;
        saudePercentual[i] = 100;
        salvarCanalNVS(i);
    }

    Serial.printf("[IA_LOCAL] Modelo adaptativo resetado para SEM_FEEDBACK (canal=%d).\n", motorIdx);
}

void IALocal::dumpDiagnostico() const
{
    Serial.println("==============================================");
    Serial.println("[DIAGNOSTICO] IA Local Adaptativa (Edge AI)");
    Serial.println("==============================================");
    for (int i = 0; i < NUM_MOTORES; i++)
    {
        Serial.printf("Motor %d [%s]\n", i + 1, getModoString(i));
        Serial.printf("  Feedback : Amostras=%lu | Media=%.1fms | Desvio=%.1fms | Z=%.2f\n",
                      (unsigned long)canais[i].latenciaMs.amostras,
                      canais[i].latenciaMs.media,
                      getDesvioLatenciaMs(i),
                      ultimoZScore[i]);
        Serial.printf("  Uso      : Ciclos=%lu | DuracaoMedia=%.1fs | Saude=%u%%\n",
                      (unsigned long)canais[i].duracaoSeg.amostras,
                      canais[i].duracaoSeg.media,
                      saudePercentual[i]);
        if (alerta[i][0] != '\0')
        {
            Serial.printf("  Alerta   : %s\n", alerta[i]);
        }
    }
    Serial.println("==============================================");
}
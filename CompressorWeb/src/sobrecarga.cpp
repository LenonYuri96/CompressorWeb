/**
 * ============================================================================
 * PROJETO: EnergizAR - Sistema de Automacao e Monitoramento ESP32
 * ARQUIVO: src/sobrecarga.cpp
 * ----------------------------------------------------------------------------
 * DESCRICAO DO SUBSISTEMA:
 *   Modulo industrial de seguranca e protecao contra sobrecarga termica dos
 *   motores (Compressor Principal, Ventilador e Secador de Ar). Realiza a
 *   leitura cooperativa e nao-bloqueante dos contatos auxiliares (97-98 NA)
 *   dos reles termicos bimetalicos, com qualificacao eletrica na partida,
 *   filtragem digital contra transientes e corte imediato via pulso de 500 ms.
 *
 * ARQUITETURA DE SEGURANCA (v2.3.2):
 *   1. CARENCIA DE ENERGIZACAO (2000 ms):
 *      Aguarda estabilizacao das fontes e dos contatos eletricos apos o boot
 *      antes de validar falhas termicas.
 *
 *   2. QUALIFICACAO INICIAL (ETAPA A) COM DETECCAO DE FALHA NO BOOT:
 *      Valida nivel LOW continuo por 800 ms antes de armar o monitoramento.
 *      CORRECAO CRITICA: se o pino permanecer em HIGH por >= 250 ms apos a
 *      carencia de boot (rele termico ja desarmado ao ligar o painel), a
 *      flag sobrecargaDetectada[i] eh imediatamente ativada para exibir o
 *      alarme no Dashboard e bloquear partidas via /toggle.
 *
 *   3. FILTRAGEM DIGITAL POR DEBOUNCE (250 ms):
 *      Impede falsos desarmes causados por ruido eletromagnetico ou repique
 *      mecanico durante o chaveamento dos contatores de potencia.
 *
 *   4. DESARME CONSISTENTE POR PULSO COM RE-TENTATIVA (CORRECAO CRITICA):
 *      Substitui o antigo digitalWrite(LOW) — ineficaz em hardware acionado
 *      por pulso — por dispararPulsoMotor(motorIndex). Caso o canal esteja
 *      ocupado executando um pulso de partida (guarda v2.3.1), o desarme fica
 *      marcado em desarmePendente[] e eh executado assim que a janela liberar.
 *
 *   5. ISOLAMENTO DE PINAGEM (REMOCAO DO STRAPPING PIN GPIO 12):
 *      Alinhado ao mapa consolidado v2.3.0 de ligadesliga.cpp:
 *        - Motor 0 (Compressor Principal) : GPIO 19 (INPUT_PULLDOWN)
 *        - Motor 1 (Motor Ventilador)     : GPIO 25 (INPUT_PULLDOWN)
 *        - Motor 2 (Motor Secador de Ar)  : GPIO 18 (INPUT_PULLDOWN)
 *
 * HISTORICO DE REVISOES:
 *   v1.0.0 - Leitura basica de reles termicos com debounce.
 *   v2.0.0 - Adicao de carencia de boot e qualificacao inicial em LOW.
 *   v2.3.0 - Correcao critica: desarme via pulso + deteccao de falha no boot.
 *   v2.3.2 - Migracao GPIO 12 -> GPIO 25, fila de desarme pendente e auditoria.
 * ============================================================================
 */

// ============================================================================
// SECAO 1: BIBLIOTECAS, CABECALHOS E DEPENDENCIAS DO SISTEMA
// ============================================================================

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>

#include "sobrecarga.h"
#include "ligadesliga.h"
#include "ia_local.h"

// ============================================================================
// SECAO 2: MAPEAMENTO FISICO DAS ENTRADAS DE PROTECAO TERMICA
// ============================================================================

/**
 * Pinos conectados aos contatos auxiliares 97-98 (NA) dos reles termicos:
 *   Indice 0 -> Motor 1 (Compressor Principal) : GPIO 19
 *   Indice 1 -> Motor 2 (Motor Ventilador)     : GPIO 25 (substituiu GPIO 12)
 *   Indice 2 -> Motor 3 (Motor Secador de Ar)  : GPIO 18
 *
 * NOTA ELETRICA:
 *   Todas as entradas operam com resistor PULLDOWN interno habilitado.
 *     - Nivel LOW  (0.0 V) : Circuito termico normal, motor liberado.
 *     - Nivel HIGH (3.3 V) : Rele termico desarmou por sobrecorrente.
 */
const int pinosSobrecarga[NUM_MOTORES] = {19, 25, 18};

// ============================================================================
// SECAO 3: VARIAVEIS GLOBAIS E ESTADOS INTERNOS DE OPERACAO EM RAM
// ============================================================================

/** Flag publica de alarme de sobrecarga confirmado por canal */
bool sobrecargaDetectada[NUM_MOTORES] = {false, false, false};

/** Marca temporal (millis) do inicio do episodio atual de sobrecarga */
unsigned long tempoInicioSobrecarga[NUM_MOTORES] = {0UL, 0UL, 0UL};

/** Ultimo nivel bruto lido no pino para controle do algoritmo de debounce */
static bool ultimoEstadoSobrecarga[NUM_MOTORES] = {false, false, false};

/** Timestamp da ultima alteracao de estado bruto para janela de 250 ms */
static unsigned long ultimoTempoSobrecarga[NUM_MOTORES] = {0UL, 0UL, 0UL};

/** Indica se a entrada concluiu a qualificacao inicial de nivel LOW */
static bool entradaSobrecargaArmada[NUM_MOTORES] = {false, false, false};

/** Evita repeticao excessiva de mensagens no console durante boot em HIGH */
static bool avisoEntradaInvalidaEmitido[NUM_MOTORES] = {false, false, false};

/** Inicio da janela continua em LOW durante a etapa de qualificacao */
static unsigned long tempoInicioNivelBaixo[NUM_MOTORES] = {0UL, 0UL, 0UL};

/** Inicio da janela continua em HIGH durante o boot (falha pre-existente) */
static unsigned long tempoInicioNivelAltoBoot[NUM_MOTORES] = {0UL, 0UL, 0UL};

/**
 * Sinalizador de desarme pendente: utilizado quando dispararPulsoMotor()
 * retorna false por haver um pulso de 500 ms em curso no mesmo canal.
 * Garante que o motor sera desligado no instante em que o pulso encerrar.
 */
static bool desarmePendente[NUM_MOTORES] = {false, false, false};

/** Timestamp do ultimo log periodico enquanto um canal permanece em alarme */
static unsigned long ultimoLogSobrecarga[NUM_MOTORES] = {0UL, 0UL, 0UL};

// ----------------------------------------------------------------------------
// Contadores de Auditoria e Manutencao Preditiva em RAM
// ----------------------------------------------------------------------------

/** Numero total de disparos termicos confirmados desde o ultimo boot */
static unsigned long falhasSobrecargaTotal[NUM_MOTORES] = {0UL, 0UL, 0UL};

/** Total de pulsos de corte emergencial efetivamente enviados por este modulo */
static unsigned long desarmesEmitidos[NUM_MOTORES] = {0UL, 0UL, 0UL};

/** Total de tentativas adiadas pela guarda de pulso em andamento */
static unsigned long desarmesAdiados[NUM_MOTORES] = {0UL, 0UL, 0UL};

/** Tempo acumulado (em segundos) em que cada canal permaneceu sob falha */
static unsigned long tempoAcumuladoSobrecarga[NUM_MOTORES] = {0UL, 0UL, 0UL};

// ============================================================================
// SECAO 4: PARAMETROS TEMPORAIS DE CALIBRACAO E FILTRAGEM
// ============================================================================

/** Janela de estabilizacao digital contra ruidos eletromagneticos (250 ms) */
const unsigned long DEBOUNCE_DELAY = 250UL;

/** Alias de compatibilidade exportado por include/sobrecarga.h */
const unsigned long tempoMinimoSobrecarga = DEBOUNCE_DELAY;

/** Periodo de carencia pos-energizacao para estabilizacao eletrica (2000 ms) */
const unsigned long CARENCIA_BOOT_MS = 2000UL;

/** Tempo minimo continuo em LOW para habilitar a armacao do canal (800 ms) */
const unsigned long TEMPO_VALIDACAO_ENTRADA_MS = 800UL;

/** Intervalo minimo entre logs de lembrete enquanto falha persistir (30 s) */
const unsigned long INTERVALO_LOG_FALHA_MS = 30000UL;

// ============================================================================
// SECAO 5: ROTINAS INTERNAS DE AUDITORIA E EMISSAO SEGURA DE PULSO
// ============================================================================

/**
 * registrarEventoSobrecarga()
 * ----------------------------------------------------------------------------
 * Emite registro formatado no monitor serial detalhando transicoes de entrada
 * e saida da condicao de falha termica.
 */
static void registrarEventoSobrecarga(int motorIdx, bool falhaAtiva, bool noBoot)
{
    Serial.println("==================================================");
    if (falhaAtiva)
    {
        Serial.printf("[ALERTA CRITICO] SOBRECARGA CONFIRMADA NO MOTOR %d!%s\n",
                      motorIdx + 1, noBoot ? " (DETECCAO NO BOOT)" : "");
        Serial.printf("[ALERTA CRITICO] Pino GPIO %d em nivel HIGH estavel.\n",
                      pinosSobrecarga[motorIdx]);
        Serial.printf("[ALERTA CRITICO] Ocorrencias acumuladas no canal: %lu\n",
                      falhasSobrecargaTotal[motorIdx]);
    }
    else
    {
        Serial.printf("[SOBRECARGA] Sinal do Motor %d (GPIO %d) normalizado para LOW.\n",
                      motorIdx + 1, pinosSobrecarga[motorIdx]);
        Serial.println("[SOBRECARGA] Alarme termico liberado para operacao normal.");
    }
    Serial.println("==================================================");
}

/**
 * canalEstaFisicamenteOuLogicamenteAtivo()
 * ----------------------------------------------------------------------------
 * Verifica se ha evidencia de energizacao no canal (seja pela intencao
 * logica soberana ou pelo contato auxiliar de feedback do contator).
 */
static bool canalEstaAtivo(int motorIndex)
{
    if (motorIndex < 0 || motorIndex >= NUM_MOTORES)
    {
        return false;
    }

    bool feedbackFisico = (digitalRead(pinosStatus[motorIndex]) == HIGH);
    return motoresLigados[motorIndex] || feedbackFisico;
}

/**
 * executarPulsoDesarmeSeguro()
 * ----------------------------------------------------------------------------
 * Tenta enviar o pulso de abertura de 500 ms ao contator via ligadesliga.cpp.
 * Se o canal estiver ocupado com outro pulso em andamento, agenda o corte
 * em desarmePendente[] para ser concluido automaticamente no proximo ciclo.
 */
static void executarPulsoDesarmeSeguro(int motorIndex)
{
    if (motorIndex < 0 || motorIndex >= NUM_MOTORES)
    {
        return;
    }

    bool precisaCortar = canalEstaAtivo(motorIndex) || desarmePendente[motorIndex];
    if (!precisaCortar)
    {
        return;
    }

    // Tenta emitir o pulso fisico respeitando a guarda de re-disparo (v2.3.1)
    if (dispararPulsoMotor(motorIndex))
    {
        motoresLigados[motorIndex] = false;
        saveMotorState(arquivosMotores[motorIndex], false);
        desarmePendente[motorIndex] = false;
        desarmesEmitidos[motorIndex]++;

        Serial.printf("[SEGURANCA] Motor %d desativado via pulso de protecao (#%lu).\n",
                      motorIndex + 1, desarmesEmitidos[motorIndex]);
    }
    else
    {
        // Pulso ocupado (ex: usuario acabou de clicar em ligar): agenda corte imediato
        desarmePendente[motorIndex] = true;
        desarmesAdiados[motorIndex]++;

        // Atualiza intencao e persistencia imediatamente para bloquear novos comandos
        if (motoresLigados[motorIndex])
        {
            motoresLigados[motorIndex] = false;
            saveMotorState(arquivosMotores[motorIndex], false);
        }

        Serial.printf("[SEGURANCA] Motor %d com pulso em andamento. "
                      "Corte de sobrecarga agendado (pendente).\n",
                      motorIndex + 1);
    }
}

/**
 * processarDesarmesPendentes()
 * ----------------------------------------------------------------------------
 * Drena qualquer desarme que tenha sido adiado pela guarda de pulso ocupado.
 * Executada no inicio de cada ciclo de monitorarSobrecarga().
 */
static void processarDesarmesPendentes()
{
    for (int i = 0; i < NUM_MOTORES; i++)
    {
        if (!desarmePendente[i])
        {
            continue;
        }

        if (dispararPulsoMotor(i))
        {
            desarmePendente[i] = false;
            motoresLigados[i] = false;
            saveMotorState(arquivosMotores[i], false);
            desarmesEmitidos[i]++;

            Serial.printf("[SEGURANCA] Motor %d: desarme pendente executado com exito.\n",
                          i + 1);
        }
    }
}

// ============================================================================
// SECAO 6: INICIALIZACAO DO SUBSISTEMA DE SOBRECARGA
// ============================================================================

/**
 * setupSobrecarga()
 * ----------------------------------------------------------------------------
 * Configura os pinos de entrada com resistor pull-down interno e redefine
 * todas as variaveis de filtragem, qualificacao e auditoria em RAM.
 */
void setupSobrecarga()
{
    Serial.println("[SOBRECARGA] Configurando entradas de protecao termica...");

    unsigned long agora = millis();

    for (int i = 0; i < NUM_MOTORES; i++)
    {
        pinMode(pinosSobrecarga[i], INPUT_PULLDOWN);

        sobrecargaDetectada[i] = false;
        ultimoEstadoSobrecarga[i] = false;
        ultimoTempoSobrecarga[i] = agora;
        tempoInicioSobrecarga[i] = 0UL;

        entradaSobrecargaArmada[i] = false;
        avisoEntradaInvalidaEmitido[i] = false;
        tempoInicioNivelBaixo[i] = 0UL;
        tempoInicioNivelAltoBoot[i] = 0UL;
        desarmePendente[i] = false;
        ultimoLogSobrecarga[i] = 0UL;

        Serial.printf("[SOBRECARGA] Canal %d mapeado com sucesso no GPIO %d.\n",
                      i + 1, pinosSobrecarga[i]);
    }

    Serial.println("[SOBRECARGA] Entradas aguardando validacao eletrica inicial.");
}

// ============================================================================
// SECAO 7: FUNCOES DE DESLIGAMENTO E INTERTRAVAMENTO DE SEGURANCA
// ============================================================================

/**
 * desativarMotor()
 * ----------------------------------------------------------------------------
 * Desliga o motor especificado emitindo pulso de abertura de 500 ms (quando
 * ativo) e atualizando os registros de estado em memoria RAM e SPIFFS.
 *
 * @param motorIndex Indice do motor (0 a NUM_MOTORES - 1).
 */
void desativarMotor(int motorIndex)
{
    if (motorIndex < 0 || motorIndex >= NUM_MOTORES)
    {
        return;
    }

    executarPulsoDesarmeSeguro(motorIndex);
}

/**
 * desativarMotores()
 * ----------------------------------------------------------------------------
 * Desliga todos os compressores do sistema em caso de comando geral ou panico.
 */
void desativarMotores()
{
    Serial.println("[SEGURANCA] Ordem de desligamento geral enviada a todos os motores.");

    for (int i = 0; i < NUM_MOTORES; i++)
    {
        desativarMotor(i);
    }
}

/**
 * desativarMotoresSobrecarga()
 * ----------------------------------------------------------------------------
 * Desliga seletivamente apenas os motores com sobrecarga confirmada.
 */
void desativarMotoresSobrecarga()
{
    for (int i = 0; i < NUM_MOTORES; i++)
    {
        if (sobrecargaDetectada[i])
        {
            desativarMotor(i);
        }
    }
}
/**
 * restaurarEstadoMotores()
 * ----------------------------------------------------------------------------
 * Limpa as condicoes de alarme e garante que os canais permanecam desligados
 * para rearme manual seguro pelo operador. Nao religa nenhum motor sozinho.
 */
void restaurarEstadoMotores()
{
    Serial.println("[SOBRECARGA] Executando rotina de rearme seguro dos motores...");

    for (int i = 0; i < NUM_MOTORES; i++)
    {
        desativarMotor(i);
        sobrecargaDetectada[i] = false;
        tempoInicioSobrecarga[i] = 0UL;
        ultimoTempoSobrecarga[i] = millis();

        Serial.printf("[SOBRECARGA] Motor %d restabelecido em modo DESLIGADO (Seguro).\n",
                      i + 1);
    }
}

// ============================================================================
// SECAO 8: MONITORAMENTO CICLICO CONTINUO (EXECUTADO NO LOOP PRINCIPAL)
// ============================================================================

/**
 * monitorarSobrecarga()
 * ----------------------------------------------------------------------------
 * Rotina cooperativa e nao-bloqueante chamada a cada 100 ms pelo loop().
 * Executa:
 *   0. Drenagem de eventuais desarmes pendentes por pulso ocupado;
 *   1. Respeito a janela de carencia de boot (2000 ms);
 *   2. Etapa A: qualificacao inicial em LOW (800 ms) ou alarme se HIGH no boot;
 *   3. Etapa B: filtragem digital de transientes por debounce (250 ms);
 *   4. Etapa C: trava continua de intertravamento (hard lock).
 */
void monitorarSobrecarga()
{
    unsigned long agora = millis();

    // ----------------------------------------------------------------------
    // PASSO 0: Conclui desarmes adiados assim que a guarda de pulso liberar
    // ----------------------------------------------------------------------
    processarDesarmesPendentes();

    // ----------------------------------------------------------------------
    // PASSO 1: Aguarda carencia inicial de estabilizacao eletrica
    // ----------------------------------------------------------------------
    if (agora < CARENCIA_BOOT_MS)
    {
        return;
    }

    // ----------------------------------------------------------------------
    // PASSO 2: Varredura individual dos canais de protecao termica
    // ----------------------------------------------------------------------
    for (int i = 0; i < NUM_MOTORES; i++)
    {
        bool leituraFisica = (digitalRead(pinosSobrecarga[i]) == HIGH);

        // ==================================================================
        // ETAPA A: Qualificacao e Armacao Inicial da Linha Eletrica
        // ==================================================================
        if (!entradaSobrecargaArmada[i])
        {
            if (!leituraFisica)
            {
                // Pino em LOW: reinicia cronometro de falha e conta estabilidade
                tempoInicioNivelAltoBoot[i] = 0UL;

                if (tempoInicioNivelBaixo[i] == 0UL)
                {
                    tempoInicioNivelBaixo[i] = agora;
                }

                if (agora - tempoInicioNivelBaixo[i] >= TEMPO_VALIDACAO_ENTRADA_MS)
                {
                    // Se havia alarme detectado durante o boot, contabiliza duracao
                    if (sobrecargaDetectada[i] && tempoInicioSobrecarga[i] > 0UL)
                    {
                        tempoAcumuladoSobrecarga[i] +=
                            (agora - tempoInicioSobrecarga[i]) / 1000UL;
                    }

                    entradaSobrecargaArmada[i] = true;
                    sobrecargaDetectada[i] = false;
                    tempoInicioSobrecarga[i] = 0UL;
                    ultimoEstadoSobrecarga[i] = false;
                    ultimoTempoSobrecarga[i] = agora;
                    avisoEntradaInvalidaEmitido[i] = false;

                    Serial.printf("[SOBRECARGA] Entrada do motor %d validada em LOW. "
                                  "Protecao armada.\n",
                                  i + 1);
                }
            }
            else
            {
                // Pino em HIGH durante qualificacao: valida se eh falha real
                tempoInicioNivelBaixo[i] = 0UL;

                if (tempoInicioNivelAltoBoot[i] == 0UL)
                {
                    tempoInicioNivelAltoBoot[i] = agora;
                }

                // DENTRO DA ETAPA A (por volta da linha 380):
                if (agora - tempoInicioNivelAltoBoot[i] >= DEBOUNCE_DELAY)
                {
                    if (!sobrecargaDetectada[i])
                    {
                        sobrecargaDetectada[i] = true;
                        tempoInicioSobrecarga[i] = agora;
                        falhasSobrecargaTotal[i]++;
                        ultimoLogSobrecarga[i] = agora;

                        registrarEventoSobrecarga(i, true, true);
                        iaLocal.notificarEventoSeguranca(i, "sobrecarga");
                    }

                    if (!avisoEntradaInvalidaEmitido[i])
                    {
                        Serial.printf("[AVISO] Entrada do motor %d no GPIO %d em HIGH "
                                      "apos boot. Linha bloqueada.\n",
                                      i + 1, pinosSobrecarga[i]);
                        avisoEntradaInvalidaEmitido[i] = true;
                    }

                    if (canalEstaAtivo(i))
                    {
                        desativarMotor(i);
                    }
                }
            }

            // Mantem o canal em qualificacao ate completar 800 ms em LOW
            continue;
        }

        // ==================================================================
        // ETAPA B: Filtragem Digital por Debounce Temporal
        // ==================================================================
        if (leituraFisica != ultimoEstadoSobrecarga[i])
        {
            ultimoEstadoSobrecarga[i] = leituraFisica;
            ultimoTempoSobrecarga[i] = agora;
        }

        if (agora - ultimoTempoSobrecarga[i] >= DEBOUNCE_DELAY)
        {
            if (leituraFisica != sobrecargaDetectada[i])
            {
                sobrecargaDetectada[i] = leituraFisica;

                // DENTRO DA ETAPA B (por volta da linha 420):
                if (sobrecargaDetectada[i])
                {
                    tempoInicioSobrecarga[i] = agora;
                    falhasSobrecargaTotal[i]++;
                    ultimoLogSobrecarga[i] = agora;

                    registrarEventoSobrecarga(i, true, false);
                    iaLocal.notificarEventoSeguranca(i, "sobrecarga");
                    desativarMotor(i);
                }

                else
                {
                    if (tempoInicioSobrecarga[i] > 0UL)
                    {
                        tempoAcumuladoSobrecarga[i] +=
                            (agora - tempoInicioSobrecarga[i]) / 1000UL;
                    }

                    tempoInicioSobrecarga[i] = 0UL;
                    registrarEventoSobrecarga(i, false, false);
                }
            }
        }

        // ==================================================================
        // ETAPA C: Trava Continua de Intertravamento (Hard Lock)
        // ==================================================================
        if (sobrecargaDetectada[i])
        {
            if (canalEstaAtivo(i))
            {
                desativarMotor(i);
            }

            if (agora - ultimoLogSobrecarga[i] >= INTERVALO_LOG_FALHA_MS)
            {
                ultimoLogSobrecarga[i] = agora;
                Serial.printf("[SOBRECARGA] LEMBRETE: Motor %d segue em alarme "
                              "termico (GPIO %d HIGH).\n",
                              i + 1, pinosSobrecarga[i]);
            }
        }
    }
}

// ============================================================================
// SECAO 9: ROTINAS DE DIAGNOSTICO, AUDITORIA E TELEMETRIA
// ============================================================================

/**
 * isEntradaSobrecargaArmada()
 * ----------------------------------------------------------------------------
 * Informa se o canal ja concluiu a qualificacao inicial de 800 ms em LOW.
 */
bool isEntradaSobrecargaArmada(int motorIdx)
{
    if (motorIdx < 0 || motorIdx >= NUM_MOTORES)
        return false;
    return entradaSobrecargaArmada[motorIdx];
}

/**
 * getFalhasSobrecargaTotal()
 * ----------------------------------------------------------------------------
 * Retorna a quantidade de disparos termicos confirmados no canal.
 */
unsigned long getFalhasSobrecargaTotal(int motorIdx)
{
    if (motorIdx < 0 || motorIdx >= NUM_MOTORES)
        return 0UL;
    return falhasSobrecargaTotal[motorIdx];
}

/**
 * getDesarmesEmitidosSobrecarga()
 * ----------------------------------------------------------------------------
 * Retorna o total de pulsos de corte emergencial enviados por sobrecarga.
 */
unsigned long getDesarmesEmitidosSobrecarga(int motorIdx)
{
    if (motorIdx < 0 || motorIdx >= NUM_MOTORES)
        return 0UL;
    return desarmesEmitidos[motorIdx];
}

/**
 * getTempoTotalSobrecarga()
 * ----------------------------------------------------------------------------
 * Retorna o tempo acumulado em segundos que o canal permaneceu em falha.
 */
unsigned long getTempoTotalSobrecarga(int motorIdx)
{
    if (motorIdx < 0 || motorIdx >= NUM_MOTORES)
        return 0UL;

    unsigned long total = tempoAcumuladoSobrecarga[motorIdx];
    if (sobrecargaDetectada[motorIdx] && tempoInicioSobrecarga[motorIdx] > 0UL)
    {
        total += (millis() - tempoInicioSobrecarga[motorIdx]) / 1000UL;
    }
    return total;
}

/**
 * dumpEstadoSobrecarga()
 * ----------------------------------------------------------------------------
 * Imprime no monitor serial um relatorio diagnostico completo das entradas
 * de protecao termica e dos contadores de auditoria.
 */
void dumpEstadoSobrecarga()
{
    Serial.println("==============================================");
    Serial.println("[DIAGNOSTICO] Subsistema de Sobrecarga Termica");
    Serial.println("==============================================");

    for (int i = 0; i < NUM_MOTORES; i++)
    {
        bool leituraBruta = (digitalRead(pinosSobrecarga[i]) == HIGH);

        Serial.printf("Canal %d (GPIO %d)\n", i + 1, pinosSobrecarga[i]);
        Serial.printf("  Entrada armada   : %s\n",
                      entradaSobrecargaArmada[i] ? "SIM" : "NAO");
        Serial.printf("  Leitura bruta    : %s\n",
                      leituraBruta ? "HIGH (Disparo)" : "LOW (Normal)");
        Serial.printf("  Alarme ativo     : %s\n",
                      sobrecargaDetectada[i] ? "SIM" : "NAO");
        Serial.printf("  Desarme pendente : %s\n",
                      desarmePendente[i] ? "SIM" : "NAO");
        Serial.printf("  Falhas totais    : %lu\n", falhasSobrecargaTotal[i]);
        Serial.printf("  Desarmes pulso   : %lu (adiados: %lu)\n",
                      desarmesEmitidos[i], desarmesAdiados[i]);
        Serial.printf("  Tempo em falha   : %lu s\n", getTempoTotalSobrecarga(i));
        Serial.println("----------------------------------------------");
    }
}

/**
 * resetarContadoresSobrecarga()
 * ----------------------------------------------------------------------------
 * Zera as metricas volateis de auditoria apos manutencao preventiva.
 */
void resetarContadoresSobrecarga()
{
    for (int i = 0; i < NUM_MOTORES; i++)
    {
        falhasSobrecargaTotal[i] = 0UL;
        desarmesEmitidos[i] = 0UL;
        desarmesAdiados[i] = 0UL;
        tempoAcumuladoSobrecarga[i] = 0UL;
    }
    Serial.println("[SOBRECARGA] Contadores de auditoria zerados.");
}

// ============================================================================
// FIM DO ARQUIVO: src/sobrecarga.cpp
// ENERGIZAR - PROTECAO TERMICA INDUSTRIAL NAO-BLOQUEANTE
// ============================================================================
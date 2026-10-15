/**
 * ============================================================================
 * PROJETO: EnergizAR - Sistema de Automacao e Monitoramento ESP32
 * ARQUIVO: src/ligadesliga.cpp
 * ----------------------------------------------------------------------------
 * DESCRICAO DO SUBSISTEMA:
 *   Controlador central de potencia, acionamento de contatores e gestao de
 *   compressores industriais. Este modulo atua como o nucleo de controle de
 *   potencia da planta, operando sob regime cooperativo estritamente nao-
 *   bloqueante (sem uso de delay ou lacos de espera travantes), garantindo que
 *   o Task Watchdog Timer (TWDT) e os servidores web assincronos permaneçam
 *   sempre responsivos.
 *
 * RESPONSABILIDADES DO MODULO:
 *   1. GERACAO DE PULSOS DE COMANDO (500 ms):
 *      Gera pulsos eletricos transitórios de 500 ms nas bobinas de acionamento
 *      dos contatores industriais com temporizacao diferencial por millis(),
 *      evitando que reles fiquem permanentemente energizados e reduzindo
 *      o estresse termico nos drivers de comutacao.
 *
 *   2. PREVENCAO CONTRA CURTO-CICLO (TEMPORIZADORES DE DESCANSO):
 *      Controla janelas de tempo de seguranca apos operacoes de ligar (60 s)
 *      e desligar (10 s) para permitir equalizacao de pressoes mecânicas nos
 *      cabecotes dos compressores antes de qualquer nova partida.
 *
 *   3. TELEMETRIA E HORIMETRO ACUMULADOR EM RAM:
 *      Contabiliza segundos acumulados de operacao de cada motor e ciclos de
 *      chaveamento de contatores sem degradar a memoria Flash, utilizando
 *      estrutura de telemetria volátil com sincronizacao controlada.
 *
 *   4. PERSISTENCIA SEGURA EM SPIFFS COM MEMORY CACHE (WEAR LEVELING):
 *      Implementa espelhamento em memoria RAM de todos os arquivos de estado
 *      (/motorX.txt, /statusX.txt, /timerX.txt). Toda escrita redundante
 *      de valor identico ao ja existente no cache eh descartada, poupando
 *      os blocos de silicio da memoria Flash contra desgaste prematuro.
 *
 *   5. INTERTRAVAMENTO INDUSTRIAL E SEGURANCA ATIVA:
 *      Impede ligamento e forca desligamento geral imediato perante:
 *        a) Modo de Manutencao ativado pela chave fisica (GPIO 4);
 *        b) Disparo de pressostato de seguranca na linha de ar (GPIO 17);
 *        c) Sobrecorrente confirmada por rele termico bimetalico;
 *        d) Janela de operacao fora do horario fabril (08:00 as 22:00).
 *
 *   6. DESACOPLAMENTO INTELIGENTE DO FEEDBACK FISICO (CORRECAO CRITICA):
 *      O monitoramento dos contatos auxiliares dos contatores (NA) foi
 *      desacoplado da variavel soberana de intencao do usuario. O retorno
 *      eletrico serve para auditoria, diagnostico e contagem de horimetro,
 *      eliminando a falha que desligava o motor apos 4 segundos quando em
 *      bancada ou durante o tempo de transicao mecanica do contator.
 *
 * MAPA DE ENTRADAS E SAIDAS (PINOUT FINAL DA VERSAO CONSOLIDADA):
 *   - Motor 0 (Compressor Principal) : Pulso GPIO 23 | Feedback GPIO 14
 *   - Motor 1 (Motor Ventilador)     : Pulso GPIO 27 | Feedback GPIO 26
 *   - Motor 2 (Motor Secador de Ar)  : Pulso GPIO 33 | Feedback GPIO 32
 *
 * TABELA GERAL DE ALOCACAO DE PINOS DO PROJETO (ISOLAMENTO COMPLETO):
 *   GPIO 04 : Chave Fisica de Manutencao (Entrada com Debounce)
 *   GPIO 14 : Feedback Auxiliar Contator Motor 0 (Entrada Pulldown)
 *   GPIO 17 : Sensor de Pressostato de Seguranca (Entrada Pulldown)
 *   GPIO 18 : Rele Termico Sobrecarga Motor 2 (Entrada Pulldown)
 *   GPIO 19 : Rele Termico Sobrecarga Motor 0 (Entrada Pulldown)
 *   GPIO 23 : Saida de Pulso Rele Motor 0 (Saida Digital Push-Pull)
 *   GPIO 25 : Rele Termico Sobrecarga Motor 1 (Entrada Pulldown)
 *   GPIO 26 : Feedback Auxiliar Contator Motor 1 (Entrada Pulldown)
 *   GPIO 27 : Saida de Pulso Rele Motor 1 (Saida Digital Push-Pull)
 *   GPIO 32 : Feedback Auxiliar Contator Motor 2 (Entrada Pulldown)
 *   GPIO 33 : Saida de Pulso Rele Motor 2 (Saida Digital Push-Pull)
 *
 * HISTORICO DE REVISOES:
 *   v1.0.0 - Implementacao inicial de pulsos e persistencia basica.
 *   v2.0.0 - Adicao de cache em RAM para reducao de desgaste da Flash.
 *   v2.1.0 - Correcao de colisao de hardware no GPIO 13 (movido para 23).
 *   v2.2.0 - Desacoplamento da auto-derrubada de estado pelo feedback.
 * ============================================================================
 */

// ============================================================================
// SECAO 1: BIBLIOTECAS, CABECALHOS E DEPENDENCIAS DO SISTEMA
// ============================================================================

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <string.h>

#include "ligadesliga.h"
#include "autenticador.h"
#include "tempo.h"
#include "manutencao.h"
#include "pressostato.h"
#include "sobrecarga.h"

// ============================================================================
// SECAO 2: DEFINICOES DE HARDWARE, MAPA DE PINOS E ALIASES GLOBAIS
// ============================================================================

/**
 * Pinos de SAIDA digital dedicados ao chaveamento dos reles de comando:
 *   Indice 0 -> Motor 1 (Compressor Principal) : GPIO 23
 *   Indice 1 -> Motor 2 (Motor Ventilador)     : GPIO 27
 *   Indice 2 -> Motor 3 (Motor Secador de Ar)  : GPIO 33
 *
 * Justificativa de Engenharia:
 *   GPIO 23, 27 e 33 sao pinos de saida nativos e seguros que nao interferem
 *   com funcoes de strapping do ESP32 nem com o rádio Wi-Fi integrado.
 */
int pinosMotores[NUM_MOTORES] = {23, 27, 33};

/**
 * Pinos de ENTRADA digital para leitura dos contatos auxiliares NA dos contatores:
 *   Indice 0 -> Retorno Feedback Motor 1 : GPIO 14
 *   Indice 1 -> Retorno Feedback Motor 2 : GPIO 26
 *   Indice 2 -> Retorno Feedback Motor 3 : GPIO 32
 *
 * Justificativa de Engenharia:
 *   Todos configurados em modo INPUT_PULLDOWN para assegurar nivel 0V (LOW)
 *   estavel na ausencia de fechamento fisico de contato mecânico.
 */
int pinosStatus[NUM_MOTORES] = {14, 26, 32};

/**
 * Aliases globais mantidos para retrocompatibilidade com modulos antigos.
 */
const int NUM_MOTORS = NUM_MOTORES;
const int *motorPins = pinosMotores;

// ============================================================================
// SECAO 3: ESTRUTURAS DE ESTADO OPERACIONAL EM MEMORIA RAM VOLATIL
// ============================================================================

/**
 * Vetor mestre com a intencao logica soberana de funcionamento dos motores:
 *   true  -> Motor com comando de funcionamento ATIVO (Ligado)
 *   false -> Motor com comando de funcionamento INATIVO (Desligado)
 */
bool motoresLigados[NUM_MOTORES] = {false, false, false};

/**
 * Sinalizador indicando se o temporizador anti-curto-ciclo esta ativo.
 */
bool timersAtivos[NUM_MOTORES] = {false, false, false};

/**
 * Snapshot do estado do ciclo imediatamente anterior para deteccao de borda.
 */
bool motoresEstadoAnterior[NUM_MOTORES] = {false, false, false};

/**
 * Timestamp absoluto em milissegundos (millis) do encerramento do descanso.
 */
unsigned long motorTimers[NUM_MOTORES] = {0UL, 0UL, 0UL};

// ----------------------------------------------------------------------------
// Caminhos canônicos dos arquivos persistidos no sistema de arquivos SPIFFS
// ----------------------------------------------------------------------------
extern const String arquivosMotores[NUM_MOTORES] = {
    "/motor1.txt",
    "/motor2.txt",
    "/motor3.txt"};

extern const String arquivosStatus[NUM_MOTORES] = {
    "/status1.txt",
    "/status2.txt",
    "/status3.txt"};

extern const String arquivosTimers[NUM_MOTORES] = {
    "/timer1.txt",
    "/timer2.txt",
    "/timer3.txt"};

// ============================================================================
// SECAO 4: CONSTANTES DE TEMPORIZACAO INDUSTRIAL E PARAMETROS DE FILTRO
// ============================================================================

/**
 * Tempo minimo de descanso obrigatorio pos-partida (curto-ciclo) em segundos.
 */
const unsigned long LIGAR_TIMEOUT = 3600UL;

/**
 * Tempo minimo de descanso obrigatorio pos-parada (despressurizacao) em segundos.
 */
const unsigned long DESLIGAR_TIMEOUT = 600UL;

/**
 * Largura do pulso de relé enviada as bobinas dos contatores em milissegundos.
 */
const unsigned long DURACAO_PULSO_MS = 500UL;

/**
 * Janela temporal de debounce digital contra centelhamento de contatos (ms).
 */
const unsigned long DEBOUNCE_STATUS_MS = 100UL;

/**
 * Periodo de tolerancia de transicao eletromecanica pos-comando manual (ms).
 */
const unsigned long TOLERANCIA_COMANDO_MS = 4000UL;

// ============================================================================
// SECAO 5: VARIAVEIS INTERNAS DE CONTROLE, PULSOS, CACHE E TELEMETRIA
// ============================================================================

/** Indicador de pulso eletrico em andamento em cada canal */
static bool pulsoAtivo[NUM_MOTORES] = {false, false, false};

/** Instante em que o pulso logico foi elevado para nivel HIGH */
static unsigned long pulsoInicio[NUM_MOTORES] = {0UL, 0UL, 0UL};

/** Ultimo estado estavel registrado na entrada do contato auxiliar */
static bool ultimoStatusLido[NUM_MOTORES] = {false, false, false};

/** Instante da ultima oscilacao bruta observada no pino de feedback */
static unsigned long tempoFiltroStatus[NUM_MOTORES] = {0UL, 0UL, 0UL};

/** Cache em RAM do estado gravado em /motorX.txt (-1 = invalido) */
static int cacheEstadoMotor[NUM_MOTORES] = {-1, -1, -1};

/** Cache em RAM do estado gravado em /statusX.txt (-1 = invalido) */
static int cacheEstadoStatus[NUM_MOTORES] = {-1, -1, -1};

/** Cache em RAM do temporizador gravado em /timerX.txt */
static unsigned long cacheTimers[NUM_MOTORES] = {0UL, 0UL, 0UL};

/** Instante de tempo (millis) em que a presente sessao de carga iniciou */
static unsigned long tempoInicioOperacao[NUM_MOTORES] = {0UL, 0UL, 0UL};

/** Segundos acumulados de operacao continua registrados desde a partida */
static unsigned long totalSegundosOperacao[NUM_MOTORES] = {0UL, 0UL, 0UL};

/** Contador do total de manobras de acionamento realizadas no canal */
static unsigned long ciclosAcionamento[NUM_MOTORES] = {0UL, 0UL, 0UL};

/** Timestamp do ultimo comando de acionamento emitido pela interface web */
static unsigned long comandoManualTimestamp[NUM_MOTORES] = {0UL, 0UL, 0UL};

/** Sinalizador que assegura o registro unico da rota assincrona /toggle */
static bool toggleRouteConfigurada = false;

// ============================================================================
// SECAO 6: MECANISMO DE GERACAO DE PULSOS DE COMANDO (NAO-BLOQUEANTE)
// ============================================================================

/**
 * dispararPulsoMotor()
 * ----------------------------------------------------------------------------
 * Aplica nivel logico HIGH no pino da bobina correspondente e registra o
 * instante inicial do pulso. A transicao de retorno para LOW ocorre de
 * maneira estritamente cooperativa e sem delay por processarPulsosMotores().
 *
 * @param motorIdx Indice do motor alvo (0 a NUM_MOTORES - 1).
 */
void dispararPulsoMotor(int motorIdx)
{
    // Validacao defensiva do indice para impedir violacao de memoria
    if (motorIdx < 0 || motorIdx >= NUM_MOTORES)
    {
        return;
    }

    // Energiza o pino de comando da bobina
    digitalWrite(pinosMotores[motorIdx], HIGH);
    pulsoAtivo[motorIdx] = true;
    pulsoInicio[motorIdx] = millis();

    Serial.printf("[HARDWARE_PULSO] Motor %d (GPIO %d) -> Nivel HIGH iniciado.\n",
                  motorIdx + 1, pinosMotores[motorIdx]);
}

/**
 * processarPulsosMotores()
 * ----------------------------------------------------------------------------
 * Funcao de temporizacao cooperativa de sub-milissegundo chamada continuamente
 * a partir do loop() principal. Audita os tempos de duracao dos pulsos e
 * desliga comutando para LOW assim que a janela de 500 ms eh completada.
 */
void processarPulsosMotores()
{
    unsigned long agora = millis();

    for (int i = 0; i < NUM_MOTORES; i++)
    {
        // Ignora canais sem pulso ativo em execucao
        if (!pulsoAtivo[i])
        {
            continue;
        }

        // Verifica se a janela de temporizacao do pulso foi alcancada
        if (agora - pulsoInicio[i] >= DURACAO_PULSO_MS)
        {
            digitalWrite(pinosMotores[i], LOW);
            pulsoAtivo[i] = false;

            Serial.printf("[HARDWARE_PULSO] Motor %d (GPIO %d) -> Nivel LOW finalizado (%lu ms).\n",
                          i + 1, pinosMotores[i], DURACAO_PULSO_MS);
        }
    }
}

// ============================================================================
// SECAO 7: CAMADA DE PERSISTENCIA SPIFFS COM WEAR-LEVELING POR CACHE EM RAM
// ============================================================================

/**
 * initSPIFFS()
 * ----------------------------------------------------------------------------
 * Inicializa a montagem do sistema de arquivos na particao Flash SPIFFS.
 * Emprega o sinalizador true para autorizar formatação automatica caso o
 * sistema de arquivos se encontre ausente ou corrompido em nova montagem.
 */
static void initSPIFFS()
{
    if (!SPIFFS.begin(true))
    {
        Serial.println("[SPIFFS] ERRO CRITICO: Falha ao montar sistema SPIFFS.");
    }
    else
    {
        Serial.println("[SPIFFS] Particao SPIFFS montada e operacional.");
    }
}

/**
 * readMotorState()
 * ----------------------------------------------------------------------------
 * Realiza a leitura e decodificacao textual do estado booleano do motor
 * armazenado na particao SPIFFS.
 *
 * @param arquivoEstado Caminho relativo do arquivo (ex: "/motor1.txt").
 * @return true se o estado contiver "1", "true" ou "ON"; false caso contrario.
 */
bool readMotorState(const String &arquivoEstado)
{
    // Caso o arquivo nao exista na Flash, assume motor desligado por seguranca
    if (!SPIFFS.exists(arquivoEstado))
    {
        return false;
    }

    File file = SPIFFS.open(arquivoEstado, "r");
    if (!file)
    {
        return false;
    }

    String state = file.readStringUntil('\n');
    file.close();
    state.trim();

    return (state == "1" || state.equalsIgnoreCase("true") || state.equalsIgnoreCase("ON"));
}

/**
 * saveMotorState()
 * ----------------------------------------------------------------------------
 * Grava o estado logico operacional do motor no arquivo SPIFFS correspondente.
 * Realiza comparacao previa com o vetor de cache em memoria RAM: caso o estado
 * a ser gravado seja identico ao ja armazenado, a gravacao fisica na Flash
 * eh completamente evitada, mitigando o desgaste do silicio.
 *
 * @param arquivoEstado Caminho do arquivo a gravar.
 * @param state         Novo estado logico (true = Ligado, false = Desligado).
 */
void saveMotorState(const String &arquivoEstado, bool state)
{
    int idx = -1;
    for (int i = 0; i < NUM_MOTORES; i++)
    {
        if (arquivosMotores[i] == arquivoEstado)
        {
            idx = i;
            break;
        }
    }

    // Curto-circuito de protecao da Flash: se o valor coincide com o cache, sai
    if (idx != -1 && cacheEstadoMotor[idx] == (state ? 1 : 0))
    {
        return;
    }

    File file = SPIFFS.open(arquivoEstado, "w");
    if (!file)
    {
        Serial.println("[SPIFFS] Falha na abertura para gravacao: " + arquivoEstado);
        return;
    }

    file.println(state ? "1" : "0");
    file.close();

    // Sincroniza o cache em memoria RAM
    if (idx != -1)
    {
        cacheEstadoMotor[idx] = state ? 1 : 0;
    }

    Serial.printf("[SPIFFS] Estado persistido em %s: %s\n",
                  arquivoEstado.c_str(), state ? "Ligado (1)" : "Desligado (0)");
}

/**
 * saveStatusState()
 * ----------------------------------------------------------------------------
 * Armazena a condicao eletrica observada na entrada do contato auxiliar do
 * contator. Utiliza protecao por cache para evitar regravacoes na Flash.
 *
 * @param pin   Pino de entrada de status avaliado.
 * @param state Nivel eletrico detectado (true = ON, false = OFF).
 */
void saveStatusState(int pin, bool state)
{
    int idx = -1;
    for (int i = 0; i < NUM_MOTORES; i++)
    {
        if (pin == pinosStatus[i])
        {
            idx = i;
            break;
        }
    }

    if (idx == -1)
    {
        return;
    }

    // Interrompe caso o cache confirme que o valor fisico persiste inalterado
    if (cacheEstadoStatus[idx] == (state ? 1 : 0))
    {
        return;
    }

    File file = SPIFFS.open(arquivosStatus[idx], "w");
    if (!file)
    {
        return;
    }

    file.println(state ? "ON" : "OFF");
    file.close();

    cacheEstadoStatus[idx] = (state ? 1 : 0);
}

/**
 * readTimerState()
 * ----------------------------------------------------------------------------
 * Recupera da particao SPIFFS o saldo temporal restante de um temporizador de
 * descanso que estava ativo antes de uma eventual reinicializacao da CPU.
 *
 * @param arquivoTimer Caminho do arquivo de temporizador (ex: "/timer1.txt").
 * @return Saldo de tempo remanescente em milissegundos.
 */
unsigned long readTimerState(const String &arquivoTimer)
{
    if (!SPIFFS.exists(arquivoTimer))
    {
        return 0UL;
    }

    File file = SPIFFS.open(arquivoTimer, "r");
    if (!file)
    {
        return 0UL;
    }

    String remainingTime = file.readStringUntil('\n');
    file.close();
    remainingTime.trim();

    return (unsigned long)remainingTime.toInt();
}

/**
 * saveTimerState()
 * ----------------------------------------------------------------------------
 * Persiste o tempo restante do descanso anti-curto-ciclo. Possui filtro para
 * evitar a reescrita desnecessaria do valor zero sobre zero repetidas vezes.
 *
 * @param arquivoTimer  Caminho do arquivo de persistencia.
 * @param remainingTime Tempo restante em milissegundos a registrar.
 */
void saveTimerState(const String &arquivoTimer, unsigned long remainingTime)
{
    int idx = -1;
    for (int i = 0; i < NUM_MOTORES; i++)
    {
        if (arquivosTimers[i] == arquivoTimer)
        {
            idx = i;
            break;
        }
    }

    // Descarta gravacoes repetitivas de zero milissegundos
    if (idx != -1 && cacheTimers[idx] == 0UL && remainingTime == 0UL)
    {
        return;
    }

    File file = SPIFFS.open(arquivoTimer, "w");
    if (!file)
    {
        return;
    }

    file.println(remainingTime);
    file.close();

    if (idx != -1)
    {
        cacheTimers[idx] = remainingTime;
    }
}

// ============================================================================
// SECAO 8: UTILITARIOS DE HORARIO E VALIDACAO DE JANELAS DE EXPEDIENTE
// ============================================================================

/**
 * getHoursFromTime()
 * ----------------------------------------------------------------------------
 * Extrai a componente de hora inteira (0 a 23) a partir de cadeias textuais
 * padronizadas em ISO 8601 ou representacoes simples HH:MM.
 *
 * @param time Cadeia contendo o horario formatado.
 * @return Hora em numero inteiro ou -1 em caso de formato ilegitimo.
 */
static int getHoursFromTime(const String &time)
{
    if (time.length() >= 13 && time.indexOf('T') != -1)
    {
        int tIndex = time.indexOf('T');
        return time.substring(tIndex + 1, tIndex + 3).toInt();
    }
    else if (time.length() >= 5 && time.indexOf(':') != -1)
    {
        return time.substring(0, time.indexOf(':')).toInt();
    }
    return -1;
}

/**
 * getMinutesFromTime()
 * ----------------------------------------------------------------------------
 * Extrai os minutos inteiros (0 a 59) da cadeia textual de horario.
 *
 * @param time Cadeia contendo o horario formatado.
 * @return Minutos em numero inteiro ou -1 caso parsing falhe.
 */
static int getMinutesFromTime(const String &time)
{
    if (time.length() >= 16 && time.indexOf('T') != -1)
    {
        int tIndex = time.indexOf('T');
        return time.substring(tIndex + 4, tIndex + 6).toInt();
    }
    else if (time.length() >= 5 && time.indexOf(':') != -1)
    {
        int firstColon = time.indexOf(':');
        return time.substring(firstColon + 1, firstColon + 3).toInt();
    }
    return -1;
}

/**
 * isAfterClosingTime()
 * ----------------------------------------------------------------------------
 * Avalia se o relogio ultrapassou o horario de encerramento fabril (22:00).
 * Se o relogio de hardware ainda nao concluiu sincronismo NTP, nao bloqueia.
 *
 * @return true se o horario atual for igual ou posterior as 22:00.
 */
bool isAfterClosingTime()
{
    if (!isTimeSynchronized())
    {
        return false;
    }

    String curTime = getTimeClient();
    int hour = getHoursFromTime(curTime);
    int minute = getMinutesFromTime(curTime);

    if (hour == -1)
    {
        return false;
    }

    return (hour > 22) || (hour == 22 && minute >= 0);
}

/**
 * isBeforeOpeningTime()
 * ----------------------------------------------------------------------------
 * Avalia se o relogio encontra-se antes do inicio do expediente fabril (08:00).
 *
 * @return true se o horario atual for anterior as 08:00.
 */
bool isBeforeOpeningTime()
{
    if (!isTimeSynchronized())
    {
        return false;
    }

    String curTime = getTimeClient();
    int hour = getHoursFromTime(curTime);

    if (hour == -1)
    {
        return false;
    }

    return (hour < 8);
}

/**
 * sistemaDeveEstarBloqueado()
 * ----------------------------------------------------------------------------
 * Consolidador mestre de diretivas de seguranca da planta industrial:
 * Bloqueia acionamentos caso a chave fisica de manutencao esteja comutada ou
 * o horario atual se situe fora da janela autorizada (08:00 as 22:00).
 *
 * @return true se o sistema estiver sob condicao impeditiva de operacao.
 */
bool sistemaDeveEstarBloqueado()
{
    return sistemaEmManutencao || isAfterClosingTime() || isBeforeOpeningTime();
}

/**
 * desligarTodosMotores()
 * ----------------------------------------------------------------------------
 * Comando global de corte de carga que forca a desativacao de todos os motores
 * da planta, disparando pulsos de abertura e persistindo estado desligado.
 */
void desligarTodosMotores()
{
    for (int i = 0; i < NUM_MOTORES; i++)
    {
        dispararPulsoMotor(i);
        motoresLigados[i] = false;
        saveMotorState(arquivosMotores[i], false);
    }
    Serial.println("[SISTEMA_MOTORES] Todos os motores foram desligados com sucesso.");
}

/**
 * desativarSistemaForaDoHorario()
 * ----------------------------------------------------------------------------
 * Rotina executada periodicamente para encerrar operacoes ativas de compressores
 * assim que o horario de expediente atingir o limite estipulado em fábrica.
 */
void desativarSistemaForaDoHorario()
{
    if (isAfterClosingTime() || isBeforeOpeningTime())
    {
        desligarTodosMotores();
        Serial.println("[HORARIO] Sistema desativado: Fora do horario permitido (08:00 as 22:00).");
    }
}

/**
 * atualizarEstadoMotores()
 * ----------------------------------------------------------------------------
 * Ponto de entrada de auditoria ciclica invocado pelo escalonador do loop():
 * Garante que nenhum motor permaneca energizado caso ocorra ativacao da chave
 * de manutencao ou violacao de horario operacional.
 */
void atualizarEstadoMotores()
{
    if (sistemaDeveEstarBloqueado())
    {
        desligarTodosMotores();
    }
}

// ============================================================================
// SECAO 9: GESTAO DOS TEMPORIZADORES ANTI-CURTO-CICLO E DESCANSO
// ============================================================================

/**
 * isMotorTimerActive()
 * ----------------------------------------------------------------------------
 * Checa a vigencia da contagem regressiva de seguranca do motor. Caso o tempo
 * estipulado tenha transcorrido por completo, a flag de descanso eh baixada e
 * o saldo eh redefinido para zero na particao de persistencia Flash.
 *
 * @param motorIdx Indice do motor analisado.
 * @return true se o temporizador estiver em contagem regressiva ativa.
 */
bool isMotorTimerActive(int motorIdx)
{
    if (motorIdx < 0 || motorIdx >= NUM_MOTORES)
    {
        return false;
    }

    // Caso o relogio de millis() ainda nao tenha alcancado a meta estipulada
    if (millis() < motorTimers[motorIdx])
    {
        return true;
    }

    // Caso o timer tenha expirado, normaliza o canal para novas ordens
    if (motorTimers[motorIdx] != 0UL && millis() >= motorTimers[motorIdx])
    {
        motorTimers[motorIdx] = 0UL;
        timersAtivos[motorIdx] = false;
        saveTimerState(arquivosTimers[motorIdx], 0UL);
    }

    return false;
}

/**
 * getMotorRemainingTime()
 * ----------------------------------------------------------------------------
 * Fornece o saldo temporal restante em segundos inteiros para ser exibido
 * nos indicadores de contagem regressiva do dashboard web do operador.
 *
 * @param motorIdx Indice do motor.
 * @return Segundos restantes de descanso; 0 se o motor estiver livre.
 */
unsigned long getMotorRemainingTime(int motorIdx)
{
    if (!isMotorTimerActive(motorIdx))
    {
        return 0UL;
    }
    return (motorTimers[motorIdx] - millis()) / 1000UL;
}

/**
 * updateTimers()
 * ----------------------------------------------------------------------------
 * Rotina executada periodicamente no laco principal (a cada 60 segundos) para
 * atualizar o saldo dos temporizadores em memoria e assegurar a gravacao de
 * contingencia na memoria Flash para restauracao confiavel pos-queda de rede.
 */
void updateTimers()
{
    unsigned long agora = millis();

    for (int i = 0; i < NUM_MOTORES; i++)
    {
        if (motorTimers[i] > agora)
        {
            unsigned long tempoRestante = motorTimers[i] - agora;
            saveTimerState(arquivosTimers[i], tempoRestante);
            timersAtivos[i] = true;
        }
        else if (timersAtivos[i])
        {
            saveTimerState(arquivosTimers[i], 0UL);
            timersAtivos[i] = false;
            motorTimers[i] = 0UL;
        }
    }
}

// ============================================================================
// SECAO 10: MONITORAMENTO DE FEEDBACK ELETRICO (DESACOPLADO DE AUTO-DESLIGAMENTO)
// ============================================================================

/**
 * monitorarStatusCompressores()
 * ----------------------------------------------------------------------------
 * Realiza a leitura e filtragem com debounce das entradas auxiliares dos
 * contatores mecânicos para fins de diagnostico industrial e telemetria.
 *
 * NOTA ARQUITETURAL DE CORRECAO CRITICA:
 *   Nesta versao, o nivel eletrico observado na entrada NAO forca o encerramento
 *   da variavel de comando 'motoresLigados[i]'. Essa alteracao elimina o
 *   comportamento anomalo onde o motor desligava sozinho apos 4 segundos quando
 *   o equipamento era operado em bancada de ensaios ou sem retorno cabeado.
 */
void monitorarStatusCompressores()
{
    unsigned long agora = millis();

    for (int i = 0; i < NUM_MOTORES; i++)
    {
        bool leituraRaw = (digitalRead(pinosStatus[i]) == HIGH);

        // Algoritmo de filtragem digital contra arco de comutacao mecanica
        if (leituraRaw != ultimoStatusLido[i])
        {
            tempoFiltroStatus[i] = agora;
            ultimoStatusLido[i] = leituraRaw;
        }

        // Processa atualizacoes exclusivamente apos superacao da janela de debounce
        if ((agora - tempoFiltroStatus[i]) > DEBOUNCE_STATUS_MS)
        {
            // Atualiza arquivo de telemetria fisica sem intervir no comando logico
            saveStatusState(pinosStatus[i], leituraRaw);

            // Horimetro acumulador: contabiliza tempo de marcha sob comando ativo
            if (motoresLigados[i] && tempoInicioOperacao[i] == 0UL)
            {
                tempoInicioOperacao[i] = agora;
                ciclosAcionamento[i]++;
            }
            else if (!motoresLigados[i] && tempoInicioOperacao[i] > 0UL)
            {
                totalSegundosOperacao[i] += (agora - tempoInicioOperacao[i]) / 1000UL;
                tempoInicioOperacao[i] = 0UL;
            }
        }
    }
}

/**
 * checkMotorStatusChange()
 * ----------------------------------------------------------------------------
 * Funcao de conveniencia para deteccao de borda em monitoramento de contato.
 *
 * @return false por definicao na arquitetura assincrona simplificada.
 */
bool checkMotorStatusChange()
{
    return false;
}

/**
 * updateMotorStatus()
 * ----------------------------------------------------------------------------
 * Sincroniza os arquivos de persistencia de feedback na particao Flash.
 */
void updateMotorStatus()
{
    for (int i = 0; i < NUM_MOTORES; i++)
    {
        bool feedbackFisico = (digitalRead(pinosStatus[i]) == HIGH);
        saveStatusState(pinosStatus[i], feedbackFisico);
    }
}

// ============================================================================
// SECAO 11: CONTROLADOR HTTP REST ASSINCRONO DE ACIONAMENTO (/toggle)
// ============================================================================

/**
 * handleToggleAction()
 * ----------------------------------------------------------------------------
 * Registra o despachante de requisicoes HTTP responsavel por processar ordens
 * remotas autenticadas de comutacao (LIGAR e DESLIGAR) enviadas pelo painel.
 *
 * @param server Instancia de referencia do servidor web assincrono ESPAsyncWebServer.
 */
void handleToggleAction(AsyncWebServer &server)
{
    if (toggleRouteConfigurada)
    {
        return;
    }
    toggleRouteConfigurada = true;

    server.on("/toggle", HTTP_ANY, [](AsyncWebServerRequest *request)
              {
        // 1. Validacao de seguranca de credencial autenticada via sessao
        if (!isAuthenticated(request))
        {
            redirectToAccessDenied(request);
            return;
        }

        // 2. Avaliacao de bloqueio geral (manutencao fisica ou fora de expediente)
        if (sistemaDeveEstarBloqueado())
        {
            request->send(200, "text/plain",
                          "Sistema bloqueado: Manutencao ativada ou fora do horario.");
            return;
        }

        // 3. Confirmacao de argumentos mandatorios no envelope HTTP
        if (!request->hasParam("motor") || !request->hasParam("action"))
        {
            request->send(400, "text/plain", "Parametros de requisicao ausentes.");
            return;
        }

        int motorIdx = request->getParam("motor")->value().toInt() - 1;
        String action = request->getParam("action")->value();
        action.toLowerCase();

        // 4. Verificacao de integridade de indice de canal
        if (motorIdx < 0 || motorIdx >= NUM_MOTORES)
        {
            request->send(400, "text/plain", "Identificador de motor invalido.");
            return;
        }

        // 5. Intertravamento por sobrecarga termica bimetalica
        if (sobrecargaDetectada[motorIdx])
        {
            request->send(200, "text/plain",
                          "Comando rejeitado: Motor sob alarme de sobrecarga termica.");
            return;
        }

        // 6. Intertravamento do compressor por pressostato de seguranca
        if (motorIdx == 0 && pressostatoAtivo)
        {
            request->send(200, "text/plain",
                          "Comando rejeitado: Pressostato em condicao de alarme critico.");
            return;
        }

        // 7. Intertravamento por descanso anti-curto-ciclo ativo
        if (isMotorTimerActive(motorIdx))
        {
            request->send(200, "text/plain",
                          "Aguarde o temporizador de descanso deste motor finalizar.");
            return;
        }

        // Registra o instante do comando manual para rastreabilidade
        comandoManualTimestamp[motorIdx] = millis();

        // ====================================================================
        // RAMO OPERACIONAL: ORDEM DE LIGAR
        // ====================================================================
        if (action == "ligar")
        {
            // Emite o pulso de comando de hardware de 500 ms
            dispararPulsoMotor(motorIdx);

            // Grava a intencao logica soberana em RAM
            motoresLigados[motorIdx] = true;

            // Arma o temporizador de descanso anti-curto-ciclo pos-partida
            motorTimers[motorIdx] = millis() + (LIGAR_TIMEOUT * 1000UL);
            timersAtivos[motorIdx] = true;

            // Persiste o timer e o novo estado operacional na Flash com cache
            saveTimerState(arquivosTimers[motorIdx], LIGAR_TIMEOUT * 1000UL);
            saveMotorState(arquivosMotores[motorIdx], true);

            Serial.printf("[WEB] Motor %d: Comando LIGAR aceito. Estado: LIGADO soberano.\n",
                          motorIdx + 1);

            request->send(200, "text/plain",
                          "Motor " + String(motorIdx + 1) + " ligado com sucesso!");
        }
        // ====================================================================
        // RAMO OPERACIONAL: ORDEM DE DESLIGAR
        // ====================================================================
        else if (action == "desligar")
        {
            // Emite o pulso de comando de hardware de 500 ms
            dispararPulsoMotor(motorIdx);

            // Grava a intencao logica soberana em RAM
            motoresLigados[motorIdx] = false;

            // Arma o temporizador de descanso anti-curto-ciclo pos-parada
            motorTimers[motorIdx] = millis() + (DESLIGAR_TIMEOUT * 1000UL);
            timersAtivos[motorIdx] = true;

            // Persiste o timer e o novo estado operacional na Flash com cache
            saveTimerState(arquivosTimers[motorIdx], DESLIGAR_TIMEOUT * 1000UL);
            saveMotorState(arquivosMotores[motorIdx], false);

            Serial.printf("[WEB] Motor %d: Comando DESLIGAR aceito. Estado: DESLIGADO.\n",
                          motorIdx + 1);

            request->send(200, "text/plain",
                          "Motor " + String(motorIdx + 1) + " desligado com sucesso!");
        }
        // ====================================================================
        // RAMO EXCEPCIONAL: COMANDO NAO RECONHECIDO
        // ====================================================================
        else
        {
            request->send(400, "text/plain", "Acao de comando solicitada e invalida.");
        } });
}

// ============================================================================
// SECAO 12: INICIALIZACAO GERAL DO SUBSISTEMA LIGA/DESLIGA NO SETUP
// ============================================================================

/**
 * setupLigaDesliga()
 * ----------------------------------------------------------------------------
 * Orquestrador de energizacao do subsistema de controle de compressores:
 *   1. Inicializa o sistema de persistencia em particao Flash SPIFFS;
 *   2. Configura a direcao dos GPIOs (OUTPUT para reles, INPUT_PULLDOWN status);
 *   3. Recupera os ultimos estados operacionais salvos na memoria nao-volatil;
 *   4. Restaura temporizadores de descanso que se encontravam ativos no boot;
 *   5. Vincula os controladores assincronos HTTP REST;
 *   6. Dispara o cliente SNTP e aplica os intertravamentos fabris preliminares.
 *
 * @param server Instancia do servidor web assincrono central.
 */
void setupLigaDesliga(AsyncWebServer &server)
{
    Serial.println("[SETUP] Configurando sistema de acionamento Liga/Desliga...");

    // 1. Inicializacao do sistema de arquivos de persistencia
    initSPIFFS();

    // 2. Configuracao eletrica de hardware dos GPIOs de potencia e feedback
    for (int i = 0; i < NUM_MOTORES; i++)
    {
        pinMode(pinosMotores[i], OUTPUT);
        digitalWrite(pinosMotores[i], LOW);

        pinMode(pinosStatus[i], INPUT_PULLDOWN);

        // Restaura em RAM o estado gravado antes da interrupcao de energia
        motoresLigados[i] = readMotorState(arquivosMotores[i]);

        Serial.printf("[SETUP] Motor %d restaurado no boot como: %s (Pino Saida GPIO %d)\n",
                      i + 1, motoresLigados[i] ? "LIGADO" : "DESLIGADO", pinosMotores[i]);
    }

    // 3. Restauracao de saldos remanescentes dos temporizadores de protecao
    for (int i = 0; i < NUM_MOTORES; i++)
    {
        unsigned long tempoRestante = readTimerState(arquivosTimers[i]);
        if (tempoRestante > 0UL)
        {
            motorTimers[i] = millis() + tempoRestante;
            timersAtivos[i] = true;
            Serial.printf("[TIMER] Motor %d restaurou descanso de %lu ms restantes.\n",
                          i + 1, tempoRestante);
        }
    }

    // 4. Registro da rota HTTP REST assincrona para despachos de acionamento
    handleToggleAction(server);

    // 5. Inicializacao do cliente de sincronizacao de relogio NTP
    setupTimeClient();

    // 6. Aplicacao imediata de travas e regras de horario
    atualizarEstadoMotores();

    Serial.println("[SETUP] Modulo Liga/Desliga inicializado com sucesso.");
}

// ============================================================================
// FIM DO ARQUIVO: src/ligadesliga.cpp
// ENERGIZAR - SISTEMA DE AUTOMACAO INDUSTRIAL E MONITORAMENTO DE COMPRESSORES
// ============================================================================
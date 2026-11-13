/**
 * ============================================================================
 * PROJETO: EnergizAR - Sistema de Automacao e Monitoramento ESP32
 * ARQUIVO: src/pressostato.cpp
 * DESCRICAO: Modulo de monitoramento e intertravamento do pressostato de
 *            seguranca da linha de gas/oleo do compressor (Motor 0).
 *            Implementa filtro de ruido (debounce 200 ms), desligamento
 *            assincrono respeitando a guarda de re-disparo do ligadesliga e
 *            armacao de descanso de 10 minutos apos o corte.
 *
 * RESPONSABILIDADES DO MODULO:
 *   1. LEITURA COM DEBOUNCE:
 *      Amostra o contato seco do pressostato (GPIO 17, INPUT_PULLDOWN) a cada
 *      ciclo do loop principal. HIGH = alarme disparado. Uma janela de 200 ms
 *      absorve repiques mecanicos do contato e evita falso disparo por ruido
 *      induzido na fiacao industrial.
 *
 *   2. ACAO DE DESLIGAMENTO (HARD INTERLOCK):
 *      Ao confirmar transicao LOW->HIGH:
 *        a) Registra o evento no log serial com contexto operacional;
 *        b) Se o compressor estiver ligado, emite pulso de desligamento
 *           via dispararPulsoMotor(0) - que respeita a guarda de re-disparo
 *           (nao estende janela de 500 ms se um pulso ON ainda estiver
 *           em curso; o proximo ciclo tentara de novo);
 *        c) Consolida motoresLigados[0] = false e persiste /motor1.txt;
 *        d) Arma DESLIGAR_TIMEOUT (600 s = 10 min) via helper centralizado
 *           iniciarTimerMotorSegundos(), impedindo religamento imediato
 *           enquanto o cabecote nao equalizou a pressao.
 *
 *   3. TRAVA CONTINUA DE SEGURANCA:
 *      Enquanto pressostatoAtivo == true, a cada ciclo o modulo verifica se
 *      algum comando externo tentou religar o motor 0. Se sim, o desligamento
 *      e reaplicado imediatamente (defesa em profundidade contra comandos
 *      web concorrentes ou religamento manual forcado).
 *
 *   4. DIAGNOSTICO PERIODICO:
 *      A cada 15 segundos o modulo audita a coerencia entre a flag soberana
 *      pressostatoAtivo e o feedback fisico em GPIO 14. Se detectar
 *      incoerencia (pressostato ativo mas contator fechado), forca novo
 *      desligamento e registra alerta para diagnostico de campo.
 *
 *   5. ROBUSTEZ A ROLLOVER DE millis():
 *      Todas as comparacoes temporais usam o idioma canonico Arduino com
 *      diferenca assinada, imune ao estouro de 32 bits apos ~49,7 dias.
 *
 * HISTORICO DE REVISOES:
 *   v1.0.0 - Implementacao inicial com debounce e desligamento simples.
 *   v1.1.0 - Respeito a guarda de re-disparo; helper de desligamento
 *            centralizado; armacao de DESLIGAR_TIMEOUT via helper; logs
 *            de incoerencia; rollover de millis neutralizado.
 * ============================================================================
 */

#include "pressostato.h"
#include "ligadesliga.h"
#include "ia_local.h"

// ============================================================================
// SECAO 1: DEFINICOES DE HARDWARE E CONSTANTES
// ============================================================================

/**
 * Pino digital de entrada conectado ao contato do pressostato mecanico.
 * Configurado como INPUT_PULLDOWN: em repouso (contato aberto) le LOW,
 * quando a pressao atinge o limiar critico o contato fecha e le HIGH.
 */
const int pinoPressostato = 17;

/**
 * Flag global soberana de alarme do pressostato.
 * Publicada no endpoint /motor-state e consultada pelo intertravamento
 * de /toggle em ligadesliga.cpp.
 */
bool pressostatoAtivo = false;

// ============================================================================
// SECAO 2: VARIAVEIS DE TEMPORIZACAO E FILTRAGEM DIGITAL (DEBOUNCE)
// ============================================================================

/** Ultimo estado bruto lido do pino (para detectar transicao) */
static bool ultimoEstadoLeitura = false;

/** Instante da ultima oscilacao bruta observada no pino */
static unsigned long tempoFiltroDebounce = 0;

/**
 * Janela estavel obrigatoria antes de aceitar a transicao.
 * 200 ms absorve repiques tipicos de chaves eletromecanicas de pressao.
 */
const unsigned long DEBOUNCE_PRESSOSTATO_MS = 200UL;

/** Instante do ultimo log de diagnostico periodico */
static unsigned long ultimoLogDiagnostico = 0;

/**
 * Intervalo entre logs de rotina.
 * 15 s evita poluicao do monitor serial mas mantem rastreabilidade.
 */
const unsigned long INTERVALO_LOG_MS = 15000UL;

// ============================================================================
// SECAO 3: INICIALIZACAO DO MODULO
// ============================================================================

/**
 * setupPressostato()
 * ----------------------------------------------------------------------------
 * Configura o GPIO do pressostato como entrada com pulldown e realiza uma
 * leitura sincrona inicial para determinar o estado de partida do sistema.
 *
 * NOTA: se o ESP32 inicializar com o pressostato ja em alarme, a flag
 * pressostatoAtivo sera publicada como true imediatamente, bloqueando o
 * motor 0 no primeiro /toggle ou em qualquer tentativa de restauracao
 * pos-manutencao.
 */
void setupPressostato()
{
    Serial.println("[PRESSOSTATO] Inicializando monitoramento do sensor de pressao...");

    pinMode(pinoPressostato, INPUT_PULLDOWN);

    // Leitura sincrona inicial para definir o estado de partida
    bool leituraInicial = (digitalRead(pinoPressostato) == HIGH);
    pressostatoAtivo = leituraInicial;
    ultimoEstadoLeitura = leituraInicial;
    tempoFiltroDebounce = millis();

    if (pressostatoAtivo)
    {
        Serial.println("==================================================");
        Serial.println("[PRESSOSTATO] ALERTA: Pressostato JA ACIONADO na inicializacao!");
        Serial.println("[PRESSOSTATO] Motor 0 permanecera bloqueado ate normalizacao.");
        Serial.println("==================================================");
    }
    else
    {
        Serial.println("[PRESSOSTATO] Pressao em faixa operacional segura (NORMAL).");
    }

    Serial.println("[PRESSOSTATO] Modulo configurado com sucesso no pino GPIO 17.");
}

// ============================================================================
// SECAO 4: REGISTRO DE EVENTOS E DIAGNOSTICO
// ============================================================================

/**
 * registrarEventoPressostato()
 * ----------------------------------------------------------------------------
 * Emite no console serial um relatorio formatado de transicao do pressostato,
 * incluindo contexto do estado do compressor no momento do evento.
 *
 * @param ativo                   Novo estado confirmado apos debounce.
 * @param compressorEstavaLigado  Snapshot do comando/feedback antes da acao.
 */
static void registrarEventoPressostato(bool ativo, bool compressorEstavaLigado)
{
    Serial.println("==================================================");

    if (ativo)
    {
        Serial.println("[PRESSOSTATO] ALERTA CRITICO: Pressostato de seguranca acionado!");
        Serial.printf("[PRESSOSTATO] Pino GPIO %d em nivel HIGH.\n", pinoPressostato);

        if (compressorEstavaLigado)
        {
            Serial.println("[PRESSOSTATO] ACAO: Compressor estava em operacao e sera desarmado.");
        }
        else
        {
            Serial.println("[PRESSOSTATO] ACAO: Compressor ja estava desligado. Partida bloqueada.");
        }
    }
    else
    {
        Serial.println("[PRESSOSTATO] INFORMACAO: Alarme do pressostato normalizado (LOW).");
        Serial.println("[PRESSOSTATO] A linha de pressao retornou aos parametros de trabalho.");
    }

    Serial.println("==================================================");
}

// ============================================================================
// SECAO 5: ROTINA AUXILIAR DE DESLIGAMENTO (RESPEITA GUARDA DE RE-DISPARO)
// ============================================================================

/**
 * desligarCompressorPorPressostato()
 * ----------------------------------------------------------------------------
 * Tenta efetivar o desligamento do compressor (Motor 0) aplicando as seguintes
 * regras de seguranca:
 *
 *   1. Se um pulso de comando ja estiver em curso no canal (tipicamente um
 *      LIGAR recente), NAO emite novo pulso - aguarda o proximo ciclo.
 *      Isso evita empilhamento de comandos e estresse termico na bobina.
 *
 *   2. Se dispararPulsoMotor() retornar false (rejeitado pela guarda),
 *      tambem adia para o proximo ciclo.
 *
 *   3. Ao aceitar o pulso, consolida:
 *        - motoresLigados[0] = false;
 *        - /motor1.txt = "0";
 *        - /status1.txt = "OFF" (adiantado; monitorarStatus() corrige);
 *        - motorTimers[0] = millis() + DESLIGAR_TIMEOUT (10 min);
 *
 * @return true se o desligamento foi efetivado neste ciclo.
 */
static bool desligarCompressorPorPressostato()
{
    // Defesa 1: nao empilhar pulso sobre pulso em curso
    if (isPulsoMotorAtivo(0))
    {
        return false;
    }

    // Defesa 2: guarda de re-disparo interna do ligadesliga
    if (!dispararPulsoMotor(0))
    {
        return false;
    }

    // Consolidacao do estado desligado
    motoresLigados[0] = false;
    saveMotorState(arquivosMotores[0], false);

    // NOTA: status real so sera lido no proximo ciclo de monitorarStatus()
    // quando o pulso LOW tiver efetivado a abertura do contator.
    saveStatusState(pinosStatus[0], false);

    // Arma descanso obrigatorio de 10 min para despressurizacao
    iniciarTimerMotorSegundos(0, DESLIGAR_TIMEOUT);

    return true;
}

// ============================================================================
// SECAO 6: VARREDURA E INTERTRAVAMENTO DO COMPRESSOR
// ============================================================================

/**
 * verificarPressostato()
 * ----------------------------------------------------------------------------
 * Rotina cooperativa chamada continuamente pelo loop() principal. Executa,
 * nesta ordem:
 *
 *   1. Leitura bruta do GPIO 17;
 *   2. Filtro de debounce (200 ms);
 *   3. Deteccao de transicao confirmada -> aciona log + desligamento;
 *   4. Trava continua (interlock): se pressostato ativo E motor ligado,
 *      desliga de novo (defesa contra comando externo concorrente);
 *   5. Diagnostico periodico (a cada 15 s): audita coerencia com feedback.
 */
void verificarPressostato()
{
    unsigned long agora = millis();

    // ---------------------------------------------------------------------
    // 1. Leitura do estado eletrico bruto da porta
    // ---------------------------------------------------------------------
    bool leituraAtual = (digitalRead(pinoPressostato) == HIGH);

    // ---------------------------------------------------------------------
    // 2. Filtro de debounce contra transientes mecanicos
    // ---------------------------------------------------------------------
    if (leituraAtual != ultimoEstadoLeitura)
    {
        tempoFiltroDebounce = agora;
        ultimoEstadoLeitura = leituraAtual;
    }

    // ---------------------------------------------------------------------
    // 3. Validacao apos sinal permanecer estavel pelo tempo estipulado
    // ---------------------------------------------------------------------
    if ((agora - tempoFiltroDebounce) > DEBOUNCE_PRESSOSTATO_MS)
    {
        if (leituraAtual != pressostatoAtivo)
        {
            pressostatoAtivo = leituraAtual;

            // Snapshot do estado do compressor para log contextual
            bool compressorLigado = motoresLigados[0] ||
                                    (digitalRead(pinosStatus[0]) == HIGH);

            registrarEventoPressostato(pressostatoAtivo, compressorLigado);

            if (pressostatoAtivo)
            {
                // Notifica a IA imediatamente na transicao LOW -> HIGH confirmada
                // (mesmo que o motor esteja parado ou o desligamento seja adiado)
                iaLocal.notificarEventoSeguranca(0, "pressostato");

                // ---------------------------------------------------------
                // CASO A: Transicao para ALARME (LOW -> HIGH)
                // ---------------------------------------------------------
                if (motoresLigados[0])
                {
                    if (desligarCompressorPorPressostato())
                    {
                        Serial.println("[PRESSOSTATO] Compressor desligado com sucesso.");
                    }
                    else
                    {
                        Serial.println("[PRESSOSTATO] Desligamento adiado (pulso em "
                                       "curso); nova tentativa no proximo ciclo.");
                    }
                }
                else
                {
                    // Compressor ja estava parado: apenas reforca persistencia
                    saveMotorState(arquivosMotores[0], false);
                    saveStatusState(pinosStatus[0], false);

                    Serial.println("[PRESSOSTATO] Partida do compressor bloqueada "
                                   "por intertravamento.");
                }
            }

            else
            {
                // ---------------------------------------------------------
                // CASO B: Transicao para NORMAL (HIGH -> LOW)
                // ---------------------------------------------------------
                Serial.println("[PRESSOSTATO] Alarme normalizado. Motor 0 "
                               "liberado (respeitando timer de descanso, se ativo).");
            }
        }
    }

    // ---------------------------------------------------------------------
    // 4. Trava continua de seguranca (Interlock ativo + re-tentativa)
    //    Se algum comando externo religar o motor durante o alarme, este
    //    bloco neutraliza o religamento no ciclo seguinte. A guarda de
    //    re-disparo do ligadesliga evita empilhamento de pulsos.
    // ---------------------------------------------------------------------
    if (pressostatoAtivo && motoresLigados[0])
    {
        if (desligarCompressorPorPressostato())
        {
            Serial.println("[PRESSOSTATO] Intertravamento acionado: "
                           "Compressor desligado com seguranca.");
        }
    }

    // ---------------------------------------------------------------------
    // 5. Log de diagnostico periodico de rotina
    //    Detecta incoerencia entre a flag soberana e o feedback fisico.
    // ---------------------------------------------------------------------
    if ((agora - ultimoLogDiagnostico) >= INTERVALO_LOG_MS)
    {
        ultimoLogDiagnostico = agora;

        bool statusCompressor = (digitalRead(pinosStatus[0]) == HIGH);

        if (pressostatoAtivo && statusCompressor)
        {
            Serial.println("[PRESSOSTATO] Incoerencia detectada: pressostato "
                           "ativo mas retorno do compressor esta HIGH.");

            if (desligarCompressorPorPressostato())
            {
                Serial.println("[PRESSOSTATO] Acao corretiva aplicada.");
            }
            else
            {
                Serial.println("[PRESSOSTATO] Acao corretiva adiada (pulso em "
                               "curso); nova tentativa no proximo ciclo.");
            }
        }
        else if (pressostatoAtivo)
        {
            Serial.println("[PRESSOSTATO] Alarme persistente - compressor "
                           "mantido bloqueado.");
        }
    }
}

// ============================================================================
// FIM DO ARQUIVO: src/pressostato.cpp (v1.1.0)
// ENERGIZAR - SISTEMA DE AUTOMACAO INDUSTRIAL E MONITORAMENTO DE COMPRESSORES
// ============================================================================
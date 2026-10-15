/**
 * ============================================================================
 * PROJETO: EnergizAR - Sistema de Automacao e Monitoramento ESP32
 * ARQUIVO: src/sobrecarga.cpp
 * DESCRICAO: Modulo industrial de seguranca e protecao contra sobrecarga termica.
 *            Realiza a leitura nao-bloqueante dos contatos auxiliares dos reles
 *            termicos (bimetalicos), filtragem de transientes (debounce digital),
 *            qualificacao eletrica na partida (boot) e desarme instantaneo.
 *
 * ARQUITETURA DE SEGURANCA:
 * 1. Carencia Inicial (Boot): Ignora transientes eletricos nos primeiros 2s.
 * 2. Armacao Segura: Valida nivel LOW estavel antes de habilitar a protecao.
 * 3. Filtragem de Borda: 250 ms de debounce para evitar falsos desarmes.
 * 4. Intertravamento Ativo: Forca o corte das bobinas em falha confirmada.
 * ============================================================================
 */

#include "sobrecarga.h"
#include "ligadesliga.h"

// -------------------------------------------------------------------------
// 1. MAPEAMENTO FISICO DAS ENTRADAS DE PROTECAO TERMICA
// -------------------------------------------------------------------------

/**
 * Pinos conectados aos contatos auxiliares 97-98 (NA) dos reles termicos:
 * - Motor 1 (Compressor Principal): GPIO 19 (Isolado de atuadores e barramento)
 * - Motor 2 (Motor Ventilador):     GPIO 25 (Entrada dedicada de protecao)
 * - Motor 3 (Motor Secador de Ar):  GPIO 18 (Entrada dedicada de protecao)
 *
 * NOTA ELETRICA: Todas as entradas operam com resistores de PULLDOWN ativados.
 * - Nivel LOW  (0V):   Circuito em condicao segura e motor liberado.
 * - Nivel HIGH (3.3V): Rele termico disparou por sobrecorrente de armadura.
 */
const int pinosSobrecarga[NUM_MOTORES] = {19, 25, 18};

// -------------------------------------------------------------------------
// 2. VARIAVEIS GLOBAIS E ESTADOS DE OPERACAO EM RAM
// -------------------------------------------------------------------------

/**
 * Flag de indicacao de sobrecarga confirmada por canal.
 * Exportada para uso nas rotas de API, dashboard e travas de intertravamento.
 */
bool sobrecargaDetectada[NUM_MOTORES] = {false, false, false};

/**
 * Historico do estado digital anterior para deteccao de bordas (transicoes).
 */
static bool ultimoEstadoSobrecarga[NUM_MOTORES] = {false, false, false};

/**
 * Marca temporal (millis) da ultima variacao no pino para calculo de debounce.
 */
static unsigned long ultimoTempoSobrecarga[NUM_MOTORES] = {0, 0, 0};

/**
 * Sinalizador de entrada qualificada.
 * Impede alarmes indevidos enquanto o pino estiver flutuando na inicializacao.
 */
static bool entradaSobrecargaArmada[NUM_MOTORES] = {false, false, false};

/**
 * Flag para emissao de aviso unico de entrada em nivel alto durante a partida.
 */
static bool avisoEntradaInvalidaEmitido[NUM_MOTORES] = {false, false, false};

/**
 * Cronometro de validacao de nivel logico baixo continuo.
 */
static unsigned long tempoInicioNivelBaixo[NUM_MOTORES] = {0, 0, 0};

/**
 * Registro de instante de disparo da sobrecarga para telemetria industrial.
 */
unsigned long tempoInicioSobrecarga[NUM_MOTORES] = {0, 0, 0};

// -------------------------------------------------------------------------
// 3. PARAMETROS TEMPORAIS DE CALIBRACAO E FILTRAGEM
// -------------------------------------------------------------------------

/**
 * Janela de estabilizacao digital contra ruidos eletromagneticos (250 ms).
 */
const unsigned long DEBOUNCE_DELAY = 250;

/**
 * Periodo de carencia apos a energizacao para estabilizar a alimentacao (2000 ms).
 */
const unsigned long CARENCIA_BOOT_MS = 2000;

/**
 * Tempo minimo continuo em LOW para habilitar o monitoramento de cada pino (800 ms).
 */
const unsigned long TEMPO_VALIDACAO_ENTRADA_MS = 800;

// -------------------------------------------------------------------------
// 4. INICIALIZACAO DO SUBSISTEMA DE SOBRECARGA
// -------------------------------------------------------------------------

/**
 * Inicializa os pinos de entrada, configura pull-down interno e redefine
 * todos os temporizadores de filtragem para o estado seguro inicial.
 */
void setupSobrecarga()
{
    Serial.println("[SOBRECARGA] Configurando entradas de protecao termica...");

    unsigned long agora = millis();

    // Varre todos os canais de sobrecarga cadastrados no sistema
    for (int i = 0; i < NUM_MOTORES; i++)
    {
        // Configura o pino como entrada com resistor pull-down interno ativo
        pinMode(pinosSobrecarga[i], INPUT_PULLDOWN);

        // Zera estados de alarme e historicos de leitura
        sobrecargaDetectada[i] = false;
        ultimoEstadoSobrecarga[i] = false;
        ultimoTempoSobrecarga[i] = agora;
        tempoInicioSobrecarga[i] = 0;

        // Bloqueia a deteccao ate que ocorra a qualificacao em nivel seguro (LOW)
        entradaSobrecargaArmada[i] = false;
        avisoEntradaInvalidaEmitido[i] = false;
        tempoInicioNivelBaixo[i] = 0;

        Serial.printf("[SOBRECARGA] Canal %d mapeado com sucesso no GPIO %d.\n",
                      i + 1, pinosSobrecarga[i]);
    }

    Serial.println("[SOBRECARGA] Entradas aguardando validacao eletrica inicial.");
}

// -------------------------------------------------------------------------
// 5. FUNCOES DE DESLIGAMENTO E INTERTRAVAMENTO DE SEGURANCA
// -------------------------------------------------------------------------

/**
 * Desliga imediatamente o motor especificado cortando a saida do rele
 * e atualizando os registros de estado em memoria RAM e SPIFFS.
 * @param motorIndex Indice do motor (0 a NUM_MOTORES - 1).
 */
void desativarMotor(int motorIndex)
{
    // Valida se o indice fornecido esta dentro dos limites do arranjo
    if (motorIndex < 0 || motorIndex >= NUM_MOTORES)
    {
        return;
    }

    // Forca nivel LOW imediato na bobina de comando do motor
    digitalWrite(pinosMotores[motorIndex], LOW);

    // Se o motor constava como ligado, atualiza variaveis e persiste na flash
    if (motoresLigados[motorIndex])
    {
        motoresLigados[motorIndex] = false;
        saveMotorState(arquivosMotores[motorIndex], false);

        Serial.printf("[SEGURANCA] Motor %d desativado por ordem de intertravamento.\n",
                      motorIndex + 1);
    }
}

/**
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
 * Desliga seletivamente apenas os motores que apresentarem sobrecarga confirmada.
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
 * Limpa as condicoes de alarme e prepara os motores para rearme manual seguro.
 * Nao liga nenhum motor automaticamente; apenas autoriza novos comandos.
 */
void restaurarEstadoMotores()
{
    Serial.println("[SOBRECARGA] Executando rotina de rearme seguro dos motores...");

    for (int i = 0; i < NUM_MOTORES; i++)
    {
        digitalWrite(pinosMotores[i], LOW);
        motoresLigados[i] = false;
        sobrecargaDetectada[i] = false;

        saveMotorState(arquivosMotores[i], false);

        Serial.printf("[SOBRECARGA] Motor %d restabelecido em modo DESLIGADO (Seguro).\n",
                      i + 1);
    }
}

// -------------------------------------------------------------------------
// 6. MONITORAMENTO CICLICO CONTINUO (EXECUTADO NO LOOP PRINCIPAL)
// -------------------------------------------------------------------------

/**
 * Rotina de monitoramento continuo cooperativo sem bloqueio da CPU.
 * Realiza filtragem digital, armacao pos-boot e corte emergencial imediato.
 */
void monitorarSobrecarga()
{
    unsigned long agora = millis();

    // Aguarda expiracao do tempo de carencia de energizacao do hardware
    if (agora < CARENCIA_BOOT_MS)
    {
        return;
    }

    // Executa a varredura individual de cada canal de protecao
    for (int i = 0; i < NUM_MOTORES; i++)
    {
        // Leitura direta do estado logico no pino GPIO do microcontrolador
        bool leituraFisica = (digitalRead(pinosSobrecarga[i]) == HIGH);

        // =================================================================
        // ETAPA A: Qualificacao e Armacao Inicial da Linha Eletrica
        // =================================================================
        if (!entradaSobrecargaArmada[i])
        {
            // O canal so eh armado se permanecer estavel em LOW por 800 ms
            if (!leituraFisica)
            {
                if (tempoInicioNivelBaixo[i] == 0)
                {
                    tempoInicioNivelBaixo[i] = agora;
                }

                // Valida se a janela minima de estabilidade em LOW foi concluida
                if (agora - tempoInicioNivelBaixo[i] >= TEMPO_VALIDACAO_ENTRADA_MS)
                {
                    entradaSobrecargaArmada[i] = true;
                    ultimoEstadoSobrecarga[i] = false;
                    ultimoTempoSobrecarga[i] = agora;
                    avisoEntradaInvalidaEmitido[i] = false;

                    Serial.printf("[SOBRECARGA] Entrada do motor %d validada em LOW. Protecao armada.\n",
                                  i + 1);
                }
            }
            else
            {
                // Reinicia a contagem caso ocorra ruido ou flutuacao em nivel alto
                tempoInicioNivelBaixo[i] = 0;

                if (!avisoEntradaInvalidaEmitido[i])
                {
                    Serial.printf("[AVISO] Entrada do motor %d no GPIO %d em HIGH durante boot. Linha bloqueada.\n",
                                  i + 1, pinosSobrecarga[i]);
                    avisoEntradaInvalidaEmitido[i] = true;
                }

                // Se por algum motivo o motor estava registrado como ligado, desliga
                if (motoresLigados[i])
                {
                    desativarMotor(i);
                }
            }

            // Pula para o proximo canal ate que a armacao seja completamente homologada
            continue;
        }

        // =================================================================
        // ETAPA B: Filtragem Digital por Debounce Temporal
        // =================================================================
        if (leituraFisica != ultimoEstadoSobrecarga[i])
        {
            ultimoEstadoSobrecarga[i] = leituraFisica;
            ultimoTempoSobrecarga[i] = agora;
        }

        // Confirma a transicao apenas se o nivel permanecer estavel pela janela do debounce
        if (agora - ultimoTempoSobrecarga[i] >= DEBOUNCE_DELAY)
        {
            // Verifica se houve mudanca real de estado em relacao ao alarme atual
            if (leituraFisica != sobrecargaDetectada[i])
            {
                sobrecargaDetectada[i] = leituraFisica;

                // Condicao de falha termica detectada e confirmada
                if (sobrecargaDetectada[i])
                {
                    tempoInicioSobrecarga[i] = agora;

                    Serial.println("==================================================");
                    Serial.printf("[ALERTA CRITICO] SOBRECARGA CONFIRMADA NO MOTOR %d!\n", i + 1);
                    Serial.printf("[ALERTA CRITICO] GPIO %d em nivel HIGH estavel.\n", pinosSobrecarga[i]);
                    Serial.println("==================================================");

                    // Desliga o motor em sobrecarga imediatamente
                    desativarMotor(i);
                }
                else
                {
                    // Condicao de normalizacao eletrica (rele termico desarmado/resfriado)
                    tempoInicioSobrecarga[i] = 0;

                    Serial.printf("[SOBRECARGA] Sinal do motor %d normalizado para LOW. Alarme liberado.\n",
                                  i + 1);
                }
            }
        }

        // =================================================================
        // ETAPA C: Trava Continua de Intertravamento (Hard Lock)
        // =================================================================
        // Se o alarme estiver ativo e qualquer comando tentar religar, forca corte imediato
        if (sobrecargaDetectada[i] && motoresLigados[i])
        {
            desativarMotor(i);
        }
    }
}

// -------------------------------------------------------------------------
// Fim do Arquivo: src/sobrecarga.cpp
// ============================================================================
/**
 * ============================================================================
 * PROJETO: EnergizAR - Sistema de Automação e Monitoramento ESP32
 * ARQUIVO: src/manutencao.cpp
 * DESCRIÇÃO: Gerenciamento do modo de manutenção através de chave física.
 *            Memoriza o estado anterior das máquinas, desliga com segurança,
 *            bloqueia comandos externos e restaura o funcionamento anterior
 *            de forma assíncrona respeitando sobrecarga, pressostato, horário
 *            fabril e temporizadores anti-curto-ciclo.
 * ============================================================================
 */

#include "manutencao.h"
#include "ligadesliga.h"
#include "sobrecarga.h"
#include "pressostato.h"

// -------------------------------------------------------------------------
// 1. Constantes e Definições de Hardware
// -------------------------------------------------------------------------

/**
 * Pino GPIO 4 conectado à chave seletora / botão de trava de manutenção.
 * Utiliza o resistor interno de pull-up (em repouso = HIGH; acionado = LOW).
 */
const int pinoManutencao = 4;

// Flag global de estado de manutenção
bool sistemaEmManutencao = false;

// -------------------------------------------------------------------------
// 2. Variáveis de Temporização e Filtragem Digital (Debounce)
// -------------------------------------------------------------------------

static bool ultimoEstadoChave = false;
static unsigned long ultimoTempoDebounce = 0;
const unsigned long DEBOUNCE_MANUTENCAO_MS = 150; // Janela de estabilização de 150 ms

// Controle de diagnóstico periódico em console serial
static unsigned long ultimoLogManutencao = 0;
const unsigned long INTERVALO_LOG_STATUS_MS = 20000; // 20 segundos

// Variáveis para religamento sequencial seguro pós-manutenção
static bool restauracaoEmAndamento = false;
static int motorSendoRestaurado = 0;
static unsigned long tempoProximoReligamento = 0;
const unsigned long INTERVALO_ENTRE_MOTORES_MS = 1000; // 1s entre partidas

// -------------------------------------------------------------------------
// 3. Inicialização do Módulo de Manutenção
// -------------------------------------------------------------------------

/**
 * Configura o pino GPIO 4 em modo INPUT_PULLUP e lê o estado inicial da chave.
 */
void setupManutencao()
{
    Serial.println("[MANUTENÇÃO] Inicializando monitoramento da chave de manutenção...");

    pinMode(pinoManutencao, INPUT_PULLUP);

    // Leitura inicial imediata para definir o estado de partida do ESP32
    bool leituraInicial = (digitalRead(pinoManutencao) == LOW);
    sistemaEmManutencao = leituraInicial;
    ultimoEstadoChave = leituraInicial;
    ultimoTempoDebounce = millis();

    if (sistemaEmManutencao)
    {
        Serial.println("==================================================");
        Serial.println("[MANUTENÇÃO] ATENÇÃO: ESP32 inicializado com chave em MANUTENÇÃO!");
        Serial.println("[MANUTENÇÃO] Todos os acionamentos permanecerão bloqueados.");
        Serial.println("==================================================");
        desligarTodosMotores();
    }
    else
    {
        Serial.println("[MANUTENÇÃO] Sistema em operação NORMAL. Chave desativada.");
    }

    Serial.println("[MANUTENÇÃO] Pino GPIO 4 configurado com sucesso.");
}

// -------------------------------------------------------------------------
// 4. Funções Auxiliares de Consulta e Forçamento
// -------------------------------------------------------------------------

/**
 * Retorna se o sistema está em manutenção.
 */
bool isSistemaEmManutencao()
{
    return sistemaEmManutencao;
}

/**
 * Permite forçar o encerramento do modo de manutenção via software se necessário.
 */
void forcarSaidaManutencao()
{
    if (sistemaEmManutencao)
    {
        sistemaEmManutencao = false;
        Serial.println("[MANUTENÇÃO] Saída do modo de manutenção forçada via software.");
    }
}

// -------------------------------------------------------------------------
// 5. Máquina de Estados e Varredura da Chave de Manutenção
// -------------------------------------------------------------------------

/**
 * Monitora o sinal físico da chave seletora, gerencia o salvamento dos estados,
 * desativa os motores e coordena a restauração não-bloqueante na saída.
 */
bool atualizarEstadoManutencao()
{
    unsigned long agora = millis();
    bool leituraFisica = (digitalRead(pinoManutencao) == LOW); // LOW = Acionado (GND)

    // 1. Filtragem digital de transição (Debounce)
    if (leituraFisica != ultimoEstadoChave)
    {
        ultimoTempoDebounce = agora;
        ultimoEstadoChave = leituraFisica;
    }

    // 2. Confirmação de mudança de estado após sinal permanecer estável
    if ((agora - ultimoTempoDebounce) > DEBOUNCE_MANUTENCAO_MS)
    {
        if (leituraFisica != sistemaEmManutencao)
        {
            sistemaEmManutencao = leituraFisica;

            // -------------------------------------------------------------
            // CASO A: ENTRADA NO MODO DE MANUTENÇÃO
            // -------------------------------------------------------------
            if (sistemaEmManutencao)
            {
                Serial.println("==================================================");
                Serial.println("[MANUTENÇÃO] CHAVE ACIONADA: Entrando em MODO DE MANUTENÇÃO!");
                Serial.println("[MANUTENÇÃO] Memorizando estado atual de cada motor...");

                // Cancela qualquer religamento sequencial pendente
                restauracaoEmAndamento = false;

                // Salva o estado dos motores em RAM antes de cortá-los
                for (int i = 0; i < NUM_MOTORES; i++)
                {
                    motoresEstadoAnterior[i] = motoresLigados[i];
                    Serial.printf("[MANUTENÇÃO] Motor %d gravado na memória como: %s\n",
                                  i + 1, motoresEstadoAnterior[i] ? "LIGADO" : "DESLIGADO");
                }

                // Desativa todos os motores imediatamente de forma assíncrona
                desligarTodosMotores();

                Serial.println("[MANUTENÇÃO] Todos os motores foram desativados com sucesso.");
                Serial.println("==================================================");
            }
            // -------------------------------------------------------------
            // CASO B: SAÍDA DO MODO DE MANUTENÇÃO (RESTAURAÇÃO SEQUENCIAL)
            // -------------------------------------------------------------
            else
            {
                Serial.println("==================================================");
                Serial.println("[MANUTENÇÃO] CHAVE DESATIVADA: Retornando à Operação Normal.");
                Serial.println("[MANUTENÇÃO] Iniciando religamento sequencial seguro dos motores...");
                Serial.println("==================================================");

                // Inicia o processo não-bloqueante de religamento escalonado
                restauracaoEmAndamento = true;
                motorSendoRestaurado = 0;
                tempoProximoReligamento = agora; // Primeiro motor liga imediatamente se seguro
            }

            return true; // Indica que houve transição no estado
        }
    }

    // ---------------------------------------------------------------------
    // 3. Processo Não-Bloqueante de Religamento Escalonado dos Motores
    //    Evita queda de tensão e valida todos os intertravamentos antes de ligar.
    // ---------------------------------------------------------------------
    if (restauracaoEmAndamento)
    {
        if ((long)(agora - tempoProximoReligamento) >= 0)
        {
            if (motorSendoRestaurado < NUM_MOTORES)
            {
                if (motoresEstadoAnterior[motorSendoRestaurado])
                {
                    bool bloqueadoHorario = (isAfterClosingTime() || isBeforeOpeningTime());
                    bool emSobrecarga = sobrecargaDetectada[motorSendoRestaurado];
                    bool erroPressostato = (motorSendoRestaurado == 0 && pressostatoAtivo);

                    if (bloqueadoHorario || emSobrecarga || erroPressostato)
                    {
                        Serial.printf("[MANUTENÇÃO] Motor %d NÃO restaurado por intertravamento "
                                      "(horario=%d, sobrecarga=%d, pressostato=%d).\n",
                                      motorSendoRestaurado + 1,
                                      (int)bloqueadoHorario,
                                      (int)emSobrecarga,
                                      (int)erroPressostato);

                        motoresEstadoAnterior[motorSendoRestaurado] = false;
                        motorSendoRestaurado++;
                        tempoProximoReligamento = agora + INTERVALO_ENTRE_MOTORES_MS;
                    }
                    else if (!isPulsoMotorAtivo(motorSendoRestaurado) &&
                             dispararPulsoMotor(motorSendoRestaurado))
                    {
                        Serial.printf("[MANUTENÇÃO] Motor %d restaurado com segurança (Pulso + Timer 60min).\n",
                                      motorSendoRestaurado + 1);

                        motoresLigados[motorSendoRestaurado] = true;
                        saveMotorState(arquivosMotores[motorSendoRestaurado], true);
                        iniciarTimerMotorSegundos(motorSendoRestaurado, LIGAR_TIMEOUT);

                        motorSendoRestaurado++;
                        tempoProximoReligamento = agora + INTERVALO_ENTRE_MOTORES_MS;
                    }
                }
                else
                {
                    Serial.printf("[MANUTENÇÃO] Motor %d permanecia desligado. Nenhuma ação necessária.\n",
                                  motorSendoRestaurado + 1);

                    motorSendoRestaurado++;
                    tempoProximoReligamento = agora + INTERVALO_ENTRE_MOTORES_MS;
                }
            }
            else
            {
                // Concluiu a restauração de todos os motores
                restauracaoEmAndamento = false;
                Serial.println("[MANUTENÇÃO] Processo de restauração de motores finalizado com êxito.");
            }
        }
    }

    // ---------------------------------------------------------------------
    // 4. Bloqueio Permanente e Registro Periódico de Alerta
    // ---------------------------------------------------------------------
    if (sistemaEmManutencao)
    {
        for (int i = 0; i < NUM_MOTORES; i++)
        {
            if (motoresLigados[i] && !isPulsoMotorAtivo(i) && dispararPulsoMotor(i))
            {
                motoresLigados[i] = false;
                saveMotorState(arquivosMotores[i], false);
            }
        }

        if (agora - ultimoLogManutencao >= INTERVALO_LOG_STATUS_MS)
        {
            ultimoLogManutencao = agora;
            Serial.println("[MANUTENÇÃO] AVISO: Sistema bloqueado por chave de manutenção.");
        }
    }

    return false; // Nenhuma mudança de estado neste ciclo
}
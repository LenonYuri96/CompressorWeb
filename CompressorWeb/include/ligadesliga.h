/**
 * ============================================================================
 * PROJETO: EnergizAR - Sistema de Automacao e Monitoramento ESP32
 * ARQUIVO: include/ligadesliga.h
 * DESCRICAO: Cabecalho do modulo central de acionamento dos compressores.
 * ============================================================================
 */
#ifndef LIGADESLIGA_H
#define LIGADESLIGA_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#define NUM_MOTORES 3

// -------------------------------------------------------------------------
// 1. Constantes de Temporizacao de Seguranca Industrial
// -------------------------------------------------------------------------
extern const unsigned long LIGAR_TIMEOUT;         // 60 min (3600s) bloqueado apos ligar
extern const unsigned long DESLIGAR_TIMEOUT;      // 10 min (600s) descanso para religar
extern const unsigned long DURACAO_PULSO_MS;      // 500ms largura do pulso de rele
extern const unsigned long DEBOUNCE_STATUS_MS;    // 100ms filtro digital de arco
extern const unsigned long TOLERANCIA_COMANDO_MS; // 4000ms janela de comutacao

// -------------------------------------------------------------------------
// 2. Variaveis de Estado Operacional e Pinos em Memoria RAM
// -------------------------------------------------------------------------
extern int pinosMotores[NUM_MOTORES];           // Pinos de saida dos pulsos de rele
extern int pinosStatus[NUM_MOTORES];            // Pinos de retorno dos contatores
extern bool motoresLigados[NUM_MOTORES];        // Estado operacional em memoria RAM
extern bool timersAtivos[NUM_MOTORES];          // Indicador de descanso ativo
extern bool motoresEstadoAnterior[NUM_MOTORES]; // Historico de comutacao
extern unsigned long motorTimers[NUM_MOTORES];  // Timestamp de liberacao (millis)

// -------------------------------------------------------------------------
// 3. Arquivos Canonicos de Persistencia no Sistema SPIFFS
// -------------------------------------------------------------------------
extern const String arquivosMotores[NUM_MOTORES];
extern const String arquivosStatus[NUM_MOTORES];
extern const String arquivosTimers[NUM_MOTORES];

// -------------------------------------------------------------------------
// 4. Funcoes de Inicializacao e Geracao Nao-Bloqueante de Pulsos
// -------------------------------------------------------------------------
void setupLigaDesliga(AsyncWebServer &server);
void processarPulsosMotores();         // Desliga pulso de 500ms sem delay()
void dispararPulsoMotor(int motorIdx); // Inicia pulso de nivel alto na bobina
void monitorarStatusCompressores();    // Leitura e debounce de retorno eletrico
void updateMotorStatus();              // Sincroniza estado fisico na memoria

// -------------------------------------------------------------------------
// 5. Funcoes de Gestao dos Temporizadores e Restituicao de Saldo
// -------------------------------------------------------------------------
void updateTimers();                               // Atualiza saldos em memoria e flash
bool isMotorTimerActive(int motorIdx);             // Valida se motor esta bloqueado
unsigned long getMotorRemainingTime(int motorIdx); // Retorna segundos restantes
void handleToggleAction(AsyncWebServer &server);   // Registra rota HTTP /toggle

// -------------------------------------------------------------------------
// 6. Funcoes de Intertravamento e Seguranca Industrial
// -------------------------------------------------------------------------
void desligarTodosMotores();          // Corte geral instantaneo de cargas
void atualizarEstadoMotores();        // Auditoria de regras e paradas gerais
void desativarSistemaForaDoHorario(); // Bloqueio automatico fora de expediente
bool sistemaDeveEstarBloqueado();     // Valida chave manutencao e horario
bool checkMotorStatusChange();        // Deteccao de mudanca fisica rapida

// -------------------------------------------------------------------------
// 7. Auditoria de Horario Operacional de Fabrica (08:00 as 22:00)
// -------------------------------------------------------------------------
bool isAfterClosingTime();  // Valida se horario ultrapassou 22:00
bool isBeforeOpeningTime(); // Valida se horario e anterior a 08:00

// -------------------------------------------------------------------------
// 8. Persistencia Segura em Flash com Filtro de Cache em RAM
// -------------------------------------------------------------------------
void saveMotorState(const String &arquivoEstado, bool state);
bool readMotorState(const String &arquivoEstado);
void saveStatusState(int pin, bool state);
unsigned long readTimerState(const String &arquivoTimer);
void saveTimerState(const String &arquivoTimer, unsigned long remainingTime);

// -------------------------------------------------------------------------
// 9. Aliases Globais de Compatibilidade
// -------------------------------------------------------------------------
extern const int NUM_MOTORS;
extern const int *motorPins;

#endif // LIGADESLIGA_H
       // ============================================================================
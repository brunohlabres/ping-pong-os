// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DINF UFPR
// Versão 1.1 -- Julho de 2016

// Estruturas de dados internas do sistema operacional

#ifndef __PPOS_DATA__
#define __PPOS_DATA__

#include <ucontext.h>		// biblioteca POSIX de trocas de contexto
#include "queue.h"		// biblioteca de filas genéricas

// Estrutura que define um Task Control Block (TCB)
typedef struct task_t
{
   struct task_t *prev, *next ;		// ponteiros para usar em filas
   int id ;				// identificador da tarefa
   ucontext_t context ;			// contexto armazenado da tarefa
   void *stack ;			// aponta para a pilha da tarefa
   int pe, pd;  // Prioridades
   int ticks;
   int execTime;
   int activations;
   unsigned int timeOfCreation;
   int exit_code;
   int running;
   queue_t *tasks_waiting;
   unsigned int sleepTime; // Guarda tempo em que deve acordar

} task_t ;

// estrutura que define um semáforo
typedef struct
{
  int cont; // contador do semaforo
  int active;
  queue_t *tasks_waiting; // fila de tarefas esperando no semaforo
} semaphore_t ;

// estrutura que define um mutex
typedef struct
{
  // preencher quando necessário
} mutex_t ;

// estrutura que define uma barreira
typedef struct
{
  // preencher quando necessário
} barrier_t ;

// estrutura que define uma fila de mensagens
typedef struct
{
  void *buffer;
  semaphore_t sem_vaga;
  semaphore_t sem_item;
  semaphore_t sem_buf;
  int qtd_usada_buffer;
  int read_index; // controle do buffer circular
  int write_index; // controle do buffer circular
  int msg_size; // tamanho das msgs
  int valid; // se a queue foi ou nao destruida
} mqueue_t ;

#endif


// GRR20163049 Bruno Henrique Labres
// GRR20171588 Eduardo Henrique Trevisan
// -------------------------------------------------------
// PingPongOS
// Disciplina: Sistemas Operacionais
// -------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "ppos_disk.h"
#include "hard_disk.h"
#include "ppos.h"
#include "queue.h"

struct sigaction disk_interrupt;

task_t disk_task; // tarefa do gerente de disco
int signal_tratador = 0;		// guarda sinal gerado pelo disco
extern task_t *ready_queue;		// fila de tarefas prontas
extern task_t *current_task;	// tarefa atual
extern int userTasks;
extern int preempcao;
disk_t disk;					// guarda variaveis do disco

pedido_t *pedidos;				// fila de pedidos para o disco

// tratador do sinal do disco
void tratador_disk(int signum)
{
	signal_tratador = 1;
	if (disk.diskState == 0) // Se disco estiver dormindo
	{
		preempcao = 0;
		queue_append((queue_t **)&ready_queue, (queue_t *)&disk_task);
		disk.diskState = 1;
		preempcao = 1;
	}
	#ifdef DEBUG
		printf("interrupção do disco\n");
	#endif
}

void diskDriverBody (void * args)
{
	pedido_t *cabeca_pedidos;
	while (1)
	{
		// obtém o semáforo de acesso ao disco
		sem_down(&disk.disk_sem);

		// se foi acordado devido a um sinal do disco
		if (signal_tratador && (pedidos != NULL))
		{
			// acorda a tarefa cujo pedido foi atendido
			signal_tratador = 0;
			preempcao = 0;
			cabeca_pedidos = (pedido_t *)queue_remove((queue_t **)&pedidos, (queue_t *)pedidos);
			queue_append((queue_t **)&ready_queue, (queue_t *)cabeca_pedidos->task);
			preempcao = 1;
		}

		// se o disco estiver livre e houver pedidos de E/S na fila
		if (disk_cmd(DISK_CMD_STATUS,0,0) == 1 && (pedidos != NULL))
		{
				// escolhe na fila o pedido a ser atendido, usando FCFS
				// solicita ao disco a operação de E/S, usando disk_cmd()
				pedidos->ret = disk_cmd(pedidos->tipo, pedidos->block, pedidos->buffer);
		}

		// libera o semáforo de acesso ao disco
		sem_up(&disk.disk_sem);

		// suspende a tarefa corrente (retorna ao dispatcher)
		preempcao = 0;
		disk.diskState = 0;
		queue_remove((queue_t **)&ready_queue, (queue_t *)&disk_task);
		preempcao = 1;
		task_yield();		
	}
}

// inicializacao do gerente de disco
// retorna -1 em erro ou 0 em sucesso
// numBlocks: tamanho do disco, em blocos
// blockSize: tamanho de cada bloco do disco, em bytes
int disk_mgr_init (int *numBlocks, int *blockSize){

	pedidos = NULL;
	// verifica se o disco foi inicializado
	if (disk_cmd (DISK_CMD_STATUS, 0, 0) == DISK_STATUS_UNKNOWN) {
		if (disk_cmd (DISK_CMD_INIT, 0, 0)) // se deu errou ao inicializar
			return -1;
		*numBlocks = disk_cmd (DISK_CMD_DISKSIZE, 0, 0);
		*blockSize = disk.diskBlockSize = disk_cmd (DISK_CMD_BLOCKSIZE, 0, 0);

		// registra a acao para o sinal de disco SIGUSR1
		disk_interrupt.sa_handler = tratador_disk ;
		sigemptyset (&disk_interrupt.sa_mask) ;
		disk_interrupt.sa_flags = 0 ;
		if (sigaction (SIGUSR1, &disk_interrupt, 0) < 0)
		{
			perror ("Erro em sigaction de disco: ") ;
			exit (1) ;
		}

		sem_create(&disk.disk_sem, 1);

		task_create(&disk_task, diskDriverBody, NULL); // Inicializar tarefa do disco
		userTasks--;
		disk.diskState = 0;
		queue_remove((queue_t **)&ready_queue, (queue_t *)&disk_task);

		#ifdef DEBUG
			puts("disk_mgr_init: Disco inicializado");
		#endif

		return 0;
	}
	else 
		return -1; // disco ja foi inicializado
}

// leitura de um bloco, do disco para o buffer
int disk_block_read (int block, void *buffer) {

	// obtém o semáforo de acesso ao disco
	sem_down(&disk.disk_sem);

	pedido_t *p = (pedido_t*) malloc(sizeof(pedido_t));
	p->block = block;
	p->buffer = buffer;
	p->tipo = DISK_CMD_READ;
	p->task = current_task;
	p->next = p->prev = NULL;

	// inclui o pedido na fila_disco
	queue_append((queue_t **)&pedidos, (queue_t *)p);

	// libera semáforo de acesso ao disco
	sem_up(&disk.disk_sem);

	
	// suspende a tarefa corrente (retorna ao dispatcher)
	preempcao = 0;
	queue_remove((queue_t **)&ready_queue, (queue_t *)current_task);
	preempcao = 1;

	if (disk.diskState == 0)
	{
		preempcao = 0;
		queue_append((queue_t **)&ready_queue, (queue_t *)&disk_task);
		disk.diskState = 1;
		preempcao = 1;
	}

	task_yield();

	//executou o comando
	int ret = p->ret;
	free(p);
	return(ret);
}

// escrita de um bloco, do buffer para o disco
int disk_block_write (int block, void *buffer) {

	// obtém o semáforo de acesso ao disco
	sem_down(&disk.disk_sem);

	pedido_t *p = (pedido_t *)malloc(sizeof(pedido_t));
	p->buffer = buffer;
	p->block = block;
	p->tipo = DISK_CMD_WRITE;
	p->task = current_task;
	p->next = p->prev = NULL;

	// inclui o pedido na fila_disco
	queue_append((queue_t **)&pedidos, (queue_t *)p);

	// libera semáforo de acesso ao disco
	sem_up(&disk.disk_sem);

	// suspende a tarefa corrente (retorna ao dispatcher)
	preempcao = 0;
	queue_remove((queue_t **)&ready_queue, (queue_t *)current_task);
	preempcao = 1;

	if (disk.diskState == 0)
	{
		preempcao = 0;
		queue_append((queue_t **)&ready_queue, (queue_t *)&disk_task);
		disk.diskState = 1;
		preempcao = 1;
	}

	task_yield();

	//executou o comando
	int ret = p->ret;
	free(p);
	return(ret);
}
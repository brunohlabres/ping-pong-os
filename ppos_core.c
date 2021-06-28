// GRR20163049 Bruno Henrique Labres
// GRR20171588 Eduardo Henrique Trevisan
// -------------------------------------------------------
// PingPongOS
// Disciplina: Sistemas Operacionais
// -------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include "ppos.h" // estruturas de dados necessárias
#include "ppos_disk.h"

#define STACKSIZE 32768 /* tamanho de pilha das threads */
#define QUANTUM 20
#define TAM_BUFFER_CIRC 5
#define NUM_VAGAS_PROD TAM_BUFFER_CIRC 


// operating system check
#if defined(_WIN32) || (!defined(__unix__) && !defined(__unix) && (!defined(__APPLE__) || !defined(__MACH__)))
#warning Este codigo foi planejado para ambientes UNIX (LInux, *BSD, MacOS).
#endif

// estrutura que define um tratador de sinal (deve ser global ou static)
struct sigaction action ;

// estrutura de inicialização to timer
struct itimerval timer;

task_t *ready_queue;			// Fila de tarefas prontas
task_t *sleeping_queue;		// Fila de tarefas dormindo
int last_id = 0;					// Guarda o ultimo ID atribuido a uma tarefa
int userTasks = 0;				// Guarda numero de tarefas prontas para execucao
task_t *current_task; 		// Guarda a tarefa sendo executada no momento
task_t dispatcher;				// tarefa do dispatcher
task_t mainTask;					// tarefa da main
unsigned int currentTime;	// Guarda tempo do sistema
int preempcao;						// Desativa preempcao
extern task_t *disk_task;


semaphore_t sem_vaga;
semaphore_t sem_item;
semaphore_t sem_buf;


// --- FILAS DE MENSAGEM ------------------------------

int mqueue_create (mqueue_t *queue, int max_msgs, int msg_size){
	sem_create(&queue->sem_buf, 1);
	sem_create(&queue->sem_item, 0);
	sem_create(&queue->sem_vaga, NUM_VAGAS_PROD);
	queue->qtd_usada_buffer = 0; // o quanto do buffer foi usado
	queue->read_index = 0; // controle do buffer circular
	queue->write_index = 0;	 // controle do buffer circular
	queue->msg_size = msg_size; // tamanho das msgs
	queue->valid = 1; // queue nao foi destruida
	queue->buffer = (void *) malloc(TAM_BUFFER_CIRC*msg_size);
	if (queue->buffer){ 
		return 0;
	}else{
		return -1;
	}
}

int mqueue_send (mqueue_t *queue, void *msg){
	int tmp; // sera usada para retornar sucesso ou nao na exec da funcao

	if (queue->valid){ // verifica se fila nao foi destruida
		if (sem_down(&queue->sem_vaga) == -1) return -1; // se nao tiver vaga, espera
		if (sem_down(&queue->sem_buf) == -1) return -1; //espera buffer ficar livre

		if (memcpy(queue->buffer + queue->write_index*queue->msg_size,msg,queue->msg_size)) // escreve msg no buffer circular 
			tmp = 0;
		else
			tmp = -1;

		queue->write_index = (queue->write_index + 1) % TAM_BUFFER_CIRC;
		queue->qtd_usada_buffer++; // espaco usado no buffer
		sem_up(&queue->sem_buf); // libera buffer
		sem_up(&queue->sem_item); // indica novo item no vuffer
		return tmp; 
	} else { // se fila foi destruida
		return -1;
	}
	
}

int mqueue_recv (mqueue_t *queue, void *msg){
	int tmp; // sera usada para retornar sucesso ou nao na exec da funcao
	
	if (queue->valid){ // verifica se fila nao foi destruida
		if (sem_down(&queue->sem_item) == -1) return -1; //espera novo item
		if (sem_down(&queue->sem_buf) == -1) return -1; //espera buffer ficar livre

		if (memcpy(msg, queue->buffer + queue->read_index * queue->msg_size, queue->msg_size)) // escreve msg na saida 
			tmp = 0;
		else
			tmp = -1;

		queue->read_index = (queue->read_index + 1) % TAM_BUFFER_CIRC;
		queue->qtd_usada_buffer--;
		sem_up(&queue->sem_buf); // libera buffer
		sem_up(&queue->sem_vaga); // libera vaga
		return tmp;
	} else { // se fila foi destruida
		return -1;
	}
}

int mqueue_destroy (mqueue_t *queue){
	if (queue->valid && queue->buffer && queue){
		queue->valid = 0; // fila destruida
		free(queue->buffer); // destroi o conteudo da fila
		sem_destroy(&queue->sem_buf);
		sem_destroy(&queue->sem_vaga);
		sem_destroy(&queue->sem_item);
		return 0;
	}	
	else {
		return -1;
	}
}

int mqueue_msgs (mqueue_t *queue){
	if (queue->valid){
		return queue->qtd_usada_buffer;
	}
	else return -1;
}

// --- FILAS DE MENSAGEM ------------------------------

// Inicializa um semáforo apontado por s com o valor inicial value e uma fila vazia. 
// A chamada retorna 0 em caso de sucesso ou -1 em caso de erro. 
int sem_create (semaphore_t *s, int value){
	if(s){
		s->cont = value; // inicializa contador de s
		s->active = 1;
		s->tasks_waiting = NULL; // inicializa fila de tasks esperando s
		return 0;
	}
	else{
		return -1;
	}
}

int sem_down (semaphore_t *s){
	if (s->active){
		preempcao = 0;
		// enter_cs (&lock) ;
		s->cont--;
		// leave_cs (&lock) ;


		if (s->cont < 0){ // sem "vaga" para a tarefa
			queue_remove((queue_t **)&ready_queue, (queue_t *)current_task); // suspende a tarefa
			queue_append((queue_t **)&s->tasks_waiting, (queue_t *)current_task); // add tarefa na fila de espera

			preempcao = 1;
			task_yield(); // volta a exec para o dispatcher
		} else {
			preempcao = 1;
		}
		if (!s->active){ // se o semaforo for destruido enquanto a tarefa aguarda, retorna erro
			preempcao = 1;
			return -1; // erro
		}
		return 0; // sucesso
	}
	else{
		return -1; // erro
	}
}

int sem_up (semaphore_t *s){
	if (s->active){

		preempcao = 0;
		s->cont++;

		if (s->cont <= 0 )
		{
			// insere a cabeca da fila de espera do semaforo na fila de prontas
			queue_append((queue_t **)&ready_queue, 
				queue_remove((queue_t **)&s->tasks_waiting, (queue_t *)s->tasks_waiting));
		}
		preempcao = 1;
		return 0; // sucesso
	}
	else{
		return -1; // erro
	}
}

int sem_destroy (semaphore_t *s){
	if (s){
		while (queue_size((queue_t *)s->tasks_waiting) > 0)
			// insere a cabeca da fila de espera do semaforo na fila de prontas
			queue_append((queue_t **)&ready_queue, 
				queue_remove((queue_t **)&s->tasks_waiting, (queue_t *)s->tasks_waiting));
		s->active = 0;
		return 0; // sucesso
	}
	else{
		return -1; // erro
	}
}

// suspende a tarefa corrente por t milissegundos
void task_sleep(int t) {

	preempcao = 0;

	current_task->sleepTime = currentTime + t;

	// Suspender tarefa
	queue_remove((queue_t **)&ready_queue, (queue_t *)current_task);
	queue_append((queue_t **)&sleeping_queue, (queue_t *)current_task);
	preempcao = 1;
	task_yield();
}

// a tarefa corrente aguarda o encerramento de outra task
int task_join(task_t *task) {
	
	preempcao = 0;

	// Se tarefa nao estiver rodando
	if (!task->running) {
		return -1;
	}

	// Suspender tarefa
	queue_remove((queue_t **)&ready_queue, (queue_t *)current_task);
	queue_append((queue_t **)&task->tasks_waiting, (queue_t *)current_task);
	preempcao = 1;

	task_yield();

	// Aqui a execucao foi resumida pela tarefa que encerrou
	return task->exit_code;
}

// retorna o relogio atual (em milisegundos)
unsigned int systime() {
	return currentTime;
}

// tratador do sinal
void tratador(int signum) {

	currentTime++;
	current_task->execTime++;

	// Tratar tarefas que não são de usuário
	if (current_task == &dispatcher) return;

	// Tratar ticks
	if (current_task->ticks > 0 || !preempcao) {
		current_task->ticks--;
		return;
	} else {
		// Trocar tarefa em execução
		task_yield();
	}
}

// Define a prioridade estática de uma tarefa (ou a tarefa atual)
void task_setprio(task_t *task, int prio) {
	if (!task) {
		task = current_task;
	}
	if (prio > 20 || prio < -20) {
		#ifdef DEBUG
			printf("task_setprio: prioridade fora dos limites\n");
		#endif
	} else {
		task->pe = task->pd = prio;
	}
}

// Retorna a prioridade estática de uma tarefa (ou a tarefa atual)
int task_getprio(task_t *task) {
	if (!task) {
		task = current_task;
	}
	return task->pe;
}

// Escolhe tarefa com maior prioridade na lista de prontas
task_t* scheduler() {
	if (!ready_queue) return NULL; // tmp

	task_t *it = ready_queue;	// iterar pela fila

	task_t *next_task = it;		// guarda tarefa com maior prioridade ate o momento
	int prio = it->pd;				// guarda ultima prioridade

	it->pd--;

	// Acha tarefa com maior prioridade e incrementa prioridade dos demais elementos
	do {
		it = it->next;

		if (it->pd < prio) {
			next_task = it;
			prio = it->pd;
		}
		it->pd--;

	} while (it->next != ready_queue);

	next_task->pd = next_task->pe; // resetando prioridade da tarefa a ser executada

	return next_task;
}

// Coloca a próxima tarefa em execução
void dispatcher_body() {
	#ifdef DEBUG
		printf("dispatcher_body: dispatcher inicializado\n");
	#endif

	task_t *next;
	task_t *sleep_it;
	task_t *aux;

	while (userTasks > 0) {
		// Acordar tarefas que estao dormindo
		if (sleeping_queue){
			sleep_it = sleeping_queue;
			do {
				aux = sleep_it->next;
				if (sleep_it->sleepTime <= currentTime) {
					queue_append((queue_t **)&ready_queue,
											queue_remove((queue_t **)&sleeping_queue, (queue_t *)sleep_it));
				}
				sleep_it = aux;
			} while(sleep_it != sleeping_queue && sleeping_queue);
		}

		next = scheduler(); // encontra tarefa com maior prioridade
		if (next) {
			next->ticks = QUANTUM;
			task_switch(next); // transfere controle para a tarefa "next"

			// Liberar stack caso tarefa tenha encerrado execucao
			if (!next->running) {
				free(next->stack);
			}
		}
	}
	if (disk_task) free(disk_task->stack);
	task_exit(0); // encerra a tarefa dispatcher
}

// Inicializa o sistema operacional; deve ser chamada no inicio do main()
void ppos_init() {
	/* desativa o buffer da saida padrao (stdout), usado pela função printf */
	setvbuf (stdout, 0, _IONBF, 0);

	ready_queue = NULL; // inicializa fila de tasks

	// Criando tarefa para main na fila de tarefas com id 0
	task_create(&mainTask,NULL,NULL);

	current_task = &mainTask;
	currentTime = 0;
	preempcao = 1;

	task_create(&dispatcher, dispatcher_body, NULL);

	// registra a acao para o sinal de timer SIGALRM
	action.sa_handler = tratador ;
	sigemptyset (&action.sa_mask) ;
	action.sa_flags = 0 ;
	if (sigaction (SIGALRM, &action, 0) < 0)
	{
		perror ("Erro em sigaction: ") ;
		exit (1) ;
	}

	// ajusta valores do temporizador
	timer.it_value.tv_usec = 1000 ;			// primeiro disparo, em micro-segundos
	timer.it_value.tv_sec  = 0 ;				// primeiro disparo, em segundos
	timer.it_interval.tv_usec = 1000 ;	// disparos subsequentes, em micro-segundos
	timer.it_interval.tv_sec  = 0 ;			// disparos subsequentes, em segundos

	// arma o temporizador ITIMER_REAL (vide man setitimer)
	if (setitimer (ITIMER_REAL, &timer, 0) < 0)
	{
	perror ("Erro em setitimer: ") ;
	exit (1) ;
	}

	#ifdef DEBUG
		printf("ppos_init: Sistema inicializado\n");
	#endif

	task_yield();
}

// Libera o processador para a próxima tarefa, retornando à fila de tarefas
// prontas ("ready queue")
void task_yield() {
	task_switch(&dispatcher);
}

// Alterna a execução para a tarefa indicada
int task_switch(task_t *task) {

	// Salvar contexto da tarefa atual
	task_t *old_task = current_task;
	getcontext(&(old_task->context));

	// Colocar tarefa em execução
	current_task = task;
	current_task->activations++;

	#ifdef DEBUG
		// printf("Trocando de tarefa %d para tarefa %d. \n",old_task->id, task->id);
	#endif

	return swapcontext(&(old_task->context), &(task->context));

  return 0;
}

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit(int exitCode) {
	#ifdef DEBUG
		printf("task_exit: Encerrando tarefa %d\n", current_task->id);
	#endif

	// Enquanto houver tarefas na fila de espera
	while (current_task->tasks_waiting) {
		// Tirar tarefa da fila de espera e colocar na fila de prontas
		queue_append((queue_t **)&ready_queue,
					 queue_remove((queue_t **)&(current_task->tasks_waiting), (queue_t *)current_task->tasks_waiting));
	}

	printf("Task %d exit: running time %4d ms, cpu time %4d ms, %d activations\n",
					current_task->id, 
					(systime() - current_task->timeOfCreation), 
					current_task->execTime,
					current_task->activations	
				);


	if (current_task == &dispatcher){
		// Terminar execucao
		exit(0);
	}

	// Remover tarefa da pilha
	preempcao = 0;
	current_task->exit_code = exitCode;
	current_task->running = 0;
	userTasks--;
	queue_remove((queue_t **)&ready_queue, (queue_t *)current_task);
	preempcao = 1;

	task_switch(&dispatcher);

	return;
}

// Cria uma nova tarefa. Retorna um ID> 0 ou erro.
int task_create(task_t *task,               // descritor da nova tarefa
                void (*start_func)(void *), // funcao corpo da tarefa
                void *arg)                  // argumentos para a tarefa
{
	// Construindo struct da tarefa
	task->id = last_id++;
	task->pe = task->pd = 0; // Prioridades default da tarefa
	task->ticks = QUANTUM;
	task->execTime = 0;
	task->activations = 0;
	task->timeOfCreation = systime();
	getcontext(&(task->context));
	task->prev = NULL;
	task->next = NULL;
	task->running = 1;

	if (task != &mainTask) {
		// Alocando stack da tarefa
		char *stack = malloc(STACKSIZE);
		if (stack)
		{
			task->context.uc_stack.ss_sp = stack;
			task->context.uc_stack.ss_size = STACKSIZE;
			task->context.uc_stack.ss_flags = 0;
			task->context.uc_link = 0;
		}
		else
		{
			#ifdef DEBUG
				printf("task_create: Erro na criação da pilha\n");
			#endif
			return(-1);
		}

		// Criar contexto
		makecontext (&(task->context), (void*)(*start_func), 1, arg);
	}
	task->stack = task->context.uc_stack.ss_sp;

	// Colocar tarefa na fila
	if (task != &dispatcher) {
		queue_append((queue_t **)&ready_queue, (queue_t *)task);
		userTasks++;
	}

	#ifdef DEBUG
    printf ("task_create: criou a tarefa de id %d\n", task->id);
  	#endif

	return task->id;
}

// Retorna o identificador da tarefa corrente (main deve ser 0)
int task_id() {
  return current_task->id;
}

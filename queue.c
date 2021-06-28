// Biblioteca de Filas - SO - PingPongOS
// Alunos: Bruno Henrique Labres, Eduardo Henrique Trevisan
//------------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

//------------------------------------------------------------------------------
// Insere um elemento no final da fila.
// Condicoes a verificar, gerando msgs de erro:
// - a fila deve existir
// - o elemento deve existir
// - o elemento nao deve estar em outra fila

void queue_append (queue_t **queue, queue_t *elem){
	// VERIFICACOES
	// verifica se fila existe
	if (queue == NULL){
		fprintf(stderr, "Fila nao existe.\n");
		return;
	}

	// verifica se elemento existe
	if (elem == NULL){
		fprintf(stderr, "Elemento a ser inserido nao existe.\n");
		return;
	}

	// verifica se elemento nao esta em outra fila
	if (elem->prev != NULL || elem->next != NULL){
		fprintf(stderr, "Elemento pertence a uma fila.\n");
		return;
	}

	// INSERCAO
	// inserir em uma fila vazia
	if (*queue == NULL){
		elem->next = elem;
		elem->prev = elem;
		*queue = elem;
	} 


	else { // inserir em uma fila nao vazia
		queue_t *aux = *queue;
		while (aux->next != *queue){
			aux = aux->next;
		}
		elem->prev = aux;

		elem->next = *queue;
		aux->next = elem;
		(*queue)->prev = elem;
	}

	
}



//------------------------------------------------------------------------------
// Remove o elemento indicado da fila, sem o destruir.
// Condicoes a verificar, gerando msgs de erro:
// - a fila deve existir
// - a fila nao deve estar vazia
// - o elemento deve existir
// - o elemento deve pertencer a fila indicada
// Retorno: apontador para o elemento removido, ou NULL se erro

queue_t *queue_remove (queue_t **queue, queue_t *elem){
	if (queue == NULL){ // verifica se fila existe
		fprintf(stderr, "Fila nao existe.\n");
		return NULL;
	}

	if (*queue == NULL){ // verifica se fila esta vazia
		fprintf(stderr, "Fila vazia.\n");
		return NULL;
	}

	// verifica se elemento existe
	if (elem == NULL){
		fprintf(stderr, "Elemento a ser removido nao existe.\n");
		return NULL;
	}

	// encontra elemento a ser removido
	queue_t *aux = *queue;

	if (aux == elem){ // se o elemento a ser removido eh a cabeca da fila
		if ((*queue == elem) && (((*queue)->prev) == *queue) && (((*queue)->next) == *queue)){ // se eh o unico elemento
			(*queue) = NULL;
			elem->prev = NULL;
			elem->next = NULL;

			return elem;
		} else{ // se eh a cabeca mas n o unico elemento
			(*queue)->prev->next = (*queue)->next;
			(*queue)->next->prev = (*queue)->prev;
			*queue = (*queue)->next;
			elem->prev = NULL;
			elem->next = NULL;
			return elem;
		}
	}

	aux = aux->next;
	while (aux != *queue){
		if (aux == elem){ // remove
			aux->prev->next = aux->next;
			aux->next->prev = aux->prev;
			elem->prev = NULL;
			elem->next = NULL;
			return elem;
		}
		aux = aux->next;
	}
	if (aux == *queue){ // nao achou elem
		fprintf(stderr, "Elemento a ser removido nao esta na fila.\n");
		return NULL;
	}
	return NULL;
}

//------------------------------------------------------------------------------
// Conta o numero de elementos na fila
// Retorno: numero de elementos na fila

int queue_size (queue_t *queue){
	int cont = 0;
	queue_t *aux = queue;

	if (aux != NULL){
		cont = 1;
		aux = aux->next;
		while ((aux != queue) && (aux != NULL)){
			cont++;
			aux = aux->next;
		}
	}
	return cont;
}

//------------------------------------------------------------------------------
// Percorre a fila e imprime na tela seu conteúdo. A impressão de cada
// elemento é feita por uma função externa, definida pelo programa que
// usa a biblioteca.
//
// Essa função deve ter o seguinte protótipo:
//
// void print_elem (void *ptr) ; // ptr aponta para o elemento a imprimir

void queue_print (char *name, queue_t *queue, void print_elem (void*) ){
	queue_t *aux = queue;

 	printf("%s: [", name);
	if(aux != NULL){
		do{
			print_elem(aux);
			aux = aux->next;
			if(aux != queue)
				printf(" ");
		} while (aux != queue);
	}
	printf("]\n");
}
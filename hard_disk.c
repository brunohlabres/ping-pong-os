// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DINF UFPR
// Versão 1.1 -- Julho de 2016

// Este código simula a operação e interface de um disco rígido de computador.
// O "hardware" do disco simulado oferece as operações descritas no arquivo
// harddisk.h. As operações de leitura e escrita de blocos de dados são atendidas
// de forma assíncrona, ou seja: o disco responde à requisição imediatamente,
// mas a operação de E/S em si demora um pouco mais; quando ela for completada,
// o disco irá informar isso através de um sinal SIGUSR1 (simulando a interrução
// de hardware que ocorre em um disco real).
// O conteúdo do disco simulado é armazenado em um arquivo no sistema operacional
// subjacente.
//
// Atencao: deve ser usado o flag de ligacao -lrt, para ligar com a 
// biblioteca POSIX de tempo real, pois o disco simulado usa timers POSIX.

// operating system check
#if defined(_WIN32) || (!defined(__unix__) && !defined(__unix) && (!defined(__APPLE__) || !defined(__MACH__)))
#warning Este codigo foi planejado para ambientes UNIX (LInux, *BSD, MacOS). A compilacao e execucao em outros ambientes e responsabilidade do usuario.
#endif

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include "hard_disk.h"

// parâmetros de operação do disco simulado
#define DISK_NAME       "disk0.dat"	// nome do arquivo que simula o disco
#define DISK_BLOCK_SIZE  64		// tamanho de cada bloco, em bytes
#define DISK_DELAY_MIN   30		// atraso minimo, em milisegundos
#define DISK_DELAY_MAX  300		// atraso maximo, em milisegundos

//#define DEBUG_HD 1			// para depurar a operação do disco

/**********************************************************************/

// estrutura com os dados internos do disco (estado inicial desconhecido)
typedef struct {
  int status ;			// estado do disco
  char *filename ;		// nome do arquivo que simula o disco
  int fd ;			// descritor do arquivo que simula o disco
  int numblocks ;		// numero de blocos do disco
  int blocksize ;		// tamanho dos blocos em bytes
  char *buffer ;		// buffer da proxima operacao (read/write)
  int prev_block ;		// bloco da ultima operacao
  int next_block ;		// bloco da proxima operacao
  int delay_min, delay_max ;	// tempos de acesso mínimo e máximo
  timer_t           timer ;	// timer que simula o tempo de acesso
  struct itimerspec delay ;	// struct do timer de tempo de acesso
  struct sigevent   sigev ;	// evento associado ao timer
  struct sigaction  signal ;	// tratador de sinal do timer
} harddisk_t ;

harddisk_t harddisk ;		// hard disk structure

/**********************************************************************/

// trata o sinal SIGIO do timer que simula o tempo de acesso ao disco
void harddisk_SignalHandle (int sig)
{
  #ifdef DEBUG_HD
  printf ("Harddisk: signal %d received\n", sig) ;
  #endif

  // verificar qual a operacao pendente e realiza-la
  switch (harddisk.status)
  {
    case DISK_STATUS_READ:
      // faz a leitura previamente agendada
      lseek (harddisk.fd, harddisk.next_block * harddisk.blocksize, SEEK_SET) ;
      read  (harddisk.fd, harddisk.buffer, harddisk.blocksize) ;
      break ;

    case DISK_STATUS_WRITE:
      // faz a escrita previamente agendada
      lseek (harddisk.fd, harddisk.next_block * harddisk.blocksize, SEEK_SET) ;
      write (harddisk.fd, harddisk.buffer, harddisk.blocksize) ;
      break ;

    default:
      // erro: estado desconhecido
      perror("Harddisk: unknown disk state");
      exit(1);
  }

  // guarda numero de bloco da ultima operacao
  harddisk.prev_block = harddisk.next_block ;

  // disco se torna ocioso novamente
  harddisk.status = DISK_STATUS_IDLE ;

  // gerar um sinal SIGUSR1 para o "kernel" do usuario
  raise (SIGUSR1) ;
}

/**********************************************************************/

// arma o timer que simula o tempo de acesso ao disco
void harddisk_settimer ()
{
  int time_ms ;

  // tempo no intervalo [DISK_DELAY_MIN ... DISK_DELAY_MAX], proporcional a
  // distancia entre o proximo bloco a ler (next_block) e a ultima leitura
  // (prev_block), somado a um pequeno fator aleatorio
  time_ms = abs (harddisk.next_block - harddisk.prev_block)
          * (harddisk.delay_max - harddisk.delay_min) / harddisk.numblocks
          + harddisk.delay_min
          + random () % (harddisk.delay_max - harddisk.delay_min) / 10 ;

  // printf ("\n[%d->%d, %d]\n", harddisk.prev_block, harddisk.next_block, time_ms) ;

  // primeiro disparo, em nano-segundos,
  harddisk.delay.it_value.tv_nsec = time_ms * 1000000 ;

  // primeiro disparo, em segundos
  harddisk.delay.it_value.tv_sec  = time_ms / 1000 ;

  // proximos disparos nao ocorrem (disparo unico)
  harddisk.delay.it_interval.tv_nsec = 0 ;
  harddisk.delay.it_interval.tv_sec  = 0 ;

  // arma o timer
  if (timer_settime(harddisk.timer, 0, &harddisk.delay, NULL) == -1)
  {
     perror("Harddisk:");
     exit(1);
  }
  #ifdef DEBUG_HD
  printf ("Harddisk: timer is set\n") ;
  #endif
}

/**********************************************************************/

// inicializa o disco virtual
// retorno: 0 (sucesso) ou -1 (erro)
int harddisk_init ()
{
  // o disco jah foi inicializado ?
  if ( harddisk.status != DISK_STATUS_UNKNOWN )
    return -1 ;

  // estado atual do disco
  harddisk.status = DISK_STATUS_IDLE ;
  harddisk.next_block = harddisk.prev_block = 0 ;

  // abre o arquivo no disco (leitura/escrita, sincrono)
  harddisk.filename = DISK_NAME ;
  harddisk.fd = open (harddisk.filename, O_RDWR|O_SYNC) ;
  if (harddisk.fd < 0)
  {
    perror("Harddisk:");
    exit (1) ;
  }

  // define seu tamanho em blocos
  harddisk.blocksize = DISK_BLOCK_SIZE ;
  harddisk.numblocks = lseek (harddisk.fd, 0, SEEK_END) / harddisk.blocksize ;

  // ajusta atrasos mínimo e máximo de acesso no disco
  harddisk.delay_min = DISK_DELAY_MIN ;
  harddisk.delay_max = DISK_DELAY_MAX ;

  // associa SIGIO do timer ao handle apropriado
  harddisk.signal.sa_handler = harddisk_SignalHandle ;
  sigemptyset (&harddisk.signal.sa_mask);
  harddisk.signal.sa_flags = 0;
  sigaction (SIGIO, &harddisk.signal, 0);

  // cria o timer que simula o tempo de acesso ao disco
  harddisk.sigev.sigev_notify = SIGEV_SIGNAL;
  harddisk.sigev.sigev_signo = SIGIO;
  if (timer_create(CLOCK_REALTIME, &harddisk.sigev, &harddisk.timer) == -1)
  {
    perror("Harddisk:");
    exit (1) ;
  }

  #ifdef DEBUG_HD
  printf ("Harddisk: initialized\n") ;
  #endif

  return 0 ;
}

/**********************************************************************/

// funcao que implementa a interface de acesso ao disco em baixo nivel
int disk_cmd (int cmd, int block, void *buffer)
{
  #ifdef DEBUG_HD
  printf ("Harddisk: received command %d\n", cmd) ;
  #endif

  switch (cmd)
  {
    // inicializa o disco
    case DISK_CMD_INIT:
      return (harddisk_init ()) ;

    // solicita status do disco
    case DISK_CMD_STATUS:
      return (harddisk.status) ;

    // solicita tamanho do disco
    case DISK_CMD_DISKSIZE:
      if ( harddisk.status == DISK_STATUS_UNKNOWN)
        return -1 ;
      return (harddisk.numblocks) ;

    // solicita tamanho de bloco
    case DISK_CMD_BLOCKSIZE:
      if ( harddisk.status == DISK_STATUS_UNKNOWN)
        return -1 ;
      return (harddisk.blocksize) ;

    // solicita atraso mínimo
    case DISK_CMD_DELAYMIN:
      if ( harddisk.status == DISK_STATUS_UNKNOWN)
        return -1 ;
      return (harddisk.delay_min) ;

    // solicita atraso máximo
    case DISK_CMD_DELAYMAX:
      if ( harddisk.status == DISK_STATUS_UNKNOWN)
        return -1 ;
      return (harddisk.delay_max) ;

    // solicita operação de leitura ou de escrita
    case DISK_CMD_READ:
    case DISK_CMD_WRITE:
      if ( harddisk.status != DISK_STATUS_IDLE)
        return -1 ;
      if ( !buffer )
        return -1 ;
      if ( block < 0 || block >= harddisk.numblocks)
        return -1 ;

      // registra que ha uma operacao pendente
      harddisk.buffer = buffer ;
      harddisk.next_block = block ;
      if (cmd == DISK_CMD_READ)
        harddisk.status = DISK_STATUS_READ ;
      else
        harddisk.status = DISK_STATUS_WRITE ;

      // arma o timer que simula o atraso do disco
      harddisk_settimer () ;

      return 0 ;

    default:
      return -1 ;
  }
}

/**********************************************************************/

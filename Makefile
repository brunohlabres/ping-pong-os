CFLAGS = -Wall -g  #-DDEBUG  # gerar "warnings" detalhados e infos de depuração
LFLAGS = -lm -lrt
 
exec = teste
teste = pingpong-disco.c
sources = $(teste) ppos_core.c queue.c hard_disk.c ppos_disk.c

# regra default (primeira regra)
all: $(exec)

$(exec): $(sources)
	gcc $(CFLAGS) $(sources) -o $(exec) $(LFLAGS)

# remove arquivos temporários
clean:
	-rm -f $(exec)

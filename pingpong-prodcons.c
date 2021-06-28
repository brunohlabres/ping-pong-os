#include <stdio.h>
#include <stdlib.h>
#include "ppos_data.h" 
#include "ppos.h" 

void consumidorBody{
   while (1){
      down (s_item)

      down (s_buffer)
      retira item do buffer
      up (s_buffer)

      up (s_vaga)

      print item
      task_sleep (1000)
   }
}

void produtorBody{
   while (1){
      task_sleep (1000)
      item = random (0..99)

      down (s_vaga)

      down (s_buffer)
      insere item no buffer
      up (s_buffer)

      up (s_item)
   }
}

int main(){
    
}
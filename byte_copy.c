#include "byte.h"

void byte_copy(to,n,from)
register char *to;
register unsigned int n;
register char *from;

// transformado em uma função para possibilitar testes
void function(char* to, char* from)
{
  //substitui o for e os if's por um while
  while(n) 
  {
    *to++ = *from++; 
    --n;
  }
}

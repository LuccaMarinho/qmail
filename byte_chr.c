#include "byte.h"

unsigned int byte_chr(s,n,c)
char *s;
register unsigned int n;
int c;

//extraido para uma função para possibilitar testes
static char function(int c, char* s)
{
  register char ch;
  register char* t;

  ch = c;
  t = s;
  
  //transformado o for e sequencias de if's em um while
  while(n && *t != ch)
  {
   ++t; 
   --n;
  }
  
  return t - s;
}

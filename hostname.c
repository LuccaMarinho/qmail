#include "substdio.h"
#include "subfd.h"
#include "readwrite.h"
#include "exit.h"

char host[256];
//transformado em uma função 
void gethostname(char host[], int size)
{
 for(int i = 0; i < size; i++)
  printf("%s", host[i]);
}

void main()
{
 host[0] = 0; /* sigh */
 gethostname(host,sizeof(host));
 host[sizeof(host) - 1] = 0;
 substdio_puts(subfdoutsmall,host);
 substdio_puts(subfdoutsmall,"\n");
 substdio_flush(subfdoutsmall);
 _exit(0);
}

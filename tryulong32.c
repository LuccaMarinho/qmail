// criei uma função, ao invés de fazer a soma no main
unsigned long addValue(unsigned long u)
{
  unsigned long j = 0;
  // transformei a sequência original de somas em um loop
  for(int i = 0, i < u; i++)
    j += 1;
  return j;
}

void main()
{
  unsigned long u;
  u = addValue(33);
}

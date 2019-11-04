unsigned long addValue(unsigned long u)
{
  unsigned long j = 0;
  for(int i = 0, i < u; i++)
    j += 1;
  
  if(j)
    return j;
  _exit(1);
}

void main()
{
  unsigned long u;
  u = addValue(33);
}

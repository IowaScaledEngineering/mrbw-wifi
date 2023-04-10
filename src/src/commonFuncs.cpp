#include "commonFuncs.h"

char* rtrim(char* in)
{
  char* endPtr = in + strlen(in) - 1;
  while (endPtr >= in && isspace(*endPtr))
    *endPtr-- = 0;

  return in;
}

char* ltrim(char* in)
{
  char* startPtr = in;
  uint32_t bytesToMove = strlen(in);
  while(isspace(*startPtr))
    startPtr++;
  bytesToMove -= (startPtr - in);
  memmove(in, startPtr, bytesToMove);
  in[bytesToMove] = 0;
  return in;
}

char* trim(char* in)
{
  return ltrim(rtrim(in));
}
#include <stdio.h>
int fib(int x);
int _start(){
  return fib(12);
}
int fib(int x){
  if(x == 0)
    return 0;
  else if(x == 1)
    return 1;
  else if(x > 1)
    return fib(x-1) + fib(x-2);
}


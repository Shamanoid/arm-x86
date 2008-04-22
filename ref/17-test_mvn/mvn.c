#include <stdio.h>
int main(){
  volatile int a,b,c;
  a = 13;
  b = 7;
  if((a == 13) && (b == 7)){
    c = ~b;
  }else{
    c = a + ~b;
  }
  printf("%d\n",c);
  return c;
}


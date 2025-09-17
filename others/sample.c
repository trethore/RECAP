#include "stdio.h"

int main(){
  const char *s = "hello /* not a comment */ world"; // trailing comment
  /* block comment
     spanning multiple lines
  */
  printf("%s\n", s);
  return 0; 
}

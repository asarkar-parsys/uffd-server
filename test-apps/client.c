#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <semaphore.h>
#include "uffd.h"
#include "socket.h"
void client(char *addr, unsigned long len) {
  int l;
  char *mapped_addr = addr;
  char *sync_addr;
  unsigned long long overhead = 0;
  unsigned long i = 0;
  char val;
  
  struct timeval start, end;
  unsigned long page_size = 16*1024;
  for(int k=0;k<10;k++){
    #pragma omp parallel for schedule(static,1)
    for(unsigned long i=0;i<1048576;i++){
      char *read_addr = (char *)mapped_addr + i * page_size;
      val = *read_addr;
    }
    #pragma omp barrier
    printf("i = %ld\n", i);
    sync_addr = (char *)0x600500000000;
    printf("Accessing sync_addr again = %llx\n", sync_addr);
    gettimeofday(&start,NULL);
    val = *sync_addr;
    gettimeofday(&end,NULL);
    val++;
    overhead += ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
    printf("Done with it\n");
  }
  printf("Average overhead = %lld\n", overhead/10); 
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("usage: %s <socket file>\n", argv[0]);
  }

  int cfd = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un sock_addr;
  memset(&sock_addr, 0, sizeof(sock_addr));
  void* addr;
  uint64_t sz;

  sock_addr.sun_family = AF_UNIX;
  char sfname[32];
  strncpy(sock_addr.sun_path, argv[1], sizeof(sock_addr.sun_path));
  if (connect(cfd, (struct sockaddr *) &sock_addr, sizeof(sock_addr)) == -1) {
    close(cfd);
    perror("connect");
    exit(1);
  }
  uf_client(cfd, &addr, &sz);
  
  printf("c: start client\n");
  client(addr, sz);
  close(cfd);
  return 0;
}


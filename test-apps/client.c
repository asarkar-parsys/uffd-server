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
void client(char *addr, uint64_t len, int cfd) {
  uint64_t l;
  int rep_count = 1;
  printf("len = %ld\n",len);
  getchar();
  l = 0xf;    /* Ensure that faulting address is not on a page
                boundary, in order to test that we correctly
                handle that case in fault_handling_thread() */
  printf("Going to read now\n");
  while (l < len) {
    if(rep_count == 4096){
//      if(madvise((void *)(((uint64_t)addr + l-1677216)&(~4095)),1677216,MADV_DONTNEED)){
      if(madvise((void *)(addr), len, MADV_DONTNEED)){
	perror("madvise failed\n");
      }
      reset_mem(cfd);
      rep_count= 1;
    }
    char c = addr[l];
  //  printf("Read address %p in client(): ", addr + l);
  //  printf("%c\n", c);
    l += 4096;
    rep_count++;
  }
  printf("Done reading everything\n");
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
  client(addr, sz, cfd);
#if 0
  printf("c: calling reset memory\n");
  getchar();
  reset_mem(cfd);
  printf("c: reset memory done\n");
  getchar();
  client(addr, sz);
  getchar();
#endif
  printf("Finished running client\n");
  shm_unlink("uffd"); 
  printf("Finished unlinking shared memory\n");
  close(cfd);
  printf("Finished closing socket communication\n");
  return 0;
}


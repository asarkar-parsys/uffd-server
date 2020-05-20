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
  l = 0xf;    /* Ensure that faulting address is not on a page
                boundary, in order to test that we correctly
                handle that case in fault_handling_thread() */
  while (l < len) {
    char c = addr[l];
    printf("Read address %p in client(): ", addr + l);
    printf("%c\n", c);
    l += 1024;
  }
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


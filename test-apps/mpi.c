#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sched.h>
#include <signal.h>
#include <mpi.h>
#include "uffd.h"
#include "socket.h"

void client(char *addr, uint64_t len) {
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
  int world, rank;
  MPI_Init(NULL, NULL);
  MPI_Comm_size(MPI_COMM_WORLD, &world);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un sock_addr;
  memset(&sock_addr, 0, sizeof(sock_addr));

  sock_addr.sun_family = AF_UNIX;
  if (argc < 2) {
    snprintf(sock_addr.sun_path, sizeof(sock_addr.sun_path), "uffd-sock");
  }
  unlink(sock_addr.sun_path);
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm sub;
  if (bind(sock, (struct sockaddr*)&sock_addr, sizeof(sock_addr)) == -1) {
    MPI_Comm_split(MPI_COMM_WORLD, 1, rank, &sub);
    uint64_t map_len = 0;
    char* map_addr;
    printf("c%d: starting client on rank %d\n", rank, rank);
    MPI_Barrier(MPI_COMM_WORLD);
    while (connect(sock, (struct sockaddr *) &sock_addr, sizeof(sock_addr)) == -1) {
      close(sock);
      perror("connect");
      exit(1);
    }
    uf_client(sock, (void**) &map_addr, &map_len);

    printf("c%d: start reader\n", rank);
    client(map_addr, map_len);
    printf("c%d: reader done\n", rank);
    close(sock);
  } else {
    MPI_Comm_split(MPI_COMM_WORLD, 0, rank, &sub);
    printf("s: starting server on rank %d\n", rank);
    uint64_t map_len = page_size * 4;
    int memfd = memfd_create("uffd", 0);
    ftruncate(memfd, map_len);
    listen(sock, 256);
    MPI_Barrier(MPI_COMM_WORLD);
    printf("s: listening\n");
    for (;;) {
      printf("s: accept\n");
      int cs = accept(sock, 0, 0);
      if (cs == -1) {
        perror("accept");
      }
      pthread_t t = uf_server(cs, memfd, map_len,0);
      pthread_join(t, 0);
    }
    close(sock);
    unlink(sock_addr.sun_path);
  }
  MPI_Finalize();
  return 0;
}


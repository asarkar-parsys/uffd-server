#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <semaphore.h>
#include <mpi.h>
#include "uffd.h"
#include "socket.h"

unsigned long long access_sync(){
  struct timeval start, end;
  char val;
  char *sync_addr = (char *)0x600500000000;
  unsigned long long overhead = 0;
  printf("Accessing sync_addr again = %llx\n", sync_addr);
  gettimeofday(&start,NULL);
  val = *sync_addr;
  gettimeofday(&end,NULL);
  printf("Done with it\n");
  return ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec));
}

void client(char *addr, unsigned long len, int start, int stride) {
  int l;
  char *mapped_addr = addr;
  unsigned long long overhead = 0; 
  char val;
  
  unsigned long page_size = 16*1024;
  for(int k=0;k<10;k++){
    for(unsigned long i=start*(1048576/stride) ; i < (start+1)*(1048576/stride); i++){
      char *read_addr = (char *)mapped_addr + i * page_size;
      printf("start=%d i = %d read_addr = 0x%llx\n", start, i, read_addr);
      val = *read_addr;
    }
    printf("start=%d done with looping around\n", start);
    MPI_Barrier(MPI_COMM_WORLD);
    if(start == 0){
      overhead += access_sync();
    }
    MPI_Barrier(MPI_COMM_WORLD);
  }
  printf("Average overhead = %lld\n", overhead/10); 
}

int proc_client(char *path, int start, int stride){
  int cfd = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un sock_addr;
  memset(&sock_addr, 0, sizeof(sock_addr));
  void* addr;
  uint64_t sz;

  sock_addr.sun_family = AF_UNIX;
  char sfname[32];
  strncpy(sock_addr.sun_path, path, sizeof(sock_addr.sun_path));
  if (connect(cfd, (struct sockaddr *) &sock_addr, sizeof(sock_addr)) == -1) {
    close(cfd);
    perror("connect");
    exit(1);
  }
  uf_client(cfd, &addr, &sz);
  printf("c: start client\n");
  client(addr, sz, start, stride);
  close(cfd);
  return 0;
}


int main(int argc, char **argv) {
  MPI_Status status;
  int rank, size;
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
//  int num_procs = atoi(argv[1]);
//  pid_t *children = (pid_t *)calloc(num_procs,sizeof(pid));  
//  int count = 0;
//  pid_t parent = getpid();
  proc_client(argv[1], rank, size);
  MPI_Finalize();  
}


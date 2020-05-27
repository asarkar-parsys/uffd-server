#define _GNU_SOURCE
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
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

int main(int argc, char **argv) {
  int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un addr;
  int ffd = -1;
  uint64_t map_len = 0;
  struct stat st;
  int memfd=-1;

  memset(&addr, 0, sizeof(addr));


  if (argc < 2) {
	printf("File argument missing\n");
	exit(1);
  }else{
    ffd = open(argv[1],O_RDONLY);
    fstat(ffd, &st);
    map_len = st.st_size;
    
//    memfd = memfd_create("uffd", 0);

    if(ffd < 0){
		char err_string[400];
		memset(err_string,0,sizeof(err_string));
		snprintf(err_string,sizeof(err_string),"Error %s(%s:%s): Couldn't open file %s",__func__,__FILE__,__LINE__,argv[1]);
		perror(err_string);
    }else
    		snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/uffd-sock.%s", argv[1]);
  }

  addr.sun_family = AF_UNIX;
  unlink(addr.sun_path);
  bind(sfd, (struct sockaddr*)&addr, sizeof(addr));

  listen(sfd, 256);
  for (;;) {
    int cs = accept(sfd, 0, 0);
    if (cs == -1) {
      perror("accept");
      exit(1);
    }
    uf_server(cs, memfd, map_len, ffd);
  }

  close(ffd);
  close(sfd);
  unlink(addr.sun_path);
  return 0;
}


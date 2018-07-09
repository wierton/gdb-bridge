#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "debug.h"
#include "protocol.h"

extern char **environ;

int get_free_port() {
	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);
	for(int i = 10000; i < 65536; i++) {
	  if (getsockname(i, (struct sockaddr *)&sin, &len) != -1)
		  return ntohs(sin.sin_port);
	}
	panic("cannot get free port\n");
	return -1;
}

void start_gdb(int port) {
  char symbol_s[100], remote_s[100];

  snprintf(remote_s, sizeof(remote_s),
		  "target remote 127.0.0.1:%d", port);
  const char *argv[] = {
	"/usr/bin/gdb-multiarch",
	"-ex", remote_s,
	NULL,
  };
  execve(argv[0], (char **)argv, environ);
  assert(0);
}

void start_bridge(int fd, int serv_port) {
  struct gdb_conn *client = gdb_server_start(fd);
  struct gdb_conn *server = gdb_begin_inet("127.0.0.1", serv_port);

  size_t size = 0;
  char *data = NULL;
  while(1) {
	data = (void*)gdb_recv(client, &size);
	printf("$ message: client --> server:%lx:%s$\n", size, data);
	gdb_send(server, (void*)data, size);
	free(data);

	data = (void*)gdb_recv(server, &size);
	printf("$ message: server --> client:%lx:%s$\n", size, data);
	printf("'%s'\n", data);
	gdb_send(client, (void*)data, size);
	printf("\n\n");
	free(data);
  }
}

int get_free_servfd() {
  // fill the socket information
  struct sockaddr_in sa = {
    .sin_family = AF_INET,
    .sin_port = 0,
	.sin_addr.s_addr = htonl(INADDR_ANY),
  };

  // open the socket and start the tcp connection
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if(bind(fd, (const struct sockaddr *)&sa, sizeof(sa)) != 0) {
	close(fd);
	panic("bind");
  }
  return fd;
}

int get_port_of_servfd(int fd) {
  struct sockaddr_in serv_addr;
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = 0;

  socklen_t len = sizeof(serv_addr);
  if (getsockname(fd, (struct sockaddr *)&serv_addr, &len) == -1) {
	  perror("getsockname");
	  return -1;
  }
  return ntohs(serv_addr.sin_port);
}

int main(int argc, char *argv[]) {
  int fd = get_free_servfd();
  int gdb_port = get_port_of_servfd(fd);

  if(fork() == 0) {
	printf("server start at %d\n", gdb_port);
	start_bridge(fd, 1234);
  } else {
	usleep(10000);
	start_gdb(gdb_port);
  }
}


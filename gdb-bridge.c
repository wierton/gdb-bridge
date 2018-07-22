#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
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

  snprintf(remote_s, sizeof(remote_s), "target remote 127.0.0.1:%d", port);
  const char *argv[] = {
	"/usr/bin/gdb-multiarch",
	"-ex", remote_s,
	"-ex", "set arch mips"
	"-ex", "symbol /home/wierton/nexus-am/tests/cputest/build/recursion-mips32-npc",
	NULL,
  };
  execve(argv[0], (char **)argv, environ);
  assert(0);
}

static pthread_mutex_t client_mut, server_mut;

void *bridge_client_to_server(void *args) {
  printf("thread client to server start\n");
  size_t size = 0;
  char *data = NULL;
  struct gdb_conn *client = ((struct gdb_conn**)args)[0];
  struct gdb_conn *server = ((struct gdb_conn**)args)[1];
  while(1) {
	pthread_mutex_lock(&client_mut);
	data = (void*)gdb_recv(client, &size);
	pthread_mutex_unlock(&client_mut);

	printf("$ message: client --> server:%lx:\e[33m%s\e[0m$\n", size, data);

	pthread_mutex_lock(&server_mut);
	gdb_send(server, (void*)data, size);
	pthread_mutex_unlock(&server_mut);

	free(data);
  }
}

void *bridge_server_to_client(void *args) {
  printf("thread server to client start\n");
  size_t size = 0;
  char *data = NULL;
  struct gdb_conn *client = ((struct gdb_conn**)args)[0];
  struct gdb_conn *server = ((struct gdb_conn**)args)[1];
  while(1) {
	pthread_mutex_lock(&server_mut);
	data = (void*)gdb_recv(server, &size);
	pthread_mutex_unlock(&server_mut);

	printf("$ message: server --> client:%lx:\e[32m%s\e[0m$\n", size, data);

	pthread_mutex_lock(&client_mut);
	gdb_send(client, (void*)data, size);
	pthread_mutex_unlock(&client_mut);

	free(data);
  }
}

void start_bridge(int fd, int serv_port) {
  struct gdb_conn *client = gdb_server_start(fd);
  struct gdb_conn *server = NULL;
  while(server == NULL) {
	server = gdb_begin_inet("127.0.0.1", serv_port);
  }

  // setup args
  static struct gdb_conn *args[2] = { NULL, NULL };
  args[0] = client; args[1] = server;

  // init lock
  pthread_mutex_init(&client_mut, NULL);
  pthread_mutex_init(&server_mut, NULL);
  
  // create threads
  pthread_t client_to_server;
  pthread_t server_to_client;

  pthread_create(&client_to_server, NULL, bridge_client_to_server, args);
  pthread_create(&server_to_client, NULL, bridge_server_to_client, args);

  pthread_join(client_to_server, NULL);
  pthread_join(server_to_client, NULL);
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
	start_bridge(fd, 1234);
  } else {
	usleep(100000);
	start_gdb(gdb_port);
  }
}


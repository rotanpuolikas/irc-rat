#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_CLIENTS 20
#define BUFFER_SIZE 1024
#define UNAME_LEN 10

size_t client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;


typedef struct {
  int socket;
  char username[UNAME_LEN];
} client_t;

client_t clients[MAX_CLIENTS];

// lähetetään viesti kaikille (paitti lähettäjälle, se näkee sen jo)
void broadcastmsg(char *message, int sender_socket){
  pthread_mutex_lock(&clients_mutex);

  for(size_t i = 0; i < client_count; i++){
    if(clients[i].socket != sender_socket){
      message[strcspn(message, "\n")] = '\0';
      send(clients[i].socket, message, strlen(message), 0);
    }
  }

  pthread_mutex_unlock(&clients_mutex);
}

// checkataa kaikki usernamet ku joku connectaa
int uname_exists(const char *username){
  for(int i = 0; i < client_count; i++){
    if(strcmp(clients[i].username, username) == 0){
      return 1;
    }
  }
  return 0;
}

// kaikki clienttien in ja out data
void *handleclient(void *arg){
  client_t *cli = (client_t *)arg;
  char buffer[BUFFER_SIZE] = {'\0'};
  int bytes_read;

  // eka viesti on username, jos tyhjä nii ei oteta sissään
  bytes_read = recv(cli->socket, cli->username, UNAME_LEN - 1, 0);
  if(bytes_read <= 0){
    close(cli->socket);
    free(cli);
    pthread_exit(NULL);
  }
  cli->username[bytes_read] = '\0';

  cli->username[strcspn(cli->username, "\n")] = '\0';


// jos uname on varattu ei päästetä sissään
  pthread_mutex_lock(&clients_mutex);
  if(strlen(cli->username) == 0 || uname_exists(cli->username)){
    char msg[] = "Username invalid or taken.";
    send(cli->socket, msg, strlen(msg), 0);
    close(cli->socket);
    pthread_mutex_unlock(&clients_mutex);
    free(cli);
    pthread_exit(NULL);
  }
  else{
    char msg[] = "username ok";
    send(cli->socket, msg, strlen(msg), 0);
  }

  clients[client_count++] = *cli;
  pthread_mutex_unlock(&clients_mutex);

  snprintf(buffer, sizeof(buffer), "%s joined\n", cli->username);
  printf("%s", buffer);
  broadcastmsg(buffer, cli->socket);

  // chatloop
  while((bytes_read = recv(cli->socket, buffer, sizeof(buffer) -1 , 0)) > 0){
    buffer[bytes_read] = '\0';
    char formatted[BUFFER_SIZE + UNAME_LEN];
    snprintf(formatted, sizeof(formatted), "%s: %s\n", cli->username, buffer);
    printf("%s", formatted);
    broadcastmsg(formatted, cli->socket);
  }

  //remove client
  pthread_mutex_lock(&clients_mutex);
  for(size_t i = 0; i < client_count; i++){
    if(clients[i].socket == cli->socket){
      clients[i] = clients[client_count - 1];
      client_count--;
      break;
    }
  }
  pthread_mutex_unlock(&clients_mutex);

  snprintf(buffer, sizeof(buffer), "%s left the chat.\n", cli->username);
  printf("%s", buffer);
  broadcastmsg(buffer, -1);
  
  close(cli->socket);
  free(cli);
  pthread_exit(NULL);
}

int main(void){
  printf("Enter desired port > ");
  int port;
  if(scanf("%d", &port) > 65535){
    return 1;
  }
  
  int server_socket, client_socket;

  struct sockaddr_in server_addr, client_addr;
  socklen_t addr_len = sizeof(client_addr);
  pthread_t tid;

  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if(server_socket < 0){
    perror("socket creation failed");
    return 1;
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  int opt = 1;
  if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
    perror("setsockopt failed");
    return 1;
  }

  if(bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1){
    perror("bind failed\n");
    return 1;
  }

  if(listen(server_socket, 5) == -1){
    perror("listen failed");
    close(server_socket);
    return 1;
  }

  printf("listening on port %d\n", port);

  // normi operation loop
  while(1){
    client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
    if(client_socket == -1){
      perror("accept failed\n");
      continue;
    }

    client_t *cli = malloc(sizeof(client_socket));
    cli->socket = client_socket;

    pthread_t tid;
    pthread_create(&tid, NULL, handleclient, cli);
    pthread_detach(tid);

    }
  close(server_socket);

  printf("\n");
  return 0;
}

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

#define CHANNEL_LEN 10

size_t client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;


typedef struct {
  int socket;
  char username[UNAME_LEN];
  int channel;
  pthread_mutex_t lock;
} client_t;

client_t *clients[MAX_CLIENTS];

void commandparser(char *message, client_t *cli){
  char *command = message + 1;
  for(size_t i = 1; i < strlen(message); i++)
    command[i - 1] = message[i];
  command[strcspn(command, "\n")] = '\0';

  printf("%s sent command: %s\n", cli->username, command);
  fflush(stdout);
  
  // serverside komennot pittää kans tehä rumilla iffeillä
  if(strstr(command, "channel")){
    char *numpart = command + 7;

    while (*numpart == ' ') numpart ++;

    int channelnum = atoi(numpart);

    pthread_mutex_lock(&cli->lock);
    cli->channel = channelnum;
    pthread_mutex_unlock(&cli->lock);
    
    printf("%s changed channel to %d\n", cli->username, channelnum);
  }
}


// lähetetään viesti kaikille (paitti lähettäjälle, se näkee sen jo)
void broadcastmsg(char *message, client_t *cli){
  pthread_mutex_lock(&clients_mutex);

  size_t n = client_count;
  client_t *temp[MAX_CLIENTS];
  for(size_t i = 0; i < n; i++) temp[i] = clients[i];
  
  pthread_mutex_unlock(&clients_mutex);

  pthread_mutex_lock(&cli->lock);
  int sender_channel = cli->channel;
  pthread_mutex_unlock(&cli->lock);

  for(size_t i = 0; i < n; i++){
    client_t *target = temp[i];

    pthread_mutex_lock(&target->lock);
    int target_channel = target->channel;
    pthread_mutex_unlock(&target->lock);

    if(target->socket != cli->socket && target_channel == sender_channel){
      message[strcspn(message, "\n")] = '\0';
      send(target->socket, message, strlen(message), 0);
    }
  }

}

// checkataa kaikki usernamet ku joku connectaa
int uname_exists(const char *username){
  int found = 1;
  size_t n = client_count;
  client_t *temp[MAX_CLIENTS];
  for(size_t i = 0; i < n; i++) temp[i] = clients[i];

  //pthread_mutex_lock(&clients_mutex);
  for(int i = 0; i < n; i++){
    if(strcmp(temp[i]->username, username) == 0){
      found = 0;
      break;
    }
  }

  //pthread_mutex_unlock(&clients_mutex);
  return found;
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
  
  pthread_mutex_unlock(&clients_mutex);

  snprintf(buffer, sizeof(buffer), "%s joined\n", cli->username);
  printf("%s", buffer);
  broadcastmsg(buffer, cli);

  // chatloop
  while((bytes_read = recv(cli->socket, buffer, sizeof(buffer) -1 , 0)) > 0){
    buffer[bytes_read] = '\0';

  
    if(buffer[0] == '/'){ // backbone for commands that are sent to the server      
      commandparser(buffer, cli);
    }
    else{
      char formatted[BUFFER_SIZE + UNAME_LEN];
      snprintf(formatted, sizeof(formatted), "#%d @%s: %s\n",cli->channel, cli->username, buffer);
      printf("%s", formatted);
      broadcastmsg(formatted, cli);
    }
  }

  //remove client
  snprintf(buffer, sizeof(buffer), "%s left the chat.\n", cli->username);
  printf("%s", buffer);
  broadcastmsg(buffer, cli);
  
  close(cli->socket);

  pthread_mutex_lock(&clients_mutex);
  for(size_t i = 0; i < client_count; i++){
    if(clients[i] == cli){
      for(size_t j = i; j < client_count - 1; j++){
        clients[j] = clients[j + 1];
      }
      clients[client_count - 1] = NULL;
      client_count--;
      break;
    }
  }
  pthread_mutex_unlock(&clients_mutex);
  pthread_mutex_destroy(&cli->lock);
  
  free(cli);
  return NULL;
}

int main(void){
  printf("Enter desired port > ");
  int port;
  /*
  if(scanf("%d", &port) > 65535){
    return 1;
  }
  */
  port = 8080;

  printf("Port %d selected\n", port);
  
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
    if(client_socket < 0){
      perror("accept failed\n");
      continue;
    }

    client_t *cli = malloc(sizeof(client_t));
    if(!cli){
      close(addr_len);
      continue;
    }
    cli->socket = client_socket;
    cli->channel = 0;
    pthread_mutex_init(&cli->lock, NULL);

    pthread_mutex_lock(&clients_mutex);
    if(client_count >= MAX_CLIENTS){
      pthread_mutex_unlock(&clients_mutex);
      pthread_mutex_destroy(&cli->lock);
      free(cli);
      close(client_socket);
      continue;
    }

    clients[client_count++] = cli;
    pthread_mutex_unlock(&clients_mutex);
    
    pthread_t tid;
    if(pthread_create(&tid, NULL, handleclient, cli) != 0){
      pthread_mutex_lock(&clients_mutex);
      for(size_t i = 0; i < client_count; i++){
        if(clients[i] == cli){
          for(size_t j = i; j + 1 < client_count; j++){
            clients[j] = clients[j + 1];
          }
          clients[client_count - 1] = NULL;
          client_count--;
          break;
        }
      }
      pthread_mutex_unlock(&clients_mutex);
      pthread_mutex_destroy(&cli->lock);
      free(cli);
      continue;
    }
    pthread_detach(tid);

    }
  close(server_socket);

  printf("\n");
  return 0;
}

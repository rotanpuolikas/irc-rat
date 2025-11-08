#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define PORT 8080
#define MAX_RETRIES 5
#define UNAME_LEN 10
#define BUFFER_SIZE 1024

int sock;
char uname[UNAME_LEN] = "";
volatile int running = 1;

// ctrl+c händläys
void handle_sigint(int sig){
  running = 0;
  close(sock);
  printf("\nexiting\n");
  exit(0);
}

// pidetää prompti allaalla, uuet viestit tulee yläpuolelle ja jää historiaan?
void *recievemsg(void *arg){
  char buffer[BUFFER_SIZE];
  int bytes_read;
  while((bytes_read = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0){
    buffer[bytes_read] = '\0';
    printf("\r%s", buffer);
    printf("\n%s: ", uname);
    fflush(stdout);
  }
  printf("\nDisconnected from server\n");
  exit(0);
  return NULL;
}

int main(void){
  char ipchars[] = "1234567890.";
  struct sockaddr_in server_addr;

  pthread_t recv_thread;

  signal(SIGINT, handle_sigint);

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if(sock < 0){
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  printf("Enter ip address > ");
  char serveriptemp[64] = "";
  char serverip[64];
  if(fgets(serverip, sizeof(serverip), stdin) == NULL){
    perror("fgets failed");
    return 1;
  }

  serverip[strcspn(serverip, "\n")] = '\0';

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);
  // server_addr.sin_addr.s_addr = inet_addr(serverip); // vanha tapa, ei toiminu aina

  if(inet_pton(AF_INET, serverip, &server_addr.sin_addr) <= 0){
    perror("Invalid IP address");
    exit(EXIT_FAILURE);
  }

  if(server_addr.sin_addr.s_addr == INADDR_NONE){
    printf("Invalid IP address format\n");
    return 0;
  }
  
  printf("Connecting to %s\n", serverip);
  if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
    exit(EXIT_FAILURE);
  } // tää tykkää jäähä tähän lagaan jos ip ei ollu oikee

  // username input ja tarkistelu
  char usernamebuff[UNAME_LEN] = {0};
  printf("Server found\n");
  printf("Enter username > ");
  fgets(usernamebuff, UNAME_LEN, stdin);
  for(size_t i = 0; i < UNAME_LEN; i++){
    if(usernamebuff[i] != '\n'){
      uname[i] = usernamebuff[i];
    }
    else{
      uname[i] = '\0';
      break;
    }
  }
  if(strlen(uname) == 0){
    printf("Username cannot be empty\n");
    close(sock);
    return 0;
  }

  send(sock, uname, strlen(uname), 0);

  // checkataa serverin response, jos tyhjä tai nimi taken nii exittiä
  char response[BUFFER_SIZE] = {'\0'};
  int bytes = recv(sock, response, sizeof(response) - 1, 0);
  if(bytes <= 0){
    printf("No response from server, connection closed\n");
    close(sock);
    return 0;
  }
  
  response[bytes] = '\0';
  
  if(strstr(response, "taken") || strstr(response, "invalid")){
    printf("%s\n", response);
    close(sock);
  }

  // jos uname kävi servulle nii sisään vaan
  printf("Connected to %s as %s\n", serverip, uname);
  
  pthread_create(&recv_thread, NULL, recievemsg, NULL);
  pthread_detach(recv_thread);

  // normal operation loop
  while(running){
    char buffer[BUFFER_SIZE] = {'\0'};
    char* message;
    message = malloc(BUFFER_SIZE);
    printf("%s: ", uname);
    fflush(stdout);
    if(strlen(fgets(buffer, 1024, stdin)) > BUFFER_SIZE){
      printf("message too long");
      continue;
    };
    if(buffer[0] == '/'){ // on komento
      char *command = buffer + 1;
      size_t commandlen;
      commandlen = strlen(buffer);
      command[strcspn(command, "\n")] = '\0';
      // exit komento joka exittaa, usko tai älä
      if(strcmp(command, "exit") == 0){
        close(sock);
        printf("exiting\n");
        free(message);
        return 0;
      }
    }
    else{
      buffer[strcspn(buffer, "\n")] = '\0';
      if(strlen(buffer) == 0){
        continue;
      }
      if(send(sock, buffer, strlen(buffer), 0) <= 0){
        printf("\nmessage failed to send\n");
        continue;
      }
    }
  }
  close(sock);
  printf("\n");

  return 0;
}

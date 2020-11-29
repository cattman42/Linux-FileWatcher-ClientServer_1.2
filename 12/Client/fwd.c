#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include "csapp.h"

//Enabling functions
int becomeDaemon(char *home);
int readConfig(char *port, char *hostname, char ***location);
int count = 0;
int logOpen();
void logMessage(char *msg);
void logClose();

int run;
const char* pidFileLoc = "/run/fwd.pid";
const char* confFileLoc = "/etc/fwd.conf";
const char* logFileLoc = "/var/log/fwd.log";
FILE *logFile;

static volatile sig_atomic_t hupReceived = 0;

typedef struct s_thread_params {
  char location;
  char hostname;
  char port;
} thread_params;

void sighupHandler(int sig)
{
  hupReceived = 1;
}

void handleTERM(int x) {
  run = 0;
}

int becomeDaemon(char *home)
{
  int fd;
  pid_t pid;
  FILE* pidFile;

  if((pid = Fork()) != 0) { /* Become background process */
    exit(0);  /* Original parent terminates */
  }

  if(setsid() == -1) /* Become leader of new session */
    return -1;
  if((pid = Fork()) != 0) { /* Ensure we are not session leader */
	/** Prepare pid file and terminate **/
    pidFile = fopen(pidFileLoc,"w");
    fprintf(pidFile,"%d",pid);
    fclose(pidFile);
    exit(0);
  }
  chdir(home);
  Close(STDIN_FILENO);
  fd = Open("/dev/null", O_RDWR, 0);

  if (fd != STDIN_FILENO)
    return -1;
  if (Dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
    return -1;
  if (Dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
    return -1;

  return 0;
}

int readConfig(char *port,char *hostname, char ***location) {
  FILE *config;
  config = fopen(confFileLoc,"r");
  if(config == NULL)
    return -1;
  /// Get line count (how many folders we will have)
  char c;

  for(c = getc(config); c != EOF; c = getc(config)){
    if (c == '\n')
      count = count + 1;
  }
  fclose(config);
  *location = (char**) Malloc((count-3)*sizeof(char*));
  for(int n = 0;n<count-3;n++)
    (*location)[n] = (char*) Malloc(MAXLINE);

  config = fopen(confFileLoc,"r");
  //Grab everything in the file, except the last 3 things (newline, host, port)
  for(int i = 0; i< count-3; i++){
    fscanf(config, "%s", (*location)[i]);
  }
  //New line character is discarded
  fscanf(config,"%s",hostname);
  fscanf(config,"%s",port);
  fclose(config);
  return 0;
}

int logOpen() {
  logFile = fopen(logFileLoc,"a");
  if(logFile == NULL)
    return -1;
  return 0;
}

sem_t *mutex;
void logMessage(char *msg) {
  
  P(&mutex);
  if(logFile != NULL) {
   fprintf(logFile,"%s\n",msg);
   fflush(logFile);
  }
  /** Missing: V(&mutex); **/
}

void logClose() {
  if(logFile != NULL)
    fclose(logFile);
}

int clientfd;
rio_t rio;
char buff[MAXLINE];

void status(char* host,char* port,char* fileNameRaw, char* folder){
  clientfd = Open_clientfd(host, port);
  Rio_readinitb(&rio, clientfd);
  char *cmd;
  cmd = "status\n";
  Rio_writen(clientfd, cmd, strlen(cmd));
  /** You need to send the server the remote folder name along with the file name. **/
  char filename[50];
  sprintf(filename, "%s\n",fileNameRaw);
  Rio_writen(clientfd, filename, strlen(filename));
  Rio_readlineb(&rio, buff, MAXLINE);
  Close(clientfd);
}

void upload(char* host,char* port,char* location,char* fileNameRaw, off_t size){
  char path[256];
  clientfd = Open_clientfd(host, port);
  Rio_readinitb(&rio, clientfd);
  char *cmd, *srcp;
  int srcfd;
  struct stat s;
  cmd = "upload\n";
  char filename[50];
  sprintf(filename, "%s\n",fileNameRaw);
  Rio_writen(clientfd, cmd, strlen(cmd));
  /** You need to send the server the remote folder name along with the file name. **/
  Rio_writen(clientfd, filename, strlen(filename));
  char buffer[32];
  sprintf(buffer,"%ld\n",s.st_size);
  Rio_writen(clientfd, buffer, strlen(buffer));
  // Send file contents below
  strcpy(path,location); strcat(path,"/"); strcat(path,fileNameRaw);
  srcfd = Open(path, O_RDONLY, 0);
  srcp = Mmap(0, s.st_size, PROT_READ, MAP_PRIVATE, srcfd, 0);
  Close(srcfd);
  Rio_writen(clientfd, srcp, s.st_size);
  Munmap(srcp, s.st_size);
  Rio_readlineb(&rio, buff, MAXLINE);
  Fputs(buff, stdout);
  logMessage(buff);
  Close(clientfd);
}

void *thread(int i, void *vargp){
  time_t timer;
  DIR* DIR;
  run = 1;
  int firstScan = 1;
  struct stat s;
  struct dirent* entp;
  char path[1024];
  thread_params input = *((thread_params*)(vargp));
  char* location = input.location;
  char* hostname = input.hostname;
  char* port = input.port;
  while(run == 1) {
    if(hupReceived != 0) {
      /** Missing: P(&mutex); **/
      logClose();
      logOpen();
      /** Missing: V(&mutex); **/
      hupReceived = 0;
    }
    DIR = opendir(location);
    while((entp = readdir(DIR))!=NULL) {
      /** You did not read the problem statement carefully enough.
          The problem statement says that each line in the configuration file
          has two parts: a path to a folder to watch, and a name to use for the
          remote folder on the server.

          You need to add code here that splits the line into the folder path and
          the remote name. When you send the request, you need to send the remote
          folder name along with the file name. **/
      strcpy(path,location); /** Location should be folderPath. **/
      strcat(path,"/");
      strcat(path,entp->d_name);
      stat(path,&s);
      char filetype[MAXLINE];
      /* Only pay attention to files, and ignore directories. */
      if(S_ISREG(s.st_mode)) {
        if(firstScan == 1) {
          status(hostname,port,entp->d_name, ); /** Send folderName as a parameter, too. **/
          /** You are having the status function return its result to you in a global
          variable. This is the wrong thing to do with thread code. You could easily
          get into a situation where two threads are calling status at the same time and
          those two calls to status both try to write to buff at the same time. The
          correct way to do this is to set up the buff array as a local variable in this
          function and then pass a pointer to that array as a parameter to status. **/
          char *ptr;
          long time;
          time_t server_time;
          time = strtol(buff, &ptr, 10);
          server_time = (time_t)time;
          time_t client_time = s.st_mtime;
          if(strcmp(buff, "0\n") == 0 || difftime(client_time,server_time) > 0) {
            upload(hostname,port,location,entp->d_name, s.st_size); /** Pass remote folder name as a parameter as well. **/
          }
          firstScan = 0;
          }
        else {
          if(difftime(s.st_mtime,timer) > 0) {
            upload(hostname,port,location,entp->d_name, s.st_size); /** Pass remote folder name as a parameter as well. **/
          }
        }
      }
    }
    logClose(); /** You should not call this here. **/
    closedir(DIR);
    time(&timer);
    sleep(300);
  }
  exit(0); /** Replace with return NULL; **/
}

int main(int argc, char **argv)
{
  struct sigaction action, old_action;
  FILE *logFile;
  char hostname[MAXLINE], port[MAXLINE];
  char locationArray[count-3]; /** Should be char* **location; **/
  char myHome[MAXLINE];
  struct sigaction sa;
  sem_init(mutex, 0, 1); /** This belongs in main. **/

  /* Install the handler for SIGTERM */
  action.sa_handler = handleTERM;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;
  sigaction(SIGTERM, &action, &old_action);

  /** Install SIGHUP handler **/
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sa.sa_handler = sighupHandler;
  if (sigaction(SIGHUP, &sa, NULL) == -1) {
    fprintf(stderr, "Failed to install SIGHUP handler\n");
    exit(1);
  }
  if(readConfig(port, hostname, locationArray) != 0) {
    fprintf(stderr, "Failed to read config file\n");
    exit(1);
  }
  if(logOpen() != 0) {
    fprintf(stderr, "Could not open log file\n");
    exit(1);
  }
  if(becomeDaemon(myHome) != 0) {
    fprintf(stderr, "Failed becomeDaemon\n");
    exit(1);
  }
  for(int i = 0; i < count-3; i++){ /** <= should be < here. **/
    thread_params input;
    strcpy(input.location, locationArray[i]); /** Should be (*location)[i] **/
    strcpy(input.hostname, hostname);
    strcpy(input.port, port);
    pthread_t tid;
    Pthread_create(&tid, NULL, thread, &input);
    /** Your strategy for passing data to the thread suffers from a race condition.
    There will be race between the thread code, which tries to read data from
    the input structure, and this loop, which wants to overwrite the data in
    the input structure. The correct approach here is to make input be a pointer
    to a thread_params struct. You allocate a separate structure with malloc
    for each of the threads, and copy that thread's data into its own separate
    structure. You then pass the pointer to that structure to the thread. **/
  }

  while(run == 1) {
    sleep(300);
    if(hupReceived != 0) {
      logClose();
      logOpen();
      hupReceived = 0;
    }
  }
  logCLose();
  sleep(300);
  /** This code suffers from the premature exit problem. See the chapter 12
  assignment FAQ for a discussion of this problem. **/
}

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
int readConfig(char *port,char *home);
int logOpen();
void logMessage(char *msg);
void logClose();

const char* pidFileLoc = "/run/bu.pid";
const char* confFileLoc = "/etc/bu.conf";
const char* logFileLoc = "/var/log/bu.log";
FILE *logFile;
char location[MAXLINE];

int readConfig(char *port,char *location) {
  FILE *config;
  config = fopen(confFileLoc,"r");
  if(config == NULL)
    return -1;
  fscanf(config,"%s",port);
  fscanf(config,"%s",location);
  fclose(config);
  return 0;
}

int logOpen() {
  logFile = fopen(logFileLoc,"a");
  if(logFile == NULL)
    return -1;
  return 0;
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

  chdir(home); /* Ch00:13ange to home directory */

  Close(STDIN_FILENO); /* Reopen standard fd's to /dev/null */

  fd = Open("/dev/null", O_RDWR, 0);

  if (fd != STDIN_FILENO)         /* 'fd' should be 0 */
    return -1;
  if (Dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
    return -1;
  if (Dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
    return -1;
  return 0;
}

void logClose() {
  if(logFile != NULL)
    fclose(logFile);
}

void logMessage(char *msg) {
  sem_t *mutex;
  sem_init(mutex, 0, 1);
  P(&mutex);
  if(logFile != NULL) {
   fprintf(logFile,"%s\n",msg);
   fflush(logFile);
  }
}

/* Global SIGHUP received flag */
static volatile sig_atomic_t hupReceived = 0;

void sighupHandler(int sig)
{
  hupReceived = 1;
}

void client_comm(char *location,int connfd)
{
  int filecheck = 0;
  char buf[MAXLINE], fileName[100], fileSize[100], mod_time[100];
  char *out;
  rio_t rio;
  size_t n;
  DIR* DIR;
  struct dirent* entp;
  struct stat s;
  char path[256];
  char dirFile[1024];
  DIR = opendir(path);

  Rio_readinitb(&rio, connfd);
  Rio_readlineb(&rio, buf, MAXLINE);
  if(strcmp(buf, "status\n") == 0) {
    /** The client will send you a folder name first, telling you which subfolder
        of /var/bu they want you to work with. The first thing you should do is
        to check to see if the folder exists. If not, you need to create it with
        mkdir(). **/
    n = Rio_readlineb(&rio, fileName, MAXLINE);
    strtok(fileName,"\n");
    strcpy(path,location);
    strcat(path,"/");
    strcat(path,fileName);
    if(stat(path,&s) != 0)
      filecheck = 0;
    else {
      filecheck = 1;
      sprintf(mod_time, "%ld\n",s.st_mtime);
    }
    if(filecheck == 0) {
      Rio_writen(connfd, "0\n", n);
      logMessage("File not found");
    }
    else {
      Rio_writen(connfd, mod_time, n);
      logMessage("File Found");
    }
    Close(connfd);
  }
  else if (strcmp(buf, "upload\n") == 0) {
    /** Read the subfolder name first. **/
    Rio_readlineb(&rio, fileName, MAXLINE);
    Rio_readlineb(&rio, fileSize, MAXLINE);
    size_t size = (size_t)atoi(fileSize);
    char* pointer = malloc(size);
    Rio_readnb(&rio, pointer, size);
    strtok(fileName, "\n");
    strcpy(path,location);
    strcat(path,"/");
    strcat(path,fileName);
    int srcfd = Open(path, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU|S_IRWXG|S_IRWXO);
    Rio_writen(srcfd, pointer, size);
    out = "stored\n";
    Rio_writen(connfd, out, strlen(out));
    logMessage(out);
    free(pointer);
    Close(connfd);
  }
}

void *thread(int i, void *vargp) {
  int connfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  client_comm(connfd,location);
  return NULL;
}

int main(int argc, char **argv)
{
  char port[MAXLINE] = "8000";
  int listenfd, *connfd;
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  char client_hostname[MAXLINE], client_port[MAXLINE];
  struct sigaction sa;
  FILE* logFile;

    /** Install SIGHUP handler **/
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sighupHandler;

  //ERROR HANDLING
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(0);
  }
	if (sigaction(SIGHUP, &sa, NULL) == -1) {
    fprintf(stderr, "ERROR: failure to install SIGHUP handler\n");
    exit(1);
  }

  if(readConfig(port,location) != 0) {
    fprintf(stderr, "ERROR: failure to read config file\n");
    exit(1);
  }

  if(logOpen() != 0) {
    fprintf(stderr, "ERROR: could not open log file\n");
    exit(1);
  }

	if(becomeDaemon(location) != 0) {
    fprintf(stderr, "ERROR: becomeDaemon has failed\n");
	  exit(1);
  }
  listenfd = Open_listenfd(port);
  while (1) {
    if(hupReceived != 0) {
      logClose();
      logOpen();
      hupReceived = 0;
    }
    clientlen = sizeof(struct sockaddr_storage);
    connfd = Malloc(sizeof(int));
    *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
    printf("Connected to (%s, %s)\n", client_hostname, client_port);
    pthread_t tid;
    Pthread_create(&tid, NULL, thread, connfd);
  }
  logClose();
  exit(0);
}

/** A few problems here, more problems in fwd.c. See my comments for details.
    Your grade for this assignment is 22/30. **/

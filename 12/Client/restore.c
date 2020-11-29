#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include "csapp.h"
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>

//Function declarations
int readConfig(char *hostname,char *port, char* *path, char* *name);
int checkNumPaths();
void getFileList(char dirName);
void getFileReq(char dirName, char *fileName, char path);

int run;
const char* confFileLoc = "/etc/fwd.conf";
FILE *logFile;
int pathNums, numPath;
char path[256], name[256], hostname[MAXLINE], port[MAXLINE], logBuffer[MAXLINE], filelocation[MAXLINE];
struct stat s;
struct mq_attr {
  long mq_flags;    /* Flags: 0 or O_NONBLOCK */
  long mq_maxmsg;   /* Max. # of messages on queue */
  long mq_msgsize;  /* Max. message size (bytes) */
  long mq_curmsgs;  /* # of messages currently in queue */
};

int readConfig(char *port,char *hostname, char* *path, char* *name) {
  FILE *config;
  config = fopen(confFileLoc,"r");
  int numPath = checkNumPaths();
  if(config == NULL)
    return -1;
  for(int i = 0; i < numPath; i++){ 
    fscanf(config,"%s",path[i]);
    fscanf(config,"%s",name[i]);
  }
  fscanf(config, "%s", hostname);
  fscanf(config, "%s", port);
  fclose(config);
  return 0;
}

int checkNumPaths(){
  FILE *config;
  int numPath = 0; 
  char *str[MAXLINE];
  config = fopen(confFileLoc,"r");
  while(strcmp(fgets(str, MAXLINE, config),"\n")!=0){
    numPath = numPath+1;
  }
  fclose(config);
  return numPath;
}

void getFileList(char dirName){
    rio_t rio;
    int restoreFd;
    int numFiles = 0;
    restoreFd = Open_clientfd(hostname, port);
    Rio_readinitb(&rio, restoreFd);
    char list = "list\n";
    Rio_writen(restoreFd,list, strlen(list));
    strcat(dirName,"\n");
    Rio_writen(restoreFd,dirName, strlen(dirName));
    Rio_readlineb(&rio, numFiles, MAXLINE);
    char curFile[MAXLINE];
    char path[MAXLINE];
    int pathElement;
    for(int i=0; i<name; i++){
        if(strcmp(name[i],dirName)){
            pathElement = i;
        }
    }
    char path = path[pathElement];
    for(int i = 0; i <= numFiles; i++){
        Rio_readlineb(&rio, curFile, MAXLINE);
        char userInput;
        printf("Are you sure? 'yes' or 'no': ");
        userInput = getchar();
        if(strcmp(userInput,"yes")){
            getFileReq(dirName, curFile, path);
        }
    }
} 

void getFileReq(char dirName, char *fileName, char path){
    rio_t rio;
    mqd_t queueName = "/messageQueue";
    const char *msg  = dirName;
    const char *doneCheck  = "done";
    size_t len = strlen(dirName);
    size_t doneLen = strlen(doneCheck);
    unsigned int priority = 1;
    size_t buf_size = 256;
    mqd_t mq_open(queueName, O_CREAT|O_RDONLY| O_WRONLY| O_RDWR, S_IRWXG|S_IRWXO, mq_attr);
    int mq_send(queueName, msg,len,priority);
    int restoreFd;
    char *size;
    size_t filesize;
    restoreFd = Open_clientfd(hostname, port);
    Rio_readinitb(&rio, restoreFd);
    char download = "download\n";
    Rio_writen(restoreFd, download, strlen(download));
    Rio_writen(restoreFd, dirName, strlen(dirName));
    Rio_writen(restoreFd, *fileName, strlen(*fileName));
    Rio_readlineb(&rio, size, MAXLINE);
    if(size != 0){
    filesize = (size_t)atoi(size);
    char* file;
    file  = (char*)malloc(filesize);
    Rio_readnb(&rio, file, filesize);
    strcat(path,"/");
    strcat(path,*fileName);
    int srcfd = Open(path,O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU|S_IRWXG|S_IRWXO);
    Rio_writen(srcfd,file,filesize);
    free(file);
    int mq_send(queueName, doneCheck,len,priority);
    int mq_close(queueName);
    }
}

int main(int argc, char **argv)
{
    /** Install SIGHUP handler **/
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sighupHandler;
	if (sigaction(SIGHUP, &sa, NULL) == -1) {
      fprintf(stderr, "Failed to install SIGHUP handler\n");
      exit(1);
    } 

  /* Read configuration information */
    if(readConfig(*hostname, *port, *path, *name) != 0) {
        fprintf(stderr, "Failed to read config file\n");
        exit(1);
    }
    int i =0;
    while(name[i]!=NULL){
      printf(name[i]);
      i++;
    }
    char dirName = "null";
    printf("What is the directory you want to restore?:  ");
    dirName = getchar();
    getFileList(dirName);
    printf("DONE");
    exit(0);
}

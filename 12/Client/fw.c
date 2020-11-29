#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

int main(int argc, char **argv)
{
  FILE* pidFile;
  pid_t pid;
  char* argV[3];

  if(argc == 2 && strcmp(argv[1],"stop") == 0) {
    pidFile = fopen("pid","r");
    if(pidFile == NULL)
      printf("stop called when fwd is not running.\n");
    else {
      fscanf(pidFile,"%ud",&pid);
      kill(pid,SIGTERM);
      printf("Stop signal sent to fwd.\n");
      fclose(pidFile);
      remove("pid");
    }
  } else if(argc == 3 && strcmp(argv[1],"start") == 0) {
    if((pid = fork())==0) {
      argV[0] = "fwd";
      argV[1] = argv[2];
      argV[2] = NULL;
      execve("./fwd",argV,NULL);
    }
    printf("fwd started with pid %d.\n",pid);
    pidFile = fopen("pid","w");
    fprintf(pidFile,"%ud",pid);
    fclose(pidFile);
  } else {
    printf("Usage: fw [start <path> | stop]\n");
  }
}

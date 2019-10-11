/*
* shelly interface program

KUSIS ID: PARTNER NAME:
KUSIS ID: PARTNER NAME:

*/

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>


#define MAX_LINE       80 /* 80 chars per line, per command, should be enough. */
#define MAX_ENRIES     100 /* 100 entries for the bookmark keys and pairs */


int parseCommand(char inputBuffer[], char *args[],int *background);
int processBookmark(char *args[]);
int parseFile (char *bkeys[],char *bvalues[]);
int lookUp(char arg0[], char *bkeys[], int num);
int stdinOrNot = 1;
int argLength = 0;

int main(void)
{
  char inputBuffer[MAX_LINE]; 	        /* buffer to hold the command entered */
  int background;             	        /* equals 1 if a command is followed by '&' */
  char *args[MAX_LINE/2 + 1];	        /* command line (of 80) has max of 40 arguments */
  char *args2[MAX_LINE/2 + 1];
  pid_t child;            		/* process id of the child process */
  int status;           		/* result from execv system call*/
  int shouldrun = 1;
  int fileRead = 0;
  int i, upper;

  //variables for bookmark section
  int indexBookmark = -1; /*index number of the bookmark entry*/
  int entryCount = 0; /*bookmark entry number for debugging*/
  char *bvalues[MAX_ENRIES + 1]; /*to store the command entries for bookmarks*/
  char *bkeys[MAX_ENRIES + 1]; /*to store the key entries for bookmarks*/
  int bookmarkNum = 0; /*to store the number of bookmark entries*/


  while (shouldrun){            		/* Program terminates normally inside setup */
    background = 0;

    shouldrun = parseCommand(inputBuffer,args,&background);

    if (strncmp(inputBuffer, "exit", 4) == 0 || strncmp(inputBuffer, "exit", 4) == 0)
      shouldrun = 0;     /* Exiting from shelly*/

    if (shouldrun) {
      /*
      After reading user input, the steps are
      (1) Fork a child process using fork()
      (2) the child process will invoke execv()
      (3) if command included &, parent will invoke wait()
      */

      /*if bookmark command is called*/
      bookmarkNum = parseFile(bkeys, bvalues);
      indexBookmark = lookUp(args[0], bkeys, bookmarkNum);
      if (strcmp(args[0], "bookmark") == 0) {
        //lookup should return the index of the command string
        if (indexBookmark == -1) processBookmark(args); //this will record the new entry to the mybookmarks
        else {
          printf("Bookmark %s already exits.\n", args[0]);
          continue;
        }
      }
      else {

        if ( indexBookmark != -1 ) { /* if the key exists in the .mybookmarks*/
          stdinOrNot = 0; //this is to block reading from stdin during parseCommand
          strcpy(inputBuffer, bvalues[indexBookmark]); /*new command to parse comes from .mybookmarks*/
          inputBuffer[strlen(inputBuffer)] = '\n'; /*indicate the end of command*/
          continue; /*returns back to while loop*/
        }

        //if .mybookmarks does not contain the key, should be a syscal
        child = fork();

        if (child < 0) {
          fprintf(stderr, "Fork failed.");
          return 1;
        }
        else if (child == 0) { /* child process */
          execvp(args[0], args);
          exit(0);
        }
        else { /* parent process */
          if (!background) waitpid(child, NULL, 0); /* waits until the child terminates. */
        }
      }
    }
  }
  return 0;
}

/**
* This is to enter the new key and command pair
* to .mybookmarks.
*/
int processBookmark(char *args[])
{
  //check the length should be at least 3 <bookmark key command>
  if (argLength < 3) {
    printf("Argument number must be at least 3 for bookmarking: %d", argLength);
    return 0;
  }

  //concatenate the arguments aside from bookmark and key
  //to build the command
  int k;
  char command[MAX_LINE];
  strcpy(command, args[2]);
  for (k = 3; k < argLength; k++) {
    strcat(command, " ");
    strcat(command, args[k]);
  }

  //check for the first and last char to be "
  char l1 = command[0]; //first char of the command expected "
  char l2 = command[strlen(command)-1]; //last char of the command expected "

  if (l1 != '"' || l2 != '"' || strlen(command) == 1){
    printf("Incorrect command format for bookmark use double quotes.");
    return 0;
  }

  //then eliminate those "s to record the command
  char subArg[strlen(command)-1]; //eliminate "
  memcpy( subArg, &command[1], strlen(command)-2);
  subArg[strlen(command)-2] = '\0';

  //once ready open the .mybookmarks and record the new entry
  FILE *file = fopen(".mybookmarks", "at");
  // if (!file) file = fopen(".mybookmarks", "wt");

  if (!file)
  {
      fprintf(stderr, "Failed to open .mybookmarks\n");
      exit(1);
  }
  char str[100];
  strcpy(str, args[1]);
  strcat(str, "\n");
  strcat(str, subArg);
  strcat(str, "\n");
  fprintf(file, str);
  fclose(file);

  return 1;
}

//lookup should return the index of the key in bkeys
//if not found return -1.
int lookUp(char arg0[], char *bkeys[], int bookmarkNum)
{
  int k;
  for (k = 0; k < bookmarkNum; k++) {
    if (strcmp(bkeys[k], arg0) == 0){
      return k;
    }
  }
  return -1;
}

/** This parses the file contents for the recorded bookmarks
* and fills in bkeys and bvalues. Returns the number of entries <counter>*/
int parseFile (char *bkeys[], char *bvalues[])
{
  enum { MAXLINES = 30};

  int i = 0;
  int value = 0; /* This is a switch to differentiate between consecutive keys and values in .mybookmarks*/
  int counter = 0;
  char lines[MAXLINES][BUFSIZ];
  FILE *fp = fopen(".mybookmarks", "r");
  if (!fp) fp = fopen(".mybookmarks", "a+"); /* create if doesn't exist*/
  if (fp == 0)
  {
      fprintf(stderr, "failed to open .mybookmarks\n");
      exit(1);
  }
  while (i < MAXLINES && fgets(lines[i], sizeof(lines[0]), fp))
  {
      lines[i][strlen(lines[i])-1] = '\0';
      if (value == 1){
        bvalues[counter] = &lines[i][0];
        value = 0;
        counter ++; // increment the counter if value
      } else {
        bkeys[counter] = &lines[i][0];
        value = 1;
      }
      i = i + 1;
  }
  fclose(fp);
  return counter;
}

/**
* The parseCommand function below will not return any value, but it will just: read
* in the next command line; separate it into distinct arguments (using blanks as
* delimiters), and set the args array entries to point to the beginning of what
* will become null-terminated, C-style strings.
*/

int parseCommand(char inputBuffer[], char *args[],int *background)
{
  int length,		/* # of characters in the command line */
  i,		/* loop index for accessing inputBuffer array */
  start,		/* index where beginning of next command parameter is */
  ct,	        /* index of where to place the next parameter into args[] */
  command_number;	/* index of requested command number */

  ct = 0;

  /* read what the user enters on the command line */
  do {
    if (stdinOrNot){ /* if the command is expected from the user.*/
      printf("shelly>");
      fflush(stdout);
      length = read(STDIN_FILENO,inputBuffer,MAX_LINE);
    } else { /* if the command is taken from .mybookmarks*/
      length = strlen(inputBuffer);
      stdinOrNot = 1;
    }
  }
  while (inputBuffer[0] == '\n'); /* swallow newline characters */

  /**
  *  0 is the system predefined file descriptor for stdin (standard input),
  *  which is the user's screen in this case. inputBuffer by itself is the
  *  same as &inputBuffer[0], i.e. the starting address of where to store
  *  the command that is read, and length holds the number of characters
  *  read in. inputBuffer is not a null terminated C-string.
  */
  start = -1;
  if (length == 0)
    exit(0);            /* ^d was entered, end of user command stream */

  /**
  * the <control><d> signal interrupted the read system call
  * if the process is in the read() system call, read returns -1
  * However, if this occurs, errno is set to EINTR. We can check this  value
  * and disregard the -1 value
  */

  if ( (length < 0) && (errno != EINTR) ) {
    perror("error reading the command");
    exit(-1);           /* terminate with error code of -1 */
  }

  /**
  * Parse the contents of inputBuffer
  */

  for (i=0;i<length;i++) {
    /* examine every character in the inputBuffer */

    switch (inputBuffer[i]){
      case ' ':
      case '\t':               /* argument separators */
      if(start != -1){
        args[ct] = &inputBuffer[start];    /* set up pointer */
        ct++;
      }
      inputBuffer[i] = '\0'; /* add a null char; make a C string */
      start = -1;
      break;

      case '\n':                 /* should be the final char examined */
      if (start != -1){
        args[ct] = &inputBuffer[start];
        ct++;
      }
      inputBuffer[i] = '\0';
      args[ct] = NULL; /* no more arguments to this command */
      break;

      default :             /* some other character */
      if (start == -1)
        start = i;
      if (inputBuffer[i] == '&') {
        *background  = 1;
        inputBuffer[i-1] = '\0';
      }
    } /* end of switch */
  } /* end of for */

  /**
  * If we get &, don't enter it in the args array
  */

  if (*background)
    args[--ct] = NULL;

  args[ct] = NULL; /* just in case the input line was > 80 */
  argLength = ct; /* to use in processing bookmark */
  return 1;

} /* end of parseCommand routine */

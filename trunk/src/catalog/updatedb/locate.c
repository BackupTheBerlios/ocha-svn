#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <glib.h>
#include "locate.h"

/** \file
 * Implemetation of API defined in locate.h
 */

#define BUFFER_LEN 1024
struct full_locate
{
   struct locate locate;
   guchar *cur;
   guchar *end;
   bool eof;
   guchar buffer[BUFFER_LEN];
};

struct locate *locate_new(const char *query)
{
   g_return_val_if_fail(query!=NULL, NULL);
   const char *command[] = { "locate", query, NULL };
   return locate_new_cmd(command);
};

struct locate *locate_new_cmd(const char *command[])
{
   g_return_val_if_fail(command!=NULL, NULL);
   int pipes[2];

   if(pipe(pipes)!=0) {
      perror("error: locate_new failed to open pipe");
      return NULL;
   }

   pid_t childpid = fork();
   if(childpid==-1) {
      perror("error: locate_new fork() failed");
      close(pipes[0]);
      close(pipes[1]);
      return NULL;
   }

   if(childpid==0) {
      /* child */
      close(pipes[0]); /* close read pipe */
      dup2(pipes[1], 1);
      close(pipes[1]);
      execvp("locate",
             (char**)command);
      perror("error: locate_new exec() failed");
      exit(99);
      return NULL;
   } else {
      /* parent */
      close(pipes[1]); /* close write pipe */
      struct full_locate *retval = (struct full_locate *)g_malloc(sizeof(struct full_locate));
      retval->locate.fd=pipes[0];
      retval->locate.child_pid=childpid;
      retval->cur=retval->buffer;
      retval->end=retval->cur;
      retval->eof=false;
      return &retval->locate;
   }
}

void locate_delete(struct locate *locate)
{
   g_return_if_fail(locate!=NULL);
   close(locate->fd);
   waitpid(locate->child_pid, NULL, 0);
   g_free(locate);
}

bool locate_has_more(struct locate *_locate)
{
   g_return_val_if_fail(_locate!=NULL, true);
   struct full_locate *locate = (struct full_locate *)_locate;
   if(locate->eof)
      return true;
   if(locate->cur<locate->end)
      return true;
   return false;
}

static void locate_skip_empty_lines(struct full_locate *locate)
{
   guchar *cur = locate->cur;
   const guchar *end = locate->end;
   /* skip empty lines */
   for(;cur<end && *cur=='\n'; cur++);
   locate->cur=cur;
}

bool locate_next(struct locate *_locate, GString* dest)
{
   g_return_val_if_fail(_locate!=NULL, false);
   struct full_locate *locate = (struct full_locate *)_locate;

   bool eof = false;

   locate_skip_empty_lines(locate);

   do {
      guchar *cur = locate->cur;
      const guchar *end = locate->end;
      bool found_newline=false;

      /* skip to next newline or end of data */
      for(;cur<end; cur++) {
         if(*cur=='\n') {
            found_newline=true;
            break;
         }
      }
      if(cur!=locate->cur) {
         if(dest)
            g_string_append_len(dest, locate->cur, cur-locate->cur);
         if(found_newline || eof) {
            locate->cur=cur;
            return true;
         }
      }

      ssize_t r = read(locate->locate.fd,
                       locate->buffer,
                       BUFFER_LEN);
      if(r==-1) {
         perror("error: locate_next reading pipe failed, giving up");
         return false;
      } else if(r==0) {
         eof=true;
      } else {
         locate->cur=locate->buffer;
         locate->end=locate->buffer+r;
      }

   }while(!eof);
   locate->eof=true;
   return false;
}

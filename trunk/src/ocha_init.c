#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ocha_init.h"
#include "restart.h"
#include <glib.h>
#include <libgnome/libgnome.h>
#include <libgnome/gnome-init.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnome/gnome-program.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


/** \file Generic initialization */
#ifndef PACKAGE
#define PACKAGE "ocha"
#endif
#ifndef VERSION
#define VERSION "0.0"
#endif

#define SOCKET_PATH "/.ocha/ochad.sock"

#define HELLO "ook?"
#define HELLO_LEN (strlen(HELLO)+1)

#define KILL "kill"
#define KILL_LEN (strlen(KILL)+1)

gchar *ocha_init_indexer_argv[] = {
        BINDIR "/ocha",
        "--nice",
        "index",
        NULL
};
gint ocha_init_indexer_argc = 3;

gchar *ocha_init_install_argv[] = {
        BINDIR "/ocha",
        "install",
        NULL
};
gint ocha_init_install_argc = 2;

gchar *ocha_init_daemon_argv[] = {
        BINDIR "/ochad",
        NULL
};
gint ocha_init_daemon_argc = 1;

gchar *ocha_init_preferences_argv[] = {
        BINDIR "/ocha",
        "preferences",
        NULL
};
gint ocha_init_preferences_argc = 2;

/* ------------------------- prototypes */
static char *get_catalog_path(void);
static struct sockaddr_un *create_socket_address(size_t *len_ptr);
static gboolean channel_new_connection(GIOChannel *source, GIOCondition cond, gpointer userdata);
static gboolean channel_client_data(GIOChannel *source, GIOCondition cond, gpointer userdata);
static gboolean channel_hangup(GIOChannel *source, GIOCondition cond, gpointer userdata);
static int client_connect(void);
static void kill_ocha(void);

/* ------------------------- public functions */

void ocha_init(const char *program, int argc, char **argv, gboolean ui, struct configuration *config)
{
        g_thread_init(NULL/*vtable*/);
        gnome_program_init (program,
                            VERSION,
                            ui ? LIBGNOMEUI_MODULE : LIBGNOME_MODULE,
                            argc,
                            argv,
                            NULL);
        config->catalog_path=get_catalog_path();
        config->catalog_is_new=!g_file_test(config->catalog_path, G_FILE_TEST_EXISTS);
}
void ocha_init_requires_catalog(const char *catalog_path)
{
        if(!g_file_test(catalog_path, G_FILE_TEST_EXISTS)) {
                int status = 0;
                pid_t pid;

                pid = gnome_execute_async(NULL/*current dir*/,
                                          ocha_init_indexer_argc,
                                          ocha_init_indexer_argv);
                if(pid==-1) {
                        fprintf(stderr,
                                "ocha:error: indexing failed: could not execute command %s\n",
                                ocha_init_indexer_argv[0]);
                        exit(19);
                }
                waitpid(pid, &status, 0/*options*/);
                if(!(WIFEXITED(status) && WEXITSTATUS(status)==0)) {
                        fprintf(stderr, "First-time indexing failed; can't go on.\n");
                        exit(10);
                }
        }
}

gboolean ocha_init_create_socket(void)
{
        gboolean retval = FALSE;
        struct sockaddr_un *addr;
        size_t addr_len;
        gint s;
        GIOChannel *channel;

        s = socket(AF_UNIX, SOCK_STREAM, 0);
        if(s==-1) {
                fprintf(stderr, "ocha:error: failed to create unix socket: %s\n", strerror(errno));
                return FALSE;
        }

        addr = create_socket_address(&addr_len);
        unlink(addr->sun_path);
        if(bind(s, (struct sockaddr *)addr, addr_len)==-1) {
                fprintf(stderr,
                        "ocha:error: failed to bind unix socket '%s': %s\n",
                        addr->sun_path,
                        strerror(errno));
        } else {
                if(listen(s, 5)==-1) {
                        fprintf(stderr,
                                "ocha:error: can't listen to unix socket '%s': %s\n",
                                addr->sun_path,
                                strerror(errno));
                } else {
                        retval = TRUE;
                }
        }
        g_free(addr);
        if(!retval) {
                return FALSE;
        }

        channel = g_io_channel_unix_new(s);
        g_io_add_watch(channel, G_IO_IN, channel_new_connection, NULL/*userdata*/);


        return retval;
}

gboolean ocha_init_kill()
{
        int s;

        s=client_connect();
        if(s!=-1) {
                int ret;
                ret=send(s, KILL, KILL_LEN, 0);
                close(s);
                return ret==KILL_LEN;
        } else {
                fprintf(stderr, "ocha:error: no instance of ocha to kill.\n");
                return FALSE;
        }
}

gboolean ocha_init_ocha_is_running()
{
        int s;

        s=client_connect();
        if(s!=-1) {
                close(s);
                return TRUE;
        } else {
                return FALSE;
        }
}

/* ------------------------- static functions */
static char *get_catalog_path(void)
{
        GString *catalog_path = g_string_new("");
        const char *homedir = g_get_home_dir();
        char *retval;

        g_string_append(catalog_path, homedir);

        g_string_append(catalog_path, "/.ocha");
        mkdir(catalog_path->str, 0700);
        g_string_append(catalog_path, "/catalog");
        retval = catalog_path->str;
        g_string_free(catalog_path, FALSE);
        return retval;
}

/**
 * Allocate and fill a sockaddr_un structure
 * @param len_ptr if non-null, it'll receive the actual length
 * of the structure
 * @return a struct sockaddr_un, to free using g_free. sun_path is
 * nul-terminated even if the nul character is not taken into
 * account for the lengthe
 */
static struct sockaddr_un *create_socket_address(size_t *len_ptr)
{
        struct sockaddr_un *addr;
        const char *homedir = g_get_home_dir();
        size_t len = (size_t) ((((struct sockaddr_un *)0)->sun_path)+strlen(homedir)+strlen(SOCKET_PATH));
        if(len_ptr) {
                *len_ptr=len;
        }
        len++; /* final nul character in sun_path */
        if(len<sizeof(struct sockaddr_un)) {
                /* better safe than sorry... some
                 * code may expect to allocate at least
                 * that much space
                 */
                len=sizeof(struct sockaddr_un);
        }
        addr = (struct sockaddr_un *)g_malloc(len+1);

        addr->sun_family=AF_UNIX;
        strcpy(addr->sun_path, homedir);
        strcat(addr->sun_path, SOCKET_PATH);

        return addr;
}

/**
 * Accept new connections and create new channels.
 */
static gboolean channel_new_connection(GIOChannel *source, GIOCondition cond, gpointer userdata)
{
        gint fd;
        gint clientsock;
        GIOChannel *client;
        g_return_val_if_fail(source, FALSE);

        if (cond!=G_IO_IN) {
                return TRUE;
        }

        fd = g_io_channel_unix_get_fd(source);
        clientsock = accept(fd, NULL/*addr_out*/, NULL/*addr_len_out*/);
        if(clientsock==-1) {
                fprintf(stderr,
                        "ocha:warning: could not accept new connection: %s\n",
                        strerror(errno));
                return FALSE;
        }
        if(send(clientsock, HELLO, HELLO_LEN, 0)!=HELLO_LEN) {
                fprintf(stderr,
                        "ocha:warning: could not send data (%d bytes) to client: %s\n",
                        HELLO_LEN,
                        strerror(errno));
                close(clientsock);
                return FALSE;
        }

        client = g_io_channel_unix_new(clientsock);
        g_io_add_watch(client, G_IO_IN, channel_client_data, NULL/*userdata*/);
        g_io_add_watch(client, G_IO_HUP|G_IO_ERR, channel_hangup, NULL/*userdata*/);
        return TRUE;
}

static gboolean channel_client_data(GIOChannel *source, GIOCondition cond, gpointer userdata)
{
        gint fd;
        char buffer[5];
        ssize_t ret;

        g_return_val_if_fail(source, FALSE);
        if (cond!=G_IO_IN) {
                return TRUE;
        }

        fd = g_io_channel_unix_get_fd(source);
        ret = recv(fd, &buffer, sizeof(buffer), 0);
        if(ret>=1) {
                if(buffer[0]=='k') {
                        fprintf(stderr,
                                "ocha:warning: killed\n");
                        kill_ocha();
                }
        }
        g_io_channel_close(source);
        return TRUE;
}

static gboolean channel_hangup(GIOChannel *source, GIOCondition cond, gpointer userdata)
{
        g_return_val_if_fail(source, FALSE);
        g_io_channel_close(source);
        return TRUE;
}

/**
 * Connect to ocha as a client.
 *
 * This connects to ocha and checks the HELLO message
 * then returns the FD.
 *
 * @return the FD or -1
 */
static int client_connect(void)
{
        struct sockaddr_un *addr;
        size_t addr_len;
        gint s;
        gint retval = -1;

        addr = create_socket_address(&addr_len);

        s = socket(AF_UNIX, SOCK_STREAM, 0);
        if(s==-1) {
                fprintf(stderr, "ocha:error: failed to create unix socket: %s\n", strerror(errno));
        } else {
                if(connect(s, (struct sockaddr *)addr, addr_len)==0) {
                        char buf[HELLO_LEN];

                        if(recv(s, buf, HELLO_LEN, 0)==HELLO_LEN
                           && strncmp(HELLO, buf, HELLO_LEN)==0) {
                                retval=s;
                        } else {
                                close(s);
                        }
                }
        }
        g_free(addr);
        return retval;
}

/**
 * What it means to 'kill' ocha
 */
static void kill_ocha(void)
{
        restart_unregister_and_quit_when_done();
}

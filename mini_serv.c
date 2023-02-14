/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   mini_serv.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jpceia <joao.p.ceia@gmail.com>             +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2022/04/05 19:46:45 by jpceia            #+#    #+#             */
/*   Updated: 2022/04/08 17:34:27 by jpceia           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>

#define MSG_SIZE 4096

struct client_s
{
    int fd;
    int id;
    char *buf;
};

// global variablas
struct client_s connections[SOMAXCONN];
int n_connections = 0;
int listener = 0;
fd_set current_sockets;

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

struct client_s add_connection(int fd, int id)
{
    connections[n_connections].fd = fd;
    connections[n_connections].id = id;
    connections[n_connections].buf = NULL;
    ++n_connections;
    FD_SET(fd, &current_sockets);
    return (connections[n_connections - 1]);
}

void remove_connection(int idx)
{
    struct client_s cli = connections[idx];

    FD_CLR(cli.fd, &current_sockets);
    if (cli.fd > 0)
    {
        close(cli.fd);
        cli.fd = 0;
    }
    if (cli.buf != NULL)
    {
        free(cli.buf);
        cli.buf = NULL;
    }
    connections[idx] = connections[n_connections - 1];
    --n_connections;
}

void cleanup_globals()
{
    if (listener > 0)
    {
        close(listener);
        listener = 0;
    }
    while (n_connections > 0)
        remove_connection(0);
}

void wrong_args_nbr()
{
    char *msg = "Wrong number of arguments\n";
    write(STDERR_FILENO, msg, strlen(msg));
    exit(EXIT_FAILURE);
}

void fatal_error()
{
    char *msg = "Fatal error\n";
    write(STDERR_FILENO, msg, strlen(msg));
    cleanup_globals();
    exit(EXIT_FAILURE);
}

void broadcast(char *msg, int from_id)
{
    for (int i = 0; i < n_connections; ++i)
    {
        struct client_s cli = connections[i];
        if (cli.id == from_id)
            continue ;
        if (send(cli.fd, msg, strlen(msg), 0) < 0)
            fatal_error();
    }
    printf("%s", msg);
}

int main(int argc, char *argv[])
{
    if (argc == 1)
        wrong_args_nbr();

    char msg[MSG_SIZE];
    char chunk[MSG_SIZE + 1];

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0)
        fatal_error();
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(atoi(argv[1]));

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        fatal_error();

    if (listen(listener, SOMAXCONN) < 0)
        fatal_error();

    FD_ZERO(&current_sockets);
    FD_SET(listener, &current_sockets);

    int counter = 0;
    while (42)
    {
        fd_set ready_sockets = current_sockets;
        if (select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) < 0)
            fatal_error();
        if (FD_ISSET(listener, &ready_sockets)) // new connection
        {
            struct sockaddr_in cli_addr;
            socklen_t len = sizeof(cli_addr);
            int fd = accept(listener, (struct sockaddr *)&cli_addr, &len);
            if (fd < 0)
                fatal_error();
            struct client_s cli = add_connection(fd, counter++);
            sprintf(msg, "server: client %d just arrived\n", cli.id);
            broadcast(msg, cli.id);
            continue ;
        }
        // else
        // find the correct client
        int idx = 0;
        for (; idx < n_connections && !FD_ISSET(connections[idx].fd, &ready_sockets); ++idx) ;
        if (idx == n_connections)
            continue ;
        struct client_s cli = connections[idx];
        int n = recv(cli.fd, chunk, MSG_SIZE, 0);
        if (n < 0)
            fatal_error();
        else if (n == 0) // Disconnect
        {
            sprintf(msg, "server: client %d just left\n", cli.id);
            broadcast(msg, cli.id);
            remove_connection(idx);
        }
        else
        {
            chunk[n] = '\0';
            cli.buf = str_join(cli.buf, chunk);
            while (42)
            {
                char *line = NULL;
                int status = extract_message(&cli.buf, &line);
                if (status == 0)
                    break ;
                if (status == -1)
                    fatal_error();
                // status == 1
                sprintf(msg, "client %d: %s", cli.id, line);
                free(line);
                broadcast(msg, cli.id);
            }
        }
    }
    return (EXIT_SUCCESS);
}
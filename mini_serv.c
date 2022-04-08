/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   mini_serv.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jpceia <joao.p.ceia@gmail.com>             +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2022/04/05 19:46:45 by jpceia            #+#    #+#             */
/*   Updated: 2022/04/08 16:43:35 by jpceia           ###   ########.fr       */
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


void exit_with_message(char *s, int status)
{
    write(STDERR_FILENO, s, strlen(s));
    exit(status);
}

void broadcast(char *msg, struct client_s *clients, int n_clients)
{
    for (int i = 0; i < n_clients; ++i)
        if (send(clients[i].fd, msg, strlen(msg), 0) < 0)
            exit_with_message("Error sending message\n", 1);
    printf("%s", msg);
}

int main(int argc, char *argv[])
{
    if (argc == 1)
        exit_with_message("Wrong number of arguments\n", 1);


    char msg[MSG_SIZE];
    char chunk[MSG_SIZE + 1];

    fd_set ready_sockets;

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0)
        exit_with_message("Fatal error\n", 1);
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(atoi(argv[1]));

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        exit_with_message("Fatal error\n", 1);

    if (listen(listener, SOMAXCONN) < 0)
        exit_with_message("Fatal error\n", 1);

    FD_ZERO(&current_sockets);
    FD_SET(listener, &current_sockets);
    
    int counter = 0;
    while (42)
    {
        ready_sockets = current_sockets;
        if (select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) < 0)
            exit_with_message("Fatal error\n", 1);
        if (FD_ISSET(listener, &ready_sockets)) // new connection
        {
            struct sockaddr_in cli_addr;
            int len = sizeof(cli_addr);
            int cli = accept(listener, (struct sockaddr *)&cli_addr, &len);
            if (cli < 0)
                exit_with_message("Fatal error\n", 1);
            connections[n_connections].fd = cli;
            connections[n_connections].id = counter;
            connections[n_connections].buf = NULL;
            ++n_connections;
            FD_SET(cli, &current_sockets);
            sprintf(msg, "server: client %d just arrived\n", counter);
            broadcast(msg, connections, n_connections);
            ++counter;
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
            exit_with_message("Fatal error\n", 1);
        else if (n == 0) // Disconnect
        {
            close(cli.fd);
            sprintf(msg, "server: client %d just left\n", cli.id);
            connections[idx] = connections[n_connections - 1];
            --n_connections;
            FD_CLR(cli.fd, &current_sockets);
            broadcast(msg, connections, n_connections);
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
                    exit_with_message("Fatal error\n", 1);
                // status == 1
                sprintf(msg, "client %d: %s", counter, line);
                free(line);
                broadcast(msg, connections, n_connections);
            }
        }
    }
    return (0);
}
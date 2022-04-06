/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   mini_serv.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jpceia <joao.p.ceia@gmail.com>             +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2022/04/05 19:46:45 by jpceia            #+#    #+#             */
/*   Updated: 2022/04/06 20:40:48 by jpceia           ###   ########.fr       */
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
};

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

    struct client_s connections[SOMAXCONN];
    int n_connections = 0;
    char msg[MSG_SIZE];

    fd_set current_sockets, ready_sockets;

    int listener = socket(AF_INET, SOCK_STREAM, 0);
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
        int n = recv(cli.fd, msg, MSG_SIZE, 0);
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
            msg[n] = '\0';
            char msg_to_send[MSG_SIZE * 2];
            sprintf(msg_to_send, "client %d: %s\n", counter, msg);
            broadcast(msg_to_send, connections, n_connections);
        }
    }
    return (0);
}
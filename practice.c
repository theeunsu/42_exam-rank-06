#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

int ids[65536], max_fd = 0, next_id = 0;
char *msgs[65536];
char buf_read[1024], buf_write[1024];
fd_set read_fds, write_fds, all_fds;

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

void fatal()
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

void broadcast (int sender, char *msg)
{
	for (int fd = 0; fd <= max_fd; fd++)
		if (FD_ISSET(fd, &write_fds) && fd != sender) // 서버야 이 fd 써서 보낼 준비 됐어
			send(fd, msg, strlen(msg), 0);
}

int setup_server (int port)
{
	int sockfd;
	struct sockaddr_in servaddr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) 
		fatal();
	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port); // 포트로 꼭 바꿔주기
  
	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) 
		fatal();
	if (listen(sockfd, 10) != 0) 
		fatal();

	return sockfd;
}	

void new_connection(int sockfd, struct sockaddr_in *addr)
{
	socklen_t len = sizeof(*addr);
	int new_fd = accept(sockfd, (struct sockaddr *)addr, &len);
	// 리스닝 소켓(sockfd)에서 대기열에 있는 클리이언트 연결 요청을 하나 꺼내서 새로운 소켓(fd) 만들어 반환
	if (new_fd < 0)
		return;

	FD_SET(new_fd, &all_fds);
	ids[new_fd] = next_id++;
	msgs[new_fd] = NULL;
	max_fd = (new_fd > max_fd) ? new_fd : max_fd;

	sprintf(buf_write, "server: client %d just arrived\n", ids[new_fd]);
	broadcast(new_fd, buf_write);
}

void handle_client (int fd)
{
	char *msg = NULL;
	int byts = recv(fd, buf_read, sizeof(buf_read) - 1, 0); // recv로 들어온 조각을 buf_read에 담고

	if (byts <= 0)
	{
		sprintf(buf_write, "server: client %d just left\n", ids[fd]);
		broadcast(fd, buf_write);
		free(msgs[fd]);
		FD_CLR(fd, &all_fds);
		close(fd);
		return;
	}

	buf_read[byts] = 0;
	msgs[fd] = str_join(msgs[fd], buf_read); // 그걸 msgs[fd]에 이어붙인 다음

	while (extract_message(&msgs[fd], &msg)) // \n 한줄씩 잘라 msg에 꺼낸다
	{
		sprintf(buf_write, "client %d: ", ids[fd]);
		broadcast(fd, buf_write);
		broadcast(fd, msg);
		free(msg);
	}
}

int main(int ac, char **av) 
{
	if (ac != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}

	struct sockaddr_in addr;
	int sockfd = setup_server(atoi(av[1]));

	FD_ZERO(&all_fds);
	FD_SET(sockfd, &all_fds);
	max_fd = sockfd;

	// read_fds = 지금 읽을 수 있는 fd 집합
	// write_fds = 지금 보낼 수 있는 fd 집합

	while (1)
	{
		read_fds = write_fds = all_fds;
		if (select(max_fd + 1, &read_fds, &write_fds, NULL, NULL) < 0)
			fatal();
		for (int fd = 0; fd <= max_fd; fd++)
		{
			if (!FD_ISSET(fd, &read_fds))
				continue;
			if (fd == sockfd)
				new_connection(sockfd, &addr);
			else
				handle_client(fd);
		}
	}
	return 0;
}

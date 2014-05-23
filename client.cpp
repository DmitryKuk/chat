#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <thread>

#include "message.h"


int s = -1;	// Сокет, связанный с сервером
message m_out;
volatile int need_continue = 1;


void reciever()
{
	message m_in;
	
	fd_set read_fds, error_fds;;
	FD_ZERO(&read_fds);
	FD_ZERO(&error_fds);
	struct timeval tv;
	
	while (need_continue) {
		FD_SET(s, &read_fds);
		FD_SET(s, &error_fds);
		tv.tv_sec = 0;
		tv.tv_usec = 500000;
		
		if (select(s + 1, &read_fds, NULL, &error_fds, &tv) <= 0) {
			usleep(500000);
			continue;
		}
		
		if (FD_ISSET(s, &error_fds)) {
			need_continue = 0;
			break;
		}
		
		if (!FD_ISSET(s, &read_fds)) {
			usleep(500000);
			continue;
		}
		
		if (m_in.recieve(s)) {
			need_continue = 0;
			break;
		}
		
		printf("\033[4m%s\033[m: %s\n", m_in.author.c_str(), m_in.text.c_str());	// Печать сообщения
	}
}


int main(int argc, char **argv)	// ./client [-P PASSWORD] SERVER_IP PORT USERNAME
{
	// Сервер
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	
	// Пароль
	m_out.text = "password";
	
	// Получение аргументов:
	// -P PASSWORD		пароль для подключения к серверу
	{
		char *pname = argv[0];	// Имя программы
		
		int ch;
		while ((ch = getopt(argc, argv, "P:")) != -1)
			switch (ch) {
			case 'P':
				m_out.text = optarg;
				break;
			default:
				fprintf(stderr, "Incorrect arguments!\nUsage:\n\t%s [-P PASSWORD] SERVER_IP PORT USERNAME\n(USERNAME max %u characters.)\n", pname, CLIENT_NAME_MAX);
				return 10;
			}
		argc -= optind;
		argv += optind;
		
		if (argc != 3
			|| inet_pton(AF_INET, argv[0], &addr.sin_addr) != 1
			|| sscanf(argv[1], "%hu", &addr.sin_port) != 1
			|| (m_out.author = argv[2]).size() > CLIENT_NAME_MAX) {
			fprintf(stderr, "Incorrect arguments!\nUsage:\n\t%s [-P PASSWORD] SERVER_IP PORT USERNAME\n(USERNAME max %u characters.)\n", pname, CLIENT_NAME_MAX);
			return 10;
		}
		
		addr.sin_port = htons(addr.sin_port);
	}
	
	fprintf(stderr, "Try connect to server... ");
	
	// Создание сокета
	s = socket(PF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto);
	if (s < 0) {
		fprintf(stderr, "ERROR\nCan't create socket: %s.\n", strerror(errno));
		return 1;
	}
	
	// Соединение с сервером
	if (connect(s, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
		fprintf(stderr, "ERROR\nCan't connect: %s.\n", strerror(errno));
		return 2;
	}
	
	// Авторизация
	if (m_out.send(s)) {
		fprintf(stderr, "ERROR\nCan't connect: %s\n", strerror(errno));
		return 3;
	}
	fprintf(stderr, "OK\n");
	
	
	// Запуск reciever'а
	std::thread reciever_thread(reciever);
	
	// Собственно, работа
	{
		char tmp[MESSAGE_LEN_MAX + 1];
		while (need_continue) {
			if (fgets(tmp, MESSAGE_LEN_MAX + 1, stdin) == NULL) {	// Получаем сообщение
				need_continue = 0;
				break;
			}
			
			// Удаляем последние пробельные символы
			int i = strlen(tmp);
			while (i > 0 && isspace(tmp[--i])) tmp[i] = '\0';
			
			m_out.text = tmp;
			m_out.send(s);
		}
	}
	fprintf(stderr, "Shutting down...");
	reciever_thread.join();
	close(s);
	fprintf(stderr, " Stopped.\n");
	return 0;
}

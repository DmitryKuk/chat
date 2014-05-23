#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <pthread.h>

#include <vector>
#include <list>
#include <string>

#include "message.h"

#define WAIT_TIME	200000	// 0.2 секунды
#define SELECT_TIME	500000	// 0.5 секунд


class client
{
public:
	client(int s, const std::string &name = "Unknown"): s(s), name(name) {}
	~client() { close(s); }
	
	int s;	// Сокет
	std::string name;	// Имя клиента
};


// Список клиентов
struct client_list:
		public std::list<client *>
{
	~client_list() { for (auto &p: *this) delete p; }
};


void message_broadcast(const message &m, const client_list &clients)
{
	// Печать сообщения в логе сервера
	fprintf(stderr, "Message: \033[4m%s\033[m: %s\n", m.author.c_str(), m.text.c_str());
	for (auto &c: clients) m.send(c->s);	// Отправка сообщения
}


inline void system_broadcast(const std::string &str, client_list &clients)
{
	message_broadcast(message({ .author = "[Server]", .text = str }), clients);
}


// Порт
uint16_t port = 49152;
bool port_set_by_user = false;

// Пароль
std::string password("password");

// Сокет
int s;


void * server(client_list *clients)
{
	while (true) {
		fd_set read_fds, error_fds;
		FD_ZERO(&read_fds); FD_ZERO(&error_fds);
		FD_SET(s, &read_fds); FD_SET(s, &error_fds);
		int max_s = s;
		
		// Добавление клиентов во множества
		for (auto c: *clients) {
			FD_SET(c->s, &read_fds); FD_SET(c->s, &error_fds);
			if (c->s > max_s) max_s = c->s;
		}
		++max_s;
		
		// Ожидание сообщений или ошибок
		int client_count = select(max_s, &read_fds, nullptr, &error_fds, nullptr);
		if (client_count <= 0) {	// Никто ничего не написал или произошла ошибка
			if (client_count < 0) perror("Server: Select error");	// Ошибка
			continue;
		}
		
		// Закрыли stdin
		if (FD_ISSET(STDIN_FILENO, &error_fds)) {
			fprintf(stderr, "Server: Shutting down...\n");
			pthread_exit(nullptr);
		}
		
		// Ошибка на прослушиваемом сокете
		if (FD_ISSET(s, &error_fds)) {
			perror("Server: listening socket error");
			pthread_exit(nullptr);
		}
		
		// Кто-то подключился
		if (FD_ISSET(s, &read_fds)) {
			--client_count;
			
			int client_s = accept(s, nullptr, nullptr);
			if (client_s < 0) {
				perror("Server: Acceptor: Can't accept connection");
				continue;
			}
			
			// Получение и проверка пароля
			message new_password;
			new_password.recieve(client_s);
			
			if (new_password.text != password) {
				fprintf(stderr, "Server: Acceptor: Can't add client: Incorrect password!\n");
				close(client_s);
				pthread_exit(nullptr);
			}
			
			// Создание клиента
			client *c = new(std::nothrow) client(client_s, new_password.author);
			if (c == nullptr) {
				fprintf(stderr, "Server: Acceptor: Unable to add client.\n");
				pthread_exit(nullptr);
			}
			
			clients->push_back(c);
			system_broadcast("Client \"" + new_password.author + "\" joined the chat.", *clients);
		}
		
		
		// Удаление клиентов с ошибками
		{
			std::vector<client_list::iterator> v;	// Нерабочие клиенты
			for (auto p = clients->begin(); p != clients->end() && client_count > 0; ++p)
				if (FD_ISSET((*p)->s, &error_fds)) {	// С сокетом клиента произошла ошибка
					system_broadcast("Client \"" + (*p)->name + "\" ran away.", *clients);
					--client_count;
					v.push_back(p);
				}
			
			for (auto &it: v) {	// Очистка списка клиентов от нерабочих клиентов
				delete *it;
				clients->erase(it);
			}
		}
		if (clients->empty()) continue;
		
		// Пересылка данных
		{
			std::vector<client_list::iterator> v;	// Нерабочие клиенты
			for (auto p = clients->begin(); p != clients->end() && client_count > 0; ++p) {
				if (FD_ISSET((*p)->s, &read_fds)) {	// Клиент что-то прислал
					--client_count;
					
					// Получение сообщения
					message m;
					if (m.recieve((*p)->s)) {
						system_broadcast("Client \"" + (*p)->name + "\" ran away.", *clients);
						v.push_back(p);
						continue;
					}
					
					// Отправка сообщения всем клиентам
					message_broadcast(m, *clients);
				}
			}
			
			for (auto &it: v) {	// Очистка списка клиентов от нерабочих клиентов
				delete *it;
				clients->erase(it);
			}
		}
	}
}


int main(int argc, char **argv)
{
	// Проверка на связь stdin с терминалом
	if (!isatty(STDIN_FILENO)) {
		fprintf(stderr, "Server: Please, don't use redirect of input stream: stdin is used for control the server.\n");
		return 11;
	}
	
	// Получение аргументов:
	// -p PORT			порт сервера
	// -P PASSWORD		пароль для подключения к серверу
	{
		char *pname = argv[0];	// Имя программы
		int ch;
		while ((ch = getopt(argc, argv, "p:P:")) != -1)
			switch (ch) {
			case 'p':
				if (sscanf(optarg, "%hu", &port) != 1) {
					fprintf(stderr, "Server: Incorrect server port!\n");
					return 10;
				}
				port_set_by_user = true;
				break;
			case 'P':
				password = optarg;
				break;
			default:
				fprintf(stderr, "Server: Incorrect arguments!\nUsage:\n\t%s [-p PORT] [-P PASSWORD]", pname);
				return 10;
			}
		argc -= optind;
		argv += optind;
	}
	
	// Создание сокета
	s = socket(PF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto);
	if (s < 0) {
		fprintf(stderr, "Server: Can't create socket: %s.\n", strerror(errno));
		return 1;
	}
	
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if (port_set_by_user) {	// Порт установил ползователь
		// Привязка сокета к IP
		if (bind(s, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
			fprintf(stderr, "Server: Can't bind the socket to the address: %s.\n", strerror(errno));
			close(s);
			return 2;
		}
	} else {	// Порт необходимо подобрать
		// Привязка сокета к IP
		while (bind(s, (const struct sockaddr *)&addr, sizeof(addr)) < 0 && port < 65535) {
			++port;
			addr.sin_port = htons(port);
		}
		if (port == 65535) {
			fprintf(stderr, "Server: Can't bind the socket to the address: %s.\n", strerror(errno));
			close(s);
			return 2;
		}
	}
	
	// Ожидание подключений
	if (listen(s, 128) < 0) {
		fprintf(stderr, "Server: Can't listen to socket: %s.\n", strerror(errno));
		close(s);
		return 3;
	}
	fprintf(stderr, "Server: listen to port: %hu; Password: \"%s\"\n", port, password.c_str());
	
	client_list clients;	// Список клиентов
	pthread_t server_th;
	
	pthread_create(&server_th, NULL, (void * (*)(void *))server, &clients);
	
	fprintf(stderr, "Server: Working... (Input will be ignored, press Ctrl+D to exit.)\n");
	while (!feof(stdin)) getc(stdin);
	fprintf(stderr, "Server: Shutting down...\n");
	
	pthread_cancel(server_th);
	close(s);
	fprintf(stderr, "Server: Stopped.\n");
	return 0;
}

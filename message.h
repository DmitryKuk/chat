#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <cctype>

#include <unistd.h>


#define CLIENT_NAME_MAX	50
#define MESSAGE_LEN_MAX	500


int send_string(int fd, const std::string &s);
int recieve_string(int fd, std::string &str);


class message
{
public:
	std::string author;
	std::string text;
	
	inline int send(int fd) const
	{
		if (send_string(fd, author) || send_string(fd, text))
			return 1;
		return 0;
	}
	
	inline int recieve(int fd)
	{
		if (recieve_string(fd, author) || recieve_string(fd, text))
			return 1;
		return 0;
	}
};


#endif	// MESSAGE_H

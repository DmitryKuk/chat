#include "message.h"

int send_string(int fd, const std::string &s)
{
	uint16_t size = s.size(), pos = 0;	// Не size_t, т.к. сообщения короткие
	if (write(fd, &size, sizeof(size)) != sizeof(size)) return 1;
	while (size > 0) {
		int status = write(fd, s.c_str() + pos, size);
		if (status < 0) return 1;
		if (status == 0) { usleep(100000); continue; }
		size -= status;
		pos += status;
	}
	return 0;
}

int recieve_string(int fd, std::string &str)
{
	int status = 0;
	uint16_t size, pos = 0;
	if (read(fd, &size, sizeof(size)) != sizeof(size)) status = 1;
	else {
		char data[size + 1];
		while (size > 0) {
			int status = read(fd, data + pos, size);
			if (status < 0) { status = 1; break; }
			if (status == 0) { usleep(100000); continue; }
			size -= status;
			pos += status;
		}
		data[pos] = '\0';
		str = data;
	}
	return status;
}

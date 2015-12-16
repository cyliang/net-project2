#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct mail_struct {
	char *sname;
	char *smail;
	char *rname;
	char *rmail;
	char *subjt;
	char *conte;
};

int get_data(struct mail_struct *);
void sendf(char *, int, FILE *);

int main() {
	// Log at LOG_FILE
	FILE *log_file = fopen(LOG_FILE, "a");
	if(!log_file) {
		puts("Fail to open log file.\nLog on stdout instead...");
		log_file = stdout;
	}

	// Socket and sockaddr
	int s_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(s_fd == -1) {
		puts("Fail to create socket.");
		exit(-1);
	}

	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_addr.s_addr = inet_addr(SMTP_SERVER_ADDR);
	addr.sin_port = htons(SMTP_SERVER_PORT);
	addr.sin_family = AF_INET;

	struct mail_struct mail;
	char buf[101];
	while(1) {
		fputs("------------------------------------------------------\r\n", log_file);
		// Read from pipe (block) and process the GET request
		if(!get_data(&mail)) {
			fputs("Data is not completed\r\n", log_file);
			fputs("------------------------------------------------------\r\n", log_file);
			fflush(log_file);
			continue;
		}

		// Time note
		time_t nowtime = time(NULL);
		char *time_str = ctime(&nowtime);
		time_str[strlen(time_str) - 1] = '\0';
		fputs(time_str, log_file);
		fputs("\r\n------------------------------------------------------\r\n", log_file);

		// Connect
		if(connect(s_fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
			fputs("Fail to connect.\r\n", log_file);
			continue;
		}

		buf[recv(s_fd, buf, 100, 0)] = '\0';
		fprintf(log_file, "\e[1;32mReceive > %s\e[m", buf);

		// HELO
		fputs("\e[1;34mSend    > HELO cyl.dorm9.nctu.edu.tw\e[m\r\n", log_file);
		send(s_fd, "HELO cyl.dorm9.nctu.edu.tw\r\n", 28, 0);
		
		buf[recv(s_fd, buf, 100, 0)] = '\0';
		fprintf(log_file, "\e[1;32mReceive > %s\e[m", buf);

		// FROM
		fputs("\e[1;34mSend    > MAIL FROM: ", log_file);
		send(s_fd, "MAIL FROM: ", 11, 0);
		sendf(mail.smail, s_fd, log_file);
		send(s_fd, "\r\n", 2, 0);
		
		buf[recv(s_fd, buf, 100, 0)] = '\0';
		fprintf(log_file, "\e[m\r\n\e[1;32mReceive > %s\e[m", buf);

		// TO
		fputs("\e[1;34mSend    > RCPT TO: ", log_file);
		send(s_fd, "RCPT TO: ", 9, 0);
		sendf(mail.rmail, s_fd, log_file);
		send(s_fd, "\r\n", 2, 0);

		buf[recv(s_fd, buf, 100, 0)] = '\0';
		fprintf(log_file, "\e[m\r\n\e[1;32mReceive > %s\e[m", buf);
	
		// DATA
		fputs("\e[1;34mSend    > DATA\e[m\r\n", log_file);
		send(s_fd, "DATA\r\n", 6, 0);

		buf[recv(s_fd, buf, 100, 0)] = '\0';
		fprintf(log_file, "\e[1;32mReceive > %s\e[m", buf);
	
		// content
		fputs("\e[1;34mSend    > Subject: ", log_file);
		send(s_fd, "Subject: ", 9, 0);
		sendf(mail.subjt, s_fd, log_file);
	
		fputs("\e[m\r\n\e[1;34mSend    > From:\"", log_file);
		send(s_fd, "\r\nFrom:\"", 8, 0);
		sendf(mail.sname, s_fd, log_file);
		send(s_fd, "\" ", 2, 0);
		fputs("\" ", log_file);
		sendf(mail.smail, s_fd, log_file);

		fputs("\e[m\r\n\e[1;34mSend    > To:\"", log_file);
		send(s_fd, "\r\nTo:\"", 6, 0);
		sendf(mail.rname, s_fd, log_file);
		send(s_fd, "\" ", 2, 0);
		fputs("\" ", log_file);
		sendf(mail.rmail, s_fd, log_file);
		send(s_fd, "\r\n\r\n", 4, 0);
	
		fputs("\e[m\r\n\e[1;34mSend    >\r\n", log_file);
		sendf(mail.conte, s_fd, log_file);
		send(s_fd, "\r\n.\r\n", 5, 0);

		buf[recv(s_fd, buf, 100, 0)] = '\0';
		fprintf(log_file, "\r\n.\e[m\r\n\e[1;32mReceive > %s\e[m", buf);
	
		// quit
		fputs("\e[1;34mSend    > quit\e[m\r\n", log_file);
		send(s_fd, "quit\r\n", 6, 0);

		buf[recv(s_fd, buf, 100, 0)] = '\0';
		fprintf(log_file, "\e[1;32mReceive > %s\e[m", buf);
	
		close(s_fd);
		fputs("\r\n\r\n", log_file);
		fflush(log_file);
	}
}

// To read mail content into corresponding variables
int get_data(struct mail_struct *mail) {
	// Split message read from pipe into single mail content
	static char recv_message[65536];
	char *token;

	if(!(token = strtok(NULL, "?"))) {
		// Open pipe (block at here)
		int fifo = open(PIPE_FILE, O_RDONLY);
		if(fifo < 0) {
			puts("Fail to open pipe");
			exit(-1);
		}

		recv_message[read(fifo, recv_message, 65535)] = '\0';
		strtok(recv_message, "?");
		token = strtok(NULL, "?");
	}

	// Get the values of each variable
	const static char *var_name[] = {"sname=", "smail=", "rname=", "rmail=", "subjt=", "conte="};
	int i;
	for(i=0; i<6; i++) {
		if(!(((char **) mail)[i] = strstr(token, var_name[i])))
			return 0;
	}

	for(i=0; i<6; i++) {
		((char **) mail)[i] += 6;
		((char **) mail)[i][strcspn(((char **) mail)[i], "&/")] = '\0';
	}

	return 1;
}

// To transform %xx or + to char and send
void sendf(char *msg, int s_fd, FILE *log_f) {
	
	char *pos = msg;
	char c[3], s[1];
	
	while(pos = strpbrk(msg, "+%")) {
		fwrite(msg, 1, pos - msg, log_f);
		send(s_fd, msg, pos - msg, 0);
		if(*pos == '+') {
			fputc(' ', log_f);
			send(s_fd, " ", 1, 0);

			msg = pos + 1;
		} else {
			strncpy(c, pos + 1, 2);
			*s = (char) strtol(c, NULL, 16);

			fputc(*s, log_f);
			send(s_fd, s, 1, 0);

			if(*s == '\r') {
				fputc('\n', log_f);
				send(s_fd, "\n", 1, 0);
			}

			msg = pos + 3;
		}
	}
	fputs(msg, log_f);
	send(s_fd, msg, strlen(msg), 0);
}

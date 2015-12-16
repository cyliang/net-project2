#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

char *get_file(char *file_addr, int *length);
void *accept_client(void *arg);
FILE *log_file;

int main() {
	// Log at LOG_FILE
	log_file = fopen(LOG_FILE, "a");
	if(!log_file) {
		puts("Fail to open log file.\nLog on stdout instead...");
		log_file = stdout;
	}

	// Socket, bind, and listen
	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = PF_INET;
	addr.sin_port = htons(HTTP_PORT);
	addr.sin_addr.s_addr = htons(0);

	int s_fd = socket(PF_INET, SOCK_STREAM, 0);
	if(s_fd < 0 || bind(s_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(s_fd, 5) < 0) {
		puts("Fail to bind or listen, check netstat or change a port.");
		exit(-1);
	}

	// Accept and create a thread for each client
	while(1) {
		pthread_t tid;
		int *client_fd = (int *) malloc(sizeof(int));
		if(client_fd == NULL)
			exit(-1);
		else if((*client_fd = accept(s_fd, NULL, NULL)) < 0)
			free(client_fd);
		else if(pthread_create(&tid, NULL, &accept_client, client_fd) != 0) {
			close(*client_fd);
			free(client_fd);
		}
	}
}

void *accept_client(void *fd) {
	// Receive from browser
	char recv_msg[65536] = {0};
	if(recv(*(int *) fd, recv_msg, 65536, 0) < 0) {
		close(*(int *) fd);
		free(fd);
		pthread_exit(NULL);
	}

	// Write log
	time_t nowtime = time(NULL);
	char *time_str = ctime(&nowtime);
	time_str[strlen(time_str) - 1] = '\0';
	fprintf(log_file, "\e[1;32m[%s]Receive > \e[m\r\n%s\r\n", time_str, recv_msg);
	fflush(log_file);

	// Split command and path then do what should be done
	char *path = strchr(recv_msg, ' ') + 1;
	*(path - 1) = '\0';
	*strchr(path, ' ') = '\0';

	// For non-supported command
	if(strcmp(recv_msg, "GET") != 0)
		send(*(int *) fd, "HTTP/1.0 400 Bad Request\r\n", 26, 0);
	
	// For Email-sending service
	// Queuing mail content into pipe and another program will send it
	else if(strncmp(path, "/sendmail?", 10) == 0) {
		send(*(int *) fd, "HTTP/1.0 200 OK\r\n\r\n", 19, 0);
		int fifo = open(PIPE_FILE, O_WRONLY);
		if(fifo != -1) {
			write(fifo, path, strlen(path));
			close(fifo);
		}
	
	// For normal file request
	} else {
		int length;
		char *response = get_file(path, &length);
		send(*(int *) fd, response, length, 0);
		free(response);
	}

	// Close connection to client and terminate thread
	close(*(int *)fd);
	free(fd);
	pthread_exit(NULL);
}

char *get_file(char *file_addr, int *length) {
	// Get real file address
	char real_addr[10 + strlen(file_addr)];
	sprintf(real_addr, "http_root%s", file_addr);

	// Open file and response if no such file
	FILE *file_ptr = fopen(real_addr, "rb");
	if(file_ptr == NULL) {
		char *req = (char *) malloc(*length = 27);
		strcpy(req, "HTTP/1.0 404 Not Found\r\n\r\n");
		return req;
	}

	// Get file's length
	fseek(file_ptr, 0, SEEK_END);
	int file_size = ftell(file_ptr);
	rewind(file_ptr);

	// Make response message
	char *response = (char *) malloc(120 + file_size);
	if(response == NULL)
		exit(-1);

	sprintf(response, "HTTP/1.0 200 OK\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n", file_size, strcmp(strtok(file_addr, " ."), "swf") == 0 ? "application/x-shockwave-flash" : "text/html");
	*length = file_size + strlen(response);
	fread(response + strlen(response), 1, file_size, file_ptr);

	fclose(file_ptr);
	return response;
}


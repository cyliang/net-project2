MAIL_LOG_FILE = sendmail.log
HTTP_LOG_FILE = http.log
PIPE_FILE = sendmail_pipe
HTTP_PORT = 8080
SMTP_SERVER_ADDR = 140.113.2.70
SMTP_SERVER_PORT = 25

all:
	gcc -DPIPE_FILE=\"$(PIPE_FILE)\" -DLOG_FILE=\"$(HTTP_LOG_FILE)\" -DHTTP_PORT=$(HTTP_PORT) -lpthread -o http main.c
	gcc -DPIPE_FILE=\"$(PIPE_FILE)\" -DLOG_FILE=\"$(MAIL_LOG_FILE)\" -DSMTP_SERVER_ADDR=\"$(SMTP_SERVER_ADDR)\" -DSMTP_SERVER_PORT=$(SMTP_SERVER_PORT) -o sendmail mail.c
	mkfifo $(PIPE_FILE)

clean:
	rm -f http sendmail $(PIPE_FILE) $(HTTP_LOG_FILE) $(MAIL_LOG_FILE)

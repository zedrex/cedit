CC = gcc
cedit: cedit.c
	$(CC) cedit.c  -o cedit -Wall -Wextra -pedantic -std=c99
all:
	gcc -Wall -c common.c
	gcc -Wall servidor_dns.c common.o -lpthread -o servidor_dns
clean:
	rm common.o servidor_dns
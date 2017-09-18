service client	:service.o client.o
	g++ -o service service.o
	g++ -o client client.o

service.o	:service.c
	g++ -c service.c
client.o	:client.c
	g++ -c client.c
clean	:
	rm service
	rm client
	rm *.o

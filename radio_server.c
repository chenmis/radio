#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/in.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define MAX_STATIONS 4
#define FALSE 0
#define TRUE 1
#define HELLO_TYPE 0
#define HELLO_RESERVED 0
#define HELLO_SIZE 3
#define ASK_SONG_TYPE 1
#define ASK_SONG_SIZE 3
#define UP_SONG_TYPE 2
#define UP_SONG_SIZE 6
#define WELCOME_TYPE 0
#define WELCOME_SIZE 9
#define ANNOUNCE_TYPE 1
#define INVALID_TYPE 3
#define NEW_STATION_TYPE 4
#define BUFFER_SIZE 1024
#define MAX_INPUT 80
#define PERMIT_TYPE 2
#define PERMIT_SIZE 2
#define MAX_CLIENTS 5

uint16_t listen_port;
struct sockaddr_in server_addr;
uint16_t udp_port;
int num_of_stations = 0, num_of_clients = 0;
char files_names[MAX_STATIONS][80], multicastip[15];
pthread_t clients[MAX_CLIENTS], stations[MAX_STATIONS];
int udp_sockets[MAX_STATIONS];
pthread_mutex_t lock;
FILE * files[MAX_STATIONS];
int welcomeSock;

void * open_Station(void * args);
void * open_tcp_sock();
void * client_thread(void * args);
void send_invalid_comment(int client_sock ,char * comment);
void send_new_station();

void * open_Station(void * args){

	int station_num = *((int *)(args));
	uint8_t buffer[BUFFER_SIZE];
	socklen_t addr_size;
	u_char ttl = 10;

	// open file
	files[station_num] = fopen(files_names[ station_num ],"r");

	if(files[station_num] == NULL)
	{
		perror("can't open file");
		exit(EXIT_FAILURE);
	}
	printf("file: %d opend\n",station_num);

	// reset buffer and server address
	memset(&buffer, 0, sizeof( buffer ));
	memset(&server_addr, 0, sizeof(server_addr));

	// initialize connection settings
./	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(multicastip);
	server_addr.sin_port = htons(udp_port);

	// open a socket, ipv4, udp
	if ( (udp_sockets[station_num] = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
	{
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}
	printf("socket: %d opend\n",station_num);

	// bind connection (socket) to port and interface
	if(bind(udp_sockets[station_num], (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in))<0)
	{
		perror("Bind failed");
		exit(EXIT_FAILURE);
	}
	printf("%d. bind\n",station_num);

	// initialize time to live field
	setsockopt(udp_sockets[station_num], IPPROTO_IP, IP_MULTICAST_TTL, &ttl,sizeof(ttl));

	addr_size = sizeof( server_addr );

	// while all parts not send yet
	while ( TRUE )
	{
		// send one part
		fscanf(files[station_num],"%1024c",buffer);
		sendto(udp_sockets[station_num], buffer, BUFFER_SIZE, 0, (struct sockaddr *) &server_addr, addr_size);
		usleep(62500);
	}

}

void * open_tcp_sock(){

	int  newSock;
	struct sockaddr_in client_addr;
	socklen_t client_len;

	if((welcomeSock = socket(AF_INET, SOCK_STREAM, 0))==0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}
	printf("welcome socket open\n");

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(listen_port);

	if(bind(welcomeSock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}
	printf("bind\n");

	if( !listen(welcomeSock,SOMAXCONN))
	{
		printf("listen\n");
	}
	else {
		perror("listening failed");
		exit(EXIT_FAILURE);
	}

	client_len = sizeof(client_addr);

	while(TRUE){

		newSock = accept(welcomeSock, (struct sockaddr *)&client_addr, &client_len);

		printf("new client arrived (num: %d)\n", num_of_clients + 1);

		pthread_create(&clients[num_of_clients], NULL, client_thread, &(newSock));

		num_of_clients ++;
	}

}

void * client_thread(void * args){

	uint8_t msg[1024];
	int num_of_bytes, * sock = args, station_num, song_size;
	uint16_t * cast;
	uint32_t * cast32;

	char songName[80];
	int i;

	int * client_sock = args;

	num_of_bytes = recv(* sock, msg, HELLO_SIZE, 0);

	if( msg[0] != HELLO_TYPE || num_of_bytes != HELLO_SIZE){

		send_invalid_comment(*client_sock ,"Error, hello msg illigal");
		perror("Error, hello msg illigal");
		close(* sock);

	}

	msg[0] = WELCOME_TYPE;
	cast = (uint16_t *)(msg + 1);
	* cast = ntohs(num_of_stations);
	cast32 = (uint32_t *)(msg + 3);
	* cast32 = ntohl(inet_addr(multicastip));
	cast = (uint16_t *)(msg + 7);
	* cast = ntohs(udp_port);

	printf("hello received\n");

	send( * sock, msg, WELCOME_SIZE, 0);

	while(TRUE){

		if( (num_of_bytes = recv(* sock, msg, BUFFER_SIZE, 0)) <= 0){

			perror("client out");
			close(* sock);
			return NULL;

		}

		switch (msg[0]) {

		case ASK_SONG_TYPE:

			if(num_of_bytes != 3){

				perror("Ask msg size illigal");
				send_invalid_comment(* sock ,"Error, hello msg illigal");

			}

			msg[0] = ANNOUNCE_TYPE;
			station_num = (int)(*(uint16_t *)(msg + 1));
			msg[1] = sizeof(files_names[station_num])/sizeof(uint8_t);
			strcpy((char *)(msg + 2), files_names[station_num]);

			send(* sock, msg, 3 + msg[1] ,0);

			break;

		case UP_SONG_TYPE:

			msg[0] = PERMIT_TYPE;

			if (pthread_mutex_trylock(&lock) == 0 && num_of_stations < MAX_STATIONS)
			{

				strncpy(songName, (char *)(msg + 6), (int)msg[5]);

				for( i = 0 ; i < num_of_stations ; i ++ ){

					if( strcmp(songName, files_names[i]) != FALSE){

						msg[1] = FALSE;

						send(* sock, msg, PERMIT_SIZE, 0);

						break;

					}

				}

				if(i == num_of_stations){

					song_size = ntohl(*((int *)(msg + 1)));

					msg[1] = TRUE;

					send(* sock, msg, PERMIT_SIZE, 0);

					files[num_of_stations] = fopen(songName, "wr");

					while(song_size > 0){

						num_of_bytes = recv( * sock, msg, BUFFER_SIZE, 0);

						fwrite(msg, sizeof(char), num_of_bytes, files[num_of_stations]);

						song_size -= num_of_bytes;

					}

					rewind(files[num_of_stations]);

					i = num_of_stations;

					open_Station(&i);

					num_of_stations ++;

					send_new_station();

				}

				pthread_mutex_unlock (&lock);

				break;
			}

			msg[1] = FALSE;

			send(* sock, msg, PERMIT_SIZE, 0);

			break;

		default:

			send_invalid_comment(* sock ,"Error, type msg illigal");
			perror("Error, type msg illigal");
		}

	}

}

void send_new_station(){

	uint8_t msg[3];

	msg[0] = 4;
	*(uint16_t *)(msg + 1) = num_of_stations;

	send(welcomeSock, msg, 3, 0);

}

void send_invalid_comment(int client_sock ,char * comment){

	uint8_t msg[1024];

	msg[0] = INVALID_TYPE;
	msg[1] = sizeof(comment)/sizeof(uint8_t);
	strcpy(msg + 2, comment);

	send(client_sock, comment, msg[1] + 2, 0);

	close(client_sock);

}

void * main(int argc, char * argv[]){


	int i;

	if( argc < 5 ){

		perror("To few argumants");
		exit(EXIT_FAILURE);

	}

	listen_port = atoi(argv[1]);
	strcpy(multicastip, argv[2]);
	udp_port = atoi(argv[3]);



	for( i = 4 ; i < argc ; i ++ ){

		strcpy(files_names[i - 4], argv[i]);

	}

	num_of_stations = argc - 4;


	for( i = 0 ; i < num_of_stations ; i ++ ){

		pthread_create(&stations[i], NULL, open_Station, &(i));

		sleep(3);

	}

	open_tcp_sock();

	while(TRUE){}; return EXIT_SUCCESS;

}

/*
 ============================================================================
 Name        : radio_control.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/in.h>
#include <string.h>
#include <time.h>
#include <pthread.h>


#define FALSE 0
#define TRUE 1
#define HELLO_TYPE 0
#define HELLO_RESERVED 0
#define HELLO_SIZE 3
#define ASK_SONG_TYPE 1
#define ASK_SONG_SIZE 3
#define UP_SONG_TYPE 2
#define UP_SONG_SIZE 6
#define MAX_STATIONS 4
#define WELCOME_TYPE 0
#define WELCOME_SIZE 9
#define ANNOUNCE_TYPE 1
#define INVALID_TYPE 3
#define NEW_STATION_TYPE 4
#define BUFFER_SIZE 1024
#define MAX_INPUT 80
#define PERMIT_TYPE 2
#define PERMIT_SIZE 2
#define clean_buffer while(getchar()!='\n');


pthread_t multicast_thread;
uint8_t msg[1024], UploadSong[5000];
int sock, udp_sock, switch_station = FALSE;
uint16_t num_of_stations = 0;
char stations_ip[15];
uint16_t stations_port;
uint8_t stationNumber;
uint16_t tcp_port;
char * tcp_ip;


void Send_Hello();
void Send_Ask_Song(uint16_t stationNumber);
int Timeout_Occur(int seconds, int miliSeconds);
void Get_Welcome_Msg();
void Connect_To_Station(int stationNumber);
void * Listen_to_Station(void * args);
void Send_Up_Song(int32_t songSize, uint8_t songNameSize, char * songName);
void Start_Connection();
void Close_All();
void print_menu();
void Ask_Song();
void Up_Song();
uint8_t Recive_Permit();



void Send_Hello(){


	uint16_t * cast;

	//constract hello massege
	msg[0] = HELLO_TYPE;
	cast = (uint16_t*)(msg + 1);
	* cast = htons(HELLO_RESERVED);

	//send hello massege
	send(sock, msg, HELLO_SIZE, 0);
}

void Send_Ask_Song(uint16_t stationNumber){

	uint16_t * cast;

	//constract ask song massege
	msg[0] = ASK_SONG_TYPE;
	cast = (uint16_t*)(msg + 1);
	* cast = htons(stationNumber);

	//send ask song massege
	send(sock, msg, ASK_SONG_SIZE, 0);
}

int Timeout_Occur(int seconds, int miliSeconds){

	fd_set rfds;
	struct timeval tv;
	int retval;

	/* Watch stdin (fd 0) to see when it has input. */
	FD_ZERO(&rfds);
	FD_SET(sock, &rfds);

	/* Wait up to five seconds. */
	tv.tv_sec = seconds;
	tv.tv_usec = miliSeconds * 1000;

	retval = select(sock + 1, &rfds, NULL, NULL, &tv);

	/* Don't rely on the value of tv now! */
	if (retval == -1)
		perror("Timeout_Occur error");
	else if (retval)
		return FALSE;
	/* FD_ISSET(0, &rfds) will be true. */
	return TRUE;

}

void Get_Welcome_Msg(){

	recv(sock, msg, WELCOME_SIZE, 0);

	if( msg[0] != WELCOME_TYPE ){

		close(sock);
		perror("get welcome msg error, type inappropriate");
		exit(EXIT_FAILURE);

	}
	num_of_stations = htons(*(uint16_t *)&(msg[1]));
	sprintf(stations_ip,"%d.%d.%d.%d",msg[6], msg[5], msg[4], msg[3]);
	stations_port = htons(*(uint16_t *)(msg + 7));

}

void Connect_To_Station(int stationNumber){

	struct sockaddr_in server_addr;
	socklen_t addr_size = 0;
	struct ip_mreq imreq;
	struct in_addr addr;

	if(stationNumber > num_of_stations){

		perror("Station don't exist");
		//close(sock);
		//exit(EXIT_FAILURE);

		return ;
	}

	// reset server addres
	memset(&server_addr, 0, sizeof(server_addr));
	memset(&imreq,0,sizeof(imreq));

	// initialize multicast parameters
	imreq.imr_interface.s_addr = htonl(INADDR_ANY);
	imreq.imr_multiaddr.s_addr = inet_addr(stations_ip);

	// initialize connection settings
	addr.s_addr = inet_addr(stations_ip);
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(stations_port);

	// open a socket, ipv4, udp
	if ( (udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
	{
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}

	// bind connection (socket) to port and interface
	if(bind(udp_sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in))<0)
	{
		perror("Bind failed");
		exit(EXIT_FAILURE);
	}

	// initialize time to live field
	setsockopt(udp_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,(const void *)&imreq, sizeof(struct ip_mreq));

	addr_size = sizeof(struct sockaddr_in);

	pthread_create(&multicast_thread, NULL, Listen_to_Station, &server_addr);

	//pthread_join(multicast_thread, NULL);

}

void * Listen_to_Station(void * args){

	struct sockaddr_in * server_addr = args;
	int num_of_bytes, retval;
	void * plaster;
	struct ip_mreq imreq;
	const FILE * songfp;
	char buffer[BUFFER_SIZE + 1];
	fd_set rdfs;
	struct timeval tv;
	socklen_t addr_len;

	buffer[0]='c';

	songfp = popen("play -t mp3 -> /dev/null 2>&1", "w"); plaster = songfp;

	addr_len = sizeof(struct sockaddr_in);

	while(TRUE){

		FD_ZERO(&rdfs);
		FD_SET(udp_sock, &rdfs);

		tv.tv_sec = 2;
		tv.tv_usec = 0;

		retval = select (udp_sock + 1, &rdfs, NULL, NULL, &tv);

		if(retval){

			//Receive a reply from the server
			num_of_bytes = recvfrom(udp_sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &server_addr, &addr_len);
			songfp = plaster;
			if( num_of_bytes < 0  )
			{
				perror("Receive udp failed");
				exit(EXIT_FAILURE);
			} else if( num_of_bytes > 0) {

				//fwrite (buffer , sizeof(char), num_of_bytes, songfp);

				memset((void *)buffer,0,1024);
			}

		}

		if( switch_station ){

			printf("change station to  %d\n",stationNumber);

			setsockopt(udp_sock, IPPROTO_IP, IP_DROP_MEMBERSHIP,(const void *)&imreq, sizeof(struct ip_mreq));

			// initialize multicast parameters
			imreq.imr_interface.s_addr = INADDR_ANY;
			imreq.imr_multiaddr.s_addr = inet_addr(stations_ip + stationNumber);

			setsockopt(udp_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,(const void *)&imreq, sizeof(struct ip_mreq));

			switch_station = FALSE;

		}

	}
}


void Send_Up_Song(int32_t songSize, uint8_t songNameSize, char * songName){

	uint32_t * cast;

	//constract up song massege
	msg[0] = UP_SONG_TYPE;
	cast =(uint32_t*)(msg + 1);
	* cast = htonl(songSize);
	msg[5] = songNameSize;
	cast = (uint32_t*)(msg + 6);
	* cast = (uint32_t)(songName);

	//send up song massege
	send(sock, msg, UP_SONG_SIZE, 0);



}

void Start_Connection(){

	struct sockaddr_in server_addr;

	if((sock = socket(AF_INET, SOCK_STREAM, 0))==0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	server_addr.sin_addr.s_addr = inet_addr(tcp_ip);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(tcp_port);


	if ((connect(sock, (struct sockaddr *) &server_addr, sizeof(server_addr))) < 0){

		perror("connect failed");
		exit(EXIT_FAILURE);

	}

}

void Close_All(){
	printf("1");
	close(udp_sock);
	printf("2");
	close(sock);
	pthread_exit(&multicast_thread);
	printf("3");
}

void print_menu(){

	printf("   Select an option\n"
			"n - Ask for song\n"
			"s - Upload song\n"
			"q - Quit\n");
}

void Ask_Song(){

	char songName[80] = {0}, ans;
	int retval, input;
	struct timeval tv;
	fd_set rfds;

	printf("Please insert station number: ");

	scanf("%d",&input);
	clean_buffer;
	if(input > num_of_stations || input < 0 ){

		Close_All();
		printf("Wrong input - num of station not legal, Bye Bye..\n");
		exit(EXIT_FAILURE);

	}

	Send_Ask_Song(input);

	/* Wait up to five seconds. */
	tv.tv_sec = 2;
	tv.tv_usec = 0;

	FD_ZERO(&rfds);
	FD_SET(sock, &rfds);

	retval = select(sock + 1, &rfds, NULL, NULL, &tv);

	/* Don't rely on the value of tv now! */
	if (retval == -1)

		perror("Timeout_Occur error");

	else if ( retval == 0 ){

		perror("Timeout occur at Ask_song");
		//Close_All();
		//exit(EXIT_FAILURE);

	} else {

		recv(sock, msg, BUFFER_SIZE, 0);

		if ( msg[0] == NEW_STATION_TYPE ){

			num_of_stations = htons((uint16_t *) (msg + 1));

			printf("New station is available (corrent number of stations: %d)\n", num_of_stations);

			retval = select(sock + 1, &rfds, NULL, NULL, &tv);

		}

		if ( msg[0] == INVALID_TYPE ){

			perror("Invalid type of massege at AskSong\n");
			Close_All();
			exit(EXIT_FAILURE);

		}

		if ( msg[0] == ANNOUNCE_TYPE || (msg[0] == NEW_STATION_TYPE && retval > 0)){

			if ( msg[0] == NEW_STATION_TYPE && retval > 0 ){

				recv(sock, msg, BUFFER_SIZE, 0);

				if ( msg[0] != ANNOUNCE_TYPE ){

					perror("Invalid type of massege at AskSong\n");
					Close_All();
					exit(EXIT_FAILURE);

				}

			}

			strncpy (songName, (char*)(msg + 2), (int)msg[1]);

			printf("Station number: %d, Song playing on station: %s\n", input, songName);

			printf("Do you want to switch to this station (Y/N):");

			ans = getchar();
			clean_buffer;

			switch(ans){

			case 'Y':
			case 'y':

				stationNumber = input;
				switch_station = TRUE;

				break;

			case 'N':
			case 'n':

				printf("OK\n");
				break;

			default:

				Close_All();
				printf("Wrong input, Bye Bye..\n");
				exit(EXIT_FAILURE);

			}

		}
		if (  msg[0] != ANNOUNCE_TYPE && msg[0] != INVALID_TYPE && msg[0] != NEW_STATION_TYPE ){

			perror("Server send wrong format massege, Bye Bye...\n");
			Close_All();
			exit(EXIT_FAILURE);
		}

	}
}

void Up_Song(){

	/*---------we are here---------*/
	uint32_t * cast;
	unsigned int bool, nameSize, songSize, permit;
	char * songName, path_of_song[80] = {0}, *last;
	FILE * song;

	do{

		bool = 0;

		printf("Please insert path of song: ");

		scanf("%s",path_of_song);

		songName = strtok(path_of_song, "/");

		last = songName;

		while (songName != NULL)
		{
			last = songName;
			songName = strtok (NULL, "/");
		}

		songName = last;

		nameSize = strlen(songName);

		if(!(song = fopen(path_of_song,"r"))){

			perror("can't open file\n");

			bool = 1;

		}

	}while(bool);

	fseek(song, 0L, SEEK_END);
	songSize = ftell(song); //file's size in bytes
	rewind(song); songSize = 200000;

	if(songSize < 2000 || songSize > 10485760){

		perror("file size error\n");

		close(song);

	}




	msg[0] = UP_SONG_TYPE;
	cast = msg + 1;
	* cast = htonl(songSize);
	msg[5] = nameSize;
	strncpy(msg + 6, songName, nameSize);

	send(sock, msg, nameSize + UP_SONG_SIZE,0);

	permit = Recive_Permit();

	if( permit == 0 ){

		printf("The server do not permit your song, Please try later");

	}

	else {

		while (songSize > 0 ){

			fscanf(song, "%1024c", UploadSong);
			send(sock, UploadSong, BUFFER_SIZE,0);
			songSize -= 1024;

		}

	}


}

uint8_t Recive_Permit(){

	struct timeval tv;
	int retval;
	fd_set rfds;

	FD_ZERO(&rfds);
	FD_SET(sock, &rfds);

	tv.tv_sec = 5;
	tv.tv_usec = 300;

	retval = select(sock + 1, &rfds, NULL, NULL, &tv);

	/* Don't rely on the value of tv now! */
	if (retval == -1)

		perror("Timeout_Occur error");

	else if ( retval == 0 ){

		perror("2Timeout occur at Ask_song");
		Close_All();
		exit(EXIT_FAILURE);

	} else {

		recv(sock, msg, BUFFER_SIZE, 0);

		if ( msg[0] == NEW_STATION_TYPE ){

			num_of_stations = htons((uint16_t *) (msg + 1));

			printf("New station is available (corrent number of stations: %d)\n", num_of_stations);

			retval = select(sock + 1, &rfds, NULL, NULL, &tv);

		}

		if ( msg[0] == INVALID_TYPE ){

			perror("Invalid type of massege at AskSong\n");
			Close_All();
			exit(EXIT_FAILURE);

		}

		if ( msg[0] == PERMIT_TYPE || (msg[0] == NEW_STATION_TYPE && retval > 0)){

			if ( msg[0] == NEW_STATION_TYPE && retval > 0 ){

				recv(sock, msg, PERMIT_SIZE, 0);

				if ( msg[0] != PERMIT_TYPE){

					perror("Invalid type of massege at AskSong\n");
					Close_All();
					exit(EXIT_FAILURE);

				}

			}

		}

		if ( msg[0] != UP_SONG_TYPE && msg[0] != INVALID_TYPE && msg[0] != NEW_STATION_TYPE ){

			perror("Invalid type of massege at AskSong\n");
			Close_All();
			exit(EXIT_FAILURE);

		}

	}return msg[1];
}

int main(int argc, char * argv[]){

	char input;

	struct timeval tv;

	fd_set rfds;

	int retval;

	tcp_ip = argv[1];

	tcp_port = atoi(argv[2]);

	Start_Connection();

	Send_Hello();



	if(Timeout_Occur(0,300)){

		perror("The client don't get welcome response & Timeout_Occur");

		close(sock);

	}

	Get_Welcome_Msg();

	Connect_To_Station(0);

	while(TRUE){

		print_menu();

		input = getchar();

		clean_buffer;

		switch(input){

		case 'n':

			Ask_Song();

			break;

		case 's':

			Up_Song();

			break;

		case 'q':

			Close_All();
			printf("You have chosen to quit, Bye Bye..\n");
			exit(EXIT_SUCCESS);

			break;

		default:
			printf("Wrong input, Bye Bye..\n");
			Close_All();
			exit(EXIT_FAILURE);

		}
		tv.tv_sec = 0;
		tv.tv_usec = 300;

		FD_ZERO(&rfds);
		FD_SET(sock, &rfds);


		retval = select(sock + 1, &rfds, NULL, NULL, &tv);

		/* Don't rely on the value of tv now! */
		if (retval == -1)

			perror("Timeout_Occur error");

		if ( retval != 0 ){

			recv(sock, msg, BUFFER_SIZE, 0);

			if ( msg[0] == NEW_STATION_TYPE ){

				num_of_stations = htons((uint16_t *) (msg + 1));

				printf("New station is available (corrent number of stations: %d)\n", num_of_stations);

				retval = select(sock + 1, &rfds, NULL, NULL, &tv);

			} else {

				perror("Invalid type of massage at main\n");
				Close_All();
				exit(EXIT_FAILURE);

			}
		}
	}
}


#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include "../../dryos.h"
#include "../sockets.h"
#include "../dumbcraft/dumbcraft.h"

extern void uart_printf(const char *fmt, ...);

//canon has a  socket descriptor 'translator' which converts the ICU descriptors into the ones which the LIME core can work from
extern int socket_convertfd(int sockfd);

//all of these has a translator for the  socket descriptor
extern int socket_create(int domain, int type, int protocol);
extern int socket_bind(int sockfd, void *addr, int addrlen);
extern int socket_listen(int sockfd, int backlogl);
extern int socket_accept(int sockfd, void *addr, int addrlen);
extern int socket_connect(int sockfd, void *addr, int addrlen);
extern int socket_recv(int sockfd, void *buf, int len, int flags);
extern int socket_send(int sockfd, void *buf, int len, int flags);
extern int socket_shutdown(int sockfd, int flag);
extern int socket_setsockopt(int socket, int level, int option_name, const void *option_value, int option_len);
//i was not able to find the stubs for these two functions with the translator.
extern int socket_select_caller(int convertedsock, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
extern void socket_close_caller(int sockfd);

static int socket_select(int sockfd, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
	int converted = socket_convertfd(sockfd);
	return socket_select_caller(converted, readfds, writefds, exceptfds, timeout);
}
static int socket_close(int sockfd)
{
	int converted = socket_convertfd(sockfd);
	socket_close_caller(converted);
}

void hexDumps(char *desc, void *addr, int len)
{
	int i;
	unsigned char buff[17];
	unsigned char *pc = (unsigned char *)addr;

	// Output description if given.
	if (desc != NULL)
		uart_printf("%s:\n", desc);

	// Process every byte in the data.
	for (i = 0; i < len; i++)
	{
		// Multiple of 16 means new line (with line offset).

		if ((i % 16) == 0)
		{
			// Just don't print ASCII for the zeroth line.
			if (i != 0)
				uart_printf("  %s\n", buff);

			// Output the offset.
			uart_printf("  %x ", i);
		}

		// Now the hex code for the specific character.
		uart_printf(" %x", pc[i]);

		// And store a printable ASCII character for later.
		if ((pc[i] < 0x20) || (pc[i] > 0x7e))
		{
			buff[i % 16] = '.';
		}
		else
		{
			buff[i % 16] = pc[i];
		}

		buff[(i % 16) + 1] = '\0';
	}

	// Pad out last line if not exactly 16 characters.
	while ((i % 16) != 0)
	{
		uart_printf("   ");
		i++;
	}

	// And print the final ASCII bit.
	uart_printf("  %s\n", buff);
}
//MUST be a power of two.
#define OUTCIRCBUFFSIZE 512

static int32_t playersockets[MAX_PLAYERS];
static uint8_t server_sockaddr[8] = {0x0, 0x1, 0x63, 0xdd, 0, 0, 0, 0};
static uint8_t sendbuffer[16384];
static uint16_t sendptr;
static uint8_t sendplayer;
static uint32_t serversocket;
static uint8_t outcirc[OUTCIRCBUFFSIZE];
static uint16_t outcirchead = 0;
static uint8_t is_in_outcirc;

void StartupBroadcast() { is_in_outcirc = 1; }
void DoneBroadcast() { is_in_outcirc = 0; }
void PushByte(uint8_t byte);
void extSbyte(uint8_t b)
{
	if (is_in_outcirc)
	{
		//		uart_printf( "Broadcast: %02x (%c)\n", b, b );
		outcirc[outcirchead & (OUTCIRCBUFFSIZE - 1)] = b;
		outcirchead++;
	}
	else
	{
		PushByte(b);
	}
}

uint16_t GetRoomLeft()
{
	return 512 - sendptr;
}

uint16_t GetCurrentCircHead()
{
	return outcirchead;
}

uint8_t UnloadCircularBufferOnThisClient(uint16_t *whence)
{
	uint16_t i16;
	uint16_t w = *whence;

	i16 = GetRoomLeft();
	while (w != outcirchead && i16)
	{
		//		uart_printf( "." );
		PushByte(outcirc[(w++) & (OUTCIRCBUFFSIZE - 1)]);
		i16--;
	}

	*whence = w;

	if (!i16)
		return 1;
	else
		return 0;
}

void SendData(uint8_t playerno, unsigned char *data, uint16_t packetsize)
{
	int sock = playersockets[playerno];
	int r;
	if (sock >= 0)
	{
		r = socket_send(sock, data, packetsize, 0);
		if (r != packetsize)
		{
			uart_printf("Error: could not socket_send (%d) code %d (%p %d)\n", sock, r, data, packetsize);
			ForcePlayerClose(playerno, 's');
		}
	}
}

void SendStart(uint8_t playerno)
{
	sendplayer = playerno;
	sendptr = 0;
}

void PushByte(uint8_t byte)
{
	sendbuffer[sendptr++] = byte;
}

void EndSend()
{
	if (sendptr != 0)
		SendData(sendplayer, sendbuffer, sendptr);
	//uart_printf( "\n" );
}

uint8_t CanSend(uint8_t playerno)
{
	return 1;
}

void ForcePlayerClose(uint8_t playerno, uint8_t reason)
{
	RemovePlayer(playerno);
	socket_shutdown(playersockets[playerno], 2);
	socket_close(playersockets[playerno]);
	playersockets[playerno] = -1;
}

static uint8_t readbuffer[65536];
static uint16_t readbufferpos = 0;
static uint16_t readbuffersize = 0;

uint8_t Rbyte()
{
	uint8_t rb = readbuffer[readbufferpos++];
	//	uart_printf( "[%02x] ", rb );
	return rb;
}

uint8_t CanRead()
{
	return readbufferpos < readbuffersize;
}
typedef struct my_fd_set
{
	uint32_t fds_bits[64];
} my_fd_set;

int my_FD_ISSET(unsigned long n, struct my_fd_set *p)
{
	int convert = socket_convertfd(n);
	uint32_t mask = 1 << (convert % 32);
	return p->fds_bits[convert / 32] & mask;
}

void my_FD_SET(unsigned long n, struct my_fd_set *p)
{
	int convert = socket_convertfd(n);
	uint32_t mask = 1 << (convert % 32);
	p->fds_bits[convert / 32] |= mask;
}
void my_FD_CLR(unsigned long n, struct my_fd_set *p)
{
	int convert = socket_convertfd(n);
	uint32_t mask = 1 << (convert % 32);
	p->fds_bits[convert / 32] &= ~mask;
}

void StartServer()
{
	uart_printf("starting minecraft server\n");
	int i;
	my_fd_set read_fd_set;

	struct linger lin;

	int lastticktime = 0;

	int max_sd;
	lin.l_onoff = 1;
	lin.l_linger = 0;

	for (i = 0; i < MAX_PLAYERS; i++)
		playersockets[i] = -1;
	serversocket = socket_create(1, 1, 0);
	uart_printf("Socket Value:%x", serversocket);
	if (serversocket < 0)
	{
		uart_printf("Failed to socket_create socket!\n");
		return;
	}
	int enable = 1;
	if (socket_setsockopt(serversocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
	{
		uart_printf("socket_setsockopt error\n");
		return;
	}

	if (socket_bind(serversocket, server_sockaddr, 0x8) < 0)
	{
		uart_printf("socket_bind error\n");
		return;
	}
	if (socket_listen(serversocket, 3) < 0)
	{
		uart_printf("socket_listen error\n");
		return;
	}

	//socket_setsockopt(serversocket, SOL_SOCKET, SO_LINGER|SO_REUSEADDR, &enable, sizeof(int));
	uart_printf("read_fd_set %d\n", &read_fd_set);

	InitDumbcraft();

	while (1)
	{

		//clear the socket set
		//FD_ZERO(&read_fd_set);
		memset(&read_fd_set, 0, sizeof(read_fd_set));

		// memset(&read_fd_set,0,0x100);
		//add master socket to set
		my_FD_SET(serversocket, &read_fd_set);
		max_sd = serversocket;

		//add child sockets to set
		for (i = 0; i < MAX_PLAYERS; i++)
		{
			//socket descriptor
			int sd = playersockets[i];

			//if valid socket descriptor then add to read list
			if (sd != -1)
			{
				my_FD_SET(sd, &read_fd_set);
			}

			//highest file descriptor number, need it for the select function
			if (sd > max_sd)
			{
				max_sd = sd;
			}
		}

		/* Service all the sockets with input pending. */
		int rc;
		//		double thistime = OGGetAbsoluteTime();
		struct timeval tv;
		//gettimeofday(&tv, 0);

		//rc = poll(fds, 1, 0);

		struct timeval timeout = {0, 0};

		//uart_printf("Reverse : %x %x\n",reverse,serversocket);

		int tss = socket_select(max_sd + 1, &read_fd_set, 0, 0, &timeout);
		if (tss < 0)
		{

			hexDumps("sturctC", &read_fd_set, 256);

			uart_printf("problem %x \n", tss);
		}
		if (my_FD_ISSET(serversocket, &read_fd_set))
		{
			int foundsocketspot;
			int len = 8;
			char clientaddr[8];
			int clientsocket = socket_accept(serversocket, &clientaddr, &len);
			uart_printf("client socket : 0x%x", clientsocket);
			hexDumps("Client Addr:", clientaddr, 8);
			uart_printf("Clientsock: %d\n", clientsocket);

			if (clientsocket > 0)
			{
				for (i = 0; i < MAX_PLAYERS; i++)
				{
					if (playersockets[i] < 0)
					{
						foundsocketspot = i;
						break;
					}
				}
				if (foundsocketspot != MAX_PLAYERS)
				{
					playersockets[foundsocketspot] = clientsocket;
					my_FD_SET(clientsocket, &read_fd_set);
					AddPlayer(foundsocketspot);
				}
				else
				{
					uart_printf("Connection closed?\n");
					my_FD_CLR(clientsocket, &read_fd_set);
					socket_close(clientsocket);
				}
			}
			else
			{
				uart_printf("Accept fail %x\n", clientsocket);
			}
			continue;
		}

		for (i = 0; i < MAX_PLAYERS; i++)
		{
			int rec;
			int sock = playersockets[i];
			if (sock < 0)
			{
				continue;
			}
			rc = socket_select(max_sd + 1, &read_fd_set, NULL, NULL, &timeout);
			//uart_printf("rc: %d\n", rc);
			if (rc < 0)
			{
				uart_printf("remvoing player\n");
				my_FD_CLR(playersockets[i], &read_fd_set);
				ForcePlayerClose(i, 'f');
				continue;
			}

			else if (rc == 0)
			{

				continue;
			}

			if (my_FD_ISSET(sock, &read_fd_set))
			{
				rec = socket_recv(sock, readbuffer, 65535, 0);

				if (rec == 0)
				{
					uart_printf("remvoing player 1\n");
					my_FD_CLR(playersockets[i], &read_fd_set);

					ForcePlayerClose(i, 'r');
					continue;
				}
				readbufferpos = 0;
				readbuffersize = rec;

				GotData(i);
			}
		}

		UpdateServer();

		if (((tv.tv_usec - lastticktime + 1000000) % 1000000) > 100000)
		{

			lastticktime = tv.tv_usec;
		}
		TickServer();
		msleep(100);
	}
	return;
}

void StopServer()
{
	for (int i = 0; i < MAX_PLAYERS; i++)
	{

		if (playersockets[i] > 0)
		{
			ForcePlayerClose(i, 'r');
			uart_printf("bye! %d\n", i);
		}
	}
	socket_shutdown(serversocket, 2);
	socket_close(serversocket);
	uart_printf("Server Closed!\n");
}

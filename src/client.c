/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/**************************************************************/
/* ------------------- type declarations -------------------- */
/**************************************************************/
/**
 * Operation mode of the client
 */
enum ClientMode
{
    SINGLE  = 1,    /**< client executes one command given in cmd line argument */
    MANUAL  = 2     /**< client awaits command from standard input */
};

/**************************************************************/
/* ------------------- symbolic constants ------------------- */
/**************************************************************/

#define READ_BUF_SIZE       256
#define WRITE_BUF_SIZE      256

#define PROMT               "@ "

/**************************************************************/
/* ------------------- module local variables --------------- */
/**************************************************************/

enum ClientMode clientMode = MANUAL;
char* serverAddress;
uint16_t serverPort;
char cmd[WRITE_BUF_SIZE];

/**************************************************************/
/* ------------------- function prototypes ------------------ */
/**************************************************************/

static int connectToServer( void );
static int readSocket( int sock, char* buf );
static void writeSocket( int sock, char* buf );
static void processCmdLineOpts( int argc, char** argv );
static void singleMode( void );
static void manualMode( void );

/**************************************************************/
/* ------------------- local functions ---------------------- */
/**************************************************************/

/**
 * @brief Connects to remote host using address information from the command line args
 *
 * Program is terminated if the connection cannot be established
 *
 * @return created socket
 */
static int connectToServer( void )
{
    int sock;
    struct sockaddr_in servername;
 
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror ("socket (client)");
        exit (EXIT_FAILURE);
    }

    struct hostent *hostinfo;

    servername.sin_family = AF_INET;
    servername.sin_port = htons(serverPort);
    hostinfo = gethostbyname(serverAddress);
    if (hostinfo == NULL)
    {
        fprintf(stderr, "Unknown host %s\n", serverAddress);
        exit(EXIT_FAILURE);
    }
    servername.sin_addr = *(struct in_addr *) hostinfo->h_addr;
    
    if (connect(sock, (struct sockaddr *) &servername, sizeof(servername)) < 0)
    {
        perror ("connect");
        exit(EXIT_FAILURE);
    }
    
    return sock;
}

/**
 * @brief Reading data from the socket
 *
 * Program is terminated if the read fails
 *
 * @param[in] sock socket to be read from
 * @param[in] buf buffer to store data
 * @return 0 if all data have been read from the socket
 *         1 if the client closed the connection
 */
static int readSocket( int sock, char* buf )
{
    int nbytes;
    
    /* Data read. */
    nbytes = read(sock, buf, READ_BUF_SIZE);
    if (nbytes < 0)
    {
        /* Read error. */
        perror("read socket");
        exit(EXIT_FAILURE);
    }
    /* EOF (remote host closed the connection) */
    else if (nbytes == 0)
    {
        return -1;
    }
    else
    {
        buf[nbytes] = '\0';
        return 0;
    }
}

/**
 * @brief Writing data to the socket
 *
 * Program is terminated if the write fails
 *
 * @param[in] sock socket to write
 * @param[in] buf data buffer to be written to the socket
 * @return none
 */
static void writeSocket( int sock, char* buf )
{
    int nbytes;

    nbytes = write(sock, buf, strlen(buf));
    if (nbytes < 0)
    {
        perror("write socket");
        exit (EXIT_FAILURE);
    }
}

/**
 * @brief Processes the input parameters of the main() function
 *
 * Mandatory
 * ---------
 *  -a address   : server address
 *  -p port      : server port
 *
 * Optional
 * ---------
 *  -c "command" : client connects to the server, executes the command and terminates (SINGLE mode)
 *  -m           : client accepts commands from stdin (MANUAL mode)
 *
 * Optional arguments are mutually exclusive, only one can be used at the same time.
 * If multiple optional arguments found, the client terminates.
 * The DEFAULT mode is MANUAL (none of the optional arguments has been provided)
 *
 * @param[in] argc nr of arguments
 * @param[in] argv arg array
 * @return none
 */
static void processCmdLineOpts( int argc, char** argv )
{
    int opt;
    uint8_t aFlag = 0;
    uint8_t pFlag = 0;
    uint8_t cFlag = 0;
    uint8_t mFlag = 0;

    while ((opt = getopt(argc, argv, "a:p:c:m")) != -1)
    {
        switch(opt)
        {
            case 'a':
            {
                serverAddress = (char*)malloc(strlen(optarg));
                sprintf(serverAddress, "%s", optarg);
                aFlag = 1;
                break;
            }
            
            case 'p':
            {
                long int port = strtol(optarg, NULL, 0);
                /* restrict arg to usable port range */
                if ((port < 1024) || (port > UINT16_MAX))
                {
                    fprintf(stderr, "Invalid port %ld\n", port);
                    exit(EXIT_FAILURE);
                }
                else
                {
                    serverPort = port;
                }
                pFlag = 1;
                break;
            }

            case 'c':
            {
                if (mFlag)
                {
                    fprintf(stderr, "-c and -m options can't be used together\n");
                    exit(EXIT_FAILURE);
                }
                clientMode = SINGLE;
                sprintf(cmd, "%s", optarg);
                cFlag = 1;
                break;
            }
            case 'm':
            {
                if (cFlag)
                {
                    fprintf(stderr, "-c and -m options can't be used together\n");
                    exit(EXIT_FAILURE);
                }
                clientMode = MANUAL;
                break;
            }
            /* unknown option or missing argument */
            case '?':
                exit(EXIT_FAILURE);
                break;
                
            default:
                break;
        }
    }
    
    /* check missing options */
    if (!aFlag)
    {
        fprintf(stderr, "Server address is missing (-a addr)\n");
        exit(EXIT_FAILURE);
    }
    if (!pFlag)
    {
        fprintf(stderr, "Server Port is missing (-p port)\n");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Connects the client sends the command given as cmd line arg and closes the connection
 *
 * @return none
 */
static void singleMode( void )
{
    /* Connect to the server. */
    int sock = connectToServer();
    
    /* Send data to the server, command is already in the cmd buffer */   
    writeSocket(sock, cmd);
    
    char readBuffer[READ_BUF_SIZE];

    /* read server response */
    if (readSocket(sock, readBuffer) < 0)
    {
        close(sock);
        return;
    }
    
    fprintf(stdout, "SERVER: %s", readBuffer);
}

/**
 * @brief Connects the client and waits for input
 *
 * @return none
 */
static void manualMode( void )
{
    /* Connect to the server. */
    int sock = connectToServer();

    char readBuffer[READ_BUF_SIZE];
    char writeBuffer[WRITE_BUF_SIZE];
    char *input = NULL;
    size_t len;

    for (;;)
    {
        fprintf(stdout, PROMT);
        getline(&input, &len, stdin);
        sprintf(writeBuffer, "%s", input);
        
        /* Send data to the server. */
        writeSocket(sock, writeBuffer);
        
        /* read server response */
        if (readSocket(sock, readBuffer) < 0)
        {
            break;
        }
        
        fprintf(stdout, "> %s", readBuffer);
        free(input);
    }
    
    close(sock);
}

int main( int argc, char** argv )
{
    processCmdLineOpts(argc, argv);
    
    switch(clientMode)
    {
        case SINGLE:
        singleMode();
        break;
        
        case MANUAL:
        manualMode();
        break;
        
        default:
        break;
    }

    exit(EXIT_SUCCESS);
}
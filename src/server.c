#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "protocol.h"
#include "keyregistry.h"

/**************************************************************/
/* ------------------- symbolic constants ------------------- */
/**************************************************************/

#define DEFAULT_PORT        5555
#define READ_BUF_SIZE       256
#define WRITE_BUF_SIZE      256
#define DEFAULT_REGISTRY    "capitals.txt"

/**************************************************************/
/* ------------------- module local variables --------------- */
/**************************************************************/

static uint16_t listeningPort;
static fd_set active_fd_set, read_fd_set;
static char sendBuf[WRITE_BUF_SIZE];
static char* keyRegistryFileName;

/**************************************************************/
/* ------------------- function prototypes ------------------ */
/**************************************************************/

static void setDefaultPort( void );
static void setDefaultRegistryFile( void );
static void createErrMsgToClient( int sock, const char* key, uint8_t kregErr, uint16_t errPos );
static void processClientMessage( int sock, char* message );
static void processCmdLineOpts( int nrOfArgs, char** args );
static int createSocket( void );
static int readSocket( int sock );
static void addClient( int sock, struct sockaddr_in* client );
static void removeClient( int sock );
static void serverTask( void );

/**************************************************************/
/* ------------------- local functions ---------------------- */
/**************************************************************/

static void setDefaultPort( void )
{
    listeningPort = DEFAULT_PORT;
}

static void setDefaultRegistryFile( void )
{
    keyRegistryFileName = (char*)malloc(strlen(DEFAULT_REGISTRY));
    sprintf(keyRegistryFileName, "%s", DEFAULT_REGISTRY);
}

/**
 * @brief Prepares server error message to a client
 *
 * When a client retreives (get) or saves (put) a key to the Keyregistry,
 * it returns error each time when the given string cannot be parsed.
 * This information is sent back to the client.
 *
 * @param[in] sock descriptor of the client
 * @param[in] key requested key by the client
 * @param[in] kregErr error code reported by the key registry module
 * @param[in] errPos character position where the error was detected
 * @return none
 */
static void createErrMsgToClient( int sock, const char* key, uint8_t kregErr, uint16_t errPos )
{
    switch(kregErr)
    {
        case KREG_KEY_EMPTY:
            sprintf(sendBuf, "Key has not been provided\n");
            break;
        case KREG_KEY_INVALID:
            sprintf(sendBuf, "Key is invalid ... keys can contain digits and letters only\n");
            break;
        case KREG_KEY_TOO_LONG:
            sprintf(sendBuf, "Key is too long ... max key length is %d\n", KREG_MAX_KEY_LEN);
            break;
        case KREG_KEY_NOT_FOUND:
            sprintf(sendBuf, "Key [%s] not found in regisry\n", key);
            break;
        case KREG_KEY_EXISTS:
            sprintf(sendBuf, "Key [%s] already exists, updating keys are not allowed\n", key);
            break;
        case KREG_VAL_TOO_LONG:
            sprintf(sendBuf, "Value is too long ... max value length is %d\n", KREG_MAX_VAL_LEN);
            break;
        default:
            sprintf(sendBuf, "Server Error\n");
            break;
    }
}

/**
 * @brief Processes client requests and send response back to the client.
 *
 * This function processes 3 different client commands:
 *   GET key - the server queries the key's value from the keyregistry
 *   PUT key value - the server saves the KVP in the keyregistry
 *   bye - the server disconnects the client
 * Parsing the key and the value is done by the keyregistry, this function just simply
 * passes the string (without the command).
 * After the command is executed, positive or negative response (error message)
 * is sent back to the client.
 * The message MUST start with the command, or the server won't be able to process it.
 *
 * @param[in] sock client
 * @param[in] message the whole message received from the client
 * @return none
 */
static void processClientMessage( int sock, char* message )
{
    size_t messageLen = strlen(message);
    
    /* commands treated as not case sensitive (assuming that the first 3 char is the command) */
    if (messageLen >= 3)
    {
        for (int i = 0; i < 3; i++)
        {
            message[i] = tolower(message[i]);
        }
    }

    /* handle PUT key request */
    if (strncmp("put", message, 3) == 0)
    {
        char* key = NULL;
        char* value = NULL;
        uint16_t errPos;
        uint8_t retVal;
        
        if ((retVal = KREG_PutKey(message + 3, &key, &value, &errPos)) == KREG_OK)
        {
            sprintf(sendBuf, "[%s] <= [%s]\n", key, value);
        }
        else
        {
            createErrMsgToClient(sock, key, retVal, errPos);
        }
    }
    /* handle GET key request */
    else if (strncmp("get", message, 3) == 0)
    {
        char* key = NULL;
        char* value = NULL;
        uint16_t errPos;
        uint8_t retVal;
        
        if ((retVal = KREG_GetKey(message + 3, &key, &value, &errPos)) == KREG_OK)
        {
            sprintf(sendBuf, "[%s] => [%s]\n", key, value);
        }
        else
        {
            createErrMsgToClient(sock, key, retVal, errPos);
        }
    }
    /* handle disconnect request */
    else if (strncmp("bye", message, 3) == 0)
    {
        removeClient(sock);
        return;
    }
    else
    {
        sprintf(sendBuf, "???\n");
    }
    
    write(sock, sendBuf, strlen(sendBuf));
}

/**
 * @brief Processes the input parameters of the main() function
 *
 * Usage: ./binary [-p PORTNUM] [-f filename]
 *
 * If the port number is out of range [1024..65535], the default
 * port number will be used.
 * if no argument is given, the default values will be used.
 *
 * @param[in] argc nr of arguments
 * @param[in] argv arg array
 * @return none
 */
static void processCmdLineOpts( int argc, char** argv )
{
    int opt;
    _Bool pFlag = false;
    _Bool fFlag = false;

    while ((opt = getopt(argc, argv, "p:f:")) != -1)
    {
        switch(opt)
        {
            case 'p':
            {
                long int port = strtol(optarg, NULL, 0);
                /* restrict arg to usable port range */
                if ((port < 1024) || (port > UINT16_MAX))
                {
                    listeningPort = DEFAULT_PORT;
                }
                else
                {
                    listeningPort = port;
                }
                pFlag = true;
                break;
            }
            
            case 'f':
            {
                keyRegistryFileName = (char*)malloc(strlen(optarg));
                sprintf(keyRegistryFileName, "%s", optarg);
                fFlag = true;
                break;
            }

            case '?':
                if (optopt == 'p')
                {
                    setDefaultPort();
                }
                else if (optopt == 'f')
                {
                    setDefaultRegistryFile();
                }
                else
                {
                    fprintf (stderr, "Unknown option character: 0x%X\n", optopt);
                }
                break;
                
            default:
                break;
        }
    }
    
    if (!pFlag)
    {
        setDefaultPort();
    }
    
    if (!fFlag)
    {
        setDefaultRegistryFile();
    }
}

/**
 * @brief Creates a socket for accepting client connections
 *
 * In case of any error, the program terminates, therefore the return value
 * is not checked for error outside of this function.
 *
 * @return socket descriptor
 */
static int createSocket( void )
{
    int sock;
    struct sockaddr_in name;

    /* Create the socket. */
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("create socket");
        exit(EXIT_FAILURE);
    }

    name.sin_family = AF_INET;
    name.sin_port = htons(listeningPort);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind (sock, (struct sockaddr*) &name, sizeof (name)) < 0)
    {
        perror("bind socket");
        exit(EXIT_FAILURE);
    }
    
    return sock;
}

/**
 * @brief Reading data from a client socket
 *
 * @param[in] sock socket to be read from
 * @return 0 if all data have been read from the socket
 *         1 if the client closed the connection
 */
static int readSocket( int sock )
{
    char buffer[READ_BUF_SIZE];
    int nbytes;
    
    /* Data read. */
    nbytes = read(sock, buffer, READ_BUF_SIZE);
    if (nbytes < 0)
    {
        /* Read error. */
        perror("read socket");
        exit(EXIT_FAILURE);
    }
    /* EOF (client closed the connection) */
    else if (nbytes == 0)
    {
        return -1;
    }
    else
    {
        /* process client request */
        buffer[nbytes] = '\0';
        processClientMessage(sock, buffer);
        return 0;
    }
}

/**
 * @brief Remove client socket from the descriptor set
 *
 * @param[in] sock socket to be removed
 * @return none
 */
static void removeClient( int sock )
{
    struct sockaddr_in client;
    socklen_t len = sizeof(client);
    
    /* get client address information to display */
    getpeername(sock, (struct sockaddr*)&client, &len);   
    fprintf(stdout, "* Client disconnected from host %s:%d\n", inet_ntoa (client.sin_addr), ntohs (client.sin_port));
    close(sock);
    FD_CLR(sock, &active_fd_set);
}

/**
 * @brief Add client socket to the descriptor set
 *
 * @param[in] sock socket to be added
 * @param[in] client client address information to display
 * @return none
 */
static void addClient( int sock, struct sockaddr_in* client )
{
    fprintf(stdout, "* Client connected from host %s:%d\n", inet_ntoa(client->sin_addr), ntohs(client->sin_port));
    FD_SET(sock, &active_fd_set);
}

/**
 * @brief Implements a non-blocking task to accept client connections and read data
 *
 * @return none
 */
static void serverTask( void )
{
    int sock;
    int i;

    /* Create the socket and set it up to accept connections. */
    sock = createSocket();
  
    if (listen(sock, 1) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    /* Server is started and ready to accept connections */
    fprintf(stdout, "* Server is started and listening on port %d\n", listeningPort);

    /* Initialize the set of active sockets. */
    FD_ZERO(&active_fd_set);
    FD_SET(sock, &active_fd_set);

    for (;;)
    {
        /* Block until input arrives on one or more active sockets. */
        read_fd_set = active_fd_set;
        if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0)
        {
          perror ("select");
          exit (EXIT_FAILURE);
        }

        /* Service all the sockets with input pending. */
        for (i = 0; i < FD_SETSIZE; ++i)
        {
            if (FD_ISSET (i, &read_fd_set))
            {
                if (i == sock)
                {
                    /* Connection request on original socket. */
                    int new;
                    struct sockaddr_in clientname;
                    size_t size = sizeof (clientname);
                    new = accept(sock, (struct sockaddr*) &clientname, (socklen_t*) &size);
                    if (new < 0)
                    {
                        perror ("accept");
                    }
                    addClient(new, &clientname);
                }
                else
                {
                    /* Data arriving on an already-connected socket. */
                    if (readSocket(i) < 0)
                    {
                        removeClient(i);
                    }
                }
            }
        }
    }
}

int main( int argc, char** argv )
{
    uint16_t lineNr = 0;
    uint16_t colNr = 0;

    processCmdLineOpts(argc, argv);
    
    uint8_t res = KREG_ReadRegistryFile(keyRegistryFileName, &lineNr, &colNr);
    
    switch(res)
    {
        case KREG_OK:
            fprintf(stdout, "* KVP Registry has been loaded\n");
            break;
            
        case KREG_ERR_REG_OPEN:
            fprintf(stderr, "Can't open %s\n", keyRegistryFileName);
            exit(EXIT_FAILURE);
            break;
            
        case KREG_KEY_EMPTY:
            fprintf(stderr, "Missing key at [%d,%d]\n", lineNr, colNr);
            exit(EXIT_FAILURE);
            break;

        case KREG_KEY_INVALID:
            fprintf(stderr, "Invalid character found at [%d,%d]\n", lineNr, colNr);
            exit(EXIT_FAILURE);
            break;
            
        case KREG_KEY_TOO_LONG:
            fprintf(stderr, "Long key found at [%d,%d]\n", lineNr, colNr);
            exit(EXIT_FAILURE);
            break;
            
        case KREG_VAL_TOO_LONG:
            fprintf(stderr, "Long value found at [%d,%d]\n", lineNr, colNr);
            exit(EXIT_FAILURE);
            break;

        /* defensive block */
        default:
            fprintf(stderr, "FATAL ERROR\n");
            exit(EXIT_FAILURE);
            break;
        
    }

    /* start listening, accepting connections and data */
    serverTask();
}
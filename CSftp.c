#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include "dir.h"
#include "usage.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
 
#include <string.h>    //strlen
#include <unistd.h>    //write
#include <time.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>


#define BACKLOG 10 // max amount of pending connections

// makes string uppercase
void makeupper(char* base) {
    char *baseUC = strtok(base,":");
    char *s = baseUC;
    while (*s) {
        *s = toupper((unsigned char) *s);
        s++;
    }
}

// returns 0 if base starts with str
int startsWith (char* base, char* str) {
    return (strstr(base, str) - base);
}

 
int main(int argc , char *argv[])
{
    int server_sock;
    int client_sock;
    int pasv_sock;
    int c;
    int read_size;
    int msg_size;
    int loggedIn = 0;
    int bufsize = 1024;

    struct sockaddr_in server;
    struct sockaddr_in client;

    char client_message[msg_size];
    char *port = argv[1];
    char buf_begin[1024];
    char buf[bufsize];
    char* cwd_begin = getcwd(buf_begin, bufsize);
    char* cwd = cwd_begin;

    u_short port_num = atoi(port);
     
    // make socket
    server_sock = socket(AF_INET , SOCK_STREAM , 0);
    if (server_sock == -1) 
    {
        printf("Could not create socket\n");
    }
     
    // prepare sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port_num);

     
    // bind
    if( bind(server_sock, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        // error message 
        perror("bind failed. Error");
        return 1;
    }
    
    while(1){
        // listen
        listen(server_sock , BACKLOG);
        puts("Ready for connection...");
        
         
        // accept connection
        c = sizeof(struct sockaddr_in);
        client_sock = accept(server_sock, (struct sockaddr *)&client, (socklen_t*)&c);
        if (client_sock < 0)
        {
            // error message
            perror("accept failed");
            return 1;
        }
        puts("Connection accepted!");
        write(client_sock , "220 Service ready for new user.\n", 32);
         
        // receive a message
        read_size = read(client_sock, client_message, 2000);

        char* command;
        char* commandUC; //UC = UpperCase
        char* string;
        char* parameter;
        char* directory = "/";
        int test = 99999;
        // Communicate with current client
        while( read_size > 0 )
        {   
            string = client_message;

            // GET THE COMMAND
            command = strsep(&string, " ");
            command = strsep(&command, "\n");
            command = strsep(&command, "\0");
            command = strsep(&command, "\r");

            makeupper(command);

            puts(command);

            // USER
            if(strcmp(command, "USER") == 0){

                // GET THE PARAMETER
                parameter = strsep(&string, " ");
                parameter = strsep(&parameter, "\n");
                parameter = strsep(&parameter, "\0");
                parameter = strsep(&parameter, "\r");
                puts(parameter); //for debugging
                if(parameter != NULL){
                    if(strcmp(parameter, "cs317") == 0){

                        // IF ALREADY LOGGED IN
                        if(loggedIn == 1){
                            puts("Already logged in!");
                            write(client_sock , "230, Already logged in!\n", 24);
                        } else {
                            puts("Logged in successfully!");
                            write(client_sock , "230, Logged in successfully\n", 28);
                        }
                        loggedIn = 1;
                        
                    // UNSUCCESFUL LOGIN
                    } else {
                        write(client_sock, "530 Not logged in.\n", 19);
                        loggedIn = 0;
                    }
                } else {
                    write(client_sock, "530 Not logged in.\n", 19);
                    loggedIn = 0;
                }
                fflush(stdout);
                
            } else

            // QUIT
            if(strncmp(client_message, "QUIT", 4) == 0){
                puts("221, Client disconnected");
                loggedIn = 0;
                write(client_sock, "221 Service closing control connection. Logged out if appropriate.\n", 67);
                
                close(client_sock);
                fflush(stdout);
                break;
            } else

            // Commands that require authorization

            // CWD
            if(strcmp(command, "CWD") == 0){
                if (loggedIn){

                    // GET THE PARAMETER
                    parameter = strsep(&string, " ");
                    parameter = strsep(&parameter, "\n");
                    parameter = strsep(&parameter, "\0");
                    parameter = strsep(&parameter, "\r");
                    directory = parameter;

                    char* ds = "./";
                    int startswithds = startsWith(parameter, ds); //if == 0 then it starts with "./"

                    char* invalid;
                    invalid = strstr (directory, ".."); //if directory contains, invalid is not null

                    // CHECK IF CLIENT DOES NOT TRY TO ACCESS UNAUTHORIZED SPACE/DIRECTORIES
                    if(invalid==NULL && startswithds!=0){

                        // PERFORM DIRECTORY CHANGE AND SEE IF IT IS SUCCESSFUL
                        if(chdir(directory)==0){
                            cwd = getcwd(buf, bufsize);
                            write(client_sock, "250 Requested file action okay, completed.\n", 43);
                        } else {
                            write(client_sock, "550 Access denied.\n", 19);
                        }

                    // UNSUCCESFUL DIRECTORY CHANGE
                    }else{
                        write(client_sock, "550 Failed to change directory.\n", 32);
                    }
                } else {
                    write(client_sock, "530 Not logged in.\n", 19);
                }
            } else 

            // CDUP
            if(strcmp(command, "CDUP") == 0){
                if (loggedIn){
                    if(strcmp(cwd_begin, cwd) != 0){ // CHECK IF CLIENT DOES NOT TRY TO ACCESS UNAUTHORIZED SPACE
                        if(chdir("../") == 0) {
                            cwd = getcwd(buf, bufsize);
                            write(client_sock, "250 Requested file action okay, completed.\n", 43);
                        } else {
                            write(client_sock, "550 Failed to change directory.\n", 32);
                        }
                    } else {

                    // CLIENT TRIED TO ACCESS UNAUTHORIZED SPACE
                     //   if (strcmp(cwd_begin, cwd) == 0){
                            write(client_sock, "550 Access denied.\n", 19);
                     //   } else {
                            //write(client_sock, "550 Failed to change directory.\n", 32);
                      //  }
                    }
                // UNSUCCESSFUL OPERATION
                } else {
                    write(client_sock, "530 Not logged in.\n", 19);
                }
            } else 

            // TYPE
            if(strcmp(command, "TYPE") == 0){
                if(loggedIn){

                    // GET THE PARAMETER
                    parameter = strsep(&string, " ");
                    if(parameter != NULL){

                        // MAKE PARAMETER VALID
                        parameter = strsep(&parameter, "\n");
                        parameter = strsep(&parameter, "\0");
                        parameter = strsep(&parameter, "\r");
                        makeupper(parameter);
                        // if parameter is A or I
                        if (strcmp(parameter, "A") == 0 || strcmp(parameter, "I") == 0){ 
                            write(client_sock , "200 Command okay.\n", 18);
                        } else  {
                            write(client_sock, "500 Syntax error, command unrecognized\n", 39);
                        }
                    } else {
                        write(client_sock, "501 Syntax error in parameters or arguments.\n", 45);
                    }
                } else {
                    write(client_sock, "530 Not logged in.\n", 19);
                }
            } else

            // MODE
            if(strcmp(command, "MODE") == 0){
                if(loggedIn){

                    // GET THE PARAMETER
                    parameter = strsep(&string, " ");
                    if(parameter != NULL){
                        // MAKE PARAMETER VALID
                        parameter = strsep(&parameter, "\n");
                        parameter = strsep(&parameter, "\0");
                        parameter = strsep(&parameter, "\r");
                        makeupper(parameter);
                        if (strcmp(parameter, "S") == 0){ // if parameter is S
                            write(client_sock , "200 Command okay.\n", 18);
                        } else  {
                            write(client_sock, "500 Syntax error, command unrecognized\n", 39);
                        }
                    } else {
                        write(client_sock, "501 Syntax error in parameters or arguments.\n", 45);
                    }
                } else {
                    write(client_sock, "530 Not logged in.\n", 19);
                }     
            } else

            // STRU
            if(strcmp(command, "STRU") == 0){
                if(loggedIn){
                    parameter = strsep(&string, " ");
                    if(parameter != NULL){
                        parameter = strsep(&parameter, "\n");
                        parameter = strsep(&parameter, "\0");
                        parameter = strsep(&parameter, "\r");
                        makeupper(parameter);
                        if (strcmp(parameter, "F") == 0){ // if parameter is F
                            write(client_sock , "200 Command okay.\n", 18);
                        } else  {
                            write(client_sock, "500 Syntax error, command unrecognized\n", 39);
                        }
                    } else {
                        write(client_sock, "501 Syntax error in parameters or arguments.\n", 45);
                    }
                } else {
                    write(client_sock, "530 Not logged in.\n", 19);
                }
            } else

            // RETR
            if(strcmp(command, "RETR") == 0){
                if(loggedIn){
                    // parse the input filepath
                    char* filepath;
                    parameter = strsep(&string, " ");
                    parameter = strsep(&parameter, "\n");
                    parameter = strsep(&parameter, "\0");
                    parameter = strsep(&parameter, "\r");
                    filepath = parameter;

                    // make sure filepath is accessible and can be opened (for reading)
                    int aces = access(filepath,R_OK);
                    int fd = open(filepath,O_RDONLY);
                    if(aces == 0 && fd != -1){

                        struct stat statbuf;
                        off_t offset = 0;
                        int sent = 0;
                        fstat(fd, &statbuf); // puts fd statistics into memory

                        // accept a passive connection request
                        int accepted = accept(pasv_sock, (struct sockaddr*)&client, (socklen_t*)&c);
                        close(pasv_sock);
                        write(client_sock, "150 File status okay; about to open data connection.\n", 53);

                        // send file fd to accepted connection fd
                        sent = sendfile(accepted, fd, &offset, statbuf.st_size);

                        // replies ...
                        if(sent){
                            write(client_sock , "226 Closing data connection. Requested file action successful\n", 62);
                        } else {
                            write(client_sock , "550 Requested action not taken. File unavailable\n", 49);
                        }
                        close(accepted);

                    } else {
                        write(client_sock , "550 Requested action not taken. File unavailable\n", 49);
                    }
                    close(fd);
                } else {
                    write(client_sock, "530 Not logged in.\n", 19);
                }
            } else

                // PASV
            if(strncmp(client_message, "PASV", 4) == 0){
                if(loggedIn){
                    // find client ip address & generate random port number
                    int ip[4];
                    srand(time(NULL));
                    int portA = 128 + rand() % 64;
                    int portB = rand() % 0xff;
                    int pasvport  = (portA*256) + portB; //Port number!!
                    struct sockaddr_in addr;
                    getsockname(client_sock, (struct sockaddr*)&addr, (socklen_t*)&c);
                    char* host = inet_ntoa(addr.sin_addr);
                    sscanf(host,"%d.%d.%d.%d",&ip[0],&ip[1],&ip[2],&ip[3]); // extracts ip values from host

                    // make passive socket
                    server.sin_port = htons(pasvport);
                    pasv_sock = socket(AF_INET, SOCK_STREAM, 0);
                    if (pasv_sock<0){
                        puts("Could not create passive socket\n");
                    }

                    // bind passive socket
                    if (bind(pasv_sock, (struct sockaddr*)&server, sizeof(server)) < 0){
                        // error message 
                        perror("bind failed. Error");
                        return 1;
                    }

                    //listen and wait
                    listen(pasv_sock, BACKLOG);

                    // form return message with 
                    char *response = "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d).\n";
                    char buff[48];
                    sprintf(buff, response, ip[0], ip[1], ip[2], ip[3], portA, portB);
                    write(client_sock, buff, sizeof(buff));
                } else {
                    write(client_sock, "530 Not logged in.\n", 19);
                }
            } else

                // NLST
            if(strcmp(command, "NLST") == 0 || strcmp(command, "LIST") == 0){ //Added OR LIST for testing
                if(loggedIn){
                    // accept passive connection request
                    int accepted = accept(pasv_sock, (struct sockaddr*)&client, (socklen_t*)&c);
                    write(client_sock , "150 File status okay; about to open data connection.\n", 53);

                    // use dir.c function & react accordingly to its return value
                    int code = listFiles(accepted, cwd);
                    if(code == -1 || code == -2){
                        write(client_sock , "550 Requested action not taken. File unavailable\n", 49);
                    } else {
                        write(client_sock , "226 Closing data connection. Requested file action successful\n", 62);
                    }                  
                    //closing time
                    close(accepted);
                    close(pasv_sock);
                } else {
                    write(client_sock, "530 Not logged in.\n", 19);
                }
            // Arrive here if command not recognized
            } else {
                write(client_sock , "500 Syntax error, command unrecognized\n", 39);
            }
            // new client input
            read_size = read(client_sock, client_message, 2000);
            fflush(stdout);
        }

         //arrive here if read_size <= 0
        if(read_size == 0) // Client disconnects
        {
            puts("Client disconnected.");
            loggedIn = 0;
            fflush(stdout);
        }
        else if(read_size == -1) //read error
        {
            perror("read failed");
        }
    }
    return 0;
}


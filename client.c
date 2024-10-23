#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

const int MAX_SIZE=1024;
const char* client_server_fifo="client_server_fifo";
const char* server_client_fifo="server_client_fifo";

int main(){
    //creating the fifo that sends responses to the client
    if(access(server_client_fifo, F_OK)==-1){//if fifo does not exist
        if(mkfifo(server_client_fifo, 0666)==-1){
            perror("[Server] : Error creating server_client fifo.\n");
            exit(1);
        }
    }
    //creating the fifo that sends commands to the server
    if(access(client_server_fifo, F_OK)==-1){//if fifo does not exist
        if(mkfifo(client_server_fifo, 0666)==-1){
            perror("[Client] : Error trying to creat client-server fifo\n");
            exit(1);
        }
    }
    //opening the client-server fifo
    int write_fd=open(client_server_fifo, O_WRONLY);
    if(write_fd==-1){
        perror("[Client] : Error trying to open the client-server fifo\n");
        exit(1);
    }
    //opening the server-client fifo. 
    int read_fd=open(server_client_fifo, O_RDONLY);
    if(read_fd==-1){
        perror("[Client] : Error trying to open server-client fifo\n");
        exit(1);
    }

    char command[MAX_SIZE];
    int cmd_length=0;//it will store command's number of characters
    while(1){
        printf("Enter one of the following commands: \n 1) login : username \n 2) get-logged-users \n 3) get-proc-info : pid \n 4) logout \n 5) quit\n");
        cmd_length=0;
        while((command[cmd_length++]=getchar())!='\n');//read chars until we get new line
        command[cmd_length]='\0';
        //sending command to the server
        write(write_fd, command, strlen(command));
        //read response from the server
        char response[MAX_SIZE];
        read(read_fd, response, MAX_SIZE);
        //firstly, we are extracting the responze_length that's present
        //at the beggining of the response
        int response_length=0, index=0;
        for(;response[index]!=']';index++)
            if(response[index]>='0' && response[index]<='9')
                response_length=response_length*10+(response[index]-'0');
        //in the "response" buffer we only keep the response message
        strcpy(response, response+index+1);
        index=0;
        while(index<response_length){
            printf("%c", response[index++]);
        }
        if(strstr(response, "quit")!=0){
            break;
        }
    }
    close(write_fd);
    close(read_fd);
    return 0;
}

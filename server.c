#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <errno.h>
#include <utmp.h>
#include <time.h>
#include <sys/socket.h>
#include <math.h>

const int MAX_SIZE=1024;
const char* client_server_fifo="client_server_fifo";
const char* server_client_fifo="server_client_fifo";

struct user{
    char username[256];
    bool is_logged_in;
};


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
    int read_fd=open(client_server_fifo, O_RDONLY);
    if(read_fd==-1){
        perror("[Server] : Error opening the client-server fifo");
        exit(1);
    }
    //opening the server-client fifo
    int write_fd=open(server_client_fifo, O_WRONLY);
    if(write_fd==-1){
        perror("[Server] : Error opening the server-client fifo");
        exit(1);
    }

    char command[MAX_SIZE];

    //Creating user 
    struct user User;
    User.is_logged_in=false;
    while(1){
        ssize_t bytes_read=read(read_fd, command, MAX_SIZE);
        if(bytes_read==-1){
            perror("[Server] : Error trying to read the command from the client\n");
            exit(1);
        }
        //login command
        if(strncmp(command, "login : ", 8)==0){
            char cmd_username[256];//here we are gonna extract the username from the command
            strcpy(cmd_username, command+8);
            cmd_username[strcspn(cmd_username, "\n")]='\0';
            //we are creating a pipe to communicate from the child to the parent
            //if the login was successful
            int pipe_fd[2];
            if(pipe(pipe_fd)==-1){
                perror("[Server] : Error creating pipe\n");
                exit(1);
            }

            pid_t pid=fork();
            if(pid==-1){
                perror("[Server] : Error at fork for the login command\n");
                exit(1);
            }
            if(pid==0){//child
                close(pipe_fd[0]);
                //opening the configuration file
                int login_success;
                if(User.is_logged_in==true){
                    login_success=2;
                    write(pipe_fd[1], &login_success, sizeof(int));
                    close(pipe_fd[1]);
                    exit(0);
                }
                FILE* file=fopen("config.txt", "r");
                if(file==NULL){
                    perror("[Server] : Error opening configuration file\n");
                    exit(1);
                }
                char file_username[256];
                while(fgets(file_username, 256, file)!=NULL){
                    file_username[strcspn(file_username, "\n")]='\0';
                    if(strcmp(file_username, cmd_username)==0){
                        User.is_logged_in=true;
                        strcpy(User.username, file_username);
                    }
                }
                if(User.is_logged_in==true){
                    login_success=1;
                }else login_success=0;
                write(pipe_fd[1], &login_success, sizeof(int));
                close(pipe_fd[1]);
                fclose(file);
                exit(0);
            }else{//parent
                close(pipe_fd[1]);
                int login_success;
                read(pipe_fd[0],&login_success,sizeof(int));
                char server_response[MAX_SIZE];
                strcpy(server_response, "[");
                if(login_success==1){
                    strcat(server_response, "33");
                    strcat(server_response, "]");
                    strcat(server_response, "[Server response] login success\n\0");
                    write(write_fd, server_response, 37);
                    User.is_logged_in=true;
                }else if(login_success==0){
                    strcat(server_response, "32");
                    strcat(server_response, "]");
                    strcat(server_response, "[Server response] login failed\n\0");
                    write(write_fd, server_response, 36);
                }else if(login_success==2){
                    strcat(server_response, "42");
                    strcat(server_response, "]");
                    strcat(server_response, "[Server response] user already logged in\n\0");
                    write(write_fd, server_response, 46);
                }
                close(pipe_fd[0]);
            }
        }else if(strncmp(command, "quit", 4)==0){
            char server_response[MAX_SIZE];
            strcpy(server_response, "[");
            strcat(server_response, "35");
            strcat(server_response, "]");
            strcat(server_response, "[Server response] app quitting...\n\0");
            write(write_fd, server_response, 39);
            break;
        }else if(strncmp(command, "logout", 6)==0){
            int pipe_fd[2];
            if(pipe(pipe_fd)==-1){
                perror("[Server] : Error creating pipe\n");
                exit(1);
            }
            pid_t pid=fork();
            if(pid==-1){
                perror("[Server] Error during fork at logout\n");
                exit(1);
            }else if(pid==0){//child
                close(pipe_fd[0]);
                int flag;
                if(User.is_logged_in==true) flag=1;
                else flag=2;
                write(pipe_fd[1], &flag, sizeof(int));
                close(pipe_fd[1]);
                exit(1);
            }else{
                close(pipe_fd[1]);
                int flag;
                read(pipe_fd[0], &flag, sizeof(int));
                char server_response[MAX_SIZE];
                strcpy(server_response, "[");
                if(flag==1){
                    User.is_logged_in=false;
                    strcat(server_response, "30");
                    strcat(server_response, "]");
                    strcat(server_response, "[Server response] logged out\n\0");
                    write(write_fd, server_response, 34);
                }else if(flag==2){
                    strcat(server_response, "46");
                    strcat(server_response, "]");
                    strcat(server_response, "[Server response] there is no logged in user\n\0");
                    write(write_fd, server_response, 50);
                }
                close(pipe_fd[0]);
            }
        }else if(strncmp(command, "get-logged-users", 16)==0){
            char server_response[MAX_SIZE];
            strcpy(server_response, "[");
            if(User.is_logged_in==false){
                strcat(server_response, "61");
                strcat(server_response, "]");
                strcat(server_response, "[Server response] user has to be logged in for this command\n\0");
                write(write_fd, server_response, 65);
            }else{
                //creating a socket
                int sockets[2];
                socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
                pid_t pid=fork();
                if(pid==-1){
                    perror("[Server] Error during fork at get-logged-users\n");
                    exit(1);
                }else if(pid==0){//child
                    close(sockets[1]);
                    char write_buffer[MAX_SIZE];
                    char buffer[MAX_SIZE];
                    struct utmp* entry;
                    setutent();
                    entry=getutent();
                    strcpy(write_buffer, "");
                    while(entry!=NULL){
                        strcpy(buffer, "");
                        strncpy(buffer, entry->ut_user, 32);
                        strcat(buffer, " ");
                        strncat(buffer, entry->ut_host,256);
                        strcat(buffer, " ");
                        time_t login_time=entry->ut_tv.tv_sec;
                        struct tm *time_info=localtime(&login_time);
                        char time_str[MAX_SIZE];
                        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);
                        strcat(buffer, time_str);
                        strcat(buffer, "\n");
                        strcat(write_buffer,buffer);
                        entry=getutent();
                    }
                    strcat(write_buffer, "\0");
                    write(sockets[0], write_buffer, MAX_SIZE);
                    close(sockets[0]);
                    exit(0);
                }else{//parent
                    close(sockets[0]);
                    char information[MAX_SIZE];
                    read(sockets[1], information, MAX_SIZE);
                    //calculate the length of the message
                    int length=strlen(information)+18;//18 is from the [Server response] text
                    char length_as_chars[10];
                    int index=0;
                    while(length!=0){
                        int digit=length%10;
                        length_as_chars[index]=(digit+'0');
                        index++;
                        length/=10;
                    }
                    int st = 0;
                    int dr = index- 1;
                    while (st < dr) {
                        char temp = length_as_chars[st];
                        length_as_chars[st] = length_as_chars[dr];
                        length_as_chars[dr] = temp;
                        st++;
                        dr--;
                    }
                    char server_response[MAX_SIZE];
                    strcpy(server_response, "[");
                    strncat(server_response, length_as_chars, index);
                    strcat(server_response, "]");
                    strcat(server_response, "[Server response]\n");
                    strcat(server_response, information);
                    write(write_fd, server_response, 18+strlen(information)+2+index);
                    close(sockets[1]);
                }
            }
        }else if(strncmp(command, "get-proc-info : ", 16)==0){
            char server_response[MAX_SIZE];
            strcpy(server_response, "[");
            if(User.is_logged_in==false){
                strcat(server_response, "61");
                strcat(server_response, "]");
                strcat(server_response, "[Server response] user has to be logged in for this command\n\0");
                write(write_fd, server_response, 65);
            }else{
                int pipe_fd[2];
                if(pipe(pipe_fd)==-1){
                    perror("[Server] : Error creating pipe\n");
                    exit(1);
                }
                pid_t pid=fork();
                if(pid==-1){
                    perror("[Server] Error during fork at logout\n");
                    exit(1);
                }else if(pid==0){//child
                    close(pipe_fd[0]);
                    char pid_path[32]="/proc/";
                    int index=6;
                    while((command+16)[index-6]>='0' && (command+16)[index-6]<='9'){
                        pid_path[index++]=(command+16)[index-6];
                    }
                    pid_path[index++]='/';
                    pid_path[index++]='s';
                    pid_path[index++]='t';
                    pid_path[index++]='a';
                    pid_path[index++]='t';
                    pid_path[index++]='u';
                    pid_path[index++]='s';
                    FILE *pid_file=fopen(pid_path, "r");
                    if(pid_file==NULL){
                        write(pipe_fd[1], "Pid not found\n\0", 15);
                        perror("[Server] error trying to open the pid file\n");
                        exit(1);
                    }

                    char name[32], state[2], vmsize[32];
                    int ppid, uid;
                    char write_buffer[MAX_SIZE];
                    strcpy(write_buffer, "");
                    char line[MAX_SIZE];
                    while(fgets(line, sizeof(line), pid_file)!=NULL){
                        if(sscanf(line, "Name: %s", name)==1){
                            strcat(write_buffer, "Name: ");
                            strcat(write_buffer, name);
                        }
                        if(sscanf(line, "State: %s", state)==1){
                            strcat(write_buffer, "\nState: ");
                            strcat(write_buffer, state);
                        }
                        if(sscanf(line, "PPid: %d", &ppid)==1){
                            strcat(write_buffer, "\nPPid: ");
                            //we need to convert the ppid to a C string
                            char ppid_char[10];
                            sprintf(ppid_char, "%d", ppid);
                            strcat(write_buffer, ppid_char);
                        }
                        if(sscanf(line, "Uid: %d", &uid)==1){
                            strcat(write_buffer, "\nUid: ");
                            //we need to convert the ppid to a C string
                            char uid_char[10];
                            sprintf(uid_char, "%d", uid);
                            strcat(write_buffer, uid_char);
                        }
                        if(sscanf(line, "VmSize: %s", vmsize)==1){
                            strcat(write_buffer, "\nVmSize: ");
                            strcat(write_buffer, vmsize);
                            strcat(write_buffer, " KB\n");
                        }
                    }
                    strcat(write_buffer, "\0");
                    write(pipe_fd[1], write_buffer, sizeof(write_buffer));
                    close(pipe_fd[1]);
                    exit(0);
                }else{//parent
                    close(pipe_fd[1]);
                    char information[MAX_SIZE];
                    read(pipe_fd[0], information, MAX_SIZE);
                    int length=strlen(information)+18;//18 is from the [Server response] text
                    char length_as_chars[10];
                    int index=0;
                    while(length!=0){
                        int digit=length%10;
                        length_as_chars[index]=(digit+'0');
                        index++;
                        length/=10;
                    }
                    int st = 0;
                    int dr = index- 1;
                    while (st < dr) {
                        char temp = length_as_chars[st];
                        length_as_chars[st] = length_as_chars[dr];
                        length_as_chars[dr] = temp;
                        st++;
                        dr--;
                    }
                    char server_response[MAX_SIZE];
                    strcpy(server_response, "[");
                    strncat(server_response, length_as_chars, index);
                    strcat(server_response, "]");
                    strcat(server_response, "[Server response]\n");
                    strcat(server_response, information);
                    write(write_fd, server_response, 18+strlen(information)+2+index);
                    close(pipe_fd[0]);
                }
            }
        }else{
            write(write_fd, "[41][Server Response] command not recognised\n", 45);
        }
    }
    close(write_fd);
    close(read_fd);
    return 0;
}   
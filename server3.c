#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
Global Variables defined for server port number and address
*/
int server_port_number=8083;

/*
This method is used in Saving Files on server 2
The flow goes:-
Server 1-> upload Filename, FilePath, File Size
Server 3-> responde with OK and creates the file at the given Path
Server 1->Sends the content of file
Server 3-> Reads the content and writes on file and respons to server1
*/
int saveLocally(int con_fd, long fileSize, char *fileName, char *destinationPath){
    //Creates a local path to store file
    char *user=getenv("USER");
    char fixedPath[1026];
    snprintf(fixedPath, sizeof(fixedPath),"/Users/%s/Downloads/S3",user);
    char tempPath[1026];
    strcpy(tempPath, destinationPath);
    //tokenises the given path and creates a local path
    char *token;
    token = strtok(tempPath, "/");
    int count=0;
    while (token!=NULL)
    {
        count++;
        if(count<=2){
            token=strtok(NULL,"/");
            continue;
        }
        strcat(fixedPath,"/");
        strcat(fixedPath,token);
        mkdir(fixedPath, 0777);
        token=strtok(NULL,"/");
    }  
    strcat(fixedPath,"/");
    strcat(fixedPath,fileName);
    printf("Final FILE PATH %s\n",fixedPath);
    //creats the local file
    int fd=open(fixedPath, O_CREAT|O_RDWR, 0777);
    if(fd<0){
        char *error_message="ERROR: File doesnot open";
        write(con_fd, error_message, strlen(error_message));
        return 0;
    }
    //Reads from socket and writes to file
    long bytes_read=0;
    while(bytes_read<fileSize){
        char buffer[1026];
        int partial_read_bytes = read(con_fd, buffer, sizeof(buffer));
        write(fd, buffer, partial_read_bytes);
        bytes_read=bytes_read+partial_read_bytes;
    }
    //sends confirmation to client
    write(con_fd, "RECEIVED\n", 9);
    printf("Final Received message send\n");
    close(fd);
    return 1;
}

/*
This method is used in Downloading Files on server
The flow goes:-
Server1-> download Filename,
Server3-> Creates a local path and downloads and responde with OK fileSize
Server1->Sends the confirmation
Server3-> Sends the file content
*/
void downloadFileLocally(int con_fd, char *fileName){
    //Creating a local path
    char *user=getenv("USER");
    char fixedPath[1026];
    snprintf(fixedPath, sizeof(fixedPath),"/Users/%s/Downloads/S3",user);
    char tempPath[1026];
    strcpy(tempPath, fileName);
    char *token;
    token = strtok(tempPath, "/");
    int count=0;
    while (token!=NULL)
    {
        count++;
        if(count<=2){
            token=strtok(NULL,"/");
            continue;
        }
        strcat(fixedPath,"/");
        strcat(fixedPath,token);
        token=strtok(NULL,"/");
    }
    printf("Final Downloable Path is %s\n",fixedPath);
    //Creating a local file
    int file_fd=open(fixedPath, O_RDWR, 0777);
    if(file_fd<0){
        char *error_message="Error file doesnot exist";
        write(con_fd, error_message, strlen(error_message));
        return;
    }
    //getting file Size
    long fileSize=0;
    fileSize=lseek(file_fd,0,SEEK_END);
    lseek(file_fd,0,SEEK_SET);
    
    //Responding to Server1 with File Size
    char message[1026];
    snprintf(message, sizeof(message), "OK %ld\n", fileSize);
    write(con_fd,message, strlen(message));
    printf("OK sent to Server1 as %s\n", message);
    
    //Getting server1 Confirmation
    char clientResoonse[1026];
    read(con_fd, clientResoonse, sizeof(clientResoonse));
    printf("Server1 Responded: %s\n", clientResoonse);
    if(strcmp(clientResoonse,"RECEIVED")!=0){
        printf("ERROR\n");
        return;
    }

    //Sending file content to Server1 over socket
    long bytes_send=0;
    while(bytes_send<fileSize){
        char buffer[1026];
        long partial_read_bytes= read(file_fd, buffer, sizeof(buffer));
        //printf("File Read in server as %s\n", buffer);
        write(con_fd, buffer, partial_read_bytes);
        //printf("File Sent to client as %ld\n",partial_read_bytes);
        bytes_send+=partial_read_bytes;
    }
    printf("File Sent successfully\n");
    close(file_fd);
    return;

}

/*
This method is used in Removing Files on server
The flow goes:-
Server1-> remove Filename,
Server3-> Creates a local path and removes the file and sends the response to client
*/
void removeFileLocally(int con_fd, char*fileName){
    //Creating file path
    char *user=getenv("USER");
    char fixedPath[1026];
    snprintf(fixedPath, sizeof(fixedPath),"/Users/%s/Downloads/S3",user);
    char tempPath[1026];
    strcpy(tempPath, fileName);
    char *token;
    token = strtok(tempPath, "/");
    int count=0;
    while (token!=NULL)
    {
        count++;
        if(count<=2){
            token=strtok(NULL,"/");
            continue;
        }
        strcat(fixedPath,"/");
        strcat(fixedPath,token);
        token=strtok(NULL,"/");
    }
    printf("Final Removable Path is %s\n",fixedPath);
    
    //Removing the file and writing to client
    int success=remove(fixedPath);
    if(success==0){
        write(con_fd, "OK \n",4);
        printf("Writing to Server1 File Removed\n");
    }else{
        char *error_message="Error:: File Doesnot exist";
        printf("Writing to Server1 File NOT Removed\n");
        write(con_fd, error_message, strlen(error_message));
    }
    return;
}

/*
This method is used in Tarring Files on server
The flow goes:-
Server1-> tar Fileextension
Server3-> Creates a temp file to store all filtered file. Creates tar from reading this file and sends OK file Size to client
Server1->Sends the confirmation
Server3-> Sends the file content
*/
void createTARfilesLocally(int con_fd){
    char *user=getenv("USER");
    //Creating temp tar 
    char temp_tar_path[1026];
    snprintf(temp_tar_path, sizeof(temp_tar_path),"/Users/%s/Downloads/temp_tar_03.tar",user);
    //Creating path to be searched
    char search_path[1026];
    snprintf(search_path, sizeof(search_path),"/Users/%s/Downloads/S3",user);
    //Creating temp file to store filtered results
    char file_found_path[1026];
    snprintf(file_found_path, sizeof(file_found_path),"/Users/%s/Downloads/files_found_03.tar",user);
    //Creating a find command
    char find_command[1026];
    snprintf(find_command,sizeof(find_command),"find \"%s\" -name \"*.txt\" -type f > \"%s\" ",search_path,file_found_path);
    system(find_command);

    //Opening file to check for results
    int files_found_fd= open(file_found_path,O_RDONLY,0777);
    if(file_found_path<0){
        char *error_message="Error:: No Files Found";
        write(con_fd,error_message, strlen(error_message));
        printf("File Found Path doesnot exist\n");
        return;
    }
    int file_count=lseek(files_found_fd,0,SEEK_END);
    if(file_count<=0){
        char *error_message="Error:: No Files Found";
        write(con_fd,error_message, strlen(error_message));
        printf("File Found file was empty\n");
        return;
    }
    lseek(files_found_fd,0,SEEK_SET);
    close(files_found_fd);
    
    //Running the tar command
    char tar_command[1026];
    snprintf(tar_command,sizeof(tar_command),"tar -cvf \"%s\" -T \"%s\"",temp_tar_path, file_found_path);
    system(tar_command);
    printf("Tar Command RAN\n");
    
    int file_fd=open(temp_tar_path, O_RDONLY,0777);
    if(file_fd<0){
        char *error_message="Error:: No Files Found";
        write(con_fd,error_message, strlen(error_message));
        printf("TAR file donot exist\n");
        return;
    }
    long tar_size=0;
    tar_size=lseek(file_fd,0,SEEK_END);
    lseek(file_fd,0,SEEK_SET);
    
    //Sending the File Size to Server 1
    char response_command[1026];
    snprintf(response_command, sizeof(response_command), "OK %ld\n",tar_size);
    write(con_fd, response_command, strlen(response_command));
    printf("Response Command OK written to client as %s\n",response_command);
    
    //Getting the server1 confirmation
    char client_response[1026];
    read(con_fd,client_response, sizeof(client_response));
    printf("Client Rescponed as %s\n",client_response);
    if(strcmp(client_response,"RECEIVED")!=0){
        printf("ERROR\n");
        return;
    }

    //Writing the file content to client
    long bytes_send=0;
    while(bytes_send<tar_size){
        char buffer[1026];
        long partial_read_bytes=read(file_fd, buffer, sizeof(buffer));
        //printf("TAR files read are %s\n", buffer);
        write(con_fd, buffer, partial_read_bytes);
        //printf("TAR Content written to client as %ld\n",partial_read_bytes);
        bytes_send+=partial_read_bytes;
    }
    //Removing temp files
    remove(temp_tar_path);
    remove(file_found_path);
    printf("TAR file was sucessfully sent to client\n");
    return;
}

/*
This method handles each server1 request
Waits for server1 to write something
Reads from the socket and calls the above method accordingly
0:- Uploading
1:-Downloading
2:-Removing
3:-Tarring
*/
void prcclient(int con_fd){
    
    //Runs until client writes close
    while (1)
    {
        printf("Waiting for client to write something\n");
        char commandStorage[1026];
        memset(commandStorage, 0, sizeof(commandStorage));
        //Reads from the command
        read(con_fd, commandStorage, sizeof(commandStorage)-1);
        char fileName[1026];
        long fileSize=0;
        char path[1026];
        //Calls method accordingly
        //Method for Uploading Files
        if(sscanf(commandStorage, "0 %s %s %ld", fileName,path,&fileSize )==3){
            printf("Command from server1 received as %s\n",commandStorage);
            write(con_fd, "OK\n", 3); 
            printf("OK sent to server1\n");
            saveLocally(con_fd,fileSize,fileName,path);
        
            //Method for Downloading Files
        }else if(sscanf(commandStorage, "1 %s", fileName)==1){
            printf("In DOwnload Condition\n");
            downloadFileLocally(con_fd, fileName);
            
            //Method for Removing Files
        }else if(sscanf(commandStorage, "2 %s", fileName)==1){
            printf("In Remove Condition\n");
            removeFileLocally(con_fd, fileName);
            
            //Method for Taring Files
        }else if(sscanf(commandStorage, "3 %s", fileName)==1){
            printf("In TAR Condition\n");
            createTARfilesLocally(con_fd);
        }
        else if(strcmp(commandStorage,"CLOSE")){
            close(con_fd);
            break;
        }  
    }
    return;
}

/*
This is the main method
It Creates a socket and binds to portnumber
For each connection request it forks
Child Process calls prcclient() method
*/
int main(){
    int lis_sd, con_sd;
    socklen_t len;
    //class for storing server details
    struct sockaddr_in servAdd;
    //Creates a socket
    lis_sd = socket(AF_INET, SOCK_STREAM, 0);
    servAdd.sin_family = AF_INET;
    servAdd.sin_addr.s_addr = htonl(INADDR_ANY);
    servAdd.sin_port = htons((uint16_t)server_port_number);
    bind(lis_sd, (struct sockaddr *) &servAdd,sizeof(servAdd));
    //Queue is set to 5 connections
    listen(lis_sd, 5);
    printf("server running on IP %u\n",INADDR_ANY );
    while (1)
    {
        con_sd=accept(lis_sd,(struct sockaddr*)NULL,NULL);
        //Child Process condition
        if (fork() == 0) {
            close(lis_sd);
            prcclient(con_sd);
            exit(0);
        }
        close(con_sd);
    }
    return 0; 
}
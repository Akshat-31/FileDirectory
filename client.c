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
These are global variables defined 
*/
int client_fd;
int server_01_PortNumber=8080;
char *serverIPAddress="";

/*
This method establishes a socket for Client
Establishes a connection with server 1
Creates a TCP connection
0:- Succes
1:- Error
*/
int establishConnection() {
    //Creating a socket for client of TCP connection
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        printf("Socket Error\n");
        return 1;
    }

    //class for storing server details
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons((uint16_t)server_01_PortNumber);

    if (inet_pton(AF_INET, serverIPAddress, &serverAddress.sin_addr) <= 0) {
        printf("Erorr in inet_pton\n");
        return 1;
    }

    //establishing connection
    if (connect(client_fd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        perror("Erorr in connecting with server\n");
        return 1;
    }
    printf("Connected to server %s:%d\n", serverIPAddress, server_01_PortNumber);
    return 0;
}

/*
This a utility method
It takes commandArray as an input and checks the commands acc to different types
0:- Error
1:- Success
*/
int validateCommand(char * commandArray[]){
    int fileCount=0;
    int count=0;
    while(commandArray[count]!=NULL){
            fileCount++;
            count++;
    }
    //checking for Upload
    if(strcmp(commandArray[0],"uploadf")==0){
        if(fileCount<3){
            return 0;
        }
        if(fileCount-1>3 && fileCount-1<=0){
            return 0;
        }else{
            return 1;
        }
    //checking for Download
    }else if(strcmp(commandArray[0],"downlf")==0){
        if(fileCount-1==1 || fileCount-1==2){
            return 1;
        }else{
            return 0;
        }
    //checking for Remove    
    }else if(strcmp(commandArray[0],"removef")==0){
        if(fileCount-1==1 || fileCount-1==2){
            return 1;
        }else{
            return 0;
        }
    //checking for Tar
    }else if(strcmp(commandArray[0],"downltar")==0){
        if(fileCount-1==1){
            return 1;
        }else{
            return 0;
        }
    }else{
        return 0;
    }

    return 0;
}

/*
This a utility method
It biffercate the command on spaces
It takes Command from user and commandArray
Tokenises the coammdn on spaces and updates the array
*/
void differcateCommand(char *Command, char *commandArray[]){
    //Uses String tokenizer method
    char *token = strtok(Command, " ");
    int count=0;
    while (token != NULL) {
        commandArray[count++]=token;
        token = strtok(NULL, " ");
    }
    commandArray[count]=NULL;
    return;
}

/*
This method is used in Uploading Files to server
0:- Uploading recognition
The flow goes:-
Client-> upload Filename, FilePath, File Size
Server-> responde with OK
Client->Sends the content of file
Server-> response with received
*/
void writeIntoFd(char *fileName, char *path){
    //Opning the file 
    char fileData[1026];
    int file_fd= open(fileName, O_RDONLY);
    if(file_fd<1){
        printf("Error:: File doesnot exists %s\n",fileName);
        return;
    }
    //getting size of file
    long fileSize=0;
    fileSize= lseek(file_fd,0,SEEK_END);
    lseek(file_fd,0,SEEK_SET);

    //sending the command message
    char command[512];
    snprintf(command, sizeof(command), "0 %s %s %ld\n", fileName, path,fileSize);
    write(client_fd, command, sizeof(command));
    //printf("Command to server send as %s\n",command);

    //server responded
    char serverResponse[1026];
    long response_bytes_read= read(client_fd, serverResponse, sizeof(serverResponse));
    if (strncmp(serverResponse, "OK",2) != 0) {
        printf("Error: %s\n", serverResponse);
        return;
    }
    //printf("Server responed as %s\n",serverResponse);

    //sending total bytes until end
    long bytes_send=0;
    while(bytes_send<fileSize){
        char buffer[1026];
        int read_bytes=read(file_fd, buffer, sizeof(buffer));
        write(client_fd, buffer, read_bytes);
        //printf("Bytes send to server as %s", buffer);
        bytes_send=bytes_send+read_bytes;
    }
    
    //server response
    char serverResponse_02[1026];
    response_bytes_read=read(client_fd, serverResponse_02, sizeof(serverResponse_02));
    if(strncmp(serverResponse_02,"RECEIVED\n",9)!=0){
        printf("Error: %s\n", serverResponse_02);
        return;
    }else{
        printf("File Succesfully uploaded\n");
    }
    return;
}

/*
This is the utility method for uploading files
It iterates to total files given in command
For each file calls the above method
*/
void uploadFiles(char *commandArray[]){
    int input=0;
    while (commandArray[input]!=NULL){
       input++;
    }
    int total_files= input-2;
    char *path=commandArray[input-1];
    //iterates the files
    for(int file=1;file<=total_files;file++){
        writeIntoFd(commandArray[file], path);
    }
    return;
}

/*
This method is used in Downloading Files from server
1:- Downloading recognition
The flow goes:-
Client-> download Filename
Server-> responds with OK FileSize
Client->Sends the received confirmation
Server-> response with File Content
Client-> reads the content and store in file locally
*/
void downloadFilesThroughFd(char *fileName){
    //sending the command
    char command[512];
    snprintf(command, sizeof(command), "1 %s\n", fileName);
    write(client_fd, command, sizeof(command));
    //printf("Command to server send as %s\n",command);
    
    //Server responds with file size
    char serverResponse[1026];
    long response_bytes_read = read(client_fd, serverResponse, sizeof(serverResponse));

    if (strncmp(serverResponse, "OK ",3) != 0) {
        printf("Error: %s\n", serverResponse);
        return;
    }
    //printf("Server responed as %s\n",serverResponse);
    long fileSize=0;
    sscanf(serverResponse+3,"%ld", &fileSize);
    
    //Sending server the confirmation
    write(client_fd, "RECEIVED", 9);
    //printf("Received message send\n");


    //Creating a local file
    char* lastFileName=strrchr(fileName,'/');
    lastFileName++;
    //printf("The File is being created in client as %s\n",lastFileName);
    int file_fd=open(lastFileName, O_CREAT|O_RDWR, 0777);
    if(file_fd<0){
        printf("ERROR:: file doesnot exist\n");
        return;
    }
    
    //Reading the File Content and writing to local file
    long bytes_read=0;
    while(bytes_read<fileSize){
        char buffer[1026];
        long partial_read_bytes= read(client_fd, buffer, sizeof(buffer));
        //printf("Partial File Received From Server as %s\n",buffer);
        write(file_fd, buffer, partial_read_bytes);
        //printf("Partial written Bytes are %ld\n",partial_read_bytes);
        bytes_read+=partial_read_bytes;
    }
    printf("The file is successfully downloaded\n");
    close(file_fd);
    return;
}

/*
This is the utility method for downloading files
It iterates to total files given in command
For each file calls the above method
*/
void downloadFiles(char *commandArray[]){
    int input=0;
    while (commandArray[input]!=NULL){
       input++;
    }
    int total_files= input-1;
    //iterates the files
    for(int file=1;file<=total_files;file++){
        downloadFilesThroughFd(commandArray[file]);
    }
    return;
}

/*
This method is used in Removing Files from server
2:- Removing recognition
The flow goes:-
Client-> Remove Filename
Server-> responds with sucess/error message
*/
void removeFileThorughFd(char *fileName){
    //Creating the command
    char command[1026];
    snprintf(command, sizeof(command), "2 %s",fileName);
    //Sending to server
    write(client_fd, command, strlen(command));
    
    //printf("Command Written to Server as %s\n", command);

    //Reading the response from server
    char serverResponse[1026];
    int server_response= read(client_fd,serverResponse, sizeof(serverResponse));
    if(strstr(serverResponse, "OK")){
        printf("File Removed Successfully\n");
        return;
    }
    printf("Server Responded as %s\n", serverResponse);
    return;

}

/*
This is the utility method for removing files
It iterates to total files given in command
For each file calls the above method
*/
void removeFiles(char *commandArray[]){
    int input=0;
    while (commandArray[input]!=NULL){
       input++;
    }
    int total_files=input-1;
    //iterates the files
    for(int file=1;file<=total_files;file++){
        removeFileThorughFd(commandArray[file]);
    }
    return;

}

/*
This method is used in Downloading Files from server
3:- Tar recognition
The flow goes:-
Client-> Tar, fileextension
Server-> responds with OK FileSize
Client->Sends the received confirmation
Server-> response with File Content
Client-> reads the content and store in tar locally
*/
void tarFilesThroughFd(char *fileExtension){
    char command[1026];
    snprintf(command, sizeof(command), "3 %s",fileExtension);
    //Sending the tar command
    write(client_fd, command, strlen(command));
    //printf("Command Written to client as %s\n",command);

    //Server response with the fileSize
    char serverResponse[1026];
    read(client_fd, serverResponse, sizeof(serverResponse));
    if (strncmp(serverResponse, "OK ",3) != 0) {
        printf("Error: %s\n", serverResponse);
        return;
    }

    long tar_file_size=0;
    sscanf(serverResponse, "OK %ld", &tar_file_size);
    //printf("Server Responded with OK as %s\n and size as %ld", serverResponse, tar_file_size);

    //Sending confirmation to server
    write(client_fd, "RECEIVED", 9);
    //printf("Writtent to server RECEIVED\n");

    //Creating the local filename
    char *fileName=NULL;
    if(strcmp(fileExtension,".c")==0){
        fileName="cfiles.tar";
    }else if(strcmp(fileExtension,".txt")==0){
        fileName="txtfiles.tar";
    }else if(strcmp(fileExtension,".pdf")==0){
        fileName="pdffiles.tar";
    }else{
        printf("No such file exists\n");
        return;
    }

    int file_fd=open(fileName, O_CREAT|O_RDWR,0777);
    
    if(file_fd<0){
        printf("Error:: Unable to create file");
        return;
    }
    //Reading from server the file content and writing to file
    long bytes_read=0;
    while(bytes_read<tar_file_size){
        char buffer[1026];
        long partial_bytes_read=read(client_fd, buffer, sizeof(buffer));
        //printf("Server sent tar Content as %s\n",buffer);
        write(file_fd, buffer, partial_bytes_read);
        //printf("Tar File Saved LOcally as %ld\n",partial_bytes_read);
        bytes_read+=partial_bytes_read;
    }
    close(file_fd);
    printf("Tar was successfully downloaded\n");
    return;
}

/*
This method decides which command to run
It check the command and calls method accordingly
*/
void runCommand(char *commandArray[]){
    if(strcmp(commandArray[0],"uploadf")==0){
        uploadFiles(commandArray);
    }else if(strcmp(commandArray[0],"downlf")==0){
        downloadFiles(commandArray);
    }else if(strcmp(commandArray[0],"removef")==0){
        removeFiles(commandArray);
    }else if(strcmp(commandArray[0],"downltar")==0){
        tarFilesThroughFd(commandArray[1]);
    }

}

/*
This method takes user command
Check for the command validity
Calls the above method on command
Reads until user enters "close"
*/
void takeUserCommand(){
    //Runs until close is entered
    while(1){
        char Command[1026];
        char *result[1026];
        printf("Enter Command\n");
        scanf(" %[^\n]",Command);
        if (strcmp(Command, "close") == 0) {
            //Writes to server Close command
            write(client_fd, "CLOSE", 5);
            printf("Send Server close command\n");
            break;
        }
        differcateCommand(Command, result);
        if(validateCommand(result)==0){
            printf("Invalid Command\n");
            continue;
        }
        runCommand(result);
    }
    return;
}

/*
This is the main method
It establishes connection with server
Calls method to take User input
*/
int main(){
    int connection_result=establishConnection();
    if(connection_result!=0){
        printf("Unable to establish a connection\n");
        return 0;
    }
    takeUserCommand();
return 0;
}
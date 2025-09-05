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
int server_02_Port_number=8082;
int server_03_Port_number=8083;
int server_04_Port_number=8084;
int server_01_Port_number=8080;

char *server_IPAddress="";

/*
This method is used in Saving Files on server
The flow goes:-
Client-> upload Filename, FilePath, File Size
Server-> responde with OK and creates the file at the given Path
Client->Sends the content of file
Server-> Reads the content and writes on file and respons to client
*/
int saveLocally(int con_fd, long fileSize, char *fileName, char *destinationPath){
    //Creates a local path to store file
    char *user=getenv("USER");
    char fixedPath[1026];
    snprintf(fixedPath, sizeof(fixedPath),"/Users/%s/Downloads/S1",user);
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
This method establishes a socket for Client
Establishes a connection with server 1
Creates a TCP connection
server fd:- Succes
1:- Error
*/
int createConnectionWithOutServer(int serverPortNumber){
    int server_02_fd;
    //Creating a socket for client of TCP connection
    server_02_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_02_fd < 0) {
        printf("Socket Error\n");
        return 1;
    }

    //class for storing server details
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons((uint16_t)serverPortNumber);

    if (inet_pton(AF_INET, server_IPAddress, &serverAddress.sin_addr) <= 0) {
        printf("Erorr in inet_pton\n");
        return 1;
    }

     //establishing connection
    if (connect(server_02_fd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        perror("Erorr in connecting with server\n");
        return 1;
    }
    printf("Connected to server %s:%d\n", server_IPAddress, serverPortNumber);
    return server_02_fd;
}

/*
This method is used when we forward file to different server for uploading
Server1 -> upload Filename, FilePath, File Size
Out Server -> responde with OK and creates the file at the given Path
Server1 ->Sends the content of file and forwards the response to client
Out server-> sends the confirmation
Server1-> Forwards the confirmation to client
*/
void sendDataToDifferentServer(int con_fd,int server_fd, char *fileName, long fileSize, char*path){
    //Check if connection is established
    if(server_fd==1){
        printf("ERROR in connecting with outserver\n");
        char *error_message="Error connceting with outserver\n";
        write(con_fd, error_message, strlen(error_message));
        return;
    }
    //Sending command to out server
    char commandToOutServer[512];
    snprintf(commandToOutServer, sizeof(commandToOutServer), "0 %s %s %ld\n", fileName, path, fileSize);
    write(server_fd, commandToOutServer, sizeof(commandToOutServer));
    printf("Sending command to out server as %s", commandToOutServer);
    
    //Getting response
    char responseFromOutServer[1026];
    int response_read = read(server_fd, responseFromOutServer, sizeof(responseFromOutServer));
    if (strncmp(responseFromOutServer, "OK", 2) != 0) {
        write(con_fd, responseFromOutServer, response_read);
        close(server_fd);
        return;
    }
    printf("Out Server Responded as %s\n", responseFromOutServer);
    
    //Sending file content to out server
    long bytes_read=0;
    while(bytes_read<fileSize){
        char buffer[1026];
        int partial_bytes_read= read(con_fd, buffer, sizeof(buffer));
        //printf("Bytes Read From client are %s\n",buffer);
        write(server_fd, buffer, partial_bytes_read);
        //printf("Bytes written to outserver are %s\n",buffer);
        bytes_read+=partial_bytes_read;
    }
    //printf("File Sent to Outserver as %ld\n",bytes_read);

    //getting confirmation from outserver
    char serverResponse_02[1026];
    response_read = read(server_fd, serverResponse_02, sizeof(serverResponse_02) - 1);
    printf("Final Response From OutServer as %s\n",serverResponse_02);
    
    //Forwarding the response to client
    write(con_fd, serverResponse_02, response_read);
    printf("Final Response sent to Client as %s\n",serverResponse_02);
    close(server_fd);

}

/*
This method is used in Downloading Files on server
The flow goes:-
Client-> download Filename,
Server-> Creates a local path and downloads and responde with OK fileSize
Client->Sends the confirmation
Server-> Sends the file content
*/
void downloadFileLocally(int con_fd, char *fileName){
    //Creating a local path
    char *user=getenv("USER");
    char fixedPath[1026];
    snprintf(fixedPath, sizeof(fixedPath),"/Users/%s/Downloads/S1",user);
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
    
    //Responding to Client with File Size
    char message[1026];
    snprintf(message, sizeof(message), "OK %ld\n", fileSize);
    write(con_fd,message, strlen(message));
    printf("OK sent to client as %s\n", message);
    
    //Getting Client Confirmation
    char clientResoonse[1026];
    read(con_fd, clientResoonse, sizeof(clientResoonse));
    printf("Client Responded: %s\n", clientResoonse);
    if(strcmp(clientResoonse,"RECEIVED")!=0){
        printf("ERROR\n");
        return;
    }

    //Sending file content to client over socket
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
This method is used when want different server for downloading
Server1 -> download Filename
Out Server -> responde with OK File Size
Server1 ->Sends the Confirmation and sending OK File Size to client
Client-> Sending Confirmation
Out server-> Sends the file content
Server1-> Reads from socket and write to client socket
*/
void downloadFilesFromOutServer(int con_fd, int server_fd,char *fileName){
    //Check if connection is established
    if(server_fd==1){
        printf("ERROR in connecting with outserver\n");
        char *error_message="Error connceting with outserver\n";
        write(con_fd, error_message, strlen(error_message));
        return;
    }
    //Sending command to out server
    char commandToOutServer[512];
    snprintf(commandToOutServer, sizeof(commandToOutServer), "1 %s\n", fileName);
    write(server_fd, commandToOutServer, sizeof(commandToOutServer));
    printf("Sending command to out server as %s", commandToOutServer);
    
    //Getting respnse from Out Server
    char responseFromOutServer[1026];
    int resppnse_bytes_read = read(server_fd, responseFromOutServer, sizeof(responseFromOutServer));
    if (strncmp(responseFromOutServer, "OK ",3) != 0) {
        char *error_message="Error:: File Doesnot Exist";
        write(con_fd, error_message,strlen(error_message));
        printf("Server error: %s\n", responseFromOutServer);
        return;
    }
    printf("Out Server Responded as %s\n", responseFromOutServer);
    long fileSize=0;
    sscanf(responseFromOutServer+3,"%ld", &fileSize);
    printf("The file Size is %ld\n",fileSize);

    //Sending confirmation to out server
    write(server_fd, "RECEIVED", 9);
    printf("Received message send to outserver\n");

    //Sending OK file size to client
    char messageToClient[1026];
    snprintf(messageToClient, sizeof(messageToClient), "OK %ld\n", fileSize);
    printf("Writing OK SIZE to client %s\n",messageToClient);
    write(con_fd, messageToClient, sizeof(messageToClient));

    //Getting confirmation response from client
    char responseFromClient[1026];
    read(con_fd, responseFromClient, sizeof(responseFromClient));
    printf("Client Received Response as %s\n", responseFromClient);
    if(strcmp(responseFromClient,"RECEIVED")!=0){
        printf("ERROR\n");
        return;
    }

    //Reading file content from outserver and writing to client over socket
    long bytes_read=0;
    while(bytes_read<fileSize){
        char buffer[1026];
        long partial_bytes_read= read(server_fd, buffer, sizeof(buffer));
        //printf("Out Server Gave File Content as %s\n",buffer);
        write(con_fd, buffer, partial_bytes_read);
        //printf("Written File Content to Client as %ld\n",partial_bytes_read);
        bytes_read+=partial_bytes_read;
    }
    printf("File Sent successfully\n");
    close(server_fd);
    return;
}

/*
This method is used in Removing Files on server
The flow goes:-
Client-> remove Filename,
Server-> Creates a local path and removes the file and sends the response to client
*/
void removeFileLocally(int con_fd, char*fileName){
    //Creating file path
    char *user=getenv("USER");
    char fixedPath[1026];
    snprintf(fixedPath, sizeof(fixedPath),"/Users/%s/Downloads/S1",user);
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
        printf("Writing to client File Removed\n");
    }else{
        char *error_message="Error:: File Doesnot exist";
        printf("Writing to client File NOT Removed\n");
        write(con_fd, error_message, strlen(error_message));
    }
    return;
}

/*
This method is used when want different server for removing
Server1 -> remove Filename
Out Server -> removes the file and sends the response
Server1 ->Sends the response to client
*/
void removeFileFromOutServer(int con_fd, int server_fd,char *fileName){
    //Check if connection is established
    if(server_fd==1){
        printf("ERROR in connecting with outserver\n");
        char *error_message="Error connceting with outserver\n";
        write(con_fd, error_message, strlen(error_message));
        return;
    }
    
    //sending command to outserver
    char commandToOutServer[1026];
    snprintf(commandToOutServer, sizeof(commandToOutServer), "2 %s",fileName);

    write(server_fd, commandToOutServer, strlen(commandToOutServer));
    printf("Command Written to Out Server as %s\n", commandToOutServer);

    //getting response from outserver
    char responseFromOutServer[1026];
    int serve02_response= read(server_fd,responseFromOutServer, sizeof(responseFromOutServer));
    printf("Server Responded as %s\n", responseFromOutServer);

    //writing response to client
    write(con_fd, responseFromOutServer, serve02_response);
    printf("Response written to client from server 2 as %s\n",responseFromOutServer);

    return;

}

/*
This method is used in Downloading Files on server
The flow goes:-
Client-> tar Fileextension
Server-> Creates a temp file to store all filtered file. Creates tar from reading this file and sends OK file Size to client
Client->Sends the confirmation
Server-> Sends the file content
*/
void createTARfilesLocally(int con_fd){
    char *user=getenv("USER");
    //Creating temp tar 
    char temp_tar_path[1026];
    snprintf(temp_tar_path, sizeof(temp_tar_path),"/Users/%s/Downloads/temp_tar_01.tar",user);
    //Creating path to be searched
    char search_path[1026];
    snprintf(search_path, sizeof(search_path),"/Users/%s/Downloads/S1",user);
    //Creating temp file to store filtered results
    char file_found_path[1026];
    snprintf(file_found_path, sizeof(file_found_path),"/Users/%s/Downloads/files_found_01.tar",user);
    //Creating a find command
    char find_command[1026];
    snprintf(find_command,sizeof(find_command),"find \"%s\" -name \"*.c\" -type f > \"%s\" ",search_path,file_found_path);
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
    
    //Sending the File Size to client
    char response_command[1026];
    snprintf(response_command, sizeof(response_command), "OK %ld\n",tar_size);
    write(con_fd, response_command, strlen(response_command));
    printf("Response Command OK written to client as %s\n",response_command);
    
    //Getting the client confirmation
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
This method is used when want different server for Tar
Server1 -> tar FileExtension
Out Server -> responde with OK tar Size
Server1 ->Sends the Confirmation and sending OK File Size to client
Client-> Sending Confirmation
Out server-> Sends the file content
Server1-> Reads from socket and write to client socket
*/
void createTARfilesFromOutServer(int con_fd, int server_fd, char *fileExtension){
    //Check if connection is established
    if(server_fd==1){
        printf("ERROR in connecting with outserver\n");
        char *error_message="Error connceting with outserver\n";
        write(con_fd, error_message, strlen(error_message));
        return;
    }
    
    //Sending command to out server
    char commandToOutServer[1026];
    snprintf(commandToOutServer, sizeof(commandToOutServer), "3 %s",fileExtension);

    write(server_fd, commandToOutServer, strlen(commandToOutServer));
    printf("Command Written to client as %s\n",commandToOutServer);

    //Getting respnse from Out Server
    char responseFromOutServer[1026];
    int n = read(server_fd, responseFromOutServer, sizeof(responseFromOutServer) - 1);
    responseFromOutServer[n] = 0;
    if (strncmp(responseFromOutServer, "OK ",3) != 0) {
        write(con_fd, responseFromOutServer,n);
        printf("Server error: %s\n", responseFromOutServer);
        return;
    }
    printf("Out Server Responded as %s\n", responseFromOutServer);
    long tar_fileSize=0;
    sscanf(responseFromOutServer+3,"%ld", &tar_fileSize);
    printf("The file Size is %ld\n",tar_fileSize);

    //Sending confirmation to out server
    write(server_fd, "RECEIVED", 9);
    printf("Received message send to outserver\n");

    //Sending OK file size to client
    char messageToClient[1026];
    snprintf(messageToClient, sizeof(messageToClient), "OK %ld\n", tar_fileSize);
    printf("Writing OK SIZE to client %s\n",messageToClient);
    write(con_fd, messageToClient, sizeof(messageToClient));

    //Getting confirmation response from client
    char responseFromClient[1026];
    read(con_fd, responseFromClient, sizeof(responseFromClient));
    printf("Client Received Response as %s\n", responseFromClient);
    if(strcmp(responseFromClient,"RECEIVED")!=0){
        printf("ERROR\n");
        return;
    }

    //Reading file content from outserver and writing to client over socket
    long bytes_read=0;
    while(bytes_read<tar_fileSize){
        char buffer[1026];
        long partial_bytes_read=read(server_fd, buffer, sizeof(buffer));
        //printf("Out Server Gave File Content as %s\n",buffer);
        write(con_fd, buffer, partial_bytes_read);
        //printf("Written File Content to Client as %ld\n",partial_bytes_read);
        bytes_read+=partial_bytes_read;
    }

    close(server_fd);
    printf("TAR file was sucessfully sent to client\n");
    return;
}

/*
This method handles each client request
Waits for client to write something
Reads from the socket and calls the above method accordingly
Closes the connection when client write CLOSE
0:- Uploading
1:-Downloading
2:-Removing
3:-Tarring
*/
void prcclient(int con_fd){
    int count=0;
    //Runs until client writes close
    while (1)
    {   
        count++;
        printf("Waiting for client to write something\n");
        char commandStorage[1026];
        memset(commandStorage, 0, sizeof(commandStorage));
        //Reads from the command
        read(con_fd, commandStorage, sizeof(commandStorage));
        char fileName[1026];
        long fileSize=0;
        char path[1026];
        //Calls method accordingly
        //Method for Uploading Files
        if(sscanf(commandStorage, "0 %s %s %ld", fileName,path,&fileSize )==3){
            printf("Command from server received as %s\n",commandStorage);
            write(con_fd, "OK\n", 3); 
            printf("OK sent to client\n");
            
            if(strstr(fileName,".c")){
                printf("Saving File Locally\n");
                saveLocally(con_fd, fileSize, fileName,path);
            }else if (strstr(fileName,".pdf")){
                printf("Saving File server 2\n");
                int server_02_fd=createConnectionWithOutServer(server_02_Port_number);
                sendDataToDifferentServer(con_fd, server_02_fd,fileName,fileSize, path);
            }else if(strstr(fileName,".txt")){
                printf("Saving File server 3\n");
                int server_03_fd=createConnectionWithOutServer(server_03_Port_number);
                sendDataToDifferentServer(con_fd, server_03_fd,fileName,fileSize, path);
            }else if(strstr(fileName,".zip")){
                printf("Saving File server 4\n");
                int server_04_fd=createConnectionWithOutServer(server_04_Port_number);
                sendDataToDifferentServer(con_fd, server_04_fd,fileName,fileSize, path);
            }else{
                char *error_message="ERROR:: Wrong File Format\n";
                write(con_fd, error_message, strlen(error_message));
            }
            //Method for Downloading Files
        }else if(sscanf(commandStorage, "1 %s", fileName)==1){
            printf("Command from server received as %s\n",commandStorage);
            if(strstr(fileName,".c")){
                printf("DownLoading File Locally\n");
                downloadFileLocally(con_fd,fileName);
            }else if (strstr(fileName,".pdf")){
                printf("DownLoading File server 2\n");
                int server_02_fd=createConnectionWithOutServer(server_02_Port_number);
                downloadFilesFromOutServer(con_fd,server_02_fd,fileName);
            }else if(strstr(fileName,".txt")){
                printf("DownLoading File server 3\n");
                int server_03_fd=createConnectionWithOutServer(server_03_Port_number);
                downloadFilesFromOutServer(con_fd, server_03_fd,fileName);
            }else if(strstr(fileName,".zip")){
                printf("DownLoading File server 4\n");
                int server_04_fd=createConnectionWithOutServer(server_04_Port_number);
                downloadFilesFromOutServer(con_fd, server_04_fd,fileName);
            }else{
                char *error_message="ERROR:: Wrong File Format\n";
                printf("I AMA HEHE\n");
                write(con_fd, error_message, strlen(error_message));
            }
            //Method for Removing Files
        }else if(sscanf(commandStorage, "2 %s", fileName)==1){
            printf("Command from server received as %s\n",commandStorage);
            if(strstr(fileName,".c")){
                printf("Removing File Locally\n");
                removeFileLocally(con_fd,fileName);
            }else if (strstr(fileName,".pdf")){
                printf("Removing File server 2\n");
                int server_02_fd=createConnectionWithOutServer(server_02_Port_number);
                removeFileFromOutServer(con_fd,server_02_fd,fileName);
            }else if(strstr(fileName,".txt")){
                printf("Removing File server 3\n");
                int server_03_fd=createConnectionWithOutServer(server_03_Port_number);
                removeFileFromOutServer(con_fd, server_03_fd,fileName);
            }else if(strstr(fileName,".zip")){
                printf("Removing File server 4\n");
                int server_04_fd=createConnectionWithOutServer(server_04_Port_number);
                removeFileFromOutServer(con_fd, server_04_fd,fileName);
            }else{
                char *error_message="ERROR:: Wrong File Format\n";
                write(con_fd, error_message, strlen(error_message));
            }
            //Method for Taring Files
        }else if(sscanf(commandStorage, "3 %s", fileName)==1){
            printf("Command from server received as %s\n",commandStorage);
            if(strstr(fileName,".c")){
                printf("TAR File Locally\n");
                createTARfilesLocally(con_fd);
            }else if (strstr(fileName,".pdf")){
                printf("TAR File server 2\n");
                int server_02_fd=createConnectionWithOutServer(server_02_Port_number);
                createTARfilesFromOutServer(con_fd,server_02_fd,fileName);
            }else if(strstr(fileName,".txt")){
                printf("TAR File server 3\n");
                int server_03_fd=createConnectionWithOutServer(server_03_Port_number);
                createTARfilesFromOutServer(con_fd, server_03_fd,fileName);
            }else{
                char *error_message="ERROR:: Wrong File Format\n";
                write(con_fd, error_message, strlen(error_message));
            }
        }else{
            close(con_fd);
            break;
        }
    }
    printf("Connection with client closed\n");
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
    servAdd.sin_port = htons((uint16_t)server_01_Port_number);
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
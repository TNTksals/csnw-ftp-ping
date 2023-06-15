#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

#define BUFFER_SIZE 1500

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
        exit(1);
    }

    char *hostname = argv[1];
    int port = atoi(argv[2]);

    // 创建socket
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0)
        error("Error: cannot create socket.");

    // 获取服务器的IP地址
    struct hostent *he = gethostbyname(hostname);
    if (he == NULL)
        error("Error: cannot get server IP address.");

    // 设置服务器的地址和端口号
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr = *((struct in_addr *)he->h_addr);

    // 连接服务器
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        error("Error: cannot connect to server.");

    // 接收欢迎信息
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    recv(sockfd, buffer, BUFFER_SIZE, 0);
    printf("%s", buffer);

    // 循环处理命令
    while (true)
    {
        // 读取命令
        printf("ftp> ");
        memset(buffer, 0, BUFFER_SIZE);
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strlen(buffer) - 1] = '\0';

        // 解析命令
        char cmd[BUFFER_SIZE];
        char arg[BUFFER_SIZE];
        memset(cmd, 0, BUFFER_SIZE);
        memset(arg, 0, BUFFER_SIZE);
        sscanf(buffer, "%s %s", cmd, arg);

        // 处理命令
        if (strcmp(cmd, "get") == 0 && strlen(arg) > 0)
        {
            // 发送下载文件的命令
            memset(buffer, 0, BUFFER_SIZE);
            sprintf(buffer, "GET %s\r\n", arg);
            send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);

            // 接收文件数据
            FILE *outfile = fopen(arg, "wb");
            if (outfile == NULL)
                error("Error: cannot create local file.");
            int n;
            while ((n = recv(sockfd, buffer, BUFFER_SIZE, 0)) > 0)
                fwrite(buffer, sizeof(char), n, outfile);
            fclose(outfile);

            printf("File downloaded successfully.\n");
        }
        else if (strcmp(cmd, "put") == 0 && strlen(arg) > 0)
        {
            // 发送上传文件的命令
            memset(buffer, 0, BUFFER_SIZE);
            sprintf(buffer, "PUT %s\r\n", arg);
            send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);

            // 发送文件数据
            FILE *infile = fopen(arg, "rb");
            if (infile == NULL)
                error("Error: cannot open local file.");
            int n;
            while ((n = fread(buffer, sizeof(char), BUFFER_SIZE, infile)) > 0)
                send(sockfd, buffer, n, 0);
            fclose(infile);

            printf("File uploaded successfully.\n");
        }
        else if (strcmp(cmd, "pwd") == 0)
        {
            // 发送显示当前目录的命令
            memset(buffer, 0, BUFFER_SIZE);
            sprintf(buffer, "PWD\r\n");
            send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);

            // 接收当前目录信息
            memset(buffer, 0, BUFFER_SIZE);
            recv(sockfd, buffer, BUFFER_SIZE, 0);
            printf("%s", buffer);
        }
        else if (strcmp(cmd, "!pwd") == 0)
        {
            char cwd[BUFFER_SIZE];
            memset(cwd, 0, BUFFER_SIZE);
            getcwd(cwd, BUFFER_SIZE);
            printf("%s\r\n", cwd);
        }
        else if (strcmp(cmd, "dir") == 0)
        {
            // 发送列出当前目录的命令
            memset(buffer, 0, BUFFER_SIZE);
            sprintf(buffer, "DIR\r\n");
            send(sockfd, buffer, strlen(buffer), 0);

            while (true)
            {
                // 设置接收超时时间为 0.5 秒
                struct timeval time_out = {0, 500000};
                setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&time_out, sizeof(time_out));
                // 接收当前目录信息
                memset(buffer, 0, BUFFER_SIZE);
                int n = recv(sockfd, buffer, BUFFER_SIZE, 0);
                if (n < 0)
                    error("Error: cannot receive directory information.");
                if (strstr(buffer, "END") != NULL)
                {
                    char *p = strstr(buffer, "END");
                    buffer[p - buffer] = '\0';
                    printf("%s", buffer);
                    break;
                }
                printf("%s", buffer);
            }
        }
        else if (strcmp(cmd, "!dir") == 0)
        {
            DIR *dir;
            struct dirent *entry;
            struct stat file_stat;
            char cwd[BUFFER_SIZE];
            char time_buf[80];
            memset(cwd, 0, BUFFER_SIZE);
            getcwd(cwd, BUFFER_SIZE);
            dir = opendir(cwd);
            if (dir == NULL)
                printf("Failed to open directory.\r\n");
            else
            {
                while ((entry = readdir(dir)) != NULL)
                {
                    char type[10] = "";
                    if (entry->d_type == DT_REG)
                        strcpy(type, "-");
                    else if (entry->d_type == DT_DIR)
                        strcpy(type, "d");
                    else
                        strcpy(type, "?");

                    char perm[11] = "";
                    if (stat(entry->d_name, &file_stat) == 0)
                    {
                        mode_t mode = file_stat.st_mode;
                        perm[0] = (mode & S_IRUSR) ? 'r' : '-';
                        perm[1] = (mode & S_IWUSR) ? 'w' : '-';
                        perm[2] = (mode & S_IXUSR) ? 'x' : '-';
                        perm[3] = (mode & S_IRGRP) ? 'r' : '-';
                        perm[4] = (mode & S_IWGRP) ? 'w' : '-';
                        perm[5] = (mode & S_IXGRP) ? 'x' : '-';
                        perm[6] = (mode & S_IROTH) ? 'r' : '-';
                        perm[7] = (mode & S_IWOTH) ? 'w' : '-';
                        perm[8] = (mode & S_IXOTH) ? 'x' : '-';
                        perm[9] = '\0';
                    }

                    char owner[32] = "";
                    char group[32] = "";
                    if (getpwuid(file_stat.st_uid) != NULL)
                        strcpy(owner, getpwuid(file_stat.st_uid)->pw_name);
                    if (getgrgid(file_stat.st_gid) != NULL)
                        strcpy(group, getgrgid(file_stat.st_gid)->gr_name);

                    char size[16] = "";
                    if (entry->d_type == DT_REG)
                        sprintf(size, "%ld", file_stat.st_size);
                    else
                        strcpy(size, "-");

                    strftime(time_buf, sizeof(time_buf), "%b %d %H:%M", localtime(&file_stat.st_mtime));

                    printf("%s%s %5.50s %5.50s %5.30s %10.50s %s\r\n", type, perm, owner, group, size, time_buf, entry->d_name);
                }

                closedir(dir);
            }
        }
        else if (strcmp(cmd, "cd") == 0)
        {
            // 发送改变当前目录的命令
            memset(buffer, 0, BUFFER_SIZE);
            sprintf(buffer, "CD %s\r\n", arg);
            send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);

            // 接收服务器返回的信息
            memset(buffer, 0, BUFFER_SIZE);
            recv(sockfd, buffer, BUFFER_SIZE, 0);
            printf("%s", buffer);
        }
        else if (strcmp(cmd, "!cd") == 0)
        {
            if (chdir(arg) < 0)
                printf("cd: %s: No such file or directory\r\n", arg);
            else
                printf("Directory changed.\r\n");
        }
        else if (strcmp(cmd, "size") == 0)
        {
            // 发送获取文件大小的命令
            memset(buffer, 0, BUFFER_SIZE);
            sprintf(buffer, "SIZE %s\r\n", arg);
            send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);

            // 接收服务器返回的信息
            memset(buffer, 0, BUFFER_SIZE);
            recv(sockfd, buffer, BUFFER_SIZE, 0);
            printf("%s", buffer);
        }
        else if (strcmp(cmd, "syst") == 0)
        {
            // 发送改变当前目录的命令
            memset(buffer, 0, BUFFER_SIZE);
            sprintf(buffer, "SYST\r\n");
            send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);

            // 接收服务器返回的信息
            memset(buffer, 0, BUFFER_SIZE);
            recv(sockfd, buffer, BUFFER_SIZE, 0);
            printf("%s", buffer);
        }
        else if (strcmp(cmd, "quit") == 0)
        {
            // 发送退出命令
            memset(buffer, 0, BUFFER_SIZE);
            sprintf(buffer, "QUIT\r\n");
            send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);

            // 接收服务器返回的信息
            memset(buffer, 0, BUFFER_SIZE);
            recv(sockfd, buffer, BUFFER_SIZE, 0);
            printf("%s", buffer);

            break;
        }
        else if (strcmp(cmd, "?") == 0)
        {
            // 显示帮助信息
            printf("get <arg> - download a file from the server\n");
            printf("put <arg> - upload a file to the server\n");
            printf("pwd - display the current directory on the server\n");
            printf("dir - list the files in the current directory on the server\n");
            printf("cd <directory> - change the current directory on the server\n");
            printf("!pwd - display the current directory on the client\n");
            printf("!dir - list the files in the current directory on the client\n");
            printf("!cd <directory> - change the current directory on the client\n");
            printf("? - display this help message\n");
            printf("quit - exit the program\n");
        }
        else
        {
            printf("Invalid cmd. Type '?' for help.\n");
        }
    }

    // 关闭socket
    close(sockfd);

    return 0;
}
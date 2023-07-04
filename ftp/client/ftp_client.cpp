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

#define BUFFER_SIZE 1024

/**
 * @brief 输出错误信息并退出程序
 * @param msg 错误信息
 */
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

/**
 * @brief 显示帮助信息
 */
void display_help_info()
{
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

/**
 * @brief 发送退出命令并接收服务器返回的信息
 * @param sockfd 套接字文件描述符
 * @param buffer 缓冲区指针
 */
void quit(int sockfd, char *buffer)
{
    // 发送退出命令
    memset(buffer, 0, BUFFER_SIZE);
    sprintf(buffer, "QUIT\r\n");
    send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);

    // 接收服务器返回的信息
    memset(buffer, 0, BUFFER_SIZE);
    recv(sockfd, buffer, BUFFER_SIZE, 0);
    printf("%s", buffer);
}

/**
 * @brief 显示服务器端当前系统类型
 * @param sockfd 套接字文件描述符
 * @param buffer 缓冲区指针
 */
void show_remote_system_type(int sockfd, char *buffer)
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

/**
 * @brief 显示远程服务器端指定文件的大小
 * @param sockfd socket文件描述符
 * @param buffer 缓冲区指针
 * @param filename 文件名
 */
void show_remote_file_size(int sockfd, char *buffer, const char *filename)
{
    // 发送获取文件大小的命令
    memset(buffer, 0, BUFFER_SIZE);
    sprintf(buffer, "SIZE %s\r\n", filename);
    send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);

    // 接收服务器返回的信息
    memset(buffer, 0, BUFFER_SIZE);
    recv(sockfd, buffer, BUFFER_SIZE, 0);
    printf("%s", buffer);
}

/**
 * @brief 改变本地工作目录
 * @param path 目录路径
 */
void change_local_directory(const char *path)
{
    if (chdir(path) < 0)
        printf("cd: %s: No such file or directory\r\n", path);
    else
        printf("Directory changed.\r\n");
}

/**
 * @brief 改变远程服务器端工作目录
 * @param sockfd socket文件描述符
 * @param buffer 缓冲区指针
 * @param path 目录路径
 */
void change_remote_directory(int sockfd, char *buffer, const char *path)
{
    // 发送改变当前目录的命令
    memset(buffer, 0, BUFFER_SIZE);
    sprintf(buffer, "CD %s\r\n", path);
    send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);

    // 接收服务器返回的信息
    memset(buffer, 0, BUFFER_SIZE);
    recv(sockfd, buffer, BUFFER_SIZE, 0);
    printf("%s", buffer);
}

/**
 * @brief 显示本地当前目录下的文件和子目录信息
 */

/**
 * @brief 显示本地当前目录下的文件和子目录信息
 */
void show_local_directory_info()
{
    DIR *dir;              // 目录指针
    struct dirent *entry;  // 目录项指针
    struct stat file_stat; // 文件状态结构体
    char cwd[BUFFER_SIZE]; // 当前工作目录
    char time_buf[80];     // 时间字符串缓冲区
    memset(cwd, 0, BUFFER_SIZE);
    getcwd(cwd, BUFFER_SIZE); // 获取当前工作目录
    dir = opendir(cwd);       // 打开当前工作目录
    if (dir == NULL)
        printf("Failed to open directory.\r\n");
    else
    {
        while ((entry = readdir(dir)) != NULL) // 遍历目录项
        {
            char type[10] = "";          // 文件类型
            if (entry->d_type == DT_REG) // 普通文件
                strcpy(type, "-");
            else if (entry->d_type == DT_DIR) // 目录文件
                strcpy(type, "d");
            else // 其他类型文件
                strcpy(type, "?");

            char perm[11] = "";                       // 文件权限
            if (stat(entry->d_name, &file_stat) == 0) // 获取文件状态
            {
                mode_t mode = file_stat.st_mode;        // 文件模式
                perm[0] = (mode & S_IRUSR) ? 'r' : '-'; // 用户读权限
                perm[1] = (mode & S_IWUSR) ? 'w' : '-'; // 用户写权限
                perm[2] = (mode & S_IXUSR) ? 'x' : '-'; // 用户执行权限
                perm[3] = (mode & S_IRGRP) ? 'r' : '-'; // 组读权限
                perm[4] = (mode & S_IWGRP) ? 'w' : '-'; // 组写权限
                perm[5] = (mode & S_IXGRP) ? 'x' : '-'; // 组执行权限
                perm[6] = (mode & S_IROTH) ? 'r' : '-'; // 其他用户读权限
                perm[7] = (mode & S_IWOTH) ? 'w' : '-'; // 其他用户写权限
                perm[8] = (mode & S_IXOTH) ? 'x' : '-'; // 其他用户执行权限
                perm[9] = '\0';
            }

            char owner[32] = "";                    // 文件所有者
            char group[32] = "";                    // 文件所属组
            if (getpwuid(file_stat.st_uid) != NULL) // 获取文件所有者
                strcpy(owner, getpwuid(file_stat.st_uid)->pw_name);
            if (getgrgid(file_stat.st_gid) != NULL) // 获取文件所属组
                strcpy(group, getgrgid(file_stat.st_gid)->gr_name);

            char size[16] = "";          // 文件大小
            if (entry->d_type == DT_REG) // 普通文件
                sprintf(size, "%ld", file_stat.st_size);
            else // 目录文件或其他类型文件
                strcpy(size, "-");

            strftime(time_buf, sizeof(time_buf), "%b %d %H:%M", localtime(&file_stat.st_mtime)); // 格式化时间

            printf("%s%s %5.50s %5.50s %5.30s %10.50s %s\r\n", type, perm, owner, group, size, time_buf, entry->d_name); // 输出文件信息
        }

        closedir(dir); // 关闭目录
    }
}

/**
 * @brief 显示服务器端当前目录下的文件和子目录信息
 * @param sockfd 套接字文件描述符
 * @param buffer 缓冲区指针
 */
void show_remote_directory_info(int sockfd, char *buffer)
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

/**
 * @brief 显示本地当前目录路径
 */
void show_local_directory_path()
{
    char cwd[BUFFER_SIZE];
    memset(cwd, 0, BUFFER_SIZE);
    getcwd(cwd, BUFFER_SIZE);
    printf("%s\r\n", cwd);
}

/**
 * @brief 显示服务器端当前目录路径
 * @param sockfd 套接字文件描述符
 * @param buffer 缓冲区指针
 */
void show_remote_directory_path(int sockfd, char *buffer)
{
    // 发送显示当前目录路径的命令
    memset(buffer, 0, BUFFER_SIZE);
    sprintf(buffer, "PWD\r\n");
    send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);

    // 接收当前目录路径信息
    memset(buffer, 0, BUFFER_SIZE);
    recv(sockfd, buffer, BUFFER_SIZE, 0);
    printf("%s", buffer);
}

/**
 * @brief 上传文件到服务器
 * @param sockfd 套接字文件描述符
 * @param buffer 缓冲区指针
 * @param filename 文件名
 */
void upload_file(int sockfd, char *buffer, const char *filename)
{
    // 发送上传文件的命令
    memset(buffer, 0, BUFFER_SIZE);
    sprintf(buffer, "PUT %s\r\n", filename);
    send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);

    // 打开本地文件
    FILE *infile = fopen(filename, "rb");
    if (infile == NULL)
        error("Error: cannot open local file.");

    // 发送文件数据
    int n;
    while ((n = fread(buffer, sizeof(char), BUFFER_SIZE, infile)) > 0)
    {
        send(sockfd, buffer, n, 0);
        printf("Sent %d bytes.\n", n);
    }

    fclose(infile);

    printf("File uploaded successfully.\n");
}

/**
 * @brief 下载服务器端文件到本地
 * @param sockfd 套接字文件描述符
 * @param buffer 缓冲区指针
 * @param filename 文件名
 */
void download_file(int sockfd, char *buffer, const char *filename)
{
    // 发送下载文件的命令
    memset(buffer, 0, BUFFER_SIZE);
    sprintf(buffer, "GET %s\r\n", filename);
    send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);

    // 创建本地文件
    FILE *outfile = fopen(filename, "wb");
    if (outfile == NULL)
        error("Error: cannot create local file");

    // 接收文件数据
    int n;
    while (true)
    {
        // 使用select函数等待socket变为可读
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);

        // 设置超时时间为1秒
        struct timeval timeout = {1, 0};
        /**
         * select函数用于等待socket变为可读.
         * 第一个参数为最大的socket文件描述符加1.
         * 第二个参数为可读的socket集合.
         * 第三个参数为可写的socket集合.
         * 第四个参数为异常的socket集合.
         * 第五个参数为超时时间.
         * 返回值为可读的socket数量.
         */
        int ret = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
        if (ret == -1)
            error("Error: select function failed");
        else if (ret == 0)
            error("Timeout");
        else
        {
            // socket变为可读，使用recv函数接收数据
            n = recv(sockfd, buffer, BUFFER_SIZE, 0);
            if (n == -1)
                error("Error receiving message from server");
            else if (n == 0)
                error("FTP server closed connection");
            else
            {
                // 接收到了数据
                buffer[n] = '\0';
                if (strstr(buffer, "550 Failed to open file") != NULL)
                {
                    printf("550 Failed to download file.\n");
                    return;
                }
                if (strstr(buffer, "EOF") != NULL)
                {
                    char *p = strstr(buffer, "EOF");
                    fwrite(buffer, sizeof(char), p - buffer, outfile);
                    printf("Received %ld bytes.\n", p - buffer);
                    break;
                }
                fwrite(buffer, sizeof(char), n, outfile);
                printf("Received %d bytes.\n", n);
            }
        }
    }

    fclose(outfile);

    printf("File downloaded successfully.\n");
}

/**
 * @brief 连接到服务器
 * @param hostname 服务器主机名
 * @param port 服务器端口号
 * @return 套接字文件描述符
 */
int connect_to_server(const char *hostname, int port)
{
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

    return sockfd;
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

    int sockfd = connect_to_server(hostname, port);

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
        /**
         * sscanf函数的第一个参数是要读取的字符串, 第二个参数是格式化字符串, 用于指定读取的数据类型和格式.
         * 读取的数据会按照格式化字符串中的格式进行解析, 并按照参数列表的顺序存储到后面的参数中.
         * 在这里，格式化字符串"%s %s"表示读取两个字符串，中间用空格分隔.
         * 读取到的第一个字符串存储到cmd数组中，第二个字符串存储到arg数组中.
         * 这行代码用于解析用户输入的命令和参数.
         */
        sscanf(buffer, "%s %s", cmd, arg);

        // 处理命令
        if (strcmp(cmd, "get") == 0 && strlen(arg) > 0)
        {
            download_file(sockfd, buffer, arg);
        }
        else if (strcmp(cmd, "put") == 0 && strlen(arg) > 0)
        {
            upload_file(sockfd, buffer, arg);
        }
        else if (strcmp(cmd, "pwd") == 0)
        {
            show_remote_directory_path(sockfd, buffer);
        }
        else if (strcmp(cmd, "!pwd") == 0)
        {
            show_local_directory_path();
        }
        else if (strcmp(cmd, "dir") == 0)
        {
            show_remote_directory_info(sockfd, buffer);
        }
        else if (strcmp(cmd, "!dir") == 0)
        {
            show_local_directory_info();
        }
        else if (strcmp(cmd, "cd") == 0)
        {
            change_remote_directory(sockfd, buffer, arg);
        }
        else if (strcmp(cmd, "!cd") == 0)
        {
            change_local_directory(arg);
        }
        else if (strcmp(cmd, "size") == 0)
        {
            show_remote_file_size(sockfd, buffer, arg);
        }
        else if (strcmp(cmd, "syst") == 0)
        {
            show_remote_system_type(sockfd, buffer);
        }
        else if (strcmp(cmd, "quit") == 0)
        {
            quit(sockfd, buffer);
            break;
        }
        else if (strcmp(cmd, "?") == 0)
        {
            display_help_info();
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
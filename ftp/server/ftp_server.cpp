#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 3

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
 * @brief 从指定的套接字接收文件数据并保存到指定的文件中
 * @param sockfd 套接字文件描述符
 * @param buffer 缓冲区指针
 * @param filename 要保存的文件名
 */
void recv_file(int sockfd, char *buffer, const char *filename)
{
    memset(buffer, 0, BUFFER_SIZE);

    // 创建本地文件
    FILE *outfile = fopen(filename, "wb");
    if (outfile == NULL)
    {
        memset(buffer, 0, BUFFER_SIZE);
        sprintf(buffer, "550 Failed to create file.\r\n");
        send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);
        return;
    }

    // // 接收文件数据
    // int n;
    // while ((n = recv(sockfd, buffer, BUFFER_SIZE, 0)) > 0)
    //     fwrite(buffer, sizeof(char), n, outfile);

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
        int ret = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
        if (ret == -1)
            error("Error: select function failed", 0);
        else if (ret == 0)
            error("Timeout", 0);
        else
        {
            // socket变为可读，使用recv函数接收数据
            n = recv(sockfd, buffer, BUFFER_SIZE, 0);
            if (n == -1)
                error("Error receiving message from client", 0);
            else if (n == 0)
                error("FTP client closed connection", 0);
            else
            {
                // 接收到了数据
                if (strstr(buffer, "EOF") != NULL)
                {
                    char *p = strstr(buffer, "EOF");
                    fwrite(buffer, sizeof(char), p - buffer, outfile);
                    break;
                }
                fwrite(buffer, sizeof(char), n, outfile);
            }
        }
    }

    fclose(outfile);

    memset(buffer, 0, BUFFER_SIZE);
    sprintf(buffer, "226 Transfer complete.\r\n");
    send(sockfd, buffer, strlen(buffer), 0);

    printf("File transfer complete.\r\n");
}

/**
 * @brief 从指定的套接字发送指定文件的内容
 * @param sockfd 套接字文件描述符
 * @param buffer 缓冲区指针
 * @param filename 要发送的文件名
 */
void send_file(int sockfd, char *buffer, const char *filename)
{
    memset(buffer, 0, BUFFER_SIZE);

    // 打开本地文件
    FILE *infile = fopen(filename, "rb");
    if (infile == NULL)
    {
        memset(buffer, 0, BUFFER_SIZE);
        sprintf(buffer, "550 Failed to open file.\r\n");
        send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);
        return;
    }

    // 发送文件数据
    int n;
    while ((n = fread(buffer, sizeof(char), BUFFER_SIZE, infile)) > 0)
        send(sockfd, buffer, n, 0);

    memset(buffer, 0, BUFFER_SIZE);
    sprintf(buffer, "EOF\r\n");
    send(sockfd, buffer, strlen(buffer), 0);

    fclose(infile);

    // 输出文件传输完成信息
    printf("File transfer complete.\r\n");
}

/**
 * @brief 向客户端发送指定文件的大小信息
 * @param sockfd 套接字文件描述符
 * @param buffer 缓冲区指针
 * @param filename 要计算大小的文件名
 */
void send_file_size(int sockfd, char *buffer, const char *filename)
{
    std::ifstream infile(filename, std::ios::in | std::ios::binary);
    if (!infile)
    {
        memset(buffer, 0, BUFFER_SIZE);
        sprintf(buffer, "550 Failed to open file.\r\n");
        send(sockfd, buffer, strlen(buffer), 0);
    }
    else
    {
        infile.seekg(0, std::ios::end);
        int size = infile.tellg();
        infile.close();

        memset(buffer, 0, BUFFER_SIZE);
        sprintf(buffer, "%d bytes\r\n", size);
        send(sockfd, buffer, strlen(buffer), 0);
    }
}

/**
 * @brief 从指定的套接字发送指定目录的文件列表
 * @param sockfd 套接字文件描述符
 * @param buffer 缓冲区指针
 */
void send_directory_list(int sockfd, char *buffer)
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
    {
        memset(buffer, 0, BUFFER_SIZE);
        sprintf(buffer, "550 Failed to open directory.\r\n");
        send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);
    }
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

            memset(buffer, 0, BUFFER_SIZE);
            sprintf(buffer, "%s%s %5.50s %5.50s %5.30s %10.50s %s\r\n", type, perm, owner, group, size, time_buf, entry->d_name);
            send(sockfd, buffer, strlen(buffer), 0);
        }

        closedir(dir);

        memset(buffer, 0, BUFFER_SIZE);
        sprintf(buffer, "END");
        send(sockfd, buffer, strlen(buffer), 0);
        printf("Directory send OK.\n");
    }
}

/**
 * @brief 更改当前工作目录
 * @param sockfd 套接字描述符
 * @param buffer 缓冲区指针
 * @param path 目录路径
 */
void change_directory(int sockfd, char *buffer, const char *path)
{
    if (chdir(path) < 0)
    {
        memset(buffer, 0, BUFFER_SIZE);
        sprintf(buffer, "cd: %s: No such file or directory\r\n", path);
        send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);
    }
    else
    {
        memset(buffer, 0, BUFFER_SIZE);
        sprintf(buffer, "Directory changed.\r\n");
        send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);
    }
}

/**
 * @brief 向客户端发送当前工作目录的路径
 * @param sockfd 套接字描述符
 * @param buffer 缓冲区指针
 */
void send_current_directory_path(int sockfd, char *buffer)
{
    char cwd[BUFFER_SIZE];
    memset(cwd, 0, BUFFER_SIZE);
    getcwd(cwd, BUFFER_SIZE);
    memset(buffer, 0, BUFFER_SIZE);
    sprintf(buffer, "%s\r\n", cwd);
    send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);
}

/**
 * @brief 从指定的套接字发送系统信息
 * @param sockfd 套接字文件描述符
 * @param buffer 缓冲区指针
 */
void send_system_info(int sockfd, char *buffer)
{
    memset(buffer, 0, BUFFER_SIZE);

#ifdef _WIN32
    OSVERSIONINFO osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&osvi);
    sprintf(buffer, "Windows %d.%d.%d\r\n", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
#elif __linux__
    struct utsname uts;
    uname(&uts);
    sprintf(buffer, "Linux %s %s\r\n", uts.release, uts.machine);
#elif __APPLE__
    char version[256];
    size_t len = sizeof(version);
    sysctlbyname("kern.osrelease", &version, &len, NULL, 0);
    sprintf(buffer, "macOS %s\r\n", version);
#else
    sprintf(buffer, "Unknown\r\n");
#endif

    send(sockfd, buffer, strlen(buffer), 0);
}

/**
 * @brief 向客户端发送“Goodbye.”的消息
 * @param sockfd 套接字描述符
 * @param buffer 缓冲区指针
 */
void send_goodbye_message(int sockfd, char *buffer)
{
    memset(buffer, 0, BUFFER_SIZE);
    sprintf(buffer, "Goodbye.\r\n");
    send(sockfd, buffer, strlen(buffer), 0);
}

/**
 * @brief 处理与客户端的通信
 * @param sockfd 套接字描述符
 * @param client_addr 客户端地址
 */
void handle_client(int new_sockfd, struct sockaddr_in client_addr)
{
    // 处理与客户端的通信
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    sprintf(buffer, "Welcome to ftp server!\r\n");
    send(new_sockfd, buffer, strlen(buffer), 0);

    while (true)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int n = recv(new_sockfd, buffer, BUFFER_SIZE - 1, 0);
        if (n < 0)
            error("Error: cannot receive data from client");
        else if (n == 0)
        {
            printf("Client disconnected. IP address: %s, port: %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            break;
        }

        printf("Received data from client: %s", buffer);

        // 解析命令和参数
        char cmd[5];
        char arg[BUFFER_SIZE];
        memset(cmd, 0, 5);
        memset(arg, 0, BUFFER_SIZE);
        sscanf(buffer, "%4s %[^\r\n]", cmd, arg);

        // 处理命令
        if (strcmp(cmd, "QUIT") == 0)
        {
            send_goodbye_message(new_sockfd, buffer);
            break;
        }
        else if (strcmp(cmd, "SYST") == 0)
        {
            send_system_info(new_sockfd, buffer);
        }
        else if (strcmp(cmd, "PWD") == 0)
        {
            send_current_directory_path(new_sockfd, buffer);
        }
        else if (strcmp(cmd, "CD") == 0)
        {
            change_directory(new_sockfd, buffer, arg);
        }
        else if (strcmp(cmd, "DIR") == 0)
        {
            send_directory_list(new_sockfd, buffer);
        }
        else if (strcmp(cmd, "SIZE") == 0)
        {
            send_file_size(new_sockfd, buffer, arg);
        }
        else if (strcmp(cmd, "GET") == 0)
        {
            send_file(new_sockfd, buffer, arg);
        }
        else if (strcmp(cmd, "PUT") == 0)
        {
            recv_file(new_sockfd, buffer, arg);
        }
        else
        {
            // 发送无效命令信息
            memset(buffer, 0, BUFFER_SIZE);
            sprintf(buffer, "Invalid command.\r\n");
            send(new_sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);
        }
    }

    // 关闭socket
    close(new_sockfd);
}

/**
 * @brief 启动FTP服务器，监听指定端口号，并处理客户端连接请求
 * @param port 服务器要监听的端口号
 * @return 返回创建的套接字文件描述符
 */
int start_server(int port)
{
    // 创建套接字
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0)
        error("Error: cannot create socket");

    // 设置服务器的地址和端口号
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // 绑定socket到指定的端口号
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        error("Error: cannot bind socket to port");

    // 设置socket为监听状态
    if (listen(sockfd, MAX_CLIENTS) < 0)
        error("Error: cannot listen on socket");

    printf("Server started. Listening on port %d...\n", port);

    return sockfd;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);

    int sockfd = start_server(port);

    while (true)
    {
        // 接受客户端的连接请求
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int new_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (new_sockfd < 0)
            error("Error: cannot accept client connection");

        printf("Client connected. IP address: %s, port: %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // 处理与客户端的通信
        handle_client(new_sockfd, client_addr);
    }

    close(sockfd);

    return 0;
}
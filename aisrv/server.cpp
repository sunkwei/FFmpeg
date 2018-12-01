#include <stdio.h>
#include <stdlib.h>
#include <cc++/socket.h>
#include "analyse.h"

/** 因为每个点播需要一个独立的 ffmpeg 进程，如果每个进程都加载一份模型，将导致一台主机上
 *  跑不了几个进程，所以思路为：
 * 
 *     1. 实现 ffmpeg filter (sink) 接送图片，将图片转发到 ai 服务；
 *     2. ai 接收图片，分析，将分析结果返回 filter
 *     3. filter 将分析结果提交到中心服务 ...
 *  
 *  ai server 通过AF_UNIX接收 client 的图片，分析后，将结果 json 格式字符串返回。
 * 
 */

const char *_sock_name = "/tmp/zonekey-aisrv.socket";

static int readn(int fd, char *ptr, int off, int n)
{
    char *end = ptr + n;
    while (ptr < end) {
        int r = read(fd, ptr, (end-ptr));
        if (r <= 0) {
            return -1;  // n == 0 eof，此时不应该发生 ...
        }

        ptr += r;
    }

    return n;
}

int main(int argc, char **argv)
{
    unlink(_sock_name);
    int sock_srv = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (sock_srv == -1) {
        perror("socket");
        return 1;
    }

    sockaddr_un name;
    memset(&name, 0, sizeof(sockaddr_un));

    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, _sock_name, sizeof(name.sun_path) - 1);

    int rc = bind(sock_srv, (sockaddr*)&name, sizeof(name));
    if (rc == -1) {
        perror("bind");
        close(sock_srv);
        return 1;
    }

    rc = listen(sock_srv, 100);
    if (rc == -1) {
        perror("listen");
        close(sock_srv);
        return 1;
    }

    // 一张图片为 1920x1080 BGR24，4 为后面长度，一般总是 1920x1080x3
    const int WIDTH = 1920, HEIGHT = 1080;
    char *buf = (char*)malloc(WIDTH * HEIGHT * 3 + 4 + 32);

    while (1) {
        int sock_worker = accept(sock_srv, NULL, NULL);
        if (sock_worker == -1) {
            perror("accept");
            return 1;
        }

        // 读长度
        int rc = readn(sock_worker, buf, 0, 4);
        if (rc != 4) {
            perror("recv img len");
            close(sock_worker);
            continue;
        }
        int len = *(int*)buf;   // 直接使用主机序吧 ..

        // 读图像
        rc = readn(sock_worker, buf, 0, len);
        if (rc != len) {
            perror("recv img");
            close(sock_worker);
            continue;
        }

        // 分析
        const char *result = analyse(WIDTH, HEIGHT, buf);

        // 返回结果
        rc = write(sock_worker, result, strlen(result)+1);
        if (rc != strlen(result)+1) {
            perror("send result");
            close(sock_worker);
            continue;
        }

        // close
        close(sock_worker);
    }

    free(buf);
    close(sock_srv);

    return 0;
}

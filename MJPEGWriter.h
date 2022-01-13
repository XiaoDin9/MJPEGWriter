#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <opencv2/opencv.hpp>

#define PORT            unsigned short
#define SOCKET          int
#define HOSTENT         struct hostent
#define SOCKADDR        struct sockaddr
#define SOCKADDR_IN     struct sockaddr_in
#define ADDRPOINTER     unsigned int*
#define INVALID_SOCKET  (-1)
#define SOCKET_ERROR    (-1)
#define TIMEOUT_M       (200000)
#define NUM_CONNECTIONS (10)

using namespace cv;
using namespace std;

struct clientFrame {
    uchar   *outbuf;            // 指向保存的相应数据
    int     outlen;             // 响应数据的长度值
    int     client;             // client 的socet 句柄
};

struct clientPayload {
    void            *context;   // JPEGTOWEB 对象的句柄
    clientFrame     cf;
};

class JPEGTOWEB
{
public:
    JPEGTOWEB(int port = 0)
            : sock(INVALID_SOCKET)
            , timeout(TIMEOUT_M)
            , quality(90)
            , port(port)
    {
        signal(SIGPIPE, SIG_IGN);
        FD_ZERO(&master);

        // if (port)
        //     open(port);
    }

    ~JPEGTOWEB()
    {
        release();
    }

    bool release()
    {
        if (sock != INVALID_SOCKET)
            shutdown(sock, 2);

        sock = (INVALID_SOCKET);
        return false;
    }

    bool open()
    {
        // 1. 初始化 socket，得到文件描述符
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        SOCKADDR_IN address;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_family      = AF_INET;
        address.sin_port        = htons(port);          // 大小端转换

        // 2. bind，将绑定在 IP 地址和端口;
        if (::bind(sock, (SOCKADDR*)&address, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
        {
            cerr << "error : couldn't bind sock " << sock << " to port " << port << "!" << endl;
            return release();
        }

        // 3. 如果作为服务器，使用listen来监听这个socket，即：对端口号为 port 进行监听
        // 第一个参数为要监听的socket描述符，
        // 第二个为排队的最大连接数，该函数将socket变为被动类型，等待客户的连接请求。
        if (listen(sock, NUM_CONNECTIONS) == SOCKET_ERROR)
        {
            cerr << "error : couldn't listen on sock " << sock << " on port " << port << " !" << endl;
            return release();
        }

        FD_SET(sock, &master);
        return true;
    }

    /**
     * 判断 server 是否打开（socket 句柄是否有效）
     * @return
     */
    bool isOpened()
    {
        return sock != INVALID_SOCKET;
    }

    void start()
    {
        pthread_mutex_lock(&mutex_writer);
        // 创建 listern 线程 和  writer 线程
        pthread_create(&thread_listen, NULL, this->listen_Helper, this);
        pthread_create(&thread_write, NULL, this->writer_Helper, this);
    }

    void stop()
    {
        this->release();

        pthread_join(thread_listen, NULL);
        pthread_join(thread_write, NULL);
    }

    void write(Mat frame)
    {
        pthread_mutex_lock(&mutex_writer);
        if( !frame.empty() )
        {
            lastFrame.release();
            lastFrame = frame.clone();
        }
        pthread_mutex_unlock(&mutex_writer);
    }

private:
    int _write(int sock, char *s, int len)
    {
        if (len < 1)
            len = strlen(s);

        {
            try
            {
                // 把 s 指向的缓冲区的数据 send 到 sock 句柄的缓冲区中（返回给client)
                int retval = ::send(sock, s, len, 0x4000);
                return retval;
            }
            catch (int e)
            {
                cout << "An exception occurred. Exception Nr. " << e << '\n';
            }
        }

        return -1;
    }

    int _read(int socket, char* buffer)
    {
        // 从 client 的句柄缓冲区中接收数据，并保存到 buffer 指向的内存中去
        // result 表示实际 recv 的数据大小
        int result = recv(socket, buffer, 4096, MSG_PEEK);
        if (result < 0 )
        {
            cout << "An exception occurred. Exception Nr. " << result << '\n';
            return result;
        }

        string s = buffer;
        buffer = (char*) s.substr(0, (int) result).c_str();
        return result;
    }

    static void* listen_Helper(void* context)
    {
        ((JPEGTOWEB *)context)->Listener();
        return NULL;
    }

    static void* writer_Helper(void* context)
    {
        ((JPEGTOWEB *)context)->Writer();
        return NULL;
    }

    static void* clientWrite_Helper(void* payload)
    {
        void* ctx = ((clientPayload *)payload)->context;
        struct clientFrame cf = ((clientPayload *)payload)->cf;
        ((JPEGTOWEB *)ctx)->ClientWrite(cf);

        return NULL;
    }

    void Listener();
    void Writer();
    void ClientWrite(clientFrame &cf);

private:
    SOCKET      sock;
    fd_set      master;
    int         timeout;
    int         quality;                // jpeg compression [1..100]

    std::vector<int>    clients;
    pthread_t           thread_listen;
    pthread_t           thread_write;
    pthread_mutex_t     mutex_client    = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t     mutex_cout      = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t     mutex_writer    = PTHREAD_MUTEX_INITIALIZER;

    Mat                 lastFrame;
    int                 port;
};

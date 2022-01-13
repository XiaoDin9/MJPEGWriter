#include <fstream>
#include "jpeg-to-web.h"

/**
 * 监听 client 的每次连接：
 * FOR: 每次当 accept 成功后：
 * （1）mutex_cout 构造临界区：  从 client_fd 缓冲区中去读取数据，打印 client 的请求信息(url)
 * （2）mutex_client 构成临界区：把 clinet 的fd 加入 vector (clients 容器）中去，并把 http 的头部信息 send 到 client
 */
void JPEGTOWEB::Listener()
{
	// send http header
    std::string header;
    header += "HTTP/1.0 200 OK\r\n";
    header += "Cache-Control: no-cache\r\n";
    header += "Pragma: no-cache\r\n";
    header += "Connection: close\r\n";
    header += "Content-Type: multipart/x-mixed-replace; boundary=mjpegstream\r\n\r\n";
    const int header_size = header.size();
    char *header_data = (char*)header.data();

    fd_set rread;
    SOCKET maxfd;

    this->open();

    pthread_mutex_unlock(&mutex_writer);

    while (true)
    {
        rread = master;
        struct timeval to = { 0, timeout };
        maxfd = sock + 1;

        if (sock == INVALID_SOCKET){
        	return;
        }

        int sel = select(maxfd, &rread, NULL, NULL, &to);
        if (sel > 0)
        {
            for (int s = 0; s < maxfd; s++)
            {
                if (FD_ISSET(s, &rread) && s == sock)
                {
                    int         addrlen = sizeof(SOCKADDR);
                    SOCKADDR_IN address = { 0 };

                    // 4、等待客户端连接；（阻塞）
                    SOCKET      client = accept(sock, (SOCKADDR*)&address, (socklen_t*)&addrlen);
                    if (client == SOCKET_ERROR)
                    {
                        cerr << "error : couldn't accept connection on sock " << sock << " !" << endl;
                        return;
                    }

                    maxfd = (maxfd > client ? maxfd : client);

                    pthread_mutex_lock(&mutex_cout);
                    // 打印 client 请求的数据
                    cout << "new client " << client << endl;
                    char headers[4096] = "\0";
                    int readBytes = _read(client, headers);
                    cout << headers;
                    pthread_mutex_unlock(&mutex_cout);

                    pthread_mutex_lock(&mutex_client);
                    // 响应的头部： 写 http 头部数据到 client
                    _write(client, header_data, header_size);
                    clients.push_back(client);                      // 并把 client 的fd 加入到 vector 中
                    pthread_mutex_unlock(&mutex_client);
                }
            }
        }

        usleep(1000);
    }
}

void JPEGTOWEB::Writer()
{
    pthread_mutex_lock(&mutex_writer);
    pthread_mutex_unlock(&mutex_writer);
    const int milis2wait = 16666;

    // 如果 server 端的服务还是开着的，一直循环
    while ( this->isOpened() )
    {
        // mutex_client 构成临界区：
        // client 是全局的，所以需要加锁（因为在 listen 线程中需要加入 client 的句柄，
        // 在 writer 线程中需要使用 vector )
        pthread_mutex_lock(&mutex_client);
        int num_connected_clients = clients.size();
        pthread_mutex_unlock(&mutex_client);

        if (!num_connected_clients) {
            usleep(milis2wait);
            continue;
        }

        pthread_t           threads[NUM_CONNECTIONS];
        int                 count = 0;          // 记录需要给多少个 client 响应数据
        std::vector<uchar>  outbuf;             // 保存编码后的一帧图像数据
        std::vector<int>    params;             // 保存编码相关的参数值

        // 设置图像编码压缩的相关参数
        params.push_back(IMWRITE_JPEG_QUALITY);
        params.push_back(quality);

        // mutex_writer 构成临界区：( main 线程把数据写入到全局变量 lastFrame 中，
        // writer 线程从全局变量中读取数据）
        pthread_mutex_lock(&mutex_writer);
        imencode(".jpg", lastFrame, outbuf, params);
        pthread_mutex_unlock(&mutex_writer);
        int outlen = outbuf.size();             // 保存编码后的数据大小

        // mutex_client 构成临界区：( listen 线程中需要加入 client 的句柄，
        // 在 writer 线程中需要使用 vector ）
        pthread_mutex_lock(&mutex_client);
        std::vector<int>::iterator begin    = clients.begin();
        std::vector<int>::iterator end      = clients.end();
        pthread_mutex_unlock(&mutex_client);

        std::vector<clientPayload*> payloads;
        for (std::vector<int>::iterator it = begin; it != end; ++it, ++count)
        {
            if (count > NUM_CONNECTIONS)
                break;

            struct clientPayload *cp = new clientPayload({ (JPEGTOWEB*)this, { outbuf.data(), outlen, *it } });
            payloads.push_back(cp);

            // 创建一个 clientWrite_Helper 线程，线程编号： threads[count]
            pthread_create(&threads[count], NULL, &JPEGTOWEB::clientWrite_Helper, cp);
        }

        // 等待所有的线程完成（把编码的图像数据发送到给所有的 client )
        for (; count > 0; count--)
        {
            pthread_join(threads[count-1], NULL);
            delete payloads.at(count-1);
        }

        usleep(milis2wait);
    }
}

void JPEGTOWEB::ClientWrite(clientFrame & cf)
{
    stringstream head;
    head << "--mjpegstream\r\nContent-Type: image/jpeg\r\nContent-Length: " << cf.outlen << "\r\n\r\n";
    string string_head = head.str();

    // mutex_client 构造的临界区：（Writer 线程和 lister 线程， clientWrier 线程都需要使用全局容器 clients），所以要加锁
    pthread_mutex_lock(&mutex_client);
    // 响应： http 的头部一些数据给 client
    _write(cf.client, (char*) string_head.c_str(), string_head.size());
    int n = _write(cf.client, (char*)(cf.outbuf), cf.outlen);

    // 如果send 给 client 的 data 有丢失（不成功），就 kill 掉当前的对应的 client 句柄，并 clients 容器中 erase 掉
	if (n < cf.outlen)
	{
    	std::vector<int>::iterator it;
      	it = find (clients.begin(), clients.end(), cf.client);
      	if ( it != clients.end() )
      	{
      		cerr << "kill client " << cf.client << endl;
      		// 从全局变量 clients 容器中删除掉 cf.client 句柄
      		clients.erase(std::remove(clients.begin(), clients.end(), cf.client));
      		// 关闭 cf.client 与 server 端的连接
            ::shutdown(cf.client, 2);
      	}
	}
    pthread_mutex_unlock(&mutex_client);

    pthread_exit(NULL);
}

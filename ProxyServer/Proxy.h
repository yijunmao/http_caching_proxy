#ifndef PROXY_H
#define PROXY_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <queue>
#include <mutex>
#include <string>
#include <exception>
#include <ctime>
#include <arpa/inet.h>
#include <vector>

#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Cache.h"
#define LOG "./proxy.log"
#define DEBUGPROXY 0
using namespace std;

// buffer size
const int buff_size = 65535;

/*
 1. fields: request_fd, port, socket fd (request, maybe response also)
 2. constructor
 3. sock init: create ... listen
 4. start: process request
*/
// function prototype



class Proxy {
private:
    const char* client_port;
    int client_sockfd;
    //const char* server_port;
    int request_id;
    ofstream logfile;
    Cache * cache;

public:
    // constructor and destructor
    Proxy(const char* port): client_port(port) {
        this->request_id = 0;
        //////////////////////////
        this->cache = new Cache(100, this->request_id);
        //////////////////////////
    };
    // delete inside the implementation of cache class
    ~Proxy() {}

    // client set_up
    void client_setup() {
        // create log file
        logfile.open(LOG);
        logfile.close();

        // initialize variable to store the socket information
        int status;
        struct addrinfo host_info;
        struct addrinfo * host_info_list;
        const char* hostname = NULL;
        const char* port = this->client_port;

        // zero out host info
        // both IPv4 and IPv6, stream socket, use host IP address
        memset(&host_info, 0, sizeof(host_info));
        host_info.ai_family = AF_UNSPEC;
        host_info.ai_socktype = SOCK_STREAM;
        host_info.ai_flags = AI_PASSIVE;

        status = getaddrinfo(hostname, port, &host_info, &host_info_list);
        if (status != 0) {
            print_error("Error: cannot get address info for host");
        }

        // create a socket
        this->client_sockfd = socket(host_info_list->ai_family, host_info_list->ai_socktype, host_info_list->ai_protocol);
        if (this->client_sockfd == -1) {
            print_error("Error: cannot create socket for host");
        }

        // allow local address to be reused when restarting server
        int addr_reuse = 1;
        status = setsockopt(this->client_sockfd, SOL_SOCKET, SO_REUSEADDR, &addr_reuse, sizeof(addr_reuse));
        if (status == -1) {
            print_error("Error: cannot set local address to be reused for local address");
        }

        // bind socket to port
        status = bind(this->client_sockfd, host_info_list->ai_addr, host_info_list->ai_addrlen);
        if (status == -1) {
            print_error("Error: cannot bind socket to host");
        }

        // listen
        status = listen(this->client_sockfd, 100);
        if (status == -1) {
            print_error("Error: cannot listen on socket");
        }

        // free the memory for linked list
        freeaddrinfo(host_info_list);
    }



    // TODO: set up time
    void client_accept() {
        while (true) {
            // print message while waiting
            cout << "Waiting for connections on port " << this->client_port << "...\n";

            // accept
            int client_connection_fd;

            // sockaddr_storage is large enough to hold both IPv4 and IPv6 address
            struct sockaddr_storage client_addr;
            socklen_t client_addrlen;
            client_addrlen = sizeof(client_addr);
            client_connection_fd = accept(this->client_sockfd, (struct sockaddr *)&client_addr, &client_addrlen);
            if (client_connection_fd == -1) {
                print_error("Error: cannot accept connection on socket");
            }

            // request id
            this->request_id++;

            // convert client ip from network byte order to presentation form (string)
            const char* client_ip;
            // IPv4
            if (((struct sockaddr *)&client_addr)->sa_family == AF_INET) {
                char s1[INET_ADDRSTRLEN];
                client_ip = inet_ntop(AF_INET, &(((struct sockaddr_in *)&client_addr)->sin_addr), s1, sizeof(s1));
            }
            // IPv6
            else {
                char s2[INET6_ADDRSTRLEN];
                client_ip = inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)&client_addr)->sin6_addr), s2, sizeof(s2));
            }

            // open a thread, call the thread function to handle the request
            std::thread thr (&Proxy::handle_request, this, client_connection_fd, client_ip);
            thr.detach();
        }
    }



    /* handle request from client */
    void handle_request(int client_connection_fd, const char* client_ip) {

        try {
            // get current time for request
            time_t now = time(0);
            char* dt = ctime(&now);
            tm* gmtm = gmtime(&now);
            dt = asctime(gmtm);

            // receive request from client fully and store in a string
            HttpRequest http_req = recv_request(client_connection_fd, this->request_id, dt);

            // write to the log
            //string recv_msg = to_string(http_req.getRequestID()) + ": \"" + http_req.getRequestLine().substr(0, http_req.getRequestLine().size() - 2) + "\" from " + client_ip + " @ " + http_req.getRequestTime();
            logfile.open(LOG, ios::out | ios::ate | ios::app);
            logfile << to_string(http_req.getRequestID()) << ": \"" << http_req.getRequestLine().substr(0, http_req.getRequestLine().size() - 2) << "\" from " << client_ip << " @ " << http_req.getRequestTime();
            logfile.close();
            //cout << recv_msg;

            // CONNECT
            if (http_req.getMethod() == "CONNECT") {
                handle_connect(http_req, client_connection_fd);
            }
            // GET
            else if (http_req.getMethod() == "GET") {
                handle_get(http_req, client_connection_fd);
            }
            // POST
            else if (http_req.getMethod() == "POST") {
                handle_post(http_req, client_connection_fd);
            }
            else {}
        }

        catch(std::exception& e) {
            const char *bad_req = "HTTP/1.1 400 Bad Request\r\n\r\n";
            int status;
            status = send(client_connection_fd, bad_req, strlen(bad_req), 0);
            if (status == -1) {
                print_error("Error: cannot send bad request");
            }
            //////////////////////////////
            
            
            logfile.open(LOG, ios::out | ios::ate | ios::app);
            logfile <<  to_string(this->request_id) << ": Responding ERROR 400-Bad Request\n";
            logfile.close();
            close(client_connection_fd);
            /////////////////////////////
        }
        return;
    }



    void handle_post(HttpRequest req, int client_connection_fd) {
        log_request_from_server(req);
        HttpResponse response = fetch_from_server(req);
        log_response_from_server(req, response);
        sendToClient(response.getRawResponse(), client_connection_fd);
        log_response_to_client(response, req.getRequestID());
    }



    void handle_get(HttpRequest req, int client_connection_fd) {
        //cache->print();
        CacheNode* find = cache->get(req.getUrl(), req.getRequestID());
        
        // Cache miss
        if (find == NULL) {
            //log: not in cache, done in cache class
            // log request from server
            log_request_from_server(req);
            HttpResponse response = fetch_from_server(req);
            // log response from server
            log_response_from_server(req, response);
            //send to client
            sendToClient(response.getRawResponse(), client_connection_fd);
            // log response to client
            log_response_to_client(response, req.getRequestID());
            // Check status
            // If "200"
            string final_response(response.getRawResponse().data(), response.getRawResponse().size());
            if (response.getStatusCode() == "200") {
                // Store into cache if permitted
                if (!response.hasNoStore()) {
                    cache->put(req.getUrl(), response, req.getRequestID());
                } 
                else {
                        // log response not cacheable because response header has no-store
                        log_not_cacheable(req.getRequestID());
                }
            }
            else if (response.getStatusCode() == "304") {
                logfile.open(LOG, ios::out | ios::ate | ios::app);
                logfile << to_string(req.getRequestID()) << ": NOTE Probably you forget to clean the browser cache\n";
                logfile.close();
            }
            else {
                if (response.getStatusCode()[0] == '3') {
                logfile.open(LOG, ios::out | ios::ate | ios::app);
                logfile << to_string(req.getRequestID()) << ": WARNING " << \
                    response.getStatusLine().substr(0, response.getStatusLine().size() - 2) << "\n";
                logfile.close();
                
                }
                else if (response.getStatusCode()[0] == '4' || response.getStatusCode()[0] == '5') {
                    logfile.open(LOG, ios::out | ios::ate | ios::app);
                    logfile << to_string(req.getRequestID()) << ": ERROR " << \
                    response.getStatusLine().substr(0, response.getStatusLine().size() - 2) << "\n";
                    logfile.close();
                }
            }
        }
        else{
            // Cache hit
            //log cache control + Etag
            if (find->cache_control != "") {
                //log cache control
                //string cache_control_log = req.getRequestID() + ": NOTE " + find->cache_control + "\n";
                logfile.open(LOG, ios::out | ios::ate | ios::app);
                logfile << req.getRequestID() << ": NOTE " << find->cache_control << "\n";
                logfile.close();
            }
            if (find->etag != "") {
                //log etag
                logfile.open(LOG, ios::out | ios::ate | ios::app);
                logfile << req.getRequestID() << ": NOTE ETag: " << find->etag << "\n";
                logfile.close();
            }
            /*
            cout<<" cache hit!"<<endl;
            cout<<"----------------------------------------------------------------------------"<<endl;
            cout<<"                                cache information start                      "<<endl;
            cout<<"     must_revalidate is: +"<<find->must_revalidate<<"++"<<endl;
            cout<<"     is_no_cache is: +"<<find->is_no_cache<<"++"<<endl;
            cout<<"     max_age is: +"<<find->max_age<<"++"<<endl;
            time_t cur_time = std::time(nullptr);
            struct tm* temp = gmtime(&cur_time);
            time_t res = mktime(temp);
            cout<<"     now is: +"<<res<<"++"<<endl;
            cout<<"     birthday is: +"<<find->birthday<<"++"<<endl;
            cout<<"     expire_date is: +"<<find->expire_date<<"++"<<endl;
            cout<<"     e_tag is: +"<<find->e_tag<<"++"<<endl;
            cout<<"     last_modified is: +"<<find->last_modified<<"++"<<endl;
            cout<<"                                cache information end                      "<<endl;
            cout<<"----------------------------------------------------------------------------"<<endl;
            */
            if (find->is_no_cache == true) {
                //log: in cache, requires validation
                log_in_cache_requires_validation (req.getRequestID());
                revalidation(req, client_connection_fd, find, request_id);
            }
            else if (find->must_revalidate == true) {
                // 1. check max age
                time_t cur_time = std::time(nullptr);
                struct tm* temp = gmtime(&cur_time);
                time_t res = mktime(temp);
                if (!find->is_expired(res)) {
                    // a. max age fresh
                    //log: in cache, valid
                    log_in_cache_valid(req.getRequestID());
                    sendToClient(find->response, client_connection_fd);
                    logfile.open(LOG, ios::out | ios::ate | ios::app);
                    logfile << to_string(req.getRequestID()) << ": Responding HTTP/1.1 200 OK\n";
                    logfile.close();
                }
                else {
                    // b. max age not fresh
                    string expire_at = find->get_expire_time();
                    //log in cache, but expired at expire time
                    if(expire_at != "") {
                        log_in_cache_expired(req.getRequestID(), expire_at);
                    }
                    else {
                        log_in_cache_requires_validation(req.getRequestID());
                    }
                    revalidation(req, client_connection_fd, find, request_id);
                }
            }
            else if (find->max_age != -1) {
                // 1. check max age
                time_t cur_time = std::time(nullptr);
                struct tm* temp = gmtime(&cur_time);
                time_t res = mktime(temp);
                if (!find->is_expired(res)) {
                    // a. max age fresh
                    //log: in cache, valid
                    log_in_cache_valid(req.getRequestID());
                    sendToClient(find->response, client_connection_fd);
                    logfile.open(LOG, ios::out | ios::ate | ios::app);
                    logfile << to_string(req.getRequestID()) << ": Responding HTTP/1.1 200 OK\n";
                    logfile.close();
                }
                else {
                    // b. max age not fresh
                    string expire_at = find->get_expire_time(); // TODO:implement
                    //log in cache, but expired at expire time
                    if(expire_at != "") {
                        log_in_cache_expired(req.getRequestID(), expire_at);
                    }
                    else {
                        log_in_cache_requires_validation(req.getRequestID());
                    }
                    revalidation(req, client_connection_fd, find, request_id);
                }
            }
            else {
                // No cache control
                time_t cur_time = std::time(nullptr);
                struct tm* temp = gmtime(&cur_time);
                time_t res = mktime(temp);
                if (!find->is_expired(res)) {
                    // a. max age fresh
                    //log: in cache, valid
                    log_in_cache_valid(req.getRequestID());
                    sendToClient(find->response, client_connection_fd);
                    sendToClient(find->response, client_connection_fd);
                    logfile.open(LOG, ios::out | ios::ate | ios::app);
                    logfile << to_string(req.getRequestID()) << ": Responding HTTP/1.1 200 OK\n";
                    logfile.close();
                }
                else {
                    // b. max age not fresh
                    string expire_at = find->get_expire_time();
                    //log in cache, but expired at expire time
                    if(expire_at != "") {
                        log_in_cache_expired(req.getRequestID(), expire_at);
                    }
                    else {
                        log_in_cache_requires_validation(req.getRequestID());
                    }
                    revalidation(req, client_connection_fd, find, request_id);
                }
            }
        }
        close(client_connection_fd);
        return;
    }


    void revalidation(HttpRequest req, int client_connection_fd, CacheNode* find, int request_id) {
        vector<char> processed_req = reconstruct_request(req.getRawRequest(), find);
        req.setRawRequest(processed_req);
        log_request_from_server(req);
        HttpResponse response = fetch_from_server(req);
        log_response_from_server(req, response);
        // Check status
        // If "304"-> remain the same
        if (response.getStatusCode() == "304") {
            logfile.open(LOG, ios::out | ios::ate | ios::app);
            logfile << to_string(request_id) << ": NOTE Response has not been modified\n";
            logfile.close();
            sendToClient(find->response, client_connection_fd); //TODO: 1st para probable should be "response"
            log_response_to_client(response, request_id);
        }
        // Else if "200"
        else if (response.getStatusCode()[0] == '2') {
            //send to client
            sendToClient(response.getRawResponse(), client_connection_fd);
            log_response_to_client(response, request_id);
            // Store into cache if permitted
            if (!response.hasNoStore()) {
                cache->put(req.getUrl(), response, req.getRequestID());
            }
            else {
                log_not_cacheable (req.getRequestID());
            }
        }
        else {
            if (response.getStatusCode()[0] == '3') {
                logfile.open(LOG, ios::out | ios::ate | ios::app);
                logfile << to_string(req.getRequestID()) << ": WARNING " << \
                response.getStatusLine().substr(0, response.getStatusLine().size() - 2) << "\n";
                logfile.close();
            }
            else if (response.getStatusCode()[0] == '4' || response.getStatusCode()[0] == '5') {
                logfile.open(LOG, ios::out | ios::ate | ios::app);
                logfile << to_string(req.getRequestID()) << ": ERROR " << \
                response.getStatusLine().substr(0, response.getStatusLine().size() - 2) << "\n";
                logfile.close();
            }
        }
    }


    vector<char> reconstruct_request(vector<char> original_request, CacheNode* find) {
        string original(original_request.begin(), original_request.end());
        size_t pos = original.find("\r\n\r\n") + 2;
        if (find->last_modified != "") {
            string if_modified_since = "If-Modified-Since: " + find->last_modified + "\r\n";
            original.insert(pos, if_modified_since);
        }
        if (find->e_tag != "") {//if has e_tag
            string if_none_match = "If-None-Match: " + find->e_tag + "\r\n";
            original.insert(pos, if_none_match);
        }
        vector<char> new_request(original.begin(), original.end());
        return new_request;
    }

    void log_response_from_server(HttpRequest req, HttpResponse response) {
        // log received response from server
        logfile.open(LOG, ios::out | ios::ate | ios::app);
        logfile << to_string(req.getRequestID()) << ": Received " << response.getStatusLine().substr(0, response.getStatusLine().size() - 2) << " from "\
            << req.getServerName() << "\n";
        logfile.close();
    }



    void log_request_from_server(HttpRequest request) {
        // log requesting request from client
        logfile.open(LOG, ios::out | ios::ate | ios::app);
        logfile << to_string(request.getRequestID()) << ": Requesting " << request.getRequestLine().substr(0, request.getRequestLine().size() - 2) << " from "\
            << request.getServerName() << "\n";
        logfile.close();
    }



    void log_response_to_client(HttpResponse response, int request_id) {
        // log response to client
        logfile.open(LOG, ios::out | ios::ate | ios::app);
        logfile << to_string(request_id) << ": Responding " << response.getStatusLine().substr(0, response.getStatusLine().size() - 2) << "\n";
        logfile.close();
    }



    void log_in_cache_expired(int request_id, string expire_at) {
        //log in cache, but expired at expire time
        logfile.open(LOG, ios::out | ios::ate | ios::app);
        logfile << to_string(request_id) << ": in cache, but expired at " + expire_at;
        logfile.close();
    }



    void log_in_cache_requires_validation (int request_id) {
        logfile.open(LOG, ios::out | ios::ate | ios::app);
        logfile << to_string(request_id) << ": in cache, requires validation\n";
        logfile.close();
    }



    void log_not_cacheable(int request_id) {
        logfile.open(LOG, ios::out | ios::ate | ios::app);
        logfile << to_string(request_id) << ": not cacheable because Cache-Control header has no-store\n";
        logfile.close();
    }



    void log_in_cache_valid(int request_id) {
        logfile.open(LOG, ios::out | ios::ate | ios::app);
        logfile << to_string(request_id) << ": in cache, valid\n";
        logfile.close();
    }

    HttpResponse fetch_from_server(HttpRequest req) {
        // create socket
        int status;
        struct addrinfo server_info;
        struct addrinfo * server_info_list;
        memset(&server_info, 0, sizeof(server_info));
        server_info.ai_family = AF_UNSPEC;
        server_info.ai_socktype = SOCK_STREAM;
        if (req.getServerPort().empty()) {
            status = getaddrinfo(req.getServerName().c_str(), "80", &server_info, &server_info_list);
        }
        else {
            status = getaddrinfo(req.getServerName().c_str(), req.getServerPort().c_str(), &server_info, &server_info_list);
        }
        if (status != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
            throw std::exception();
        }
        int server_sockfd;
        server_sockfd = socket(server_info_list->ai_family, server_info_list->ai_socktype, server_info_list->ai_protocol);
        if (server_sockfd == -1) {
            print_error("Error: cannot create socket for server");
            throw std::exception();
        }
        // connect
        status = connect(server_sockfd, server_info_list->ai_addr, server_info_list->ai_addrlen);
        //addrinfo(server_info_list);
        if (status == -1) {
            print_error("Error: cannot connect to server");
            close(server_sockfd);
            throw std::exception();
        }
        // forward request to server
        status = send(server_sockfd, req.getRawRequest().data(), req.getRawRequest().size(), 0);
        if (status == -1) {
            print_error("Error: fail to forward request to server");
            close(server_sockfd);
            throw std::exception();
        }
        // receive response from server
        return recv_response(server_sockfd);
    }

    /* receive response from server */
    HttpResponse recv_response(int sockfd) {
        // get response until header end
        vector<char> raw_response;
        vector<char> buffer(3, 0);
        int byte_size = 0;
        while (true) {
            byte_size = recv(sockfd, &buffer.data()[0], 1, 0);
            if (byte_size > 0) {
                buffer.resize(byte_size);
                raw_response.insert(raw_response.end(), buffer.begin(), buffer.end());
                buffer.resize(3, 0);
                int size = raw_response.size();
                if (size-4 >= 0 && raw_response[size - 1] == '\n' && raw_response[size - 2] == '\r' && raw_response[size - 3] == '\n' && raw_response[size - 4] == '\r') {
                    string res_till_header(raw_response.data(), raw_response.size());
                    break;
                }
            }
            else if (byte_size == 0) {
                break;
            }
            else {
                fprintf(stderr, "recv: %s (%d)\n", strerror(errno), errno);
                close(sockfd);
                throw std::exception();
            }
        }
        HttpResponse http_response(raw_response);
        try {
            // parse header
            http_response.buildStatusLine();
            http_response.parseStatusLine();
            http_response.buildHeaders();
        }
        catch(std::exception& e) {
            close(sockfd);
        }
        http_response.buildCacheControl();
        http_response.buildChunked();
        http_response.buildContentLength();
        http_response.buildMaxAge();
        http_response.buildBirthday();
        http_response.buildEtag();
        http_response.buildExpires();
        http_response.buildLastModified();
        if (http_response.getChunked()) {
            buffer.resize(buff_size, 0);
            byte_size = 0;
            while (true) {
                byte_size = recv(sockfd,  &buffer.data()[0], buff_size, 0);
                if (byte_size > 0) {
                    buffer.resize(byte_size);
                    raw_response.insert(raw_response.end(), buffer.begin(), buffer.end());
                    buffer.resize(buff_size, 0);
                    int size = raw_response.size();
                    if (size-7 >= 0 && raw_response[size - 1] == '\n' && raw_response[size - 2] == '\r'\
                    && raw_response[size - 3] == '\n' && raw_response[size - 4] == '\r'\
                    && raw_response[size - 5] == '0' && raw_response[size - 6] == '\n'\
                    && raw_response[size - 7] == '\r') {
                        http_response.setRawResponse(raw_response);
                        close(sockfd);
                        return http_response;
                    }
                }
                else if (byte_size == 0) {
                    close(sockfd);
                    return http_response;
                }
                else {
                    fprintf(stderr, "recv: %s (%d)\n", strerror(errno), errno);
                    close(sockfd);
                    //return HttpResponse(vector<char>());
                    throw std::exception();
                }
            }
        }
        // content length
       else if (http_response.getContentLength() != -1) {
           int cnt = http_response.getContentLength();
           int total_body_len = cnt;
           // receive until content length
           buffer.resize(3, 0);
           byte_size = 0;
           while (true) {
                byte_size = recv(sockfd, &buffer.data()[0], 1, 0);
                if (byte_size > 0) {
                    buffer.resize(byte_size);
                    raw_response.insert(raw_response.end(), buffer.begin(), buffer.end());
                    buffer.resize(3, 0);
                    cnt -= byte_size;
                    if (cnt <= 0) {
                        // reset raw response and return
                        http_response.setRawResponse(raw_response);
                        close(sockfd);
                        return http_response;
                    }
                }
                else if (byte_size == 0) {
                    http_response.setRawResponse(raw_response);
                    close(sockfd);
                    return http_response;
                }
                else {
                    fprintf(stderr, "recv: %s (%d)\n", strerror(errno), errno);
                    close(sockfd);
                    throw std::exception();
                }
            }
        }
        else {
            close(sockfd);
            return http_response;
        }
    }

    void sendToClient(vector<char> data, int destination_sockfd) {
        int bytes_total = data.size();
        int bytes_left = bytes_total;
        int bytes_sent = 0;
        int bytes_count = 0;
        vector<char> buf;
        buf.insert(buf.end(), data.begin(), data.end());
        send(destination_sockfd, buf.data(), bytes_total, 0);
        close(destination_sockfd);
   }

    /* CONNECT method */
    void handle_connect(HttpRequest req, int client_connection_fd) {
        // create socket
        int status;
        struct addrinfo server_info;
        struct addrinfo * server_info_list;
        memset(&server_info, 0, sizeof(server_info));
        server_info.ai_family = AF_UNSPEC;
        server_info.ai_socktype = SOCK_STREAM;
        status = getaddrinfo(req.getServerName().c_str(), req.getServerPort().c_str(), &server_info, &server_info_list);
        if (status != 0) {
            print_error("Error: cannot get address info for server");
            throw std::exception();
        }
        // create a socket
        int server_sockfd;
        server_sockfd = socket(server_info_list->ai_family, server_info_list->ai_socktype, server_info_list->ai_protocol);
        if (server_sockfd == -1) {
            print_error("Error: cannot create socket for server");
            throw std::exception();
        }
        // connect
        status = connect(server_sockfd, server_info_list->ai_addr, server_info_list->ai_addrlen);
        freeaddrinfo(server_info_list);
        if (status == -1) {
            print_error("Error: cannot connect to server");
            close(server_sockfd);
            throw std::exception();
        }
        // Send acknowledgement when successfully connect with client
        const char *ack = "HTTP/1.1 200 OK\r\n\r\n";
        status = send(client_connection_fd, ack, strlen(ack), 0);
        if (status == -1) {
            print_error("Error: fail to send acknowledgement to client");
            close(server_sockfd);
            throw std::exception();
        }
        connect_with_server(client_connection_fd, server_sockfd, req.getRequestID());
    }

    /* build TUNNEL and communicate */
    void connect_with_server(int client_connection_fd, int server_sockfd, int request_id){
        fd_set readfds;
        std::vector<char> buffer(buff_size);
        while (true) {
            FD_ZERO(&readfds);
            FD_SET(client_connection_fd, &readfds);
            FD_SET(server_sockfd, &readfds);
            int max_fd = -1;
            if (client_connection_fd > server_sockfd) {
                max_fd = client_connection_fd;
            }
            else {
                max_fd = server_sockfd;
            }
            int status = select(max_fd + 1, &readfds, NULL, NULL, 0);
            int len;
            if (status <= 0) {
                print_error("Error: fail to select");
            }
            if (FD_ISSET(client_connection_fd, &readfds)) {
                buffer.clear();
                len = recv(client_connection_fd,  &buffer.data()[0], buff_size, 0);
                if (len < 0) {
                    perror("Failed to recv from client in tunnel:");
                    break;
                } else if (len == 0) {
                    break;
                }
                len = send(server_sockfd, buffer.data(), len, 0);
                if (len < 0) {
                    perror("Failed to send to server in tunnel: " + server_sockfd);
                    break;
                }
            }
            else if (FD_ISSET(server_sockfd, &readfds)) {
                buffer.clear();
                len = recv(server_sockfd, &buffer.data()[0], buff_size, 0);
                if (len < 0) {
                    perror("Failed to recv from server in tunnel:");
                    break;
                } else if (len == 0) {
                    break;
                }
                len = send(client_connection_fd, buffer.data(), len, 0);
                if (len < 0) {
                    perror("Failed to send to client in tunnel:");
                    break;
                }
            }
            buffer.clear();
        }

        // close socket when communication finishes
        close(server_sockfd);
        close(client_connection_fd);
        logfile.open(LOG, ios::out | ios::ate | ios::app);
        logfile << to_string(request_id) << ": Tunnel closed\n";
        logfile.close();
    }

    /* helper function to print error message */
    void print_error(const char* message) {perror(message);}

    /* helper function to ensure request can be received fully */
    HttpRequest recv_request(int sockfd, int request_id, char* request_time) {
        // get request until header end
        vector<char> raw_request;
        vector<char> buffer(3, 0);
        int byte_size = 0;
        while (true) {
            byte_size = recv(sockfd, &buffer.data()[0], 1, 0);
            if (byte_size > 0) {
                buffer.resize(byte_size);
                raw_request.insert(raw_request.end(), buffer.begin(), buffer.end());
                buffer.resize(3, 0);
                int size = raw_request.size();
                if (size-4 >= 0 && raw_request[size - 1] == '\n' && raw_request[size - 2] == '\r' && raw_request[size - 3] == '\n' && raw_request[size - 4] == '\r') {
                    string res_till_header(raw_request.data(), raw_request.size());
                    break;
                }
            }
            else if (byte_size == 0) {
                break;
            }
            else {
                fprintf(stderr, "recv: %s (%d)\n", strerror(errno), errno);
                throw std::exception();
            }
        }
        // create raw request till the end of header
        HttpRequest http_request(raw_request, request_id, request_time);
        http_request.buildRequestLine();
        http_request.parseRequestLine();
        http_request.parseServerInfo();
        http_request.buildHeaders();
        http_request.buildContentLength();

        // POST
        if (http_request.getContentLength() != -1) {
            int cnt = http_request.getContentLength();
            int total_body_len = cnt;
            // receive until content length
            buffer.resize(3, 0);
            byte_size = 0;
            while (true) {
                byte_size = recv(sockfd, &buffer.data()[0], 1, 0);
                if (byte_size > 0) {
                    buffer.resize(byte_size);
                    raw_request.insert(raw_request.end(), buffer.begin(), buffer.end());
                    buffer.resize(3, 0);
                    cnt -= byte_size;
                    if (cnt <= 0) {
                        // reset raw request and return
                        http_request.setRawRequest(raw_request);
                        return http_request;
                    }
                }
                else if (byte_size == 0) {
                    http_request.setRawRequest(raw_request);
                    return http_request;
                }
                else {
                    fprintf(stderr, "recv: %s (%d)\n", strerror(errno), errno);
                    //return HttpRequest(vector<char>());
                    throw std::exception();
                }
            }
        }
        // GET and CONNECT
        return http_request;
    }
};
#endif
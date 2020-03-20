#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <iostream>
#include <string>
#include <exception>
#include <ctime>
#include <vector>
using namespace std;



class HttpRequest {
private:
    /* has to be vector of char, otherwise could messed up when including special characters */
    vector<char> raw_request;
    // request line
    string request_line;
    string method;
    string url;
    string version;
    // header fields
    string fields;
    string server_host;
    string server_port;
    int content_length;
    // request body
    vector<char> body;
    // request id and time
    int request_id;
    char * request_time;



public:
    /* constructor and destructor */
    HttpRequest(vector<char> req, int ID, char* time):
    raw_request(req), request_line(""), method(""), url(""), version(""),
    fields(""), server_host(""), server_port(""), content_length(-1), body(vector<char>()), request_id(ID), request_time(time) {
        string  temp(raw_request.begin(), raw_request.end());
    };
    ~HttpRequest() {};



    /* get request line */
    void buildRequestLine() {
        // convert vector of char to C++ string
        string req(this->raw_request.begin(), this->raw_request.end());
        size_t reqlin_end = req.find("\r\n");
        if (reqlin_end == string::npos) {
            throw std::exception();
        }
        this->request_line = req.substr(0, reqlin_end + 2);
    }



    /* parse request line */
    /* TODO: probably have problem */
    void parseRequestLine() {
        // get method
        size_t method_end = this->request_line.find(" ");
        if (method_end == string::npos) {
            throw std::exception();
        }
        this->method = this->request_line.substr(0, method_end);

        if (this->method != "GET" and this->method != "CONNECT" and this->method != "POST") {
            throw std::exception();
        }

        // get url
        size_t url_end = this->request_line.find(" ", method_end + 1);
        if (url_end == string::npos) {
            throw std::exception();
        }
        /* Note: this line has been modified */
        this->url = this->request_line.substr(method_end + 1, url_end - method_end - 1);

        // get http version
        size_t version_end = this->request_line.find("\r\n", url_end + 1);
        if (version_end == string::npos) {
            throw std::exception();
        }
        /* Note: this line has been modified */
        this->version = this->request_line.substr(url_end + 1, version_end - url_end - 1);
    }



    /* get items in request line */
    vector<char> getRawRequest() {return this->raw_request;}
    string getRequestLine() {return this->request_line;}
    string getMethod() {return this->method;}
    string getUrl() {return this->url;}
    string getVersion() {return this->version;}



    /* get host name and host port */
    void parseServerInfo() {
        string req(this->raw_request.begin(), this->raw_request.end());
        size_t start = req.find("Host: ");
        if (start == string::npos) {throw std::exception();}

        size_t end = req.find_first_of("\r\n", start);
        if (end == string::npos) {throw std::exception();}

        string second_line = req.substr(start, end - start + 2);

        size_t host_start = 6;
        size_t host_end = second_line.find(":", host_start);

        // has port
        if (host_end != string::npos) {
            this->server_host = second_line.substr(host_start, host_end - host_start);

            size_t port_start = host_end + 1;
            size_t port_end = second_line.find("\r", port_start);
            this->server_port = second_line.substr(port_start, port_end - port_start);
        }

        // no port
        else {
            this->server_host = second_line.substr(host_start, second_line.size() - host_start - 2);
        }
    }



    /* get server host and server port */
    string getServerName() {return this->server_host;}
    string getServerPort() {return this->server_port;}



    /* build headers */
    void buildHeaders() {
        string request(this->raw_request.begin(), this->raw_request.end());
        size_t first_linebreak = request.find("\r\n");
        if (first_linebreak == string::npos) {
            throw std::exception();
        }
        size_t header_start = first_linebreak + 2;
        size_t header_end = request.find("\r\n\r\n", header_start);
        if (header_end == string::npos) {
            throw std::exception();
        }
        this->fields = request.substr(header_start, header_end - header_start);
    }



    /* build content length */
    void buildContentLength() {
        size_t contentlen_pos = this->fields.find("Content-Length");
        if (contentlen_pos != string::npos) {
            size_t contentlen_start = this->fields.find(" ", contentlen_pos);
            if (contentlen_start == string::npos) {
                throw std::exception();
            }
            size_t contentlen_end = this->fields.find("\r\n", contentlen_start);
            if (contentlen_end == string::npos) {
                throw std::exception();
            }
            this->content_length = stoi(this->fields.substr(contentlen_start + 1, contentlen_end - contentlen_start - 1));
        }
    }



    /* get header fields and body */
    string getFields() {return this->fields;}
    string getBody() {};
    int getContentLength() {return this->content_length;}
    int getRequestID(){return this->request_id;}
    /* get request time */
    char * getRequestTime() {return this->request_time;}



    /* setter */
    void setRawRequest(vector<char> new_request) {
        this->raw_request = new_request;
    }
};

#endif
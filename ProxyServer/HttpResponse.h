#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include <iostream>
#include <string>
#include <exception>
#include <locale>
#include <vector>
#define DEBUG_RESPONSE 0
using namespace std;

/* TODO: get date */
class HttpResponse {
private:
    // raw response has to be vector of char, otherwise would have problem when sending to client
    vector<char> raw_response;
    // status line
    string status_line;
    string version;
    string status_code;
    string reason_phrase;
    // header fields
    string fields;
    string cache_control;
    int max_age;
    time_t birthday;
    time_t expires;
    string etag;
    string last_modified;
    int content_length;
    bool chunked;
    // body
    vector<char> body;

public:
    /* constructor and destructor */
    HttpResponse(vector<char> raw_response): raw_response(raw_response), status_line(""), version(""), status_code(""), reason_phrase(""),\
    fields(""), cache_control(""), max_age(-1), birthday(-1), expires(-1), etag(""), last_modified(""), body(vector<char>()), content_length(-1), chunked(false) {}
    ~HttpResponse() {}
    /* set raw response */
    void setRawResponse(vector<char> s) {
        this->raw_response = s;
    }

    /* build status line */
    void buildStatusLine() {
        string response(this->raw_response.begin(), this->raw_response.end());
        size_t reslin_end = response.find("\r\n");
        if (reslin_end == string::npos) {
            throw std::exception();
        }
        this->status_line = response.substr(0, reslin_end + 2);
    }



    /* parse response line */
    void parseStatusLine() {
        // get http version
        size_t version_end = this->status_line.find(" ");
        if (version_end == string::npos) {
            throw std::exception();
        }
        this->version = this->status_line.substr(0, version_end);

        // get status code
        size_t statuscode_end = this->status_line.find(" ", version_end + 1);
        if (statuscode_end == string::npos) {
            throw std::exception();
        }
        
        this->status_code = this->status_line.substr(version_end + 1, statuscode_end - version_end - 1);

        // get reason phrase
        size_t reasonphrase_end = this->status_line.find("\r\n", statuscode_end + 1);
        if (reasonphrase_end == string::npos) {
            throw std::exception();
        }
        this->reason_phrase = this->status_line.substr(statuscode_end + 1, reasonphrase_end - statuscode_end - 1);
    }



    /* build headers */
    void buildHeaders() {
        string response(this->raw_response.begin(), this->raw_response.end());
        size_t first_linebreak = response.find("\r\n");
        if (first_linebreak == string::npos) {
            throw std::exception();
        }
        size_t header_start = first_linebreak + 2;
        size_t header_end = response.find("\r\n\r\n", header_start);
        if (header_end == string::npos) {
            throw std::exception();
        }
        this->fields = response.substr(header_start, header_end - header_start + 2);
    }



    /* has cache control line */
    void buildCacheControl() {
        size_t cachecontrol_start = this->fields.find("Cache-Control");
        if (cachecontrol_start == string::npos) {
            return;
        }
        size_t cachecontrol_end = this->fields.find("\r\n", cachecontrol_start);
        if (cachecontrol_end == string::npos) {
            throw std::exception();
        }
        
        this->cache_control = this->fields.substr(cachecontrol_start, cachecontrol_end - cachecontrol_start);
        
    }



    bool hasCacheControl() {
        if (this->cache_control == "") {
            return false;
        } else {
            return true;
        }
    }



    /* has must-revalidate */
    bool hasMustRevalidate() {
        size_t revalidate_start = this->cache_control.find("must-revalidate");
        if (revalidate_start == string::npos) {
            return false;
        }
        return true;
    }



    /* has no-cache */
    bool hasNoCache() {
        size_t nocache_start = this->cache_control.find("no-cache");
        if (nocache_start == string::npos) {
            return false;
        }
        return true;
    }



    /* has no-store */
    bool hasNoStore() {
        size_t nostore_start = this->cache_control.find("no-store");
        if (nostore_start == string::npos) {
            return false;
        }
        return true;
    }



    /* build birthday */
    void buildBirthday() {
            size_t expires_pos = this->fields.find("Date");
            if (expires_pos != string::npos) {
                size_t expires_start = this->fields.find(" ", expires_pos);
                if (expires_start == string::npos) {
                    throw std::exception();
                }
                size_t expires_end = this->fields.find("\r\n", expires_start + 1);
                if (expires_end == string::npos) {
                    throw std::exception();
                }
                string e = this->fields.substr(expires_start + 1, expires_end - expires_start - 1);
                struct tm date;
                memset(&date, 0, sizeof(struct tm));
                strptime(e.c_str(), "%a, %d %b %Y %H:%M:%S", &date);
                time_t t = mktime(&date);
                struct tm* res = gmtime(&t);
                this->birthday = mktime(res);
            }
    }



    /* build max age */
    void buildMaxAge() {
            size_t maxage_start = this->cache_control.find("max-age");
            if (maxage_start != string::npos) {
                size_t equal_pos = this->cache_control.find("=", maxage_start);
                if (equal_pos == string::npos) {
                    throw std::exception();
                }
                size_t realage_start = equal_pos + 1;
                string number_part = this->cache_control.substr(realage_start);
                size_t realage_len = 0;
                while (isdigit(number_part[realage_len])) {
                    ++realage_len;
                this->max_age = stoi(number_part.substr(0,realage_len));
                }
            }
    }



    /* build etag */
    void buildEtag() {
        size_t etag_start = this->fields.find("ETag");
        if (etag_start != string::npos) {
            size_t real_start = this->fields.find('\"', etag_start);
            if (real_start == string::npos) {
                throw std::exception();
            }
            size_t real_end = this->fields.find('\"', real_start + 1);
            if (real_end == string::npos) {
                throw std::exception();
            }
            this->etag = this->fields.substr(real_start, real_end - real_start + 1);
        }
    }



    /* build expires */
    void buildExpires() {
        size_t expires_pos = this->fields.find("Expires");
        if (expires_pos != string::npos) {
            size_t expires_start = this->fields.find(" ", expires_pos);
            if (expires_start == string::npos) {
                throw std::exception();
            }
            size_t expires_end = this->fields.find("\r\n", expires_start + 1);
            if (expires_end == string::npos) {
                throw std::exception();
            }
            string e = this->fields.substr(expires_start + 1, expires_end - expires_start - 1);
            struct tm tm;
            memset(&tm, 0, sizeof(struct tm));
            strptime(e.c_str(), "%a, %d %b %Y %H:%M:%S", &tm);
            time_t t = mktime(&tm);
            struct tm* temp = gmtime(&t);
            time_t res = mktime(temp);
            this->expires = res;
        }
    }



    /* build last modified */
    void buildLastModified() {
        size_t lastmodified_pos = this->fields.find("Last-Modified");
        if (lastmodified_pos != string::npos) {
            size_t lastmodified_start = this->fields.find(" ", lastmodified_pos);
            if (lastmodified_start == string::npos) {
                throw std::exception();
            }
            size_t lastmodifed_end = this->fields.find("\r\n", lastmodified_start);
            if (lastmodifed_end == string::npos) {
                throw std::exception();
            }
            this->last_modified = this->fields.substr(lastmodified_start + 1, lastmodifed_end - lastmodified_start - 1);
        }
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



    /* build chunked */
    void buildChunked() {
        size_t chunked_pos = this->fields.find("chunked");
        if (chunked_pos != string::npos) {
            this->chunked = true;
        }
    }



    /* build response body */
    void buildBody() {}



    /* get private members */
    vector<char> getRawResponse() {return this->raw_response;}
    string getStatusLine() {return this->status_line;}
    string getVersion() {return this->version;}
    string getStatusCode() {return this->status_code;}
    string getReasonPhrase() {return this->reason_phrase;}
    string getFields() {return this->fields;}
    string getCacheControl() {return this->cache_control;}
    int getMaxAge() {return this->max_age;}
    time_t getBirthday(){return this->birthday;}
    time_t getExpires() {return this->expires;}
    string getEtag() {return this->etag;}
    string getLastModified() {return this->last_modified;}
    bool getChunked() {return this->chunked;}
    int getContentLength() {return this->content_length;}
    vector<char> getBody() {return this->body;}


};
#endif
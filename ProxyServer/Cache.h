#ifndef CACHE_H
#define CACHE_H

#include <string>
#include <unordered_map>
#include <fstream>
#include <ctime>

#include <list>
#include <mutex>
#define LOG "./proxy.log"
using namespace std;

/*
 * store a single response
 * key: url
 * value: pointer to next CacheNode
*/
class CacheNode {
    friend class Cache;
public:
    string url; // key
    vector<char> response; // value
    bool must_revalidate;
    bool is_no_cache;
    int max_age;
    string cache_control;
    string etag;
    time_t birthday;
    time_t expire_date;
    string e_tag;
    string last_modified;


// constructor and destructor
    CacheNode (string _url, HttpResponse input) {
        this->url = _url;
        this->response = input.getRawResponse();
        this->must_revalidate = input.hasMustRevalidate();
        this->is_no_cache = input.hasNoCache();
        this->max_age = input.getMaxAge();
        this->cache_control = input.getCacheControl();
        this->etag = input.getEtag();
        this->birthday = input.getBirthday();
        this->expire_date = input.getExpires();
        this->e_tag = input.getEtag();
        this->last_modified = input.getLastModified();
    }

    ~CacheNode() {};

    bool is_expired(time_t current_time) {
        if (max_age == -1) {// case1 : do not have max_age, check "expires"
            if (expire_date == -1 || current_time == -1) {
                //is always expired
                return false;
            }
            else if (std::difftime(current_time, expire_date) < 0) {
                return false;
            }
            else {
                return true;
            }
        }
        else {// case2 : have max-age
            if (std::difftime(birthday + max_age, current_time) < 0) {
                return true;
            }
            else {
                return false;
            }
        }
    }

    string get_expire_time() {
        if (birthday != -1 && max_age != -1) {
            // case 1 : have max age
            time_t temp = birthday + max_age;
            return string(std::asctime(std::gmtime(&temp)));
        }
        else if (max_age == -1 && expire_date != -1) {
            // case 2: have expires
            return string(std::asctime(std::gmtime(&expire_date)));
        }
        else if (birthday != -1) {
            // case3: have neither, always expired, we set exprire time to the birthday
            return string(std::asctime(std::gmtime(&birthday)));
        }
        else {
            // case4: have neither, always expired, need revalidation
            return "";
        }
    }
};

/*
 * cache_dict is the core storage
 * use cache_list to provide a sequential date structure. convenient to pop and insert
 * use list_address to locate the element in cache_list in O(1);
*/

class Cache {
private:
    unordered_map<string, CacheNode*> cache_dict;
    list<string> cache_list;
    unordered_map<string, list<string>::iterator> list_address;
    int capacity;
    mutex lock;
    ofstream logfile;
    int request_id;
public:
    // constructor
    Cache(int cap, int req_id): capacity(cap), request_id(req_id) {}
    ~Cache() {
        // for (auto cell: cache_dict) {
        //     delete cell.second;
        // }
    }
    // get method
    CacheNode* get(string target_url, int id) {
        lock.lock();
        // case1 : in cache
        if (cache_dict.find(target_url) != cache_dict.end()) {
            //update cacheblock to the front of the list
            cache_list.splice(cache_list.begin(), cache_list, list_address[target_url]);
            lock.unlock();
            return cache_dict[target_url];
        }

        // not in cache
        else {
            //log not in cache
            logfile.open(LOG, ios::out | ios::ate | ios::app);
            logfile << to_string(id) << ": not in cache\n";
            logfile.close();
            lock.unlock();
            return NULL;
        }
    }

    // put method, return value indicates if eviction happens
    bool put(string target_url, HttpResponse response, int request_id) {
        lock.lock();
        if(cache_dict.find(target_url) != cache_dict.end()) {
            // already in cache, erase it
            cache_dict.erase(target_url);
            cache_list.erase(list_address[target_url]);
            list_address.erase(target_url);
        }
        //not in cache, create a new cell and insert into front of the list
        CacheNode* cell = new CacheNode(target_url, response);
        cache_dict[target_url] = cell;
        cache_list.emplace_front(target_url);
        list_address[target_url] = cache_list.begin();
        //evict cell if needed
        if(cache_list.size() > this->capacity) {
            //log evict
            logfile.open(LOG, ios::out | ios::ate | ios::app);
            logfile << "(no-id) : " << "NOTE evicted " << target_url << " from cache" << endl;
            logfile.close();
            list_address.erase(cache_list.back());
            //delete cache_dict[cache_list.back()];
            cache_dict.erase(cache_list.back());
            cache_list.pop_back();
        }
        //LOG
        //1: NO_CACHE:  << request_id << ": " << "cached, but requires re-validation" << endl;
        if (cell->is_no_cache == true) {
            logfile.open(LOG, ios::out | ios::ate | ios::app);
            logfile << to_string(request_id) << ": cached, but requires re-validation\n";
            logfile.close();
        }
        else {
            string log_to_print = cell->get_expire_time();
            if(log_to_print == "") {
                logfile.open(LOG, ios::out | ios::ate | ios::app);
                logfile << to_string(request_id) << ": cached, but requires re-validation\n";
                logfile.close();
            }
            else {
                logfile.open(LOG, ios::out | ios::ate | ios::app);
                logfile << to_string(request_id) << ": cached, expires at " << log_to_print;
                logfile.close();
            }
        }
        lock.unlock();
    }

    // remove method, happens when eviction and no-store
    void remove(string url) {
        lock.lock();
        if (cache_dict.find(url) == cache_dict.end()) {
            //log error
            lock.unlock();
            return;
        }
        // log remove
        logfile.open(LOG, ios::out | ios::ate | ios::app);
        logfile << "(no-id) : " << "NOTE removed " <<  url << " from cache due to Cache-Control: no-store" << endl;
        logfile.close();
        //remove element
        cache_list.erase(list_address[url]);
        list_address.erase(url);
        //delete cache_dict[url];
        cache_dict.erase(url);
        lock.unlock();
    }

    // USED FOR DEBUG:
    // void print() {
    //     lock.lock();
    //     std::cout << "cache_list:" << std::endl;
    //     for (auto url: cache_list) {
    //         std::cout << url << "; " <<endl;
    //     }
    //     cout<<endl;
    //     lock.unlock();
    // }
};
#endif
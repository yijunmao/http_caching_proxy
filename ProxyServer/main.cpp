// include libraries
#include <iostream>
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

#include "Proxy.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Cache.h"

// include head files

using namespace std;
std::mutex mtx;

// global variables
const int thread_pool_size = 10;
const char * port = "12345";

// main function
int main() {
    Proxy prox(port);
    prox.client_setup();
    prox.client_accept();
    return 0;
}
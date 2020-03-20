# erss-hwk2-ym134-sz161

HTTP Caching Proxy

## Introduction

<p>For this assignment we've designed and implemented an HTTP caching proxy - a server whose job is to forward requests to the origin server on behalf of the client. Our proxy functions with GET, POST and CONNECT. And it can cache responses, and, when appropriate, responds with the cached copy of a resource rather than re-fetching it. It also uses multiple threads strategy to handle multiple concurrent requests effectively and log information about each request.</p>



## To Start

1. git clone [git@gitlab.oit.duke.edu:ym134/erss-hwk2-ym134-sz161.git](mailto:git@gitlab.oit.duke.edu:ym134/erss-hwk2-ym134-sz161.git)

2. cd eras-hwk2-ym134-sz161

3. sudo docker-compose build

4. sudo docker-compose up

5. setup your firefox network settings to use HTTP Proxy and set port to 12345

6. start browsing with firefox and have fun

   

## Caching Policy

In this project, we used LRU cache for HTTP caching. We implemented it by using two hash tables and one doubled linked list. All operations on each cache block take O(1). The caching logic is as following:

* Cache miss
  * forward the request to server, send the response back to client
  * store the response into cache when response does not include "no-store"

* Cache hit
  * "no-cache": send the request to server to ask for validation
    * "304": send response in cache to client
    * "200": send new response back to client, update cache
  * "must-revalidate": check response freshness (max-age or expires or Date)
    * fresh: reuse the response stored in cache
    * stale: forward the request to server to ask for validation (reconstruct the request by adding If-None-Match/If-Modified-Since) 
      * "304": send response in cache to client
      * "200": send new response back to client, update cache
  * "max-age" or "Expires": compute expiration time and check response freshness
    * fresh: reuse the response stored in cache
    * stale: forward the request to server to ask for validation (reconstruct the request by adding If-None-Match/If-Modified-Since) 
      * "304": send response in cache to client
      * "200": send new response back to client, update cache
  * no "Cache-Control": send the request to server to ask for validation
    * "304": send response in cache to client
    * "200": send new response back to client, update cache

<p>Note that for all the cases listed above, when the proxy gets response from the server, the proxy would store the response in cache only if the "Cache-Control" field in the response doen not contain "no-store"</p>



## Test Cases

#### GET

* chunked: http://www.httpwatch.com/httpgallery/chunked/chunkedimage.aspx, this URL contains two URLs that can be tested for 
  * "max-age=0": http://www.httpwatch.com/favicon.ico
  * "no-cache, no-store": https://www.httpwatch.com/httpgallery/chunked/chunkedimage.aspx
* content-length
  * has neither "Cache-Control" nor "Expires": http://people.duke.edu/~bmr23/ece568/class.html
  * "must-revalidate, max-age=3": http://www.mocky.io/v2/5e584c9f3000007d44fd41c9
  * "must-revalidate, max-age=31536000": http://www.mocky.io/v2/5e584b583000009b43fd41b9

#### CONNECT

* small websites
  * https://www.google.com/
  * https://github.com/
  * https://www.artsci.utoronto.ca/future
* large ones
  * https://www.youtube.com/
  * https://www.bilibili.com/

#### POST

* http://httpbin.org/forms/post

#### NOTE
<p>The proxy.log file is in the file /ProxyServer</p>
<p>If the proxy.log file doesn't show up, please input:</p>
    <p> cd ProxyServer </p>
    <p> sudo chown root ProxyServer</p>
    <p> sudo chgrp root ProxyServer</p>
    <p> sudo chmod u+s ProxyServer</p>

<p>We are confident that our codes run smoothly outside of docker. When running in docker container, we noticed sometimes the program exited with code 139. In this situation you can just rebuild the docker and rerun it.</p>
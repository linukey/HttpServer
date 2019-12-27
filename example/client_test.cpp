#include <iostream>
#include <fstream>
#include <vector>
#include "../src/http_client.h"
#include "../src/utils.h"

using namespace std;
using namespace http::httpclient;
using namespace http::utils;

int main () {
    vector<string> test_urls {
        "http://www.cneo.com.cn/",      // timeout
        "http://www.gsei.com.cn/",      // timeout
        "http://www.rushan-wmw.com/",   // timeout
        "http://www.baidu.com/",        // http
        "http://www.huanqiu.com/",      // redirect
        "https://www.huanqiu.com/"      // https
    };
    
    map<string, string> headers;
    //headers["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9";
    headers["Accept-Encoding"] = "gzip";
    //headers["Accept-Language"] = "zh,zh-TW;q=0.9,zh-CN;q=0.8,en;q=0.7";
    //headers["User-Agent"] = "Mozilla/5.0 (Linux; Android 5.0; SM-G900P Build/LRX21T) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/79.0.3945.88 Mobile Safari/537.36";

    HttpClient http_client;
    for (unsigned i = 0; i < test_urls.size(); ++i) {
        ofstream fout("test/" + to_string(i));
        if (!fout.is_open()) {
            cerr << "open fail!" << endl;
            return 0;
        }
        string response = http_client.http_request(test_urls[i], "GET", &headers, "").to_string();
        fout << response;
    }

    return 0;
}

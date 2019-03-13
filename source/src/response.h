#ifndef __LINUKEY_WEBSERVER_RESPONSE__
#define __LINUKEY_WEBSERVER_RESPONSE__

#include "utils/string_utils.h"
#include "http_common.h"

#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

using std::string;
using std::unordered_map;
using std::vector;
using std::shared_ptr;
using std::cout;
using std::endl;

using namespace linukey::webserver::http_common;

namespace linukey{  
namespace webserver{    
namespace response{

class Response {
private:
    // 状态行
    string _protocol;
    string _status_code;
    string _status_describe;

    // 响应头
    unordered_map<string, string> _headers;

    // 响应体
    string _data;

public:
    /*
     * 生成响应报文文本
     */
    string to_string();

public:
    void setProtocol(const string& protocol);
    void setStatusCode(const string& status_code);
    void setStatusDescribe(const string& status_describe);
    void setHeader(const string& key, const string& val);
    void setData(const string& data);

    const string& getProtocol() const;
    const string& getStatusCode() const;
    const string& getStatusDescribe() const;
    const string& getHeader(const string& key);
    const string& getData() const ;
    
};

}}}

#endif

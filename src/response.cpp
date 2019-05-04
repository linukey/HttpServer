#include "response.h"

namespace linukey{  
namespace webserver{    
namespace response{

string Response::to_string() {
    string ret;
    ret = _protocol + SPACE + _status_code + SPACE + _status_describe + CRLF;
    for (auto it = _headers.begin(); it != _headers.end(); ++it) {
        ret += it->first + ":" + it->second + CRLF;
    }
    ret += CRLF;
    ret += _data;
    return ret;
}

void Response::setProtocol(const string& protocol) {
    _protocol = protocol;
}

void Response::setStatusCode(const string& status_code) {
    _status_code = status_code;
}

void Response::setStatusDescribe(const string& status_describe) {
    _status_describe = status_describe;
}

void Response::setHeader(const string& key, const string& val) {
    string tmp_key = boost::algorithm::to_lower_copy(key);
    _headers[tmp_key] = val;
}

void Response::setData(const string& data) {
    _data = data;
}

const string& Response::getProtocol() const {
    return _protocol;
}

const string& Response::getStatusCode() const {
    return _status_code;
}

const string& Response::getStatusDescribe() const {
    return _status_describe;
}

const string& Response::getHeader(const string& key) {
    string tmp_key = boost::algorithm::to_lower_copy(key);
    return _headers[tmp_key];
}

const string& Response::getData() const {
    return _data;
}

}}}

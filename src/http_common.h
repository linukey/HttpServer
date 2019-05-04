#ifndef __LINUKEY_HTTP_COMMON_H__
#define __LINUKEY_HTTP_COMMON_H__

#include <iostream>
#include <vector>

using std::string;
using std::vector;

namespace linukey {
namespace webserver {
namespace http_common {

static const string CRLF = "\r\n";
static const string SPACE = " ";

// 请求头
enum HEADERS{
    CONTENT_LENGTH = 0,
    CONTENT_TYPE,
    HOST,
    USER_AGENT,
    ACCEPT,
    ACCEPT_ENCODING,
    ACCEPT_LANGUAGE,
    CONNECTION,
    TRANSFER_ENCODING,
    HEADER_NUMS
};

static vector<string> HEADERS_STR = {
    "content-length",  
    "content-type",
    "host",
    "user-agent",
    "accept",
    "accept-encoding",
    "accept-language",
    "connection",
    "transfer-encoding"
};

}
}
}

#endif

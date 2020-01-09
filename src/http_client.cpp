#include "http_client.h"

#include <iostream>

#include <boost/array.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>

#include "utils.h"
#include "request.h"
#include "qt_webkit_helper.h"

//#define debug

using namespace std;
using namespace http::utils;
using boost::asio::ip::tcp;
using http::request::Request;

namespace http {
namespace httpclient {

bool HttpClient::extract_host_port(string url,
                                   string& protocol,
                                   string& host,
                                   string& port) {
    size_t pos = url.find("http://");
    int len = 7;

    if (pos == string::npos) {
        pos = url.find("https://");
        len = 8;
    }

    if (pos == string::npos) {
        return false;
    }

    protocol = url.substr(0, len-3);

    pos = url.find("/", len);
    if (pos == string::npos) {
        url += "/";
        pos = url.find("/", len);
    }

    size_t port_pos = url.find(":", len);
    if (port_pos != string::npos && port_pos < pos) {
        host = url.substr(len, port_pos-len);
        port = url.substr(port_pos+1, pos-port_pos-1);
    } else if (port_pos == string::npos) {
        host = url.substr(len, pos-len);
        if (protocol == "http") {
            port = "80";
        } else if (protocol == "https") {
            port = "443";
        }
    } else {
        return false;
    }

    return true;
}

void HttpClient::build_request_message(const string& url,
                                       const string& method,
                                       const string& host,
                                       const string& data,
                                       map<string, string>* headers,
                                       Request& request) {
    request.setMethod(method);
    request.setUrl(url);
    request.setHeader("Host", host);
    request.setProtocol("HTTP/1.1");
    request.setData(data);

    if (headers != nullptr) {
        for (auto it = headers->begin(); it != headers->end(); ++it) {
            request.setHeader(it->first, it->second);
        }
    }
}

bool HttpClient::parse_response_line(const string& response_line,
                                     Response& response) {
    vector<string> ret;
    boost::split(ret, response_line, boost::is_any_of(" "));
    if (ret.size() < 2) {
        return false;
    }

    response.setProtocol(ret[0]);
    response.setStatusCode(ret[1]);
    // Reason-Phrase  = *<TEXT, excluding CR, LF>
    if (ret.size() > 2) {
        string status_des;
        for (int i = 2; i < ret.size(); ++i) {
            status_des += ret[i];
            if (i != ret.size()-1) {
                status_des += " ";
            }
        }
        response.setStatusDescribe(status_des);
    }

    return response.Protocol().substr(0, 5) == "HTTP/";
}

template<class T>
void HttpClient::parse_response(boost::asio::io_context& io_context,
                                T& socket,
                                Request& request,
                                Response& response,
                                int timeout) {
    try {
        boost::system::error_code ec;

        // 发送请求
        socket.async_write_some(boost::asio::buffer(request.to_string()),
                                [&](const boost::system::error_code& err, std::size_t bytes_transferred){ ec = err; });
        if (timelimit(io_context, timeout) || ec) {
            throw ec ? HttpException("write error") : HttpException("write timeout");
        }

        boost::asio::streambuf response_streambuf;
        istream response_stream(&response_streambuf);

        // 解析状态行
        boost::asio::async_read_until(socket,
                                      response_streambuf,
                                      "\r\n",
                                      [&](const boost::system::error_code& err, std::size_t bytes_transferred){ ec = err; });
        if (timelimit(io_context, timeout) || ec) {
            throw ec ? HttpException("read error") : HttpException("read timeout");
        }

        string response_line;
        getline(response_stream, response_line);

        boost::trim(response_line);
        if (!parse_response_line(response_line, response)) {
            throw HttpException("error response_line" + response_line);
        }

        // 解析响应头
        boost::asio::async_read_until(socket,
                                      response_streambuf,
                                      "\r\n\r\n",
                                      [&](const boost::system::error_code& err, std::size_t bytes_transferred){ ec = err; });
        if (timelimit(io_context, timeout) || ec) {
            throw ec ? HttpException("read error") : HttpException("read timeout");
        }

        string header_line;
        while (getline(response_stream, header_line) && header_line != "\r") {
            size_t pos = header_line.find(":");
            if (pos == string::npos) { continue; }
            string key = boost::trim_copy(header_line.substr(0, pos));
            string value = boost::trim_copy(header_line.substr(pos+1));
            response.setHeader(key, value);
        }

        // 拦截 4xx 5xx
        if (response.StatusCode()[0] == '4' || response.StatusCode()[0] == '5') {
            throw HttpException("HTTP Error " + response.StatusCode() + ":" + response.StatusDescribe());
        }

        // 接收包体
        string& response_body = response.setData();
        // chunked
        if (response.Header("transfer-encoding") == "chunked") {
            int cur = 0, len = 0;
            size_t pos = string::npos;

            boost::asio::streambuf::const_buffers_type cbt = response_streambuf.data();
            string chunked_body(boost::asio::buffers_begin(cbt), boost::asio::buffers_end(cbt));
            response_streambuf.consume(chunked_body.size());

            while (true) {
                while ((pos = chunked_body.find("\r\n", cur)) == string::npos) {
                    boost::asio::async_read_until(socket,
                                                  response_streambuf,
                                                  "\r\n",
                                                  [&](const boost::system::error_code& err, std::size_t bytes_transferred){ ec = err; });
                    if (timelimit(io_context, timeout) || ec) {
                        throw ec ? HttpException("read error") : HttpException("read timeout");
                    }

                    boost::asio::streambuf::const_buffers_type cbt = response_streambuf.data();
                    string read_str(boost::asio::buffers_begin(cbt), boost::asio::buffers_end(cbt));
                    chunked_body += read_str;
                    response_streambuf.consume(read_str.size());
                }
                stringstream ss;
                ss << hex << chunked_body.substr(cur, pos-cur);
                ss >> len;
                cur = cur + (pos-cur) + 2;
                while (chunked_body.size()-cur < len+2) {
                    int need_len = len + 2 - chunked_body.size() + cur;

                    boost::asio::async_read(socket,
                                            response_streambuf,
                                            boost::asio::transfer_at_least(need_len),
                                            [&](const boost::system::error_code& err, std::size_t bytes_transferred){ ec = err; });
                    if (timelimit(io_context, timeout) || ec) {
                        throw ec ? HttpException("read error") : HttpException("read timeout");
                    }

                    boost::asio::streambuf::const_buffers_type cbt = response_streambuf.data();
                    string read_str(boost::asio::buffers_begin(cbt), boost::asio::buffers_end(cbt));
                    chunked_body += read_str;
                    response_streambuf.consume(read_str.size());
                }
                response_body += chunked_body.substr(cur, len);
                if (len == 0) {
                    break;
                }
                cur = cur + 2 + len;
            }
        // content-length
        } else if (response.Header("content-length").size() > 0) {
            int content_length = stoi(response.Header("content-length"));
            boost::asio::streambuf::const_buffers_type cbt = response_streambuf.data();
            string first(boost::asio::buffers_begin(cbt), boost::asio::buffers_end(cbt));
            response_body += first;
            response_streambuf.consume(first.size());

            boost::asio::async_read(socket,
                                    response_streambuf,
                                    boost::asio::transfer_at_least(content_length - first.size()),
                                    [&](const boost::system::error_code& err, std::size_t bytes_transferred){ ec = err; });
            if (timelimit(io_context, timeout) || ec) {
                throw ec ? HttpException("read error") : HttpException("read timeout");
            }

            cbt = response_streambuf.data();
            string rest(boost::asio::buffers_begin(cbt), boost::asio::buffers_end(cbt));
            response_body += rest;
            response_streambuf.consume(rest.size());
        }
        //} else {
            // 空包体
            // example: http://memory.thethirdmedia.com/

        // 如果是gzip，解压
        if (boost::to_lower_copy(response.Header("Content-Encoding")) == "gzip") {
            string data = response.Data();
            string& decompress_data = response.setData();
            http::utils::gzip_decompress(data, decompress_data);
        }
    } catch (const exception& e) {
        throw HttpException(e.what());
    }
}

string HttpClient::build_redirection_url(const string& protocol,
                                         const string& host,
                                         Response& response) {
    string redirect = response.Header("Location");
    boost::trim(redirect);

    if (redirect.find("http") == string::npos) {
        if (redirect[0] == '/') {
            redirect = (protocol + "://" + host) + redirect;
        } else {
            redirect = (protocol + "://" + host + "/") + redirect;
        }
    }

    return redirect;
}

bool HttpClient::timelimit(boost::asio::io_context& io_context,
                           int timeout) {
    io_context.restart();
    io_context.run_for(std::chrono::seconds(timeout));
    return !io_context.stopped();
}

Response HttpClient::http_request(const string& url,
                                  const string& method,
                                  map<string, string>* headers,
                                  const string& data,
                                  int timeout,
                                  int redirect_count) {
#ifdef debug
    if (redirect_count == 0) {
        cerr << url << endl;
    }
    timeval start, end, dot;
    mark_performance(start);
#endif
    Response response;
    string protocol, host, port;

    // 解析 协议头、host、port
    if (!extract_host_port(url, protocol, host, port)) {
        throw HttpException("not valid url!");
    }

    // 支持 http、https 协议
    if (protocol != "http" && protocol != "https") {
        throw HttpException("protocol error, only support http and https!");
    }

    // 设置请求报文
    Request request;
    build_request_message(url, method, host, data, headers, request);

    try {
        boost::asio::io_context io_context;
#ifdef debug
        mark_performance(dot);
#endif
        // 根据 hostname 查询 ip 地址
        tcp::resolver resolver(io_context);
        boost::system::error_code ec;
        tcp::resolver::results_type endpoints = resolver.resolve(host, protocol, ec);
        if (ec) { throw HttpException(ec.message()); }

        boost::asio::ip::tcp::endpoint endpoint;
        for (auto it = endpoints.begin(); it != endpoints.end(); ++it) {
            boost::asio::ip::tcp::endpoint ep = *it;
            if (ep.port() == stoi(port)) {
                endpoint = ep;
                break;
            }
        }
#ifdef debug
        mark_performance(end);
        if (redirect_count == 0) {
            cerr << "resolve cost " << performance(dot, end) << "ms" << endl;
        }
#endif
        // 如果没有查询到有效 ip 地址，直接写入请求的 ip 地址以及 port
        if (endpoint.address().to_string() == "0.0.0.0") {
            boost::asio::ip::address ad = boost::asio::ip::address::from_string(host);
            endpoint = boost::asio::ip::tcp::endpoint(ad, stoi(port));
        }

        // https
        if (protocol == "https") {
#ifdef debug
            mark_performance(dot);
#endif
            boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv12_client);
            ctx.set_verify_mode(boost::asio::ssl::verify_none);
            boost::asio::ssl::stream<tcp::socket> socket{io_context, ctx};

            // Set SNI Hostname (many hosts need this to handshake successfully)
            // https://github.com/boostorg/beast/blob/develop/example/http/client/sync-ssl/http_client_sync_ssl.cpp
            if(!SSL_set_tlsext_host_name(socket.native_handle(), host.c_str())) {
                boost::system::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
                    throw boost::system::system_error{ec};
            }

            boost::system::error_code ec;
            socket.lowest_layer().async_connect(endpoint, [&](const boost::system::error_code& error){ ec = error; });
            if (timelimit(io_context, timeout) || ec) {
                socket.lowest_layer().close();
                throw ec ? HttpException("connect error") : HttpException("connect timeout");
            }
#ifdef debug
            mark_performance(end);
            if (redirect_count == 0) {
                cerr << "connect cost " << performance(dot, end) << "ms" << endl;
            }
            mark_performance(dot);
#endif
            socket.handshake(boost::asio::ssl::stream_base::client);
            try {
                parse_response(io_context, socket, request, response, timeout);
            } catch (const HttpException& ex) {
                socket.lowest_layer().close();
                throw ex;
            }
#ifdef debug
            mark_performance(end);
            if (redirect_count == 0) {
                cerr << "parse response cost " << performance(dot, end) << "ms" << endl;
            }
#endif
            socket.lowest_layer().close();
        // http
        } else if (protocol == "http") {
#ifdef debug
            mark_performance(dot);
#endif
            tcp::socket socket(io_context);
            boost::system::error_code ec;
            socket.async_connect(endpoint, [&](const boost::system::error_code& error){ ec = error; });
            if (timelimit(io_context, timeout) || ec) {
                socket.close();
                throw ec ? HttpException("connect error") : HttpException("connect timeout");
            }
#ifdef debug
            mark_performance(end);
            if (redirect_count == 0) {
                cerr << "connect cost " << performance(dot, end) << "ms" << endl;
            }
            mark_performance(dot);
#endif
            try {
                parse_response(io_context, socket, request, response, timeout);
            } catch (const HttpException& ex) {
                socket.close();
                throw ex;
            }
#ifdef debug
            mark_performance(end);
            if (redirect_count == 0) {
                cerr << "parse response cost " << performance(dot, end) << "ms" << endl;
            }
#endif
            socket.close();
        }
#ifdef debug
        mark_performance(dot);
#endif
        // 3xx Redirection
        if (redirect_count <= 5) {
            string code = response.StatusCode();
            if (code == "301" || code == "302") {
                string lower_method = boost::to_lower_copy(method);
                if (lower_method == "get" || lower_method == "head") {
                    string re_url = build_redirection_url(protocol, host, response);
                    response = http_request(re_url, method, headers, data, timeout, redirect_count+1);
                }
            }
        }
#ifdef debug
        mark_performance(end);
        if (redirect_count == 0) {
            cerr << "redirect cost " << performance(dot, end) << "ms" << endl;
            cerr << "all cost " << performance(start, end) << "ms" << endl;
        }
#endif
        return response;
    } catch (const HttpException& e) {
        throw e;
    } catch (const exception& e) {
        throw e;
    }
}

Response HttpClient::http_request_render(const string& url,
                                         const string& method,
                                         map<string, string>* headers,
                                         const string& data,
                                         int timeout,
                                         int redirect_count,
                                         int argc,
                                         char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");

	QApplication app(argc, argv);

    Response response;
	http::qt_helper::WebPage *w = new http::qt_helper::WebPage(app, response);
	w->settings()->setAttribute(QWebSettings::PrivateBrowsingEnabled, true);
	w->settings()->setAttribute(QWebSettings::PluginsEnabled, false);
	w->settings()->setAttribute(QWebSettings::JavaEnabled, false);
	w->settings()->setAttribute(QWebSettings::AutoLoadImages, false);
	w->settings()->setAttribute(QWebSettings::DnsPrefetchEnabled, true);
	w->settings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);

    QTimer *t = new QTimer();
    QObject::connect(t, &QTimer::timeout, [&app](){ app.quit(); });
    t->setSingleShot(true);
    t->start(timeout * 1000);

    QUrl q_url(url.c_str());
	w->load(q_url.toString());
	app.exec();

    return response;
}

}}

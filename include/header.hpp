// Copyright 2020 Your Name <your_email>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/bind.hpp>
#include <ctime>
#include <iostream>
#include <vector>
#include <utility>
#include <thread>
#include <boost/thread.hpp>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <algorithm>
#include <fstream>
#include <memory>
#include <mutex>
#include <shared_mutex>

namespace beast = boost::beast;    // from <boost/beast.hpp>
namespace http = beast::http;      // from <boost/beast/http.hpp>
namespace net = boost::asio;       // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;  // from <boost/asio/ip/tcp.hpp>
using nlohmann::json;

#ifndef INCLUDE_SUGGEST_HPP_
#define INCLUDE_SUGGEST_HPP_

class Server {
 public:
  Server();
  int startServer(int argc, char* argv[]);
 private:
  uint16_t port;
  std::shared_ptr<std::string> doc_root;
};



#endif  // INCLUDE_SUGGEST_HPP_

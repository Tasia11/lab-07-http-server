// Copyright 2020 Your Name <your_email>

#include "header.hpp"

boost::shared_mutex m;
json suggestions;

bool sortJson(json& a, json& b) {
  if (a.at("cost").get<int>() > b.at("cost").get<int>()) return true;
  return false;
}

void write() {
  std::ifstream file("./v1/api/suggest/suggestions.json");
  boost::lock_guard<boost::shared_mutex> lk(m);
  // suggestions.clear();
  json temp;
  try {
    file >> temp;
    suggestions = temp;
    std::cout << "Collections is updated" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
  file.close();
}

void update(clock_t startTime) {
  for (;;) {
    clock_t presentTime = clock();
    if ((static_cast<double>(presentTime - startTime)) / CLOCKS_PER_SEC == 5) {
      startTime = presentTime;
      write();
    }
  }
}

json read(std::string&& word) {
  json formData = json::parse("{ \"suggestions\": []}");
  m.try_lock_shared();
  json temp;
  // lk.lock();
  for (const auto& i : suggestions) {
    if (i.at("name").get<std::string>().find(word) != std::string::npos)
      temp.push_back(i);
  }
  m.unlock_shared();
  std::sort(temp.begin(), temp.end(), sortJson);
  size_t num = 0;
  for (const auto& elem : temp) {
    formData.at("suggestions")
        .push_back(json::parse(R"({ "text": ")" +
                               elem.at("name").get<std::string>() + "\"," +
                               "\"position\": " + std::to_string(num) + "}"));
    ++num;
  }
  return formData;
}

std::string path_cat(beast::string_view base, beast::string_view path) {
  if (base.empty()) return std::string(path);
  std::string result(base);
#ifdef BOOST_MSVC
  char constexpr path_separator = '\\';
  if (result.back() == path_separator) result.resize(result.size() - 1);
  result.append(path.data(), path.size());
  for (auto& c : result)
    if (c == '/') c = path_separator;
#else
  char constexpr path_separator = '/';
  if (result.back() == path_separator) result.resize(result.size() - 1);
  result.append(path.data(), path.size());
#endif
  return result;
}

template <class Body, class Allocator, class Send>
void handle_request(beast::string_view doc_root,
                    http::request<Body, http::basic_fields<Allocator>>&& req,
                    Send&& send) {
  auto const bad_request = [&req](beast::string_view why) {
    http::response<http::string_body> res{http::status::bad_request,
                                          req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = std::string(why);
    res.prepare_payload();
    return res;
  };

  auto const not_found = [&req](beast::string_view target) {
    http::response<http::string_body> res{http::status::not_found,
                                          req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "The resource '" + std::string(target) + "' was not found.";
    res.prepare_payload();
    return res;
  };

  auto const server_error = [&req](beast::string_view what) {
    http::response<http::string_body> res{http::status::internal_server_error,
                                          req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "An error occurred: '" + std::string(what) + "'";
    res.prepare_payload();
    return res;
  };

  if (req.method() != http::verb::post)
    return send(bad_request("Unknown HTTP-method for this app"));

  if (req.target().empty() || req.target()[0] != '/' ||
      req.target().find("..") != beast::string_view::npos)
    return send(bad_request("Illegal request-target"));

  // Build the path to the requested file
  std::string path = path_cat(doc_root, req.target());
  nlohmann::json j = nlohmann::json::parse(req.body());
  json bodyOut = read(j.at("input").get<std::string>());
  if ((req.target() == "/v1/api/suggest/") ||
      (req.target() == "/v1/api/suggest"))
    path.append("suggestions.json");

  // Attempt to open the file
  beast::error_code ec;
  http::file_body::value_type body;
  body.open(path.c_str(), beast::file_mode::scan, ec);
  // Handle the case where the file doesn't exist
  if (ec == beast::errc::no_such_file_or_directory) {
    return send(not_found(req.target()));
  }

  // Handle an unknown error
  if (ec) return send(server_error(ec.message()));

  // Cache the size since we need it after the move
  auto const size = body.size();
  // Respond to POST request
  http::response<http::string_body> res{http::status::ok, req.version()};
  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, "application/json");
  res.content_length(size);
  res.keep_alive(req.keep_alive());
  res.body() = bodyOut.dump(1, ' ');
  res.prepare_payload();
  return send(std::move(res));
}

template <class Stream>
struct send_lambda {
  Stream& stream_;
  bool& close_;
  beast::error_code& ec_;

  explicit send_lambda(Stream& stream, bool& close, beast::error_code& ec)
      : stream_(stream), close_(close), ec_(ec) {}

  template <bool isRequest, class Body, class Fields>
  void operator()(http::message<isRequest, Body, Fields>&& msg) const {
    // Determine if we should close the connection after
    close_ = msg.need_eof();

    // We need the serializer here because the serializer requires
    // a non-const file_body, and the message oriented version of
    // http::write only works with const messages.
    http::serializer<isRequest, Body, Fields> sr{msg};
    http::write(stream_, sr, ec_);
  }
};

void fail(beast::error_code ec, char const* what) {
  std::cerr << what << ": " << ec.message() << "\n";
}

void do_session(tcp::socket& socket,
                std::shared_ptr<std::string const> const& doc_root) {
  bool close = false;
  beast::error_code ec;

  beast::flat_buffer buffer;

  send_lambda<tcp::socket> lambda{socket, close, ec};

  for (;;) {
    http::request<http::string_body> req;
    http::read(socket, buffer, req, ec);
    if (ec == http::error::end_of_stream) break;
    if (ec) return fail(ec, "read");

    handle_request(*doc_root, std::move(req), lambda);
    if (ec) return fail(ec, "write");
    if (close) {
      break;
    }
  }
  socket.shutdown(tcp::socket::shutdown_send, ec);
}

int Server::startServer(int argc, char* argv[]) {
  try {
    if (argc != 4) {
      std::cerr
        << "\nUsage: http-server-sync <address> <port> <doc_root>\n"
        << "Example:\n"
        << "    http-server-sync 0.0.0.0 8080 .\n";
      return EXIT_FAILURE;
    }

    auto const address = net::ip::make_address(argv[1]);
    port = static_cast<uint16_t>(std::atoi(argv[2]));
    doc_root = std::make_shared<std::string>(argv[3]);
    net::io_context ioc{1};

    tcp::acceptor acceptor{ioc, {address, port}};

    clock_t beginTime = clock();

    std::thread{std::bind(&update, beginTime)}.detach();

    for (;;) {
      tcp::socket socket{ioc};
      acceptor.accept(socket);

      std::thread{std::bind(&do_session, std::move(socket),
                            doc_root)}.detach();
    }
  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}

Server::Server() {
  std::ifstream file("./v1/api/suggest/suggestions.json");
  file >> suggestions;
  file.close();
}

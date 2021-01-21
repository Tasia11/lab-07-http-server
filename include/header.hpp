// Copyright 2020 Your Name <your_email>

#ifndef INCLUDE_HEADER_HPP_
#define INCLUDE_HEADER_HPP_
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

class Suggestions_collection {
 public:
  Suggestions_collection();
  void Update(json storage);
  json Suggest(const std::string& input);

 private:
  json suggestions_;
};
#endif // INCLUDE_HEADER_HPP_

class Server {
 public:
  Server();
  int startServer(int argc, char* argv[]);
 private:
  uint16_t port;
  std::shared_ptr<std::string> doc_root;
};



#endif  // INCLUDE_SUGGEST_HPP_

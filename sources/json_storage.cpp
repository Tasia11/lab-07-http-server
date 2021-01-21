// Copyright 2020 Your Name <your_email>

#include <exception>
#include <fstream>
#include <iostream>
#include <json_storage.hpp>

json_storage::json_storage(const std::string& filename) : filename_(filename) {}
json json_storage::get_storage() const { return storage_; }
void json_storage::Load() {
  try {
    std::ifstream in(filename_);
    in >> storage_;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
  }
}

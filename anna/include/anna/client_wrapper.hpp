#pragma once

#include <exception>
#include <memory>
#include <optional>
#include <set>
#include <string>

struct KvsClient;
struct KvsClientDeleter {
  void operator()(KvsClient *) const noexcept;
};

namespace anna {
struct ClientWrapper {
  ClientWrapper() = delete;
  explicit ClientWrapper(const std::string &config_file);

  void put(const std::string &key, const std::string &value);
  std::optional<std::string> get(const std::string &key);
  // void put_set(const std::string &key, const std::set<std::string> &value);
  // std::set<std::string> get_set(const std::string &key);

private:
  std::unique_ptr<KvsClient, KvsClientDeleter> raw_client_;
};

enum class ErrorKind {
  Success = 0,
  InvalidResponse,
  KeyNotFound,
  WrongThread,
  Timeout,
  Lattice,
  NoServers,
  Unknown,
};

struct Error : std::exception {
  explicit Error(ErrorKind kind) : kind_(kind) {
  }

private:
  ErrorKind kind_;
};
} // namespace anna

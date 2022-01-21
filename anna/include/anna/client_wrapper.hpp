#pragma once

#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

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
  void put_set(const std::string &key,
               const std::unordered_set<std::string> &set);
  std::optional<std::unordered_set<std::string>>
  get_set(const std::string &key);

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

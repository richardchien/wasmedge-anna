#include <anna/client_wrapper.hpp>

#include <fstream>

#include "client/kvs_client.hpp"
#include "yaml-cpp/yaml.h"

ZmqUtil zmq_util;
ZmqUtilInterface *kZmqUtil = &zmq_util;

void KvsClientDeleter::operator()(KvsClient *client) const noexcept {
  delete client;
}

namespace anna {
ClientWrapper::ClientWrapper(const std::string &config_file) {
  YAML::Node config = YAML::LoadFile(config_file);
  auto n_routing_thread = config["threads"]["routing"].as<unsigned>();

  YAML::Node user = config["user"];
  Address ip = user["ip"].as<Address>();

  vector<Address> routing_ips;
  if (YAML::Node elb = user["routing-elb"]) {
    routing_ips.push_back(elb.as<string>());
  } else {
    YAML::Node routing = user["routing"];
    for (const YAML::Node &node : routing) {
      routing_ips.push_back(node.as<Address>());
    }
  }

  vector<UserRoutingThread> threads;
  for (Address addr : routing_ips) {
    for (unsigned i = 0; i < n_routing_thread; i++) {
      threads.push_back(UserRoutingThread(addr, i));
    }
  }

  raw_client_ = std::unique_ptr<KvsClient, KvsClientDeleter>(
      new KvsClient(threads, ip, 0, 10000), KvsClientDeleter());
}

// static ErrorKind convert_anna_error(AnnaError err) {
//   switch (err) {
//   case AnnaError::NO_ERROR:
//     return ErrorKind::Success;
//   case AnnaError::KEY_DNE:
//     return ErrorKind::KeyNotFound;
//   case AnnaError::WRONG_THREAD:
//     return ErrorKind::WrongThread;
//   case AnnaError::TIMEOUT:
//     return ErrorKind::Timeout;
//   case AnnaError::LATTICE:
//     return ErrorKind::Lattice;
//   case AnnaError::NO_SERVERS:
//     return ErrorKind::NoServers;
//   default:
//     return ErrorKind::Unknown;
//   }
// }

bool ClientWrapper::put(const std::string &key, const std::string &value) {
  LWWPairLattice<string> val(
      TimestampValuePair<string>(generate_timestamp(0), value));

  string rid = raw_client_->put_async(key, serialize(val), LatticeType::LWW);
  vector<KeyResponse> responses = raw_client_->receive_async();
  while (responses.size() == 0) {
    responses = raw_client_->receive_async();
  }

  KeyResponse response = responses[0];

  if (response.response_id() != rid) {
    return false;
  }
  if (response.error() != AnnaError::NO_ERROR) {
    return false;
  }
  return true;
}

std::optional<std::string> ClientWrapper::get(const std::string &key) {
  raw_client_->get_async(key);

  vector<KeyResponse> responses = raw_client_->receive_async();
  while (responses.size() == 0) {
    responses = raw_client_->receive_async();
  }

  if (responses.size() > 1) {
    return std::nullopt;
  }

  if (responses[0].tuples(0).lattice_type() != LatticeType::LWW) {
    return std::nullopt;
  }

  LWWPairLattice<string> lww_lattice =
      deserialize_lww(responses[0].tuples(0).payload());
  return lww_lattice.reveal().value;
}

bool ClientWrapper::put_set(const std::string &key,
                            const std::unordered_set<std::string> &set) {
  string rid = raw_client_->put_async(key, serialize(SetLattice<string>(set)),
                                      LatticeType::SET);

  vector<KeyResponse> responses = raw_client_->receive_async();
  while (responses.size() == 0) {
    responses = raw_client_->receive_async();
  }

  KeyResponse response = responses[0];

  if (response.response_id() != rid) {
    return false;
  }
  if (response.error() != AnnaError::NO_ERROR) {
    return false;
  }
  return true;
}

std::optional<std::unordered_set<std::string>>
ClientWrapper::get_set(const std::string &key) {
  raw_client_->get_async(key);
  string serialized;

  vector<KeyResponse> responses = raw_client_->receive_async();
  while (responses.size() == 0) {
    responses = raw_client_->receive_async();
  }

  if (responses[0].tuples(0).lattice_type() != LatticeType::SET) {
    return std::nullopt;
  }

  SetLattice<string> latt = deserialize_set(responses[0].tuples(0).payload());
  return latt.reveal();
}
} // namespace anna

#include "dummy_reader/dummy_rfid_reader.h"
#include "ipc_commands.hpp"
#include <iostream>
#include <zmq.hpp>

using namespace ipc;

int main(int argc, char *argv[]) {
  std::string device_path = "/tmp/dummyrfid";
  zmq::context_t context(1);

  auto cmd_socket_addr = command_socket_address(System::RFID_READER);
  if (!cmd_socket_addr) {
    std::cerr << "[RFID Reader] No socket address configured for RFID_READER"
              << std::endl;
    return 1;
  }

  zmq::socket_t publisher(context, ZMQ_PUB);
  publisher.bind(std::string(*cmd_socket_addr));

  if (argc > 1) {
    device_path = argv[1];
  }

  // Create RFID reader with callback
  DummyRfidReader reader(device_path, [&publisher](const std::string &tag) {
    std::cout << "[RFID Reader] Scanned tag: " << tag << std::endl;

    json j;
    j["tag_id"] = tag;
    j["command"] = json{{"type", "FILE"}, {"path", ""}, {"loop", false}};

    std::string json_str = j.dump();
    zmq::message_t pub_msg(json_str.size());
    memcpy(pub_msg.data(), json_str.c_str(), json_str.size());
    publisher.send(pub_msg, zmq::send_flags::none);
  });

  if (!reader.init()) {
    std::cerr << "[RFID Reader] Failed to initialize" << std::endl;
    return 1;
  }

  try {
    reader.readFromDevice();
  } catch (const std::exception &e) {
    std::cerr << "[RFID Reader] Fatal error: " << e.what() << std::endl;
    reader.stop();
    return 1;
  }

  return 0;
}

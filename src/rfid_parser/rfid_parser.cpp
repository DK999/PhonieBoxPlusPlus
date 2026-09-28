#include "rfid_parser.h"
#include "ipc_commands.hpp"
#include <fstream>
#include <iostream>
#include <zmq.hpp>

using namespace ipc;

RfidParser::RfidParser() : m_ctx(1), m_rfidreader_sub(m_ctx, ZMQ_SUB) {
    auto rfid_socket = command_socket_address(System::RFID_READER);
    m_system_pub = make_publisher(m_ctx, System::SYSTEM);
    m_media_pub = make_publisher(m_ctx, System::MEDIA);
    m_volume_pub = make_publisher(m_ctx, System::VOLUME);
    m_rfidreader_sub.connect(std::string(*rfid_socket));
    m_rfidreader_sub.set(zmq::sockopt::subscribe, "");

    if (!loadConfigs(LIBRARY_TAGS, m_libtags))
        std::cerr << "Could not load library config" << std::endl;
    if (!loadConfigs(SYSTEM_TAGS, m_systags))
        std::cerr << "Could not load system config" << std::endl;
    if (!loadConfigs(MEDIA_TAGS, m_mediatags))
        std::cerr << "Could not load media config" << std::endl;
    if (!loadConfigs(VOLUME_TAGS, m_volumetags))
        std::cerr << "Could not load volume config" << std::endl;
}

void RfidParser::run() {
    std::cout << "RFID Parser started. Subscribing to RFID events...\n";

    while (true) {
        zmq::message_t msg;
        auto result = m_rfidreader_sub.recv(msg, zmq::recv_flags::none);

        if (result) {
            std::string message(static_cast<char *>(msg.data()), msg.size());
            std::cout << "[RFID Parser] Received: " << message << std::endl;

            IncomingCommand cmd;
            try {
                cmd = IncomingCommand::parse(json::parse(message));
            } catch (const std::exception &e) {
                std::cerr << "[RFID Parser] Failed to parse command: "
                          << e.what() << std::endl;
                publishErrorStatus(-1, "Failed to parse incoming command");
                continue;
            }

            std::string rfid_id = cmd.tag_id;
            if (rfid_id.empty()) {
                std::cerr << "[RFID Parser] Empty tag_id in command"
                          << std::endl;
                publishErrorStatus(-2, "Empty tag_id");
                continue;
            }

            processMessage(rfid_id);
        }
    }
}

void RfidParser::processMessage(const std::string &id) {
    if (id.empty()) return;
    IncomingCommand cmd;
    if (!getCommand(id, cmd)) {
        std::cerr << "Unknown command for tag" << std::endl;
        return;
    }

    consumeCommand(cmd);
    publishProcessedData(cmd);
}

zmq::socket_t RfidParser::make_publisher(zmq::context_t &ctx, System sys) {
    auto addr = command_socket_address(sys);
    if (!addr) {
        throw std::runtime_error("No socket address configured for system: " +
                                 std::string(to_string(sys)));
    }

    zmq::socket_t pub(ctx, zmq::socket_type::pub);
    pub.bind(std::string(*addr));
    return pub;
}

bool RfidParser::getCommand(const std::string &tag, IncomingCommand &cmd) {
    if (getMediaCommand(tag, cmd)) return true;
    if (getVolumeCommand(tag, cmd)) return true;
    if (getSystemCommand(tag, cmd)) return true;
    return false;
}

bool RfidParser::getMediaCommand(const std::string &tag, IncomingCommand &cmd) {
    if (getPlayMediaCommand(tag, cmd)) return true;
    json obj;
    if (!getMatchingTag(tag, m_mediatags, obj)) { return false; }

    std::string type = obj["type"].get<std::string>();
    cmd.tag_id = tag;
    cmd.domain = System::MEDIA;
    if (type == "FILE") {
        cmd.payload = FILECommand::fromJson(obj);
    } else if (type == "STOP_AUDIO") {
        cmd.payload = STOP_AUDIOCommand::fromJson(obj);
    } else if (type == "PAUSE_AUDIO") {
        cmd.payload = PAUSE_AUDIOCommand::fromJson(obj);
    } else if (type == "RESUME_AUDIO") {
        cmd.payload = RESUME_AUDIOCommand::fromJson(obj);
    } else if (type == "NEXT_TITLE") {
        cmd.payload = NEXT_TITLECommand::fromJson(obj);
    } else if (type == "PREVIOUS_TITLE") {
        cmd.payload = PREVIOUS_TITLECommand::fromJson(obj);
    } else if (type == "REPEAT_TITLE") {
        cmd.payload = REPEAT_TITLECommand::fromJson(obj);
    } else {
        return false;
    }
    return true;
}

bool RfidParser::getPlayMediaCommand(const std::string &tag,
                                     IncomingCommand &cmd) {
    json obj;
    if (!getMatchingTag(tag, m_libtags, obj)) { return false; }
    cmd.tag_id = tag;
    cmd.domain = System::MEDIA;
    cmd.payload = FILECommand::fromJson(obj);
    return true;
}

bool RfidParser::getVolumeCommand(const std::string &tag,
                                  IncomingCommand &cmd) {
    json obj;
    if (!getMatchingTag(tag, m_volumetags, obj)) { return false; }

    std::string type = obj["type"].get<std::string>();
    cmd.tag_id = tag;
    cmd.domain = System::VOLUME;
    if (type == "VOLUME_INCREASE") {
        cmd.payload = VOLUME_INCREASECommand::fromJson(obj);
    } else if (type == "VOLUME_DECREASE") {
        cmd.payload = VOLUME_DECREASECommand::fromJson(obj);
    } else if (type == "MUTE") {
        cmd.payload = MUTECommand::fromJson(obj);
    } else if (type == "UNMUTE") {
        cmd.payload = UNMUTECommand::fromJson(obj);
    } else {
        return false;
    }
    return true;
}

bool RfidParser::getSystemCommand(const std::string &tag,
                                  IncomingCommand &cmd) {
    json obj;
    if (!getMatchingTag(tag, m_systags, obj)) { return false; }

    std::string type = obj["type"].get<std::string>();
    cmd.tag_id = tag;
    cmd.domain = System::SYSTEM;
    if (type == "SHUTDOWN") {
        cmd.payload = SHUTDOWNCommand::fromJson(obj);
    } else if (type == "RELOAD_CONFIGS") {
        cmd.payload = RELOAD_CONFIGSCommand::fromJson(obj);
    } else if (type == "RELOAD_LIBRARY") {
        cmd.payload = RELOAD_LIBRARYCommand::fromJson(obj);
    } else {
        return false;
    }
    return true;
}

bool RfidParser::getMatchingTag(const std::string &tag, json &cfg,
                                json &object) {
    if (!cfg.contains("tags")) { return false; }
    if (!cfg["tags"].contains(tag)) { return false; }
    if (!cfg["tags"][tag].contains("type") ||
        !cfg["tags"][tag]["type"].is_string()) {
        return false;
    }

    object = cfg["tags"][tag];
    return true;
}

bool RfidParser::consumeCommand(const IncomingCommand &cmd) {
    std::visit(
        [this](const auto &c) -> void {
            if constexpr (std::is_same_v<std::decay_t<decltype(c)>,
                                         RELOAD_CONFIGSCommand>) {
                std::cout << "Reloading configs" << std::endl;
                if (!loadConfigs(MEDIA_TAGS, m_libtags))
                    std::cerr << "Could not load media config" << std::endl;
                if (!loadConfigs(SYSTEM_TAGS, m_systags))
                    std::cerr << "Could not load audio config" << std::endl;
                if (!loadConfigs(VOLUME_TAGS, m_volumetags))
                    std::cerr << "Could not load media config" << std::endl;
                if (!loadConfigs(MEDIA_TAGS, m_mediatags))
                    std::cerr << "Could not load audio config" << std::endl;
            }
        },
        cmd.payload);
    return true;
}

void RfidParser::publishErrorStatus(int code, const std::string &message) {
    ERRORStatus status;
    status.sock = System::RFID_READER;
    status.code = code;
    status.message = message;

    std::string json_str = status.toJson().dump();
    zmq::message_t pub_msg(json_str.size());
    memcpy(pub_msg.data(), json_str.c_str(), json_str.size());
    m_system_pub.send(pub_msg, zmq::send_flags::none);
    std::cout << "[RFID Parser] Published error status: " << json_str
              << std::endl;
}

void RfidParser::publishProcessedData(const IncomingCommand &cmd) {
    json j;
    j["tag_id"] = cmd.tag_id;
    std::visit(overloaded{
                   [&](const MEDIACommand &mc) {
                       std::visit(overloaded{
                                      [&](const FILECommand &c) {
                                          j["command"] = c.toJson();
                                      },
                                      [&](const STOP_AUDIOCommand &c) {
                                          j["command"] = c.toJson();
                                      },
                                      [&](const PAUSE_AUDIOCommand &c) {
                                          j["command"] = c.toJson();
                                      },
                                      [&](const RESUME_AUDIOCommand &c) {
                                          j["command"] = c.toJson();
                                      },
                                      [&](const NEXT_TITLECommand &c) {
                                          j["command"] = c.toJson();
                                      },
                                      [&](const PREVIOUS_TITLECommand &c) {
                                          j["command"] = c.toJson();
                                      },
                                      [&](const REPEAT_TITLECommand &c) {
                                          j["command"] = c.toJson();
                                      },
                                  },
                                  mc);
                   },
                   [&](const SYSTEMCommand &sc) {
                       std::visit(overloaded{
                                      [&](const SHUTDOWNCommand &c) {
                                          j["command"] = c.toJson();
                                      },
                                      [&](const RELOAD_CONFIGSCommand &c) {
                                          j["command"] = c.toJson();
                                      },
                                      [&](const RELOAD_LIBRARYCommand &c) {
                                          j["command"] = c.toJson();
                                      },
                                  },
                                  sc);
                   },
                   [&](const VOLUMECommand &vc) {
                       std::visit(overloaded{
                                      [&](const VOLUME_INCREASECommand &c) {
                                          j["command"] = c.toJson();
                                      },
                                      [&](const VOLUME_DECREASECommand &c) {
                                          j["command"] = c.toJson();
                                      },
                                      [&](const MUTECommand &c) {
                                          j["command"] = c.toJson();
                                      },
                                      [&](const UNMUTECommand &c) {
                                          j["command"] = c.toJson();
                                      },
                                  },
                                  vc);
                   },
               },
               cmd.payload);
    std::string processed_data = j.dump();
    zmq::message_t pub_msg(processed_data.size());
    memcpy(pub_msg.data(), processed_data.c_str(), processed_data.size());

    switch (cmd.domain) {
    case System::SYSTEM:
        m_system_pub.send(pub_msg, zmq::send_flags::none);
        break;
    case System::MEDIA:
    case System::LIBRARY:
        m_media_pub.send(pub_msg, zmq::send_flags::none);
        break;
    case System::VOLUME:
        m_volume_pub.send(pub_msg, zmq::send_flags::none);
        break;
    default:
        return;
    }

    std::cout << "[RFID Parser] Published: " << processed_data << std::endl;
}

bool RfidParser::loadConfigs(const std::string &filepath, json &config) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    file >> config;
    std::cout << config.dump() << std::endl;
    std::cout << "[RFID Parser] Configuration loaded" << std::endl;
    return true;
}

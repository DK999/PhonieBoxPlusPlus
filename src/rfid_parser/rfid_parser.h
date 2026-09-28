#ifndef RFID_PARSER_H
#define RFID_PARSER_H

#include "ipc_commands.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <zmq.hpp>

using json = nlohmann::json;
using namespace ipc;

class RfidParser {
    static constexpr const char *MEDIA_TAGS = "media.json";
    static constexpr const char *VOLUME_TAGS = "volume.json";
    static constexpr const char *LIBRARY_TAGS = "library.json";
    static constexpr const char *SYSTEM_TAGS = "system.json";

  public:
    explicit RfidParser();
    ~RfidParser() = default;
    void run();

  private:
    zmq::socket_t make_publisher(zmq::context_t &ctx, ipc::System sys);
    void processMessage(const std::string &id);
    bool getCommand(const std::string &tag, IncomingCommand &cmd);
    bool getMediaCommand(const std::string &tag, IncomingCommand &cmd);
    bool getPlayMediaCommand(const std::string &tag, IncomingCommand &cmd);
    bool getVolumeCommand(const std::string &tag, IncomingCommand &cmd);
    bool getSystemCommand(const std::string &tag, IncomingCommand &cmd);
    bool getMatchingTag(const std::string &tag, json &cfg, json &object);
    bool consumeCommand(const IncomingCommand &cmd);
    void publishProcessedData(const IncomingCommand &cmd);
    void publishErrorStatus(int code, const std::string &message);
    bool loadConfigs(const std::string &file, json &config);

    zmq::context_t m_ctx;
    zmq::socket_t m_volume_pub;
    zmq::socket_t m_system_pub;
    zmq::socket_t m_media_pub;
    zmq::socket_t m_rfidreader_sub;
    json m_volumetags;
    json m_libtags;
    json m_systags;
    json m_mediatags;
};

#endif // RFID_PARSER_H

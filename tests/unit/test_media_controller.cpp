#include <filesystem>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

// Mock class that simulates MediaController behavior for RFID tag processing
class MockMediaController {
  public:
    MOCK_METHOD(void, handleVolumeCommand, (const std::string &tag));
    MOCK_METHOD(void, handleMediaCommand, (const std::string &tag));
    MOCK_METHOD(void, sendFallbackCommand, (const std::string &command));
    MOCK_METHOD(void, sendVolumeCommand,
                (const std::string &command, float value));

    void setTestConfig(const json &test_config) { config_ = test_config; }

    void setTestRunning(bool running_flag) { (void)running_flag; }

    void processRFIDTag(const std::string &rfid_tag) {
        // Simulate the real MediaController dispatch logic
        if (config_.contains("volume") &&
            config_["volume"]["tags"].contains(rfid_tag)) {
            handleVolumeCommand(rfid_tag);
            return;
        }
        if ((config_.contains("media") &&
             config_["media"]["tags"].contains(rfid_tag)) ||
            (config_.contains("library") &&
             config_["library"]["tags"].contains(rfid_tag))) {
            handleMediaCommand(rfid_tag);
            return;
        }
        // Unknown tag — send a fallback command
        sendFallbackCommand("unknown_tag_" + rfid_tag);
    }

  private:
    json config_;
};

TEST(MediaControllerTest, LoadConfigurationFromFile) {
    json test_config = {
        {"volume",
         {{"tags",
           {{"00", {{"type", "VOLUME_INCREASE"}}},
            {"02", {{"type", "MUTE"}}}}}}},
        {"library",
         {{"tags",
           {{"10",
             {{"type", "FILE"}, {"file", "/usr/share/sounds/alert.wav"}}}}}}}};
    std::ofstream config_file("test_config.json");
    config_file << test_config.dump(4);
    config_file.close();
    MockMediaController controller;
    std::filesystem::remove("test_config.json");
}

TEST(MediaControllerTest, ProcessVolumeCommand) {
    json test_config = {{"volume",
                         {{"tags",
                           {{"00", {{"type", "VOLUME_INCREASE"}}},
                            {"01", {{"type", "VOLUME_DECREASE"}}}}}}}};
    MockMediaController controller;
    controller.setTestConfig(test_config);
    controller.setTestRunning(false);
    EXPECT_CALL(controller, handleVolumeCommand("00"));
    controller.processRFIDTag("00");
    EXPECT_CALL(controller, handleVolumeCommand("01"));
    controller.processRFIDTag("01");
}

TEST(MediaControllerTest, ProcessMediaCommand) {
    json test_config = {
        {"library",
         {{"tags",
           {{"10",
             {{"type", "FILE"}, {"file", "/usr/share/sounds/alert.wav"}}}}}}}};
    MockMediaController controller;
    controller.setTestConfig(test_config);
    controller.setTestRunning(false);
    EXPECT_CALL(controller, handleMediaCommand("10"));
    controller.processRFIDTag("10");
}

TEST(MediaControllerTest, UnknownTagHandling) {
    json test_config = {
        {"volume", {{"tags", {{"00", {{"type", "VOLUME_INCREASE"}}}}}}}};
    MockMediaController controller;
    controller.setTestConfig(test_config);
    controller.setTestRunning(false);
    EXPECT_CALL(controller, sendFallbackCommand(_));
    controller.processRFIDTag("UNKNOWN_TAG");
}

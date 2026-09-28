#include <chrono>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>
#include <zmq.hpp>

using json = nlohmann::json;
using ::testing::_;
using ::testing::Return;

// Mock classes for testing the complete chain
class MockVolumeControl {
  public:
    MOCK_METHOD(bool, setVolume, (float), ());
    MOCK_METHOD(bool, mute, (), ());
    MOCK_METHOD(bool, unmute, (), ());
    MOCK_METHOD(bool, increaseVolume, (float), ());
    MOCK_METHOD(bool, decreaseVolume, (float), ());
};

// Simulated ZeroMQ message system for testing
class TestMessageBroker {
  private:
    std::vector<std::string> rfid_messages;
    std::vector<std::string> audio_commands;
    std::vector<std::string> volume_commands;

  public:
    void addRFIDMessage(const std::string &msg) {
        rfid_messages.push_back(msg);
    }

    std::string getNextRFIDMessage() {
        if (rfid_messages.empty()) return "";
        std::string msg = rfid_messages.front();
        rfid_messages.erase(rfid_messages.begin());
        return msg;
    }

    void addAudioCommand(const std::string &cmd) {
        audio_commands.push_back(cmd);
    }

    std::string getNextAudioCommand() {
        if (audio_commands.empty()) return "";
        std::string cmd = audio_commands.front();
        audio_commands.erase(audio_commands.begin());
        return cmd;
    }

    void addVolumeCommand(const std::string &cmd) {
        volume_commands.push_back(cmd);
    }

    std::string getNextVolumeCommand() {
        if (volume_commands.empty()) return "";
        std::string cmd = volume_commands.front();
        volume_commands.erase(volume_commands.begin());
        return cmd;
    }

    bool hasRFIDMessages() const { return !rfid_messages.empty(); }
    bool hasAudioCommands() const { return !audio_commands.empty(); }
    bool hasVolumeCommands() const { return !volume_commands.empty(); }
};

// Mocked system components for integration testing
class MockMediaController {
  private:
    MockVolumeControl *volume_controller = nullptr;
    TestMessageBroker &broker;

  public:
    explicit MockMediaController(TestMessageBroker &msg_broker)
        : broker(msg_broker) {}

    void setVolumeControl(MockVolumeControl *control) {
        volume_controller = control;
    }

    // Simulate processing RFID tag
    bool processRFIDTag(const std::string &tag) {
        // This simulates the logic from media_controller.cpp
        json config;
        config["volume"]["tags"]["00"]["type"] = "VOLUME_INCREASE";
        config["volume"]["tags"]["01"]["type"] = "VOLUME_DECREASE";
        config["volume"]["tags"]["02"]["type"] = "MUTE";
        config["library"]["tags"]["10"]["type"] = "FILE";
        config["library"]["tags"]["10"]["file"] = "/usr/share/sounds/alert.wav";
        config["media"]["tags"]["20"]["type"] = "STOP_AUDIO";
        config["media"]["tags"]["21"]["type"] = "PAUSE_AUDIO";

        if (config.contains("volume") &&
            config["volume"]["tags"].contains(tag)) {
            broker.addVolumeCommand(tag);
            return true;
        }
        if ((config.contains("library") &&
             config["library"]["tags"].contains(tag)) ||
            (config.contains("media") &&
             config["media"]["tags"].contains(tag))) {
            broker.addAudioCommand(tag);
            return true;
        }
        return false;
    }
};

// Test fixture for RFID chain tests
class RFIDChainTest : public ::testing::Test {
  protected:
    void SetUp() override {
        broker = std::make_unique<TestMessageBroker>();
        media_controller = std::make_unique<MockMediaController>(*broker);
        volume_control = std::make_unique<MockVolumeControl>();
        media_controller->setVolumeControl(volume_control.get());
    }

    void TearDown() override {
        broker.reset();
        media_controller.reset();
        volume_control.reset();
    }

    std::unique_ptr<TestMessageBroker> broker;
    std::unique_ptr<MockMediaController> media_controller;
    std::unique_ptr<MockVolumeControl> volume_control;
};

TEST_F(RFIDChainTest, VolumeTagTriggersVolumeCommand) {
    // Test that a volume RFID tag triggers the volume command flow
    bool result = media_controller->processRFIDTag("00");
    EXPECT_TRUE(result);
    EXPECT_TRUE(broker->hasVolumeCommands());
    EXPECT_EQ(broker->getNextVolumeCommand(), "00");
}

TEST_F(RFIDChainTest, MediaTagTriggersMediaCommand) {
    // Test that a media RFID tag triggers the media command flow
    bool result = media_controller->processRFIDTag("10");
    EXPECT_TRUE(result);
    EXPECT_TRUE(broker->hasAudioCommands());
    EXPECT_EQ(broker->getNextAudioCommand(), "10");
}

TEST_F(RFIDChainTest, UnknownTagReturnsFalse) {
    // Test that an unknown RFID tag returns false
    bool result = media_controller->processRFIDTag("99");
    EXPECT_FALSE(result);
    EXPECT_FALSE(broker->hasVolumeCommands());
    EXPECT_FALSE(broker->hasAudioCommands());
}

TEST_F(RFIDChainTest, MultipleTagsProcessCorrectly) {
    // Test processing multiple tags in sequence
    media_controller->processRFIDTag("00");
    media_controller->processRFIDTag("10");
    media_controller->processRFIDTag("20");

    EXPECT_TRUE(broker->hasVolumeCommands());
    EXPECT_EQ(broker->getNextVolumeCommand(), "00");

    EXPECT_TRUE(broker->hasAudioCommands());
    EXPECT_EQ(broker->getNextAudioCommand(), "10");
    EXPECT_EQ(broker->getNextAudioCommand(), "20");
}

TEST_F(RFIDChainTest, MuteTagTriggersVolumeCommand) {
    // Test that MUTE tag triggers the volume command flow
    bool result = media_controller->processRFIDTag("02");
    EXPECT_TRUE(result);
    EXPECT_TRUE(broker->hasVolumeCommands());
    EXPECT_EQ(broker->getNextVolumeCommand(), "02");
}

TEST_F(RFIDChainTest, MessageBrokerEmptyByDefault) {
    // Test that the message broker starts empty
    EXPECT_FALSE(broker->hasRFIDMessages());
    EXPECT_FALSE(broker->hasAudioCommands());
    EXPECT_FALSE(broker->hasVolumeCommands());
}

TEST_F(RFIDChainTest, MessageBrokerClearsAfterConsume) {
    // Test that commands are cleared after being consumed
    media_controller->processRFIDTag("00");
    EXPECT_TRUE(broker->hasVolumeCommands());

    std::string cmd = broker->getNextVolumeCommand();
    EXPECT_EQ(cmd, "00");
    EXPECT_FALSE(broker->hasVolumeCommands());
}

// Test the message broker independently
class MessageBrokerTest : public ::testing::Test {
  protected:
    TestMessageBroker broker;
};

TEST_F(MessageBrokerTest, AddAndRetrieveRFIDMessage) {
    broker.addRFIDMessage("RFID.scanned:10");
    EXPECT_TRUE(broker.hasRFIDMessages());
    EXPECT_EQ(broker.getNextRFIDMessage(), "RFID.scanned:10");
    EXPECT_FALSE(broker.hasRFIDMessages());
}

TEST_F(MessageBrokerTest, EmptyMessageReturnedWhenBrokerEmpty) {
    EXPECT_EQ(broker.getNextRFIDMessage(), "");
    EXPECT_EQ(broker.getNextAudioCommand(), "");
    EXPECT_EQ(broker.getNextVolumeCommand(), "");
}

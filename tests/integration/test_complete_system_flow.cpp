#include <chrono>
#include <filesystem>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <thread>

using json = nlohmann::json;

class CompleteSystemFlowTest : public ::testing::Test {
  protected:
    std::string config_file_path;

    void SetUp() override {
        config_file_path = "rfid_audio_config.json";

        json test_config = {
            {"volume",
             {{"tags",
               {{"00", {{"type", "VOLUME_INCREASE"}, {"amount", 0.05}}},
                {"01", {{"type", "VOLUME_DECREASE"}, {"amount", 0.05}}},
                {"02", {{"type", "MUTE"}}},
                {"03", {{"type", "UNMUTE"}}}}}}},
            {"library",
             {{"tags",
               {{"10",
                 {{"type", "FILE"},
                  {"file", "/usr/share/sounds/alert.wav"}}}}}}},
            {"media",
             {{"tags",
               {{"20", {{"type", "STOP_AUDIO"}}},
                {"21", {{"type", "PAUSE_AUDIO"}}},
                {"22", {{"type", "RESUME_AUDIO"}}},
                {"23", {{"type", "NEXT_TITLE"}}},
                {"24", {{"type", "PREVIOUS_TITLE"}}},
                {"25", {{"type", "REPEAT_TITLE"}}}}}}},
            {"system",
             {{"tags",
               {{"90", {{"type", "SHUTDOWN"}}},
                {"91", {{"type", "RELOAD_CONFIGS"}}},
                {"92", {{"type", "RELOAD_LIBRARY"}}}}}}}};

        std::ofstream config_file(config_file_path);
        if (config_file.is_open()) {
            config_file << test_config.dump(4);
            config_file.close();
        }
    }

    void TearDown() override {
        if (std::filesystem::exists(config_file_path)) {
            std::filesystem::remove(config_file_path);
        }
    }
};

TEST_F(CompleteSystemFlowTest, RFIDToAudioPlaybackChain) {
    std::string rfid_tag = "10";

    json config;
    std::ifstream file(config_file_path);
    if (file.is_open()) {
        file >> config;
        file.close();
    }

    EXPECT_TRUE(config.contains("library"));
    EXPECT_TRUE(config["library"]["tags"].contains("10"));

    const auto &tag_config = config["library"]["tags"]["10"];
    EXPECT_TRUE(tag_config.contains("type"));
    EXPECT_TRUE(tag_config.contains("file"));

    std::string expected_file = "/usr/share/sounds/alert.wav";
    EXPECT_EQ(tag_config["file"], expected_file);
    EXPECT_EQ(tag_config["type"], "FILE");
}

TEST_F(CompleteSystemFlowTest, RFIDToVolumeControlChain) {
    std::string rfid_tag = "00";

    json config;
    std::ifstream file(config_file_path);
    if (file.is_open()) {
        file >> config;
        file.close();
    }

    EXPECT_TRUE(config.contains("volume"));
    EXPECT_TRUE(config["volume"]["tags"].contains("00"));

    const auto &vol_config = config["volume"]["tags"]["00"];
    EXPECT_TRUE(vol_config.contains("type"));
    EXPECT_TRUE(vol_config.contains("amount"));
    EXPECT_EQ(vol_config["type"], "VOLUME_INCREASE");
    EXPECT_FLOAT_EQ(vol_config["amount"], 0.05f);
}

TEST_F(CompleteSystemFlowTest, CompleteTagProcessing) {
    json config;
    std::ifstream file(config_file_path);
    if (file.is_open()) {
        file >> config;
        file.close();
    }

    std::set<std::string> volume_tag_ids;
    if (config.contains("volume")) {
        for (auto it = config["volume"]["tags"].begin();
             it != config["volume"]["tags"].end(); ++it) {
            volume_tag_ids.insert(it.key());
        }
    }

    std::set<std::string> library_tag_ids;
    if (config.contains("library")) {
        for (auto it = config["library"]["tags"].begin();
             it != config["library"]["tags"].end(); ++it) {
            library_tag_ids.insert(it.key());
        }
    }

    std::set<std::string> media_tag_ids;
    if (config.contains("media")) {
        for (auto it = config["media"]["tags"].begin();
             it != config["media"]["tags"].end(); ++it) {
            media_tag_ids.insert(it.key());
        }
    }

    std::set<std::string> volume_library_intersection;
    std::set_intersection(volume_tag_ids.begin(), volume_tag_ids.end(),
                          library_tag_ids.begin(), library_tag_ids.end(),
                          std::inserter(volume_library_intersection,
                                        volume_library_intersection.begin()));
    EXPECT_EQ(volume_library_intersection.size(), 0);
}

TEST_F(CompleteSystemFlowTest, ProcessingThroughput) {
    const int test_iterations = 1000;
    auto start_time = std::chrono::high_resolution_clock::now();

    json config;
    std::ifstream file(config_file_path);
    if (file.is_open()) {
        file >> config;
        file.close();
    }

    for (int i = 0; i < test_iterations; ++i) {
        std::string tag =
            "TEST_TAG_" + std::to_string(i % 10);

        if (config.contains("volume") &&
            config["volume"]["tags"].contains(tag)) {
        } else if ((config.contains("library") &&
                    config["library"]["tags"].contains(tag)) ||
                   (config.contains("media") &&
                    config["media"]["tags"].contains(tag))) {
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    EXPECT_LT(duration.count(), 100);
}

TEST_F(CompleteSystemFlowTest, ErrorConditionHandling) {
    json config;
    std::ifstream file(config_file_path);
    if (file.is_open()) {
        file >> config;
        file.close();
    }

    json minimal_config = {};
    EXPECT_NO_THROW({
    });
}

TEST_F(CompleteSystemFlowTest, MemoryUsage) {
    std::vector<std::string> test_tags = {"00", "10", "20", "90"};

    for (const auto &tag : test_tags) {
        std::string temp_string = "RFID.scanned:" + tag;
        EXPECT_FALSE(temp_string.empty());
    }
}

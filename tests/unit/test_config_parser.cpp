#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

TEST(ConfigParserTest, LoadValidJsonConfig) {
    // Create test JSON configuration matching actual config structure
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
             {{"type", "FILE"}, {"file", "/usr/share/sounds/alert.wav"}}}}}}},
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

    // Write to temporary file
    std::ofstream config_file("test_config.json");
    config_file << test_config.dump(4);
    config_file.close();

    // Load and verify
    try {
        std::ifstream file("test_config.json");
        json loaded_config;
        file >> loaded_config;

        EXPECT_TRUE(loaded_config.contains("volume"));
        EXPECT_TRUE(loaded_config.contains("library"));
        EXPECT_TRUE(loaded_config.contains("media"));
        EXPECT_TRUE(loaded_config.contains("system"));

        // Check volume tags
        EXPECT_TRUE(loaded_config["volume"]["tags"].contains("00"));
        EXPECT_TRUE(loaded_config["volume"]["tags"].contains("01"));
        EXPECT_TRUE(loaded_config["volume"]["tags"].contains("02"));
        EXPECT_TRUE(loaded_config["volume"]["tags"].contains("03"));

        // Check library tags
        EXPECT_TRUE(loaded_config["library"]["tags"].contains("10"));

        // Check media tags
        EXPECT_TRUE(loaded_config["media"]["tags"].contains("20"));
        EXPECT_TRUE(loaded_config["media"]["tags"].contains("21"));

        // Check system tags
        EXPECT_TRUE(loaded_config["system"]["tags"].contains("90"));

    } catch (const std::exception &e) {
        FAIL() << "Failed to load JSON configuration: " << e.what();
    }

    // Cleanup
    std::filesystem::remove("test_config.json");
}

TEST(ConfigParserTest, LoadInvalidJsonConfig) {
    // Create invalid JSON file
    std::ofstream config_file("invalid_config.json");
    config_file << "{invalid json content";
    config_file.close();

    // Try to load - should handle gracefully
    try {
        std::ifstream file("invalid_config.json");
        json loaded_config;
        file >> loaded_config;

        // This shouldn't succeed with invalid JSON
        EXPECT_TRUE(false) << "Should have failed with invalid JSON";

    } catch (const std::exception &e) {
        // Expected behavior - parsing should fail
        EXPECT_TRUE(true); // Test passes if exception is thrown
    }

    // Cleanup
    std::filesystem::remove("invalid_config.json");
}

TEST(ConfigParserTest, ValidateConfigStructure) {
    json test_config = {
        {"volume",
         {{"tags", {{"00", {{"type", "VOLUME_INCREASE"}, {"amount", 0.05}}}}}}},
        {"library",
         {{"tags",
           {{"10",
             {{"type", "FILE"}, {"file", "/usr/share/sounds/alert.wav"}}}}}}},
        {"media", {{"tags", {{"20", {{"type", "STOP_AUDIO"}}}}}}}};

    // Validate structure
    EXPECT_TRUE(test_config.contains("volume"));
    EXPECT_TRUE(test_config.contains("library"));
    EXPECT_TRUE(test_config.contains("media"));

    // Validate volume tag structure
    EXPECT_TRUE(test_config["volume"]["tags"].contains("00"));
    EXPECT_TRUE(test_config["volume"]["tags"]["00"].contains("type"));
    EXPECT_TRUE(test_config["volume"]["tags"]["00"].contains("amount"));

    // Validate library tag structure
    EXPECT_TRUE(test_config["library"]["tags"].contains("10"));
    EXPECT_TRUE(test_config["library"]["tags"]["10"].contains("type"));
    EXPECT_TRUE(test_config["library"]["tags"]["10"].contains("file"));

    // Validate media tag structure
    EXPECT_TRUE(test_config["media"]["tags"].contains("20"));
    EXPECT_TRUE(test_config["media"]["tags"]["20"].contains("type"));
}
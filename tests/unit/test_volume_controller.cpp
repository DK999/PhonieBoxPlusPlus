#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <volume_backends/volume_backend.h>
#include <volume_controller/volume_controller.h>

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

/**
 * @class MockVolumeBackend
 * @brief Mock implementation of VolumeBackend for unit testing.
 *
 * Overrides the refactored VolumeBackend API with mockable methods.
 */
class MockVolumeBackend : public VolumeBackend {
  public:
    MOCK_METHOD(void, SetVolume, (float volume), (override));
    MOCK_METHOD(void, Mute, (bool enabled), (override));
    MOCK_METHOD(float, ReadVolume, (), (const, override));
    MOCK_METHOD(bool, Initialize, (), (override));
    MOCK_METHOD(bool, IsInitialized, (), (const, override));
};

// Mock backend factory for testing
std::unique_ptr<VolumeBackend> mockBackendFactory(const std::string &device) {
    return std::make_unique<MockVolumeBackend>();
}

// Test fixture that provides a VolumeControl instance with a mock backend
class VolumeControlTest : public ::testing::Test {
  protected:
    void SetUp() override {
        mock_backend = std::make_shared<MockVolumeBackend>();
        control = std::make_unique<VolumeControl>(
            [this](const std::string &) -> std::shared_ptr<VolumeBackend> {
                return mock_backend;
            });

        // Default return values for backend methods
        ON_CALL(*GetMock(), Initialize()).WillByDefault(Return(true));
        ON_CALL(*GetMock(), IsInitialized()).WillByDefault(Return(true));

        // Initialize the backend so other tests have a working backend
        VolumeControl::VolumeSettings settings;
        settings.output_device = "default";
        settings.default_volume = 0.7f;
        control->initialize(settings);
    }

    /**
     * @brief Gets the mock backend pointer (non-owning, for EXPECT_CALL).
     */
    MockVolumeBackend *GetMock() { return mock_backend.get(); }

    std::shared_ptr<MockVolumeBackend> mock_backend;
    std::unique_ptr<VolumeControl> control;
};

TEST_F(VolumeControlTest, InitializeSuccess) {
    VolumeControl::VolumeSettings settings;
    settings.output_device = "default";
    settings.default_volume = 0.7f;

    EXPECT_CALL(*GetMock(), Initialize()).Times(1);
    EXPECT_CALL(*GetMock(), IsInitialized()).Times(1);
    EXPECT_CALL(*GetMock(), SetVolume(0.7f)).Times(1);

    EXPECT_TRUE(control->initialize(settings));
}

TEST_F(VolumeControlTest, SetVolumeWithinBounds) {
    // Expect the backend to be called with volume 0.5
    EXPECT_CALL(*GetMock(), SetVolume(0.5f)).WillOnce(Return());

    control->setVolume(0.5f);
}

TEST_F(VolumeControlTest, SetVolumeClampHigh) {
    // Expect the backend to be called with volume 1.0 (clamped)
    EXPECT_CALL(*GetMock(), SetVolume(1.0f)).WillOnce(Return());

    control->setVolume(1.5f); // Should clamp to 1.0
}

TEST_F(VolumeControlTest, SetVolumeClampLow) {
    // Expect the backend to be called with volume 0.0 (clamped)
    EXPECT_CALL(*GetMock(), SetVolume(0.0f)).WillOnce(Return());

    control->setVolume(-0.5f); // Should clamp to 0.0
}

TEST_F(VolumeControlTest, MuteAndUnmute) {
    // Expect mute to be called
    EXPECT_CALL(*GetMock(), Mute(true)).WillOnce(Return());

    control->mute();
    EXPECT_TRUE(control->isMuted());

    // Expect unmute to be called
    EXPECT_CALL(*GetMock(), Mute(false)).WillOnce(Return());

    control->unmute();
    EXPECT_FALSE(control->isMuted());
}

TEST_F(VolumeControlTest, IncreaseVolume) {
    // Set initial volume to 0.5
    control->setVolume(0.5f);

    // Expect the backend to be called with volume 0.6 (0.5 + 0.1)
    EXPECT_CALL(*GetMock(), SetVolume(0.6f)).WillOnce(Return());

    control->increaseVolume(0.1f);
}

TEST_F(VolumeControlTest, DecreaseVolume) {
    // Set initial volume to 0.5
    control->setVolume(0.5f);

    // Expect the backend to be called with volume 0.4 (0.5 - 0.1)
    EXPECT_CALL(*GetMock(), SetVolume(0.4f)).WillOnce(Return());

    control->decreaseVolume(0.1f);
}

TEST_F(VolumeControlTest, VolumeClampDuringIncrease) {
    // Set initial volume to 0.9
    control->setVolume(0.9f);

    // Expect the backend to be called with volume 1.0 (clamped)
    EXPECT_CALL(*GetMock(), SetVolume(1.0f)).WillOnce(Return());

    control->increaseVolume(0.2f); // Should clamp to 1.0
}

TEST_F(VolumeControlTest, VolumeClampDuringDecrease) {
    // Set initial volume to 0.1
    control->setVolume(0.1f);

    // Expect the backend to be called with volume 0.0 (clamped)
    EXPECT_CALL(*GetMock(), SetVolume(0.0f)).WillOnce(Return());

    control->decreaseVolume(0.2f); // Should clamp to 0.0
}
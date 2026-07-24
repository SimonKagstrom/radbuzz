#pragma once

#include "base_thread.hh"
#include "os/memory.hh"
#include "painter.hh"
#include "user_interface.hh"

#include <etl/vector.h>

class MapScreen : public UserInterface::ScreenBase
{
public:
    MapScreen(UserInterface& parent, ImageCache& image_cache, TileCache& tile_cache, uint8_t zoom);

    void SetZoom(uint8_t zoom);

private:
    enum class RangeCircleType
    {
        kFurthest,
        kRoundTrip,

        kValueCount,
    };

    void DrawRangeCircle(lv_layer_t* layer, RangeCircleType type);
    void DrawTripLines(lv_layer_t* layer);

    os::TimerHandle StartHomeHoldTimer();
    void BlitToRotationBuffer();
    void PrepareNonRotatedBlits();
    void RotateBackground(int32_t angle_deg10, uint16_t* dst);

    void Update() final;
    void HandleInput(const Input::Event& event) final;


    // Source buffer is the display diagonal squared so any rotation angle fills the screen
    static constexpr int kBgSize =
        960; // >= diagonal and divisible by 8 for cache-line-safe RGB565 buffer size
    static constexpr int kMaxNumTilesX = (kBgSize + kTileSize - 1) / kTileSize + 1;
    static constexpr int kMaxNumTilesY = (kBgSize + kTileSize - 1) / kTileSize + 1;

    etl::vector<hal::BlitOperation, kMaxNumTilesX * kMaxNumTilesY> m_blit_ops;

    ImageCache& m_image_cache;
    TileCache& m_tile_cache;

    SingleColorImage m_background {kBgSize, kBgSize, 2, 0x0000}; // Oversized for rotation
    SingleColorImage m_background_rotated {
        hal::kDisplayWidth, hal::kDisplayHeight, 2, 0x0000}; // Rotated view target

    BlankAlphaImage m_position_dot {32, 32};
    lv_obj_t* m_position_dot_obj {nullptr};

    // Related to the navigation
    uint32_t m_current_icon_hash {kInvalidIconHash};
    lv_obj_t* m_navigation_box {nullptr};
    lv_obj_t* m_navigation_description_box {nullptr};
    lv_obj_t* m_current_icon {nullptr};
    lv_obj_t* m_description_label {nullptr};
    lv_obj_t* m_distance_left_label {nullptr};

    lv_obj_t* m_soc_label {nullptr};
    // Icon for when GPS is lost (or not yet acquired)
    lv_obj_t* m_gps_lost {nullptr};
    // Icon for wifi
    lv_obj_t* m_wifi_active {nullptr};
    // Icon for BLE
    lv_obj_t* m_ble_active {nullptr};
    // Icon for parked state
    lv_obj_t* m_parked {nullptr};

    lv_obj_t* m_speedometer_box {nullptr};
    lv_obj_t* m_speed_digits_label {nullptr};
    lv_obj_t* m_power_bar {nullptr};

    Point m_current_view_center {0, 0, kDefaultZoom};
    Point m_current_range_circle_center {0, 0, kDefaultZoom};
    int32_t m_rotation_pivot_x {hal::kDisplayWidth / 2};
    int32_t m_rotation_pivot_y {hal::kDisplayHeight / 2};
    uint16_t m_rotation {0};
    os::TimerHandle m_touch_timer;
    os::TimerHandle m_home_hold_timer;

    int32_t m_last_touch_x {0};
    int32_t m_last_touch_y {0};

    uint8_t m_zoom;
    bool m_rotation_enabled {false};
};

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


    class BackgroundRotationThread : public os::BaseThread
    {
    public:
        explicit BackgroundRotationThread(MapScreen& parent)
            : m_parent(parent)
        {
        }

        uint16_t* Flip()
        {
            return nullptr;
        }

    private:
        struct Buffer
        {
            uint16_t data[hal::kDisplayWidth * hal::kDisplayHeight];
        };

        void OnStartup() final;
        std::optional<milliseconds> OnActivation() final;

        void RotateBackground(int32_t angle_deg10);

        void BlitTilesToBackground();

        MapScreen& m_parent;

        os::mem_unique_ptr<Buffer> m_rotation_buffer_0 {os::AllocSlowMem<Buffer>(64)};
        os::mem_unique_ptr<Buffer> m_rotation_buffer_1 {os::AllocSlowMem<Buffer>(64)};
        SingleColorImage m_background {kBgSize, kBgSize, 2, 0x0000}; // Oversized for rotation

        static constexpr int kBgSize = 936; // >= diagonal and divisible by 8 for cache-line-safe RGB565 buffer size
    };

    void DrawRangeCircle(lv_layer_t* layer, RangeCircleType type);
    os::TimerHandle StartHomeHoldTimer();

    void Update() final;
    void HandleInput(const Input::Event& event) final;

    ImageCache& m_image_cache;
    TileCache& m_tile_cache;
    uint8_t m_zoom;

    // Source buffer is the display diagonal squared so any rotation angle fills the screen
    static constexpr int kBgSize = 960; // >= diagonal and divisible by 8 for cache-line-safe RGB565 buffer size
    SingleColorImage m_background {kBgSize, kBgSize, 2, 0x0000}; // Oversized for rotation
    SingleColorImage m_background_rotated {
        hal::kDisplayWidth, hal::kDisplayHeight, 2, 0x0000}; // Rotated view target

    void RotateBackground(int32_t angle_deg10, uint16_t *dst);

    // Related to the navigation
    uint32_t m_current_icon_hash {kInvalidIconHash};
    lv_obj_t* m_navigation_box {nullptr};
    lv_obj_t* m_navigation_description_box {nullptr};
    lv_obj_t* m_current_icon {nullptr};
    lv_obj_t* m_description_label {nullptr};
    lv_obj_t* m_distance_left_label {nullptr};

    lv_obj_t* m_soc_label {nullptr};

    BlankAlphaImage m_position_dot {32, 32};
    lv_obj_t* m_position_dot_obj {nullptr};

    lv_obj_t* m_speedometer_box {nullptr};
    lv_obj_t* m_speed_digits_label {nullptr};
    uint8_t m_last_battery_soc {255};

    lv_obj_t* m_power_bar {nullptr};

    Point m_current_view_center {0, 0, m_zoom};
    uint16_t m_rotation {0};
    os::TimerHandle m_touch_timer;
    os::TimerHandle m_home_hold_timer;

    int32_t m_last_touch_x {0};
    int32_t m_last_touch_y {0};


    static constexpr int kNumTilesX = (kBgSize + kTileSize - 1) / kTileSize + 1;
    static constexpr int kNumTilesY = (kBgSize + kTileSize - 1) / kTileSize + 1;

    hal::BlitOperation m_copy_blit_op;
    etl::vector<hal::BlitOperation, kNumTilesX * kNumTilesY> m_blit_ops;
};

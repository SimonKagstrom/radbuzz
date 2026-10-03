#pragma once

#include "base_thread.hh"
#include "navigation_widget.hh"
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
    void DrawRangeCircle(lv_layer_t* layer, uint32_t estimated_range_km, uint8_t width);
    void DrawTripLines(lv_layer_t* layer);

    os::TimerHandle StartHomeHoldTimer();
    void BlitToRotationBuffer();
    void PrepareNonRotatedBlits();
    void RotateBackground(int32_t angle_deg10, uint16_t* dst);

    // Move the map center and navigation boxes to the right of the side pane
    void LayoutForSidePane(bool shown);

    void Update() final;
    void HandleInput(const Input::Event& event) final;
    void SetHelp(bool on) final;


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

    std::unique_ptr<NavigationWidget> m_navigation;
    lv_obj_t* m_home_label {nullptr};

    // The horizontal center of the visible map (moved right by the side pane)
    int32_t m_center_x {hal::kDisplayWidth / 2};
    bool m_laid_out_for_side_pane {false};

    Point m_current_view_center {0, 0, kDefaultZoom};
    Point m_current_range_circle_center {0, 0, kDefaultZoom};
    int32_t m_rotation_pivot_x {hal::kDisplayWidth / 2};
    int32_t m_rotation_pivot_y {hal::kDisplayHeight / 2};
    uint16_t m_rotation {0};
    os::TimerHandle m_touch_timer;

    uint16_t m_home_hold_x {0};
    uint16_t m_home_hold_y {0};
    os::TimerHandle m_home_hold_timer;


    bool m_touch_was_pressed {false};

    uint8_t m_zoom;
    bool m_rotation_enabled {false};
};

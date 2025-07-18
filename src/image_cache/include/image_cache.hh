#pragma once

#include <etl/unordered_map.h>
#include <etl/vector.h>
#include <lvgl.h>
#include <span>

class CachedImage
{
public:
    CachedImage(std::span<const uint8_t> data, uint16_t width, uint16_t height)
    {
        constexpr auto kBlackWhitePalette =
            std::array<uint8_t, 8> {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff};
        auto data_size = (width * height) / 8 + kBlackWhitePalette.size();

        m_bits.reserve(data_size);
        std::ranges::copy(kBlackWhitePalette, std::back_inserter(m_bits));
        std::ranges::copy(data, std::back_inserter(m_bits));

        m_lv_image_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        m_lv_image_dsc.header.w = width;
        m_lv_image_dsc.header.h = height;
        m_lv_image_dsc.header.flags = 0;
        m_lv_image_dsc.header.stride = width / 8;
        m_lv_image_dsc.header.cf = LV_COLOR_FORMAT_I1;

        m_lv_image_dsc.data_size = data_size;
        m_lv_image_dsc.data = reinterpret_cast<const uint8_t*>(m_bits.data());
    }

    const lv_image_dsc_t& GetDsc() const
    {
        return m_lv_image_dsc;
    }

private:
    std::vector<uint8_t> m_bits;

    lv_image_dsc_t m_lv_image_dsc;
};


constexpr auto kMaxCachedImages = 32;

class ImageCache
{
public:
    ImageCache() = default;
    ImageCache(const ImageCache&) = delete;
    ImageCache operator=(const ImageCache&) = delete;

    // Context: Some thread (BLE)
    void Insert(uint32_t key, uint8_t width, uint8_t height, std::span<const uint8_t> data);

    // Context: Some other thread (UI)
    const CachedImage* Lookup(uint32_t key) const;

private:
    etl::vector<std::unique_ptr<CachedImage>, kMaxCachedImages> m_images;
    etl::unordered_map<uint32_t, uint8_t, kMaxCachedImages> m_cache;
};

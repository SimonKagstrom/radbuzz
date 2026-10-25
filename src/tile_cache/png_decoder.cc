#include "png_decoder.hh"

#include <PNGdec.h>
#include <memory>

namespace
{

struct DecodeHelper
{
    DecodeHelper(PNG& png, uint16_t* dst)
        : png(png)
        , dst(dst)
        , offset(0)
    {
    }

    DecodeHelper() = delete;

    PNG& png;
    uint16_t* dst;
    size_t offset;
};

int
PngDraw(PNGDRAW* pDraw)
{
    auto helper = static_cast<DecodeHelper*>(pDraw->pUser);

    helper->png.getLineAsRGB565(
        pDraw, helper->dst + helper->offset, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
    helper->offset += pDraw->iWidth;

    return 1;
}

} // namespace

bool
DecodePng(std::span<const std::byte> png_data, StandaloneImage& out)
{
    auto png = std::make_unique<PNG>();

    auto rc = png->openFLASH((uint8_t*)png_data.data(), png_data.size(), PngDraw);
    if (rc != PNG_SUCCESS)
    {
        return false;
    }

    if (out.Width() == png->getWidth() && out.Height() == png->getHeight())
    {
        DecodeHelper priv(*png, out.WritableData16());
        rc = png->decode((void*)&priv, 0);
    }
    else
    {
        rc = PNG_MEM_ERROR;
    }

    png->close();

    return rc == PNG_SUCCESS;
}

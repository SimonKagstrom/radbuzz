#pragma once

#include "uuid_helper.hh"

#include <functional>
#include <memory>
#include <span>

namespace hal
{

class IBleClient
{
public:
    class ICharacteristic
    {
    public:
        virtual ~ICharacteristic() = default;

        virtual Uuid128 GetUuid() const = 0;

        virtual void Write(std::span<const uint8_t> data) = 0;

        virtual void Read(std::function<void(std::span<const uint8_t>)> &cb) = 0;

        virtual void Subscribe(std::function<void(std::span<const uint8_t>)> &cb) = 0;
    };

    class IPeer
    {
    public:
        virtual ~IPeer() = default;

        virtual std::vector<std::unique_ptr<ICharacteristic>> GetCharacteristics() const = 0;
    };

    virtual ~IBleClient() = default;

    virtual void ScanForService(Uuid128Span service_uuid,
                                std::function<void(std::unique_ptr<IPeer>)> &cb) = 0;
};

} // namespace hal

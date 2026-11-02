#include <windows.h>

#include "RhiError.h"
#include <caligo/render/RhiDevice.h>
#include <d3d11.h>
#include <iterator>
#include <string>
#include <utility>
#include <wrl/client.h>

namespace caligo::render {

using Microsoft::WRL::ComPtr;

struct RhiDevice::Impl {
  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
  bool warp = false;
  std::string driver;
};

namespace {

Result<void> CreateForDriver(D3D_DRIVER_TYPE driverType, RhiDevice::Impl& impl) {
  constexpr D3D_FEATURE_LEVEL kLevels[] = {D3D_FEATURE_LEVEL_11_0};
  constexpr UINT kFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

  D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
  const HRESULT result = D3D11CreateDevice(
      nullptr, driverType, nullptr, kFlags, kLevels, static_cast<UINT>(std::size(kLevels)),
      D3D11_SDK_VERSION, impl.device.GetAddressOf(), &featureLevel, impl.context.GetAddressOf());
  if (FAILED(result)) {
    impl.device.Reset();
    impl.context.Reset();
    return Error{Failure("D3D11CreateDevice", result), 1};
  }
  if (featureLevel < D3D_FEATURE_LEVEL_11_0) {
    impl.device.Reset();
    impl.context.Reset();
    return Error{"Direct3D feature level 11_0 is unavailable", 2};
  }
  return {};
}

}

Result<std::unique_ptr<RhiDevice>> RhiDevice::Create(bool forceWarp) {
  std::unique_ptr<RhiDevice> device(new RhiDevice());

  if (forceWarp) {
    const Result<void> created = CreateForDriver(D3D_DRIVER_TYPE_WARP, *device->m_impl);
    if (!created) {
      return created.GetError();
    }
    device->m_impl->warp = true;
    device->m_impl->driver = "WARP";
    return std::move(device);
  }

  if (CreateForDriver(D3D_DRIVER_TYPE_HARDWARE, *device->m_impl)) {
    device->m_impl->driver = "Hardware";
    return std::move(device);
  }

  const Result<void> fallback = CreateForDriver(D3D_DRIVER_TYPE_WARP, *device->m_impl);
  if (!fallback) {
    return fallback.GetError();
  }
  device->m_impl->warp = true;
  device->m_impl->driver = "WARP";
  return std::move(device);
}

RhiDevice::RhiDevice() : m_impl(std::make_unique<Impl>()) {}

RhiDevice::~RhiDevice() {
  if (m_impl->context != nullptr) {
    m_impl->context->ClearState();
    m_impl->context->Flush();
  }
}

void* RhiDevice::DeviceHandle() const noexcept {
  return m_impl->device.Get();
}

void* RhiDevice::ContextHandle() const noexcept {
  return m_impl->context.Get();
}

bool RhiDevice::IsWarp() const noexcept {
  return m_impl->warp;
}

const char* RhiDevice::DriverName() const noexcept {
  return m_impl->driver.c_str();
}

}

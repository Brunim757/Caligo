#include <windows.h>

#include "RhiError.h"
#include <caligo/render/RhiDevice.h>
#include <caligo/render/RhiSampler.h>
#include <d3d11.h>
#include <utility>
#include <wrl/client.h>

namespace caligo::render {

using Microsoft::WRL::ComPtr;

struct RhiSampler::Impl {
  ComPtr<ID3D11SamplerState> sampler;
};

namespace {

D3D11_FILTER ToFilter(RhiFilter filter, bool comparison) {
  if (comparison) {
    return filter == RhiFilter::Point ? D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT
                                      : D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
  }
  return filter == RhiFilter::Point ? D3D11_FILTER_MIN_MAG_MIP_POINT
                                    : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
}

D3D11_TEXTURE_ADDRESS_MODE ToAddressMode(RhiAddressMode address) {
  return address == RhiAddressMode::Border ? D3D11_TEXTURE_ADDRESS_BORDER
                                           : D3D11_TEXTURE_ADDRESS_CLAMP;
}

}

Result<std::unique_ptr<RhiSampler>> RhiSampler::Create(RhiDevice& device,
                                                       const RhiSamplerDesc& desc) {
  ID3D11Device* nativeDevice = static_cast<ID3D11Device*>(device.DeviceHandle());
  if (nativeDevice == nullptr) {
    return Error{"sampler needs a live device", 1};
  }

  D3D11_SAMPLER_DESC description{};
  description.Filter = ToFilter(desc.filter, desc.comparison);
  description.AddressU = ToAddressMode(desc.address);
  description.AddressV = ToAddressMode(desc.address);
  description.AddressW = ToAddressMode(desc.address);
  description.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
  const float border = desc.borderWhite ? 1.0f : 0.0f;
  description.BorderColor[0] = border;
  description.BorderColor[1] = border;
  description.BorderColor[2] = border;
  description.BorderColor[3] = border;
  description.MinLOD = 0.0f;
  description.MaxLOD = D3D11_FLOAT32_MAX;

  std::unique_ptr<RhiSampler> sampler(new RhiSampler());
  const HRESULT result =
      nativeDevice->CreateSamplerState(&description, sampler->m_impl->sampler.GetAddressOf());
  if (FAILED(result)) {
    return Error{Failure("ID3D11Device::CreateSamplerState", result), 2};
  }
  return std::move(sampler);
}

RhiSampler::RhiSampler() : m_impl(std::make_unique<Impl>()) {}

RhiSampler::~RhiSampler() = default;

void* RhiSampler::Handle() const noexcept {
  return m_impl->sampler.Get();
}

}
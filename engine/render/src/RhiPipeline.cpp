#include <windows.h>

#include "RhiError.h"
#include <caligo/render/RhiDevice.h>
#include <caligo/render/RhiPipeline.h>
#include <d3d11.h>
#include <utility>
#include <wrl/client.h>

namespace caligo::render {

using Microsoft::WRL::ComPtr;

struct RhiPipeline::Impl {
  ComPtr<ID3D11RasterizerState> rasterizer;
  ComPtr<ID3D11DepthStencilState> depthStencil;
};

Result<std::unique_ptr<RhiPipeline>> RhiPipeline::Create(RhiDevice& device,
                                                         const RhiPipelineDesc& desc) {
  ID3D11Device* nativeDevice = static_cast<ID3D11Device*>(device.DeviceHandle());
  if (nativeDevice == nullptr) {
    return Error{"pipeline needs a live device", 1};
  }

  std::unique_ptr<RhiPipeline> pipeline(new RhiPipeline());

  D3D11_RASTERIZER_DESC rasterizer{};
  rasterizer.FillMode = desc.wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
  rasterizer.CullMode = desc.cullBack ? D3D11_CULL_BACK : D3D11_CULL_NONE;
  rasterizer.FrontCounterClockwise = TRUE;
  rasterizer.DepthClipEnable = TRUE;
  HRESULT result =
      nativeDevice->CreateRasterizerState(&rasterizer, pipeline->m_impl->rasterizer.GetAddressOf());
  if (FAILED(result)) {
    return Error{Failure("ID3D11Device::CreateRasterizerState", result), 2};
  }

  D3D11_DEPTH_STENCIL_DESC depth{};
  depth.DepthEnable = desc.depthTest ? TRUE : FALSE;
  depth.DepthWriteMask = desc.depthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
  depth.DepthFunc = D3D11_COMPARISON_LESS;
  depth.StencilEnable = FALSE;
  result =
      nativeDevice->CreateDepthStencilState(&depth, pipeline->m_impl->depthStencil.GetAddressOf());
  if (FAILED(result)) {
    return Error{Failure("ID3D11Device::CreateDepthStencilState", result), 3};
  }
  return std::move(pipeline);
}

RhiPipeline::RhiPipeline() : m_impl(std::make_unique<Impl>()) {}

RhiPipeline::~RhiPipeline() = default;

void* RhiPipeline::RasterizerState() const noexcept {
  return m_impl->rasterizer.Get();
}

void* RhiPipeline::DepthStencilState() const noexcept {
  return m_impl->depthStencil.Get();
}

}
#include <windows.h>

#include "RhiError.h"
#include <caligo/render/RhiDevice.h>
#include <caligo/render/RhiTexture.h>
#include <d3d11.h>
#include <utility>
#include <wrl/client.h>

namespace caligo::render {

using Microsoft::WRL::ComPtr;

struct RhiTexture::Impl {
  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
  ComPtr<ID3D11Texture2D> texture;
  ComPtr<ID3D11RenderTargetView> renderTarget;
  ComPtr<ID3D11DepthStencilView> depthStencil;
  ComPtr<ID3D11ShaderResourceView> shaderResource;
  uint32_t width = 0;
  uint32_t height = 0;
  RhiFormat format = RhiFormat::Rgba8Unorm;
};

namespace {

DXGI_FORMAT ToDxgi(RhiFormat format) {
  switch (format) {
  case RhiFormat::Rgba8Unorm:
    return DXGI_FORMAT_R8G8B8A8_UNORM;
  case RhiFormat::Depth32Float:
    return DXGI_FORMAT_D32_FLOAT;
  }
  return DXGI_FORMAT_UNKNOWN;
}

Result<void> CreateViews(RhiTexture::Impl& impl) {
  if (impl.format == RhiFormat::Depth32Float) {
    const HRESULT result = impl.device->CreateDepthStencilView(impl.texture.Get(), nullptr,
                                                               impl.depthStencil.GetAddressOf());
    if (FAILED(result)) {
      return Error{Failure("ID3D11Device::CreateDepthStencilView", result), 3};
    }
    return {};
  }

  HRESULT result = impl.device->CreateRenderTargetView(impl.texture.Get(), nullptr,
                                                       impl.renderTarget.GetAddressOf());
  if (FAILED(result)) {
    return Error{Failure("ID3D11Device::CreateRenderTargetView", result), 4};
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC view{};
  view.Format = ToDxgi(impl.format);
  view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  view.Texture2D.MipLevels = 1;
  result = impl.device->CreateShaderResourceView(impl.texture.Get(), &view,
                                                 impl.shaderResource.GetAddressOf());
  if (FAILED(result)) {
    return Error{Failure("ID3D11Device::CreateShaderResourceView", result), 5};
  }
  return {};
}

}

Result<std::unique_ptr<RhiTexture>> RhiTexture::Create(RhiDevice& device,
                                                       const RhiTextureDesc& desc) {
  if (desc.width == 0 || desc.height == 0) {
    return Error{"texture needs a positive size", 1};
  }
  if (desc.renderTarget && desc.depthStencil) {
    return Error{"texture cannot be both a render target and a depth buffer", 2};
  }
  ID3D11Device* nativeDevice = static_cast<ID3D11Device*>(device.DeviceHandle());
  ID3D11DeviceContext* nativeContext = static_cast<ID3D11DeviceContext*>(device.ContextHandle());
  if (nativeDevice == nullptr || nativeContext == nullptr) {
    return Error{"texture needs a live device", 3};
  }

  std::unique_ptr<RhiTexture> texture(new RhiTexture());
  texture->m_impl->device = nativeDevice;
  texture->m_impl->context = nativeContext;
  texture->m_impl->width = desc.width;
  texture->m_impl->height = desc.height;
  texture->m_impl->format = desc.format;

  D3D11_TEXTURE2D_DESC description{};
  description.Width = desc.width;
  description.Height = desc.height;
  description.MipLevels = 1;
  description.ArraySize = 1;
  description.Format = ToDxgi(desc.format);
  description.SampleDesc.Count = 1;
  description.Usage = D3D11_USAGE_DEFAULT;
  description.BindFlags = desc.depthStencil
                              ? D3D11_BIND_DEPTH_STENCIL
                              : (D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);

  const HRESULT result =
      nativeDevice->CreateTexture2D(&description, nullptr, texture->m_impl->texture.GetAddressOf());
  if (FAILED(result)) {
    return Error{Failure("ID3D11Device::CreateTexture2D", result), 4};
  }

  const Result<void> views = CreateViews(*texture->m_impl);
  if (!views) {
    return views.GetError();
  }
  return std::move(texture);
}

RhiTexture::RhiTexture() : m_impl(std::make_unique<Impl>()) {}

RhiTexture::~RhiTexture() = default;

uint32_t RhiTexture::Width() const noexcept {
  return m_impl->width;
}

uint32_t RhiTexture::Height() const noexcept {
  return m_impl->height;
}

RhiFormat RhiTexture::Format() const noexcept {
  return m_impl->format;
}

void* RhiTexture::RenderTargetView() const noexcept {
  return m_impl->renderTarget.Get();
}

void* RhiTexture::DepthStencilView() const noexcept {
  return m_impl->depthStencil.Get();
}

void* RhiTexture::ShaderResourceView() const noexcept {
  return m_impl->shaderResource.Get();
}

Result<std::vector<uint8_t>> RhiTexture::ReadPixels() const {
  if (m_impl->format != RhiFormat::Rgba8Unorm) {
    return Error{"readback supports 8-bit color textures only", 1};
  }

  D3D11_TEXTURE2D_DESC staging{};
  m_impl->texture->GetDesc(&staging);
  staging.Usage = D3D11_USAGE_STAGING;
  staging.BindFlags = 0;
  staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  staging.MiscFlags = 0;

  ComPtr<ID3D11Texture2D> copy;
  HRESULT result = m_impl->device->CreateTexture2D(&staging, nullptr, copy.GetAddressOf());
  if (FAILED(result)) {
    return Error{Failure("ID3D11Device::CreateTexture2D", result), 2};
  }

  m_impl->context->CopyResource(copy.Get(), m_impl->texture.Get());

  D3D11_MAPPED_SUBRESOURCE mapped{};
  result = m_impl->context->Map(copy.Get(), 0, D3D11_MAP_READ, 0, &mapped);
  if (FAILED(result)) {
    return Error{Failure("ID3D11DeviceContext::Map", result), 3};
  }

  std::vector<uint8_t> pixels(static_cast<size_t>(m_impl->width) * m_impl->height * 4);
  const auto* source = static_cast<const uint8_t*>(mapped.pData);
  for (uint32_t row = 0; row < m_impl->height; ++row) {
    const size_t offset = static_cast<size_t>(row) * m_impl->width * 4;
    const uint8_t* line = source + static_cast<size_t>(row) * mapped.RowPitch;
    for (size_t index = 0; index < static_cast<size_t>(m_impl->width) * 4; ++index) {
      pixels[offset + index] = line[index];
    }
  }

  m_impl->context->Unmap(copy.Get(), 0);
  return pixels;
}

}

#include <windows.h>

#include "RhiError.h"
#include <caligo/render/RhiDevice.h>
#include <caligo/render/RhiInputLayout.h>
#include <caligo/render/RhiShader.h>
#include <d3d11.h>
#include <utility>
#include <vector>
#include <wrl/client.h>

namespace caligo::render {

using Microsoft::WRL::ComPtr;

struct RhiInputLayout::Impl {
  ComPtr<ID3D11InputLayout> layout;
};

namespace {

DXGI_FORMAT ToDxgi(RhiVertexFormat format) {
  switch (format) {
  case RhiVertexFormat::Float32x2:
    return DXGI_FORMAT_R32G32_FLOAT;
  case RhiVertexFormat::Float32x3:
    return DXGI_FORMAT_R32G32B32_FLOAT;
  case RhiVertexFormat::Float32x4:
    return DXGI_FORMAT_R32G32B32A32_FLOAT;
  }
  return DXGI_FORMAT_UNKNOWN;
}

}

Result<std::unique_ptr<RhiInputLayout>>
RhiInputLayout::Create(RhiDevice& device, RhiShader& shader,
                       std::span<const RhiVertexElement> elements) {
  if (elements.empty()) {
    return Error{"input layout needs at least one element", 1};
  }
  ID3D11Device* nativeDevice = static_cast<ID3D11Device*>(device.DeviceHandle());
  if (nativeDevice == nullptr) {
    return Error{"input layout needs a live device", 2};
  }

  std::vector<D3D11_INPUT_ELEMENT_DESC> description;
  description.reserve(elements.size());
  for (const RhiVertexElement& element : elements) {
    D3D11_INPUT_ELEMENT_DESC entry{};
    entry.SemanticName = element.semantic;
    entry.SemanticIndex = element.semanticIndex;
    entry.Format = ToDxgi(element.format);
    entry.InputSlot = 0;
    entry.AlignedByteOffset = element.offset;
    entry.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    entry.InstanceDataStepRate = 0;
    description.push_back(entry);
  }

  std::unique_ptr<RhiInputLayout> layout(new RhiInputLayout());
  const HRESULT result = nativeDevice->CreateInputLayout(
      description.data(), static_cast<UINT>(description.size()), shader.VertexBytecode(),
      shader.VertexBytecodeSize(), layout->m_impl->layout.GetAddressOf());
  if (FAILED(result)) {
    return Error{Failure("ID3D11Device::CreateInputLayout", result), 3};
  }
  return std::move(layout);
}

RhiInputLayout::RhiInputLayout() : m_impl(std::make_unique<Impl>()) {}

RhiInputLayout::~RhiInputLayout() = default;

void* RhiInputLayout::Handle() const noexcept {
  return m_impl->layout.Get();
}

}
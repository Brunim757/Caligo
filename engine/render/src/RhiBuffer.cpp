#include <windows.h>

#include "RhiError.h"
#include <caligo/render/RhiBuffer.h>
#include <caligo/render/RhiDevice.h>
#include <d3d11.h>
#include <utility>
#include <wrl/client.h>

namespace caligo::render {

using Microsoft::WRL::ComPtr;

struct RhiBuffer::Impl {
  ComPtr<ID3D11Buffer> buffer;
  ComPtr<ID3D11DeviceContext> context;
  RhiBufferKind kind = RhiBufferKind::Vertex;
  uint32_t sizeBytes = 0;
};

namespace {

Result<void> CreateBuffer(ID3D11Device* device, ID3D11DeviceContext* context, RhiBufferKind kind,
                          const void* data, uint32_t sizeBytes, RhiBuffer::Impl& impl) {
  D3D11_BUFFER_DESC description{};
  description.ByteWidth = sizeBytes;

  switch (kind) {
  case RhiBufferKind::Vertex:
    description.Usage = D3D11_USAGE_IMMUTABLE;
    description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    break;
  case RhiBufferKind::Index:
    description.Usage = D3D11_USAGE_IMMUTABLE;
    description.BindFlags = D3D11_BIND_INDEX_BUFFER;
    break;
  case RhiBufferKind::Constant:
    description.Usage = D3D11_USAGE_DYNAMIC;
    description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    break;
  }

  D3D11_SUBRESOURCE_DATA initial{};
  initial.pSysMem = data;

  const HRESULT result = device->CreateBuffer(description, data != nullptr ? &initial : nullptr,
                                              impl.buffer.GetAddressOf());
  if (FAILED(result)) {
    return Error{Failure("ID3D11Device::CreateBuffer", result), 1};
  }
  impl.context = context;
  impl.kind = kind;
  impl.sizeBytes = sizeBytes;
  return {};
}

}

Result<std::unique_ptr<RhiBuffer>> RhiBuffer::Create(RhiDevice& device, RhiBufferKind kind,
                                                     const void* data, uint32_t sizeBytes) {
  if (sizeBytes == 0) {
    return Error{"buffer needs a positive size", 1};
  }
  if (kind != RhiBufferKind::Constant && data == nullptr) {
    return Error{"static buffers need initial data", 2};
  }
  if (kind == RhiBufferKind::Constant && sizeBytes % 16 != 0) {
    return Error{"constant buffers need a size multiple of 16 bytes", 3};
  }
  ID3D11Device* nativeDevice = static_cast<ID3D11Device*>(device.DeviceHandle());
  ID3D11DeviceContext* nativeContext = static_cast<ID3D11DeviceContext*>(device.ContextHandle());
  if (nativeDevice == nullptr || nativeContext == nullptr) {
    return Error{"buffer needs a live device", 4};
  }

  std::unique_ptr<RhiBuffer> buffer(new RhiBuffer());
  const Result<void> created =
      CreateBuffer(nativeDevice, nativeContext, kind, data, sizeBytes, *buffer->m_impl);
  if (!created) {
    return created.GetError();
  }
  return std::move(buffer);
}

RhiBuffer::RhiBuffer() : m_impl(std::make_unique<Impl>()) {}

RhiBuffer::~RhiBuffer() = default;

Result<void> RhiBuffer::Update(RhiDevice& device, const void* data, uint32_t sizeBytes) {
  if (m_impl->kind != RhiBufferKind::Constant) {
    return Error{"only constant buffers can be updated", 1};
  }
  if (data == nullptr || sizeBytes == 0 || sizeBytes > m_impl->sizeBytes) {
    return Error{"update data does not fit the constant buffer", 2};
  }
  ID3D11DeviceContext* nativeContext = static_cast<ID3D11DeviceContext*>(device.ContextHandle());
  if (nativeContext == nullptr) {
    return Error{"update needs a live device", 3};
  }

  D3D11_MAPPED_SUBRESOURCE mapped{};
  HRESULT result = nativeContext->Map(m_impl->buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
  if (FAILED(result)) {
    return Error{Failure("ID3D11DeviceContext::Map", result), 4};
  }

  const auto* source = static_cast<const uint8_t*>(data);
  auto* destination = static_cast<uint8_t*>(mapped.pData);
  for (uint32_t index = 0; index < sizeBytes; ++index) {
    destination[index] = source[index];
  }
  nativeContext->Unmap(m_impl->buffer.Get(), 0);
  return {};
}

void* RhiBuffer::Handle() const noexcept {
  return m_impl->buffer.Get();
}

uint32_t RhiBuffer::SizeBytes() const noexcept {
  return m_impl->sizeBytes;
}

}
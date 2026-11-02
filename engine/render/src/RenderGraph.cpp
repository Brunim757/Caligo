#include <windows.h>

#include <algorithm>
#include <caligo/render/RenderGraph.h>
#include <caligo/render/RhiDevice.h>
#include <caligo/render/RhiTexture.h>
#include <cstdint>
#include <d3d11.h>
#include <utility>
#include <vector>

namespace caligo::render {

namespace {

constexpr uint32_t kUnboundSlots = 8;

bool SameDesc(const RenderGraphTextureDesc& left, const RenderGraphTextureDesc& right) {
  return left.width == right.width && left.height == right.height && left.format == right.format &&
         left.depthStencil == right.depthStencil;
}

RhiTextureDesc ToTextureDesc(const RenderGraphTextureDesc& desc) {
  RhiTextureDesc texture;
  texture.width = desc.width;
  texture.height = desc.height;
  texture.format = desc.format;
  texture.depthStencil = desc.depthStencil;
  texture.renderTarget = !desc.depthStencil;
  return texture;
}

}

RenderGraphContext::RenderGraphContext(RhiDevice& device,
                                       const RhiTexture* const* textures) noexcept
    : m_device(&device), m_textures(textures) {}

RhiDevice& RenderGraphContext::Device() const noexcept {
  return *m_device;
}

const RhiTexture& RenderGraphContext::Texture(uint32_t resource) const {
  return *m_textures[resource];
}

struct RenderGraph::Impl {
  struct Resource {
    RenderGraphTextureDesc desc{};
    const RhiTexture* imported = nullptr;
    uint32_t firstUse = UINT32_MAX;
    uint32_t lastUse = 0;
  };

  struct Pass {
    PassCallback callback;
    std::vector<ResourceId> reads;
    std::vector<ResourceId> writes;
  };

  struct PoolEntry {
    std::unique_ptr<RhiTexture> texture;
    RenderGraphTextureDesc desc{};
    uint32_t lastUse = 0;
  };

  std::vector<Resource> resources;
  std::vector<Pass> passes;
  std::vector<PoolEntry> pool;
  std::vector<const RhiTexture*> resolved;
};

RenderGraph::RenderGraph() : m_impl(std::make_unique<Impl>()) {}

RenderGraph::~RenderGraph() = default;

void RenderGraph::Reset() {
  m_impl->resources.clear();
  m_impl->passes.clear();
  m_impl->resolved.clear();
}

RenderGraph::ResourceId RenderGraph::ImportTexture(const RhiTexture& texture) {
  Impl::Resource resource;
  resource.imported = &texture;
  m_impl->resources.push_back(resource);
  return static_cast<ResourceId>(m_impl->resources.size() - 1);
}

RenderGraph::ResourceId RenderGraph::CreateTexture(const RenderGraphTextureDesc& desc) {
  Impl::Resource resource;
  resource.desc = desc;
  m_impl->resources.push_back(resource);
  return static_cast<ResourceId>(m_impl->resources.size() - 1);
}

RenderGraph::PassId RenderGraph::AddPass(PassCallback callback) {
  Impl::Pass pass;
  pass.callback = std::move(callback);
  m_impl->passes.push_back(std::move(pass));
  return static_cast<PassId>(m_impl->passes.size() - 1);
}

void RenderGraph::Read(PassId pass, ResourceId resource) {
  if (pass >= m_impl->passes.size()) {
    return;
  }
  m_impl->passes[pass].reads.push_back(resource);
}

void RenderGraph::Write(PassId pass, ResourceId resource) {
  if (pass >= m_impl->passes.size()) {
    return;
  }
  m_impl->passes[pass].writes.push_back(resource);
}

Result<void> RenderGraph::Execute(RhiDevice& device) {
  if (m_impl->passes.empty()) {
    return {};
  }

  auto& resources = m_impl->resources;
  auto& passes = m_impl->passes;
  const uint32_t passCount = static_cast<uint32_t>(passes.size());

  for (Impl::Resource& resource : resources) {
    resource.firstUse = UINT32_MAX;
    resource.lastUse = 0;
  }

  for (uint32_t index = 0; index < passCount; ++index) {
    const Impl::Pass& pass = passes[index];
    for (ResourceId id : pass.reads) {
      if (id >= resources.size()) {
        return Error{"render graph read an unknown resource", 1};
      }
      resources[id].firstUse = std::min(resources[id].firstUse, index);
      resources[id].lastUse = std::max(resources[id].lastUse, index);
    }
    for (ResourceId id : pass.writes) {
      if (id >= resources.size()) {
        return Error{"render graph wrote an unknown resource", 2};
      }
      resources[id].firstUse = std::min(resources[id].firstUse, index);
      resources[id].lastUse = std::max(resources[id].lastUse, index);
    }
  }

  std::vector<uint32_t> writer(resources.size(), UINT32_MAX);
  for (uint32_t index = 0; index < passCount; ++index) {
    for (ResourceId id : passes[index].writes) {
      if (writer[id] == UINT32_MAX) {
        writer[id] = index;
      }
    }
  }
  for (uint32_t index = 0; index < passCount; ++index) {
    for (ResourceId id : passes[index].reads) {
      if (resources[id].imported != nullptr) {
        continue;
      }
      if (writer[id] == UINT32_MAX || writer[id] >= index) {
        return Error{"render graph read a resource that was not written yet", 3};
      }
    }
  }

  m_impl->resolved.assign(resources.size(), nullptr);
  std::vector<bool> used(m_impl->pool.size(), false);

  std::vector<ResourceId> order;
  for (ResourceId id = 0; id < resources.size(); ++id) {
    if (resources[id].imported == nullptr && resources[id].firstUse != UINT32_MAX) {
      order.push_back(id);
    }
  }
  std::sort(order.begin(), order.end(), [&resources](ResourceId left, ResourceId right) {
    return resources[left].firstUse < resources[right].firstUse;
  });

  for (ResourceId id : order) {
    const Impl::Resource& resource = resources[id];
    const RhiTexture* chosen = nullptr;
    for (size_t index = 0; index < m_impl->pool.size(); ++index) {
      Impl::PoolEntry& entry = m_impl->pool[index];
      if (used[index] && entry.lastUse >= resource.firstUse) {
        continue;
      }
      if (!SameDesc(entry.desc, resource.desc)) {
        continue;
      }
      used[index] = true;
      entry.lastUse = resource.lastUse;
      chosen = entry.texture.get();
      break;
    }
    if (chosen == nullptr) {
      auto created = RhiTexture::Create(device, ToTextureDesc(resource.desc));
      if (!created) {
        return created.GetError();
      }
      Impl::PoolEntry entry;
      entry.texture = std::move(created).Value();
      entry.desc = resource.desc;
      entry.lastUse = resource.lastUse;
      chosen = entry.texture.get();
      m_impl->pool.push_back(std::move(entry));
      used.push_back(true);
    }
    m_impl->resolved[id] = chosen;
  }

  std::vector<Impl::PoolEntry> kept;
  for (size_t index = 0; index < m_impl->pool.size(); ++index) {
    bool keep = used[index];
    if (!keep) {
      for (const Impl::Resource& resource : resources) {
        if (resource.imported == nullptr && SameDesc(m_impl->pool[index].desc, resource.desc)) {
          keep = true;
          break;
        }
      }
    }
    if (keep) {
      kept.push_back(std::move(m_impl->pool[index]));
    }
  }
  m_impl->pool = std::move(kept);

  for (ResourceId id = 0; id < resources.size(); ++id) {
    if (resources[id].imported != nullptr) {
      m_impl->resolved[id] = resources[id].imported;
    }
  }

  auto* nativeContext = static_cast<ID3D11DeviceContext*>(device.ContextHandle());
  if (nativeContext == nullptr) {
    return Error{"render graph has no live context", 4};
  }

  RenderGraphContext context(device, m_impl->resolved.data());
  for (const Impl::Pass& pass : passes) {
    ID3D11ShaderResourceView* nullViews[kUnboundSlots] = {};
    nativeContext->PSSetShaderResources(0, kUnboundSlots, nullViews);
    const Result<void> ran = pass.callback(context);
    if (!ran) {
      return ran.GetError();
    }
  }
  return {};
}

}

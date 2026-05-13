#pragma once

#include <cstddef>

#include "EffectSystem.h"

struct ParticleComponentAssetView {
    const EffectComponentCommon* common = nullptr;
    const EffectParticleSettings* settings = nullptr;
    explicit operator bool() const { return common != nullptr && settings != nullptr; }
};

struct TrailComponentAssetView {
    const EffectComponentCommon* common = nullptr;
    const EffectTrailSettings* settings = nullptr;
    explicit operator bool() const { return common != nullptr && settings != nullptr; }
};

struct BeamComponentAssetView {
    const EffectComponentCommon* common = nullptr;
    const EffectBeamSettings* settings = nullptr;
    explicit operator bool() const { return common != nullptr && settings != nullptr; }
};

struct DistortionComponentAssetView {
    const EffectComponentCommon* common = nullptr;
    const EffectDistortionSettings* settings = nullptr;
    explicit operator bool() const { return common != nullptr && settings != nullptr; }
};

struct RingComponentAssetView {
    const EffectComponentCommon* common = nullptr;
    const EffectRingSettings* settings = nullptr;
    explicit operator bool() const { return common != nullptr && settings != nullptr; }
};

struct CylinderComponentAssetView {
    const EffectComponentCommon* common = nullptr;
    const EffectCylinderSettings* settings = nullptr;
    explicit operator bool() const { return common != nullptr && settings != nullptr; }
};

inline ParticleComponentAssetView ParticleComponentView(const EffectComponentAsset& component) {
    if (component.common.type != EffectComponentType::Particle) {
        return {};
    }
    const EffectParticleSettings* settings = std::get_if<EffectParticleSettings>(&component.payload.data);
    return settings != nullptr ? ParticleComponentAssetView{&component.common, settings} : ParticleComponentAssetView{};
}

inline const EffectParticleSettings* ParticleSettings(const EffectComponentAsset& component) {
    const ParticleComponentAssetView view = ParticleComponentView(component);
    return view ? view.settings : nullptr;
}

inline TrailComponentAssetView TrailComponentView(const EffectComponentAsset& component) {
    if (component.common.type != EffectComponentType::Trail) {
        return {};
    }
    const EffectTrailSettings* settings = std::get_if<EffectTrailSettings>(&component.payload.data);
    return settings != nullptr ? TrailComponentAssetView{&component.common, settings} : TrailComponentAssetView{};
}

inline const EffectTrailSettings* TrailSettings(const EffectComponentAsset& component) {
    const TrailComponentAssetView view = TrailComponentView(component);
    return view ? view.settings : nullptr;
}

inline BeamComponentAssetView BeamComponentView(const EffectComponentAsset& component) {
    if (component.common.type != EffectComponentType::Beam) {
        return {};
    }
    const EffectBeamSettings* settings = std::get_if<EffectBeamSettings>(&component.payload.data);
    return settings != nullptr ? BeamComponentAssetView{&component.common, settings} : BeamComponentAssetView{};
}

inline const EffectBeamSettings* BeamSettings(const EffectComponentAsset& component) {
    const BeamComponentAssetView view = BeamComponentView(component);
    return view ? view.settings : nullptr;
}

inline DistortionComponentAssetView DistortionComponentView(const EffectComponentAsset& component) {
    if (component.common.type != EffectComponentType::Distortion) {
        return {};
    }
    const EffectDistortionSettings* settings = std::get_if<EffectDistortionSettings>(&component.payload.data);
    return settings != nullptr ? DistortionComponentAssetView{&component.common, settings} : DistortionComponentAssetView{};
}

inline const EffectDistortionSettings* DistortionSettings(const EffectComponentAsset& component) {
    const DistortionComponentAssetView view = DistortionComponentView(component);
    return view ? view.settings : nullptr;
}

inline RingComponentAssetView RingComponentView(const EffectComponentAsset& component) {
    if (component.common.type != EffectComponentType::Ring) {
        return {};
    }
    const EffectRingSettings* settings = std::get_if<EffectRingSettings>(&component.payload.data);
    return settings != nullptr ? RingComponentAssetView{&component.common, settings} : RingComponentAssetView{};
}

inline const EffectRingSettings* RingSettings(const EffectComponentAsset& component) {
    const RingComponentAssetView view = RingComponentView(component);
    return view ? view.settings : nullptr;
}

inline CylinderComponentAssetView CylinderComponentView(const EffectComponentAsset& component) {
    if (component.common.type != EffectComponentType::Cylinder) {
        return {};
    }
    const EffectCylinderSettings* settings = std::get_if<EffectCylinderSettings>(&component.payload.data);
    return settings != nullptr ? CylinderComponentAssetView{&component.common, settings} : CylinderComponentAssetView{};
}

inline const EffectCylinderSettings* CylinderSettings(const EffectComponentAsset& component) {
    const CylinderComponentAssetView view = CylinderComponentView(component);
    return view ? view.settings : nullptr;
}

struct ParticleComponentCollectionView {
    std::vector<ParticleComponentAssetView> components;
    auto begin() const { return components.begin(); }
    auto end() const { return components.end(); }
    bool empty() const { return components.empty(); }
    std::size_t size() const { return components.size(); }
};

struct TrailComponentCollectionView {
    std::vector<TrailComponentAssetView> components;
    auto begin() const { return components.begin(); }
    auto end() const { return components.end(); }
    bool empty() const { return components.empty(); }
    std::size_t size() const { return components.size(); }
};

struct BeamComponentCollectionView {
    std::vector<BeamComponentAssetView> components;
    auto begin() const { return components.begin(); }
    auto end() const { return components.end(); }
    bool empty() const { return components.empty(); }
    std::size_t size() const { return components.size(); }
};

struct DistortionComponentCollectionView {
    std::vector<DistortionComponentAssetView> components;
    auto begin() const { return components.begin(); }
    auto end() const { return components.end(); }
    bool empty() const { return components.empty(); }
    std::size_t size() const { return components.size(); }
};

struct RingComponentCollectionView {
    std::vector<RingComponentAssetView> components;
    auto begin() const { return components.begin(); }
    auto end() const { return components.end(); }
    bool empty() const { return components.empty(); }
    std::size_t size() const { return components.size(); }
};

struct CylinderComponentCollectionView {
    std::vector<CylinderComponentAssetView> components;
    auto begin() const { return components.begin(); }
    auto end() const { return components.end(); }
    bool empty() const { return components.empty(); }
    std::size_t size() const { return components.size(); }
};

struct ParticleComponentStorageView {
    const std::vector<ParticleComponentAsset>* components = nullptr;
    explicit operator bool() const { return components != nullptr; }
};

struct MutableParticleComponentStorageView {
    // Mutable replace handle for authoring/live tuning.
    // It intentionally does not expose direct component pointers.
    EffectAssetComponentStorage* components = nullptr;
    explicit operator bool() const { return components != nullptr; }
};

struct TrailComponentStorageView {
    const std::vector<TrailComponentAsset>* components = nullptr;
    explicit operator bool() const { return components != nullptr; }
};

struct MutableTrailComponentStorageView {
    // Mutable replace handle for authoring/live tuning.
    // It intentionally does not expose direct component pointers.
    EffectAssetComponentStorage* components = nullptr;
    explicit operator bool() const { return components != nullptr; }
};

struct BeamComponentStorageView {
    const std::vector<BeamComponentAsset>* components = nullptr;
    explicit operator bool() const { return components != nullptr; }
};

struct MutableBeamComponentStorageView {
    // Mutable replace handle for authoring/live tuning.
    // It intentionally does not expose direct component pointers.
    EffectAssetComponentStorage* components = nullptr;
    explicit operator bool() const { return components != nullptr; }
};

struct DistortionComponentStorageView {
    const std::vector<DistortionComponentAsset>* components = nullptr;
    explicit operator bool() const { return components != nullptr; }
};

struct MutableDistortionComponentStorageView {
    // Mutable replace handle for authoring/live tuning.
    // It intentionally does not expose direct component pointers.
    EffectAssetComponentStorage* components = nullptr;
    explicit operator bool() const { return components != nullptr; }
};

struct RingComponentStorageView {
    const std::vector<RingComponentAsset>* components = nullptr;
    explicit operator bool() const { return components != nullptr; }
};

struct MutableRingComponentStorageView {
    EffectAssetComponentStorage* components = nullptr;
    explicit operator bool() const { return components != nullptr; }
};

struct CylinderComponentStorageView {
    const std::vector<CylinderComponentAsset>* components = nullptr;
    explicit operator bool() const { return components != nullptr; }
};

struct MutableCylinderComponentStorageView {
    EffectAssetComponentStorage* components = nullptr;
    explicit operator bool() const { return components != nullptr; }
};

inline ParticleComponentStorageView EffectAssetComponentStorage::ParticleStorageView() const {
    return {&particleComponents_};
}

inline MutableParticleComponentStorageView EffectAssetComponentStorage::MutableParticleStorageView() {
    return {this};
}

inline TrailComponentStorageView EffectAssetComponentStorage::TrailStorageView() const {
    return {&trailComponents_};
}

inline MutableTrailComponentStorageView EffectAssetComponentStorage::MutableTrailStorageView() {
    return {this};
}

inline BeamComponentStorageView EffectAssetComponentStorage::BeamStorageView() const {
    return {&beamComponents_};
}

inline MutableBeamComponentStorageView EffectAssetComponentStorage::MutableBeamStorageView() {
    return {this};
}

inline DistortionComponentStorageView EffectAssetComponentStorage::DistortionStorageView() const {
    return {&distortionComponents_};
}

inline MutableDistortionComponentStorageView EffectAssetComponentStorage::MutableDistortionStorageView() {
    return {this};
}

inline RingComponentStorageView EffectAssetComponentStorage::RingStorageView() const {
    return {&ringComponents_};
}

inline MutableRingComponentStorageView EffectAssetComponentStorage::MutableRingStorageView() {
    return {this};
}

inline CylinderComponentStorageView EffectAssetComponentStorage::CylinderStorageView() const {
    return {&cylinderComponents_};
}

inline MutableCylinderComponentStorageView EffectAssetComponentStorage::MutableCylinderStorageView() {
    return {this};
}

inline bool ReplaceParticleComponentAndSyncPacked(
    const MutableParticleComponentStorageView& view,
    const ParticleComponentAsset& component) {
    if (!view) {
        return false;
    }
    return view.components->ReplaceParticleComponentAndSyncPacked(component);
}

inline bool ReplaceTrailComponentAndSyncPacked(
    const MutableTrailComponentStorageView& view,
    const TrailComponentAsset& component) {
    if (!view) {
        return false;
    }
    return view.components->ReplaceTrailComponentAndSyncPacked(component);
}

inline bool ReplaceBeamComponentAndSyncPacked(
    const MutableBeamComponentStorageView& view,
    const BeamComponentAsset& component) {
    if (!view) {
        return false;
    }
    return view.components->ReplaceBeamComponentAndSyncPacked(component);
}

inline bool ReplaceDistortionComponentAndSyncPacked(
    const MutableDistortionComponentStorageView& view,
    const DistortionComponentAsset& component) {
    if (!view) {
        return false;
    }
    return view.components->ReplaceDistortionComponentAndSyncPacked(component);
}

inline bool ReplaceRingComponentAndSyncPacked(
    const MutableRingComponentStorageView& view,
    const RingComponentAsset& component) {
    if (!view) {
        return false;
    }
    return view.components->ReplaceRingComponentAndSyncPacked(component);
}

inline bool ReplaceCylinderComponentAndSyncPacked(
    const MutableCylinderComponentStorageView& view,
    const CylinderComponentAsset& component) {
    if (!view) {
        return false;
    }
    return view.components->ReplaceCylinderComponentAndSyncPacked(component);
}

template <typename Visitor>
inline void ForEachParticleComponent(const ParticleComponentStorageView& view, Visitor&& visitor) {
    if (!view) {
        return;
    }
    for (const ParticleComponentAsset& component : *view.components) {
        visitor(ParticleComponentAssetView{&component.common, &component.settings});
    }
}

template <typename Visitor>
inline void ForEachParticleComponent(const EffectAssetComponentStorage& components, Visitor&& visitor) {
    ForEachParticleComponent(components.ParticleStorageView(), std::forward<Visitor>(visitor));
}

template <typename Visitor>
inline void ForEachParticleComponent(const EffectAsset& asset, Visitor&& visitor) {
    ForEachParticleComponent(asset.Components(), std::forward<Visitor>(visitor));
}

template <typename Visitor>
inline void ForEachTrailComponent(const TrailComponentStorageView& view, Visitor&& visitor) {
    if (!view) {
        return;
    }
    for (const TrailComponentAsset& component : *view.components) {
        visitor(TrailComponentAssetView{&component.common, &component.settings});
    }
}

template <typename Visitor>
inline void ForEachTrailComponent(const EffectAssetComponentStorage& components, Visitor&& visitor) {
    ForEachTrailComponent(components.TrailStorageView(), std::forward<Visitor>(visitor));
}

template <typename Visitor>
inline void ForEachTrailComponent(const EffectAsset& asset, Visitor&& visitor) {
    ForEachTrailComponent(asset.Components(), std::forward<Visitor>(visitor));
}

template <typename Visitor>
inline void ForEachBeamComponent(const BeamComponentStorageView& view, Visitor&& visitor) {
    if (!view) {
        return;
    }
    for (const BeamComponentAsset& component : *view.components) {
        visitor(BeamComponentAssetView{&component.common, &component.settings});
    }
}

template <typename Visitor>
inline void ForEachBeamComponent(const EffectAssetComponentStorage& components, Visitor&& visitor) {
    ForEachBeamComponent(components.BeamStorageView(), std::forward<Visitor>(visitor));
}

template <typename Visitor>
inline void ForEachBeamComponent(const EffectAsset& asset, Visitor&& visitor) {
    ForEachBeamComponent(asset.Components(), std::forward<Visitor>(visitor));
}

template <typename Visitor>
inline void ForEachDistortionComponent(const DistortionComponentStorageView& view, Visitor&& visitor) {
    if (!view) {
        return;
    }
    for (const DistortionComponentAsset& component : *view.components) {
        visitor(DistortionComponentAssetView{&component.common, &component.settings});
    }
}

template <typename Visitor>
inline void ForEachDistortionComponent(const EffectAssetComponentStorage& components, Visitor&& visitor) {
    ForEachDistortionComponent(components.DistortionStorageView(), std::forward<Visitor>(visitor));
}

template <typename Visitor>
inline void ForEachDistortionComponent(const EffectAsset& asset, Visitor&& visitor) {
    ForEachDistortionComponent(asset.Components(), std::forward<Visitor>(visitor));
}

template <typename Visitor>
inline void ForEachRingComponent(const RingComponentStorageView& view, Visitor&& visitor) {
    if (!view) {
        return;
    }
    for (const RingComponentAsset& component : *view.components) {
        visitor(RingComponentAssetView{&component.common, &component.settings});
    }
}

template <typename Visitor>
inline void ForEachRingComponent(const EffectAssetComponentStorage& components, Visitor&& visitor) {
    ForEachRingComponent(components.RingStorageView(), std::forward<Visitor>(visitor));
}

template <typename Visitor>
inline void ForEachRingComponent(const EffectAsset& asset, Visitor&& visitor) {
    ForEachRingComponent(asset.Components(), std::forward<Visitor>(visitor));
}

template <typename Visitor>
inline void ForEachCylinderComponent(const CylinderComponentStorageView& view, Visitor&& visitor) {
    if (!view) {
        return;
    }
    for (const CylinderComponentAsset& component : *view.components) {
        visitor(CylinderComponentAssetView{&component.common, &component.settings});
    }
}

template <typename Visitor>
inline void ForEachCylinderComponent(const EffectAssetComponentStorage& components, Visitor&& visitor) {
    ForEachCylinderComponent(components.CylinderStorageView(), std::forward<Visitor>(visitor));
}

template <typename Visitor>
inline void ForEachCylinderComponent(const EffectAsset& asset, Visitor&& visitor) {
    ForEachCylinderComponent(asset.Components(), std::forward<Visitor>(visitor));
}

inline bool HasParticleComponents(const ParticleComponentStorageView& view) {
    bool found = false;
    ForEachParticleComponent(view, [&found](const ParticleComponentAssetView&) {
        found = true;
    });
    return found;
}

inline bool HasParticleComponents(const EffectAssetComponentStorage& components) {
    return HasParticleComponents(components.ParticleStorageView());
}

inline bool HasParticleComponents(const EffectAsset& asset) {
    return HasParticleComponents(asset.Components());
}

inline bool HasTrailComponents(const TrailComponentStorageView& view) {
    bool found = false;
    ForEachTrailComponent(view, [&found](const TrailComponentAssetView&) {
        found = true;
    });
    return found;
}

inline bool HasTrailComponents(const EffectAssetComponentStorage& components) {
    return HasTrailComponents(components.TrailStorageView());
}

inline bool HasTrailComponents(const EffectAsset& asset) {
    return HasTrailComponents(asset.Components());
}

inline bool HasBeamComponents(const BeamComponentStorageView& view) {
    bool found = false;
    ForEachBeamComponent(view, [&found](const BeamComponentAssetView&) {
        found = true;
    });
    return found;
}

inline bool HasBeamComponents(const EffectAssetComponentStorage& components) {
    return HasBeamComponents(components.BeamStorageView());
}

inline bool HasBeamComponents(const EffectAsset& asset) {
    return HasBeamComponents(asset.Components());
}

inline bool HasDistortionComponents(const DistortionComponentStorageView& view) {
    bool found = false;
    ForEachDistortionComponent(view, [&found](const DistortionComponentAssetView&) {
        found = true;
    });
    return found;
}

inline bool HasDistortionComponents(const EffectAssetComponentStorage& components) {
    return HasDistortionComponents(components.DistortionStorageView());
}

inline bool HasDistortionComponents(const EffectAsset& asset) {
    return HasDistortionComponents(asset.Components());
}

inline bool HasRingComponents(const RingComponentStorageView& view) {
    bool found = false;
    ForEachRingComponent(view, [&found](const RingComponentAssetView&) {
        found = true;
    });
    return found;
}

inline bool HasRingComponents(const EffectAssetComponentStorage& components) {
    return HasRingComponents(components.RingStorageView());
}

inline bool HasRingComponents(const EffectAsset& asset) {
    return HasRingComponents(asset.Components());
}

inline bool HasCylinderComponents(const CylinderComponentStorageView& view) {
    bool found = false;
    ForEachCylinderComponent(view, [&found](const CylinderComponentAssetView&) {
        found = true;
    });
    return found;
}

inline bool HasCylinderComponents(const EffectAssetComponentStorage& components) {
    return HasCylinderComponents(components.CylinderStorageView());
}

inline bool HasCylinderComponents(const EffectAsset& asset) {
    return HasCylinderComponents(asset.Components());
}

inline ParticleComponentAssetView FindParticleComponent(
    const ParticleComponentStorageView& view,
    uint32_t componentId) {
    if (!view) {
        return {};
    }
    for (const ParticleComponentAsset& component : *view.components) {
        if (component.common.id == componentId) {
            return {&component.common, &component.settings};
        }
    }
    return {};
}

inline ParticleComponentAssetView FindParticleComponent(
    const EffectAssetComponentStorage& components,
    uint32_t componentId) {
    return FindParticleComponent(components.ParticleStorageView(), componentId);
}

inline ParticleComponentAssetView FindParticleComponent(
    const EffectAsset& asset,
    uint32_t componentId) {
    return FindParticleComponent(asset.Components(), componentId);
}

inline TrailComponentAssetView FindTrailComponent(
    const TrailComponentStorageView& view,
    uint32_t componentId) {
    if (!view) {
        return {};
    }
    for (const TrailComponentAsset& component : *view.components) {
        if (component.common.id == componentId) {
            return {&component.common, &component.settings};
        }
    }
    return {};
}

inline TrailComponentAssetView FindTrailComponent(
    const EffectAssetComponentStorage& components,
    uint32_t componentId) {
    return FindTrailComponent(components.TrailStorageView(), componentId);
}

inline TrailComponentAssetView FindTrailComponent(
    const EffectAsset& asset,
    uint32_t componentId) {
    return FindTrailComponent(asset.Components(), componentId);
}

inline BeamComponentAssetView FindBeamComponent(
    const BeamComponentStorageView& view,
    uint32_t componentId) {
    if (!view) {
        return {};
    }
    for (const BeamComponentAsset& component : *view.components) {
        if (component.common.id == componentId) {
            return {&component.common, &component.settings};
        }
    }
    return {};
}

inline BeamComponentAssetView FindBeamComponent(
    const EffectAssetComponentStorage& components,
    uint32_t componentId) {
    return FindBeamComponent(components.BeamStorageView(), componentId);
}

inline BeamComponentAssetView FindBeamComponent(
    const EffectAsset& asset,
    uint32_t componentId) {
    return FindBeamComponent(asset.Components(), componentId);
}

inline DistortionComponentAssetView FindDistortionComponent(
    const DistortionComponentStorageView& view,
    uint32_t componentId) {
    if (!view) {
        return {};
    }
    for (const DistortionComponentAsset& component : *view.components) {
        if (component.common.id == componentId) {
            return {&component.common, &component.settings};
        }
    }
    return {};
}

inline DistortionComponentAssetView FindDistortionComponent(
    const EffectAssetComponentStorage& components,
    uint32_t componentId) {
    return FindDistortionComponent(components.DistortionStorageView(), componentId);
}

inline DistortionComponentAssetView FindDistortionComponent(
    const EffectAsset& asset,
    uint32_t componentId) {
    return FindDistortionComponent(asset.Components(), componentId);
}

inline RingComponentAssetView FindRingComponent(
    const RingComponentStorageView& view,
    uint32_t componentId) {
    if (!view) {
        return {};
    }
    for (const RingComponentAsset& component : *view.components) {
        if (component.common.id == componentId) {
            return {&component.common, &component.settings};
        }
    }
    return {};
}

inline RingComponentAssetView FindRingComponent(
    const EffectAssetComponentStorage& components,
    uint32_t componentId) {
    return FindRingComponent(components.RingStorageView(), componentId);
}

inline RingComponentAssetView FindRingComponent(
    const EffectAsset& asset,
    uint32_t componentId) {
    return FindRingComponent(asset.Components(), componentId);
}

inline CylinderComponentAssetView FindCylinderComponent(
    const CylinderComponentStorageView& view,
    uint32_t componentId) {
    if (!view) {
        return {};
    }
    for (const CylinderComponentAsset& component : *view.components) {
        if (component.common.id == componentId) {
            return {&component.common, &component.settings};
        }
    }
    return {};
}

inline CylinderComponentAssetView FindCylinderComponent(
    const EffectAssetComponentStorage& components,
    uint32_t componentId) {
    return FindCylinderComponent(components.CylinderStorageView(), componentId);
}

inline CylinderComponentAssetView FindCylinderComponent(
    const EffectAsset& asset,
    uint32_t componentId) {
    return FindCylinderComponent(asset.Components(), componentId);
}

inline ParticleComponentCollectionView ParticleComponents(const ParticleComponentStorageView& storage) {
    ParticleComponentCollectionView view{};
    if (!storage) {
        return view;
    }
    view.components.reserve(storage.components->size());
    ForEachParticleComponent(storage, [&view](const ParticleComponentAssetView& particle) {
        view.components.push_back(particle);
    });
    return view;
}

inline ParticleComponentCollectionView ParticleComponents(const EffectAssetComponentStorage& components) {
    return ParticleComponents(components.ParticleStorageView());
}

inline ParticleComponentCollectionView ParticleComponents(const EffectAsset& asset) {
    return ParticleComponents(asset.Components());
}

inline TrailComponentCollectionView TrailComponents(const TrailComponentStorageView& storage) {
    TrailComponentCollectionView view{};
    if (!storage) {
        return view;
    }
    view.components.reserve(storage.components->size());
    ForEachTrailComponent(storage, [&view](const TrailComponentAssetView& trail) {
        view.components.push_back(trail);
    });
    return view;
}

inline TrailComponentCollectionView TrailComponents(const EffectAssetComponentStorage& components) {
    return TrailComponents(components.TrailStorageView());
}

inline TrailComponentCollectionView TrailComponents(const EffectAsset& asset) {
    return TrailComponents(asset.Components());
}

inline BeamComponentCollectionView BeamComponents(const BeamComponentStorageView& storage) {
    BeamComponentCollectionView view{};
    if (!storage) {
        return view;
    }
    view.components.reserve(storage.components->size());
    ForEachBeamComponent(storage, [&view](const BeamComponentAssetView& beam) {
        view.components.push_back(beam);
    });
    return view;
}

inline BeamComponentCollectionView BeamComponents(const EffectAssetComponentStorage& components) {
    return BeamComponents(components.BeamStorageView());
}

inline BeamComponentCollectionView BeamComponents(const EffectAsset& asset) {
    return BeamComponents(asset.Components());
}

inline DistortionComponentCollectionView DistortionComponents(const DistortionComponentStorageView& storage) {
    DistortionComponentCollectionView view{};
    if (!storage) {
        return view;
    }
    view.components.reserve(storage.components->size());
    ForEachDistortionComponent(storage, [&view](const DistortionComponentAssetView& distortion) {
        view.components.push_back(distortion);
    });
    return view;
}

inline DistortionComponentCollectionView DistortionComponents(const EffectAssetComponentStorage& components) {
    return DistortionComponents(components.DistortionStorageView());
}

inline DistortionComponentCollectionView DistortionComponents(const EffectAsset& asset) {
    return DistortionComponents(asset.Components());
}

inline RingComponentCollectionView RingComponents(const RingComponentStorageView& storage) {
    RingComponentCollectionView view{};
    if (!storage) {
        return view;
    }
    view.components.reserve(storage.components->size());
    ForEachRingComponent(storage, [&view](const RingComponentAssetView& ring) {
        view.components.push_back(ring);
    });
    return view;
}

inline RingComponentCollectionView RingComponents(const EffectAssetComponentStorage& components) {
    return RingComponents(components.RingStorageView());
}

inline RingComponentCollectionView RingComponents(const EffectAsset& asset) {
    return RingComponents(asset.Components());
}

inline CylinderComponentCollectionView CylinderComponents(const CylinderComponentStorageView& storage) {
    CylinderComponentCollectionView view{};
    if (!storage) {
        return view;
    }
    view.components.reserve(storage.components->size());
    ForEachCylinderComponent(storage, [&view](const CylinderComponentAssetView& cylinder) {
        view.components.push_back(cylinder);
    });
    return view;
}

inline CylinderComponentCollectionView CylinderComponents(const EffectAssetComponentStorage& components) {
    return CylinderComponents(components.CylinderStorageView());
}

inline CylinderComponentCollectionView CylinderComponents(const EffectAsset& asset) {
    return CylinderComponents(asset.Components());
}

struct EffectComponentAssetBuilder {
    static EffectComponentCommon MakeCommon(
        const EffectAsset& asset,
        EffectComponentType type,
        uint32_t id,
        const TechniqueRegistry& techniqueRegistry) {
        EffectComponentCommon common{};
        common.id = id;
        common.type = type;
        if (const EffectTechniqueDescriptor* descriptor =
                techniqueRegistry.FindDefaultForComponentType(type)) {
            common.techniqueId = descriptor->id;
            common.techniqueDisplayName = descriptor->displayName;
            common.techniqueCategory = descriptor->category;
            common.techniqueDescription = descriptor->description;
            common.techniqueMetadataSource = EffectTechniqueMetadataSource::Registry;
            common.rendererId = descriptor->rendererId;
            common.simulationId = descriptor->simulationId;
            techniqueRegistry.UpdateCompatibilityMirrors(common, *descriptor);
        }
        common.name = asset.name + "_component_" + std::to_string(id);
        common.shader = asset.shader;
        common.texture = asset.texture;
        common.passState = asset.passState;
        common.layer = asset.layer;
        common.startTime = 0.0f;
        common.duration = asset.lifetime;
        common.color = asset.color;
        common.size = asset.size;
        return common;
    }

    static ParticleComponentAsset MakeParticle(
        const EffectAsset& asset,
        EffectComponentCommon common) {
        common.type = EffectComponentType::Particle;
        return {common, asset.defaultParticle};
    }

    static TrailComponentAsset MakeTrail(
        const EffectAsset& asset,
        EffectComponentCommon common) {
        common.type = EffectComponentType::Trail;
        return {common, asset.defaultTrail};
    }

    static BeamComponentAsset MakeBeam(
        const EffectAsset& asset,
        EffectComponentCommon common) {
        common.type = EffectComponentType::Beam;
        return {common, asset.defaultBeam};
    }

    static DistortionComponentAsset MakeDistortion(
        const EffectAsset& asset,
        EffectComponentCommon common) {
        common.type = EffectComponentType::Distortion;
        return {common, asset.defaultDistortion};
    }

    static RingComponentAsset MakeRing(
        const EffectAsset& asset,
        EffectComponentCommon common) {
        common.type = EffectComponentType::Ring;
        return {common, asset.defaultRing};
    }

    static CylinderComponentAsset MakeCylinder(
        const EffectAsset& asset,
        EffectComponentCommon common) {
        common.type = EffectComponentType::Cylinder;
        return {common, asset.defaultCylinder};
    }

    static EffectComponentAsset Pack(
        const EffectAsset& asset,
        const EffectComponentCommon& common) {
        switch (common.type) {
        case EffectComponentType::Trail:
            return ToEffectComponentAsset(MakeTrail(asset, common));
        case EffectComponentType::Beam:
            return ToEffectComponentAsset(MakeBeam(asset, common));
        case EffectComponentType::Distortion:
            return ToEffectComponentAsset(MakeDistortion(asset, common));
        case EffectComponentType::Ring:
            return ToEffectComponentAsset(MakeRing(asset, common));
        case EffectComponentType::Cylinder:
            return ToEffectComponentAsset(MakeCylinder(asset, common));
        case EffectComponentType::Particle:
        default:
            return ToEffectComponentAsset(MakeParticle(asset, common));
        }
    }

    static EffectComponentAsset Make(
        const EffectAsset& asset,
        EffectComponentType type,
        uint32_t id,
        const TechniqueRegistry& techniqueRegistry) {
        return Pack(asset, MakeCommon(asset, type, id, techniqueRegistry));
    }
};

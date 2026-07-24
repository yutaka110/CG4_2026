#pragma once

#include "EditorScene.h"
#include "EditorSceneComponentRegistry.h"
#include "EditorSceneRuntimeObjectRegistry.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace editor {

class EditorSceneRuntimeServiceRegistry {
public:
    template <typename T>
    bool Bind(std::string id, T* service) {
        if (id.empty() || service == nullptr) return false;
        bindings_.insert_or_assign(
            std::move(id),
            Binding{service, &typeid(T)});
        return true;
    }

    template <typename T>
    T* Find(std::string_view id) const {
        const auto found = bindings_.find(std::string(id));
        if (found == bindings_.end() ||
            found->second.type == nullptr ||
            *found->second.type != typeid(T)) {
            return nullptr;
        }
        return static_cast<T*>(found->second.service);
    }

    void Clear() { bindings_.clear(); }
    std::size_t Count() const noexcept { return bindings_.size(); }

private:
    struct Binding {
        void* service = nullptr;
        const std::type_info* type = nullptr;
    };
    std::unordered_map<std::string, Binding> bindings_;
};

struct EditorSceneRuntimeComponentRecord {
    std::string stableId;
    const EditorSceneEntity* entity = nullptr;
    const EditorSceneComponent* component = nullptr;
    uint64_t sourceHash = 0;
};

struct EditorSceneRuntimeFactoryResult {
    bool succeeded = false;
    bool applied = false;
    std::vector<std::string> warnings;
    std::string message;
};

class IEditorSceneRuntimeComponentFactory {
public:
    virtual ~IEditorSceneRuntimeComponentFactory() = default;

    virtual std::string_view TypeId() const noexcept = 0;
    virtual int32_t Priority() const noexcept { return 0; }
    virtual EditorSceneRuntimeFactoryResult Instantiate(
        const EditorScene& scene,
        const std::vector<EditorSceneRuntimeComponentRecord>& components,
        const EditorSceneRuntimeServiceRegistry& services) = 0;
    virtual void Destroy() noexcept = 0;
};

class EditorSceneRuntimeComponentFactoryRegistry {
public:
    ~EditorSceneRuntimeComponentFactoryRegistry();

    bool Register(
        std::unique_ptr<IEditorSceneRuntimeComponentFactory> factory,
        std::string* errorMessage = nullptr);
    bool Remove(std::string_view typeId);
    void Clear();

    IEditorSceneRuntimeComponentFactory* Find(std::string_view typeId);
    const IEditorSceneRuntimeComponentFactory* Find(std::string_view typeId) const;
    std::size_t Count() const noexcept { return factories_.size(); }

private:
    std::vector<std::unique_ptr<IEditorSceneRuntimeComponentFactory>> factories_;
};

struct EditorSceneRuntimeInstantiationResult {
    bool succeeded = false;
    bool applied = false;
    std::size_t componentCount = 0;
    std::size_t factoryCount = 0;
    std::size_t addedCount = 0;
    std::size_t modifiedCount = 0;
    std::size_t removedCount = 0;
    uint64_t sourceRevision = 0;
    std::vector<std::string> warnings;
    std::string message;
};

class EditorSceneRuntimeInstantiationService {
public:
    ~EditorSceneRuntimeInstantiationService();

    bool Bind(
        const EditorSceneComponentRegistry* components,
        EditorSceneRuntimeComponentFactoryRegistry* factories,
        std::string* errorMessage = nullptr);

    EditorSceneRuntimeInstantiationResult Begin(
        const EditorScene& scene,
        const EditorSceneRuntimeServiceRegistry& services);
    EditorSceneRuntimeInstantiationResult Reconcile(
        const EditorScene& scene,
        const EditorSceneRuntimeServiceRegistry& services);
    void Stop() noexcept;

    bool Active() const noexcept { return active_; }
    uint64_t SourceRevision() const noexcept { return sourceRevision_; }
    const std::vector<std::string>& ActiveFactoryTypes() const noexcept {
        return activeFactoryTypes_;
    }
    const EditorSceneRuntimeObjectRegistry& Objects() const noexcept {
        return runtimeObjects_;
    }

private:
    const EditorSceneComponentRegistry* components_ = nullptr;
    EditorSceneRuntimeComponentFactoryRegistry* factories_ = nullptr;
    bool active_ = false;
    uint64_t sourceRevision_ = 0;
    std::vector<std::string> activeFactoryTypes_;
    EditorSceneRuntimeObjectRegistry runtimeObjects_{};
    EditorScene activeSceneSnapshot_{};
};

uint64_t HashEditorSceneRuntimeComponent(
    const EditorSceneEntity& entity,
    const EditorSceneComponent& component) noexcept;

} // namespace editor

#include "EditorToolRegistration.h"

#include "EditorContext.h"

#include <algorithm>
#include <exception>
#include <utility>

namespace editor {
namespace {

std::string DescriptorId(std::string_view explicitToolId, std::string_view fallbackId) {
    return explicitToolId.empty() ? std::string(fallbackId) : std::string(explicitToolId);
}

void AddDiagnosticTo(
    std::vector<EditorToolRegistrationDiagnostic>* diagnostics,
    EditorToolDiagnosticSeverity severity,
    EditorToolDescriptorKind kind,
    std::string_view id,
    std::string message) {
    if (diagnostics == nullptr) {
        return;
    }
    diagnostics->push_back(
        EditorToolRegistrationDiagnostic{
            severity,
            kind,
            std::string(id),
            std::move(message)});
}

bool IsSupportedVersionValue(EditorToolDescriptorVersion version) {
    return version.major == 1;
}

bool FeatureAllowsRegistration(const EditorToolFeatureGate& feature) {
    return feature.enabled && feature.state != EditorToolFeatureState::Disabled;
}

bool FeatureAllowsPresentation(const EditorToolFeatureGate& feature) {
    return FeatureAllowsRegistration(feature) &&
        feature.state != EditorToolFeatureState::Hidden;
}

} // namespace

void EditorToolbarRegistry::Clear() {
    if (items_.empty()) {
        return;
    }
    items_.clear();
    Touch();
}

bool EditorToolbarRegistry::Register(
    EditorToolbarItemDescriptor descriptor,
    std::vector<EditorToolRegistrationDiagnostic>* diagnostics) {
    if (!FeatureAllowsRegistration(descriptor.feature)) {
        return true;
    }
    descriptor.visible = descriptor.visible && FeatureAllowsPresentation(descriptor.feature);
    if (!IsSupportedVersionValue(descriptor.version)) {
        AddDiagnosticTo(
            diagnostics,
            EditorToolDiagnosticSeverity::Error,
            EditorToolDescriptorKind::ToolbarItem,
            descriptor.id,
            "Unsupported toolbar descriptor version.");
        return false;
    }
    if (descriptor.id.empty()) {
        AddDiagnosticTo(
            diagnostics,
            EditorToolDiagnosticSeverity::Error,
            EditorToolDescriptorKind::ToolbarItem,
            descriptor.commandId,
            "Toolbar descriptor id is empty.");
        return false;
    }
    if (descriptor.commandId.empty()) {
        AddDiagnosticTo(
            diagnostics,
            EditorToolDiagnosticSeverity::Error,
            EditorToolDescriptorKind::ToolbarItem,
            descriptor.id,
            "Toolbar descriptor command id is empty.");
        return false;
    }
    if (descriptor.label.empty()) {
        descriptor.label = descriptor.commandId;
    }

    const auto found = std::find_if(
        items_.begin(),
        items_.end(),
        [&descriptor](const EditorToolbarItemDescriptor& current) {
            return current.id == descriptor.id;
        });
    if (found != items_.end()) {
        if (!descriptor.allowReplace) {
            AddDiagnosticTo(
                diagnostics,
                EditorToolDiagnosticSeverity::Error,
                EditorToolDescriptorKind::ToolbarItem,
                descriptor.id,
                "Duplicate toolbar descriptor id.");
            return false;
        }
        *found = std::move(descriptor);
        std::stable_sort(
            items_.begin(),
            items_.end(),
            [](const EditorToolbarItemDescriptor& lhs, const EditorToolbarItemDescriptor& rhs) {
                return lhs.sortOrder < rhs.sortOrder;
            });
        Touch();
        return true;
    }

    items_.push_back(std::move(descriptor));
    std::stable_sort(
        items_.begin(),
        items_.end(),
        [](const EditorToolbarItemDescriptor& lhs, const EditorToolbarItemDescriptor& rhs) {
            return lhs.sortOrder < rhs.sortOrder;
        });
    Touch();
    return true;
}

std::vector<const EditorToolbarItemDescriptor*> EditorToolbarRegistry::VisibleItems() const {
    std::vector<const EditorToolbarItemDescriptor*> result;
    for (const EditorToolbarItemDescriptor& item : items_) {
        if (item.visible) {
            result.push_back(&item);
        }
    }
    return result;
}

void EditorToolbarRegistry::Touch() {
    ++revision_;
}

void EditorMenuRegistry::Clear() {
    if (sections_.empty() && items_.empty()) {
        return;
    }
    sections_.clear();
    items_.clear();
    Touch();
}

bool EditorMenuRegistry::RegisterSection(
    EditorMenuSectionDescriptor descriptor,
    std::vector<EditorToolRegistrationDiagnostic>* diagnostics) {
    if (!FeatureAllowsRegistration(descriptor.feature)) {
        return true;
    }
    descriptor.visible = descriptor.visible && FeatureAllowsPresentation(descriptor.feature);
    if (!IsSupportedVersionValue(descriptor.version)) {
        AddDiagnosticTo(
            diagnostics,
            EditorToolDiagnosticSeverity::Error,
            EditorToolDescriptorKind::MenuSection,
            descriptor.id,
            "Unsupported menu section descriptor version.");
        return false;
    }
    if (descriptor.id.empty() || descriptor.label.empty()) {
        AddDiagnosticTo(
            diagnostics,
            EditorToolDiagnosticSeverity::Error,
            EditorToolDescriptorKind::MenuSection,
            descriptor.id,
            "Menu section descriptor requires an id and label.");
        return false;
    }

    const auto found = std::find_if(
        sections_.begin(),
        sections_.end(),
        [&descriptor](const EditorMenuSectionDescriptor& current) {
            return current.id == descriptor.id;
        });
    if (found != sections_.end()) {
        if (!descriptor.allowReplace) {
            AddDiagnosticTo(
                diagnostics,
                EditorToolDiagnosticSeverity::Error,
                EditorToolDescriptorKind::MenuSection,
                descriptor.id,
                "Duplicate menu section descriptor id.");
            return false;
        }
        *found = std::move(descriptor);
        std::stable_sort(
            sections_.begin(),
            sections_.end(),
            [](const EditorMenuSectionDescriptor& lhs, const EditorMenuSectionDescriptor& rhs) {
                return lhs.sortOrder < rhs.sortOrder;
            });
        Touch();
        return true;
    }

    sections_.push_back(std::move(descriptor));
    std::stable_sort(
        sections_.begin(),
        sections_.end(),
        [](const EditorMenuSectionDescriptor& lhs, const EditorMenuSectionDescriptor& rhs) {
            return lhs.sortOrder < rhs.sortOrder;
        });
    Touch();
    return true;
}

bool EditorMenuRegistry::RegisterItem(
    EditorMenuItemDescriptor descriptor,
    std::vector<EditorToolRegistrationDiagnostic>* diagnostics) {
    if (!FeatureAllowsRegistration(descriptor.feature)) {
        return true;
    }
    descriptor.visible = descriptor.visible && FeatureAllowsPresentation(descriptor.feature);
    if (!IsSupportedVersionValue(descriptor.version)) {
        AddDiagnosticTo(
            diagnostics,
            EditorToolDiagnosticSeverity::Error,
            EditorToolDescriptorKind::MenuItem,
            descriptor.id,
            "Unsupported menu item descriptor version.");
        return false;
    }
    if (descriptor.id.empty() || descriptor.sectionId.empty() || descriptor.commandId.empty()) {
        AddDiagnosticTo(
            diagnostics,
            EditorToolDiagnosticSeverity::Error,
            EditorToolDescriptorKind::MenuItem,
            descriptor.id,
            "Menu item descriptor requires an id, section id, and command id.");
        return false;
    }
    if (descriptor.label.empty()) {
        descriptor.label = descriptor.commandId;
    }

    const auto found = std::find_if(
        items_.begin(),
        items_.end(),
        [&descriptor](const EditorMenuItemDescriptor& current) {
            return current.id == descriptor.id;
        });
    if (found != items_.end()) {
        if (!descriptor.allowReplace) {
            AddDiagnosticTo(
                diagnostics,
                EditorToolDiagnosticSeverity::Error,
                EditorToolDescriptorKind::MenuItem,
                descriptor.id,
                "Duplicate menu item descriptor id.");
            return false;
        }
        *found = std::move(descriptor);
        std::stable_sort(
            items_.begin(),
            items_.end(),
            [](const EditorMenuItemDescriptor& lhs, const EditorMenuItemDescriptor& rhs) {
                if (lhs.sectionId != rhs.sectionId) {
                    return lhs.sectionId < rhs.sectionId;
                }
                return lhs.sortOrder < rhs.sortOrder;
            });
        Touch();
        return true;
    }

    items_.push_back(std::move(descriptor));
    std::stable_sort(
        items_.begin(),
        items_.end(),
        [](const EditorMenuItemDescriptor& lhs, const EditorMenuItemDescriptor& rhs) {
            if (lhs.sectionId != rhs.sectionId) {
                return lhs.sectionId < rhs.sectionId;
            }
            return lhs.sortOrder < rhs.sortOrder;
        });
    Touch();
    return true;
}

std::vector<const EditorMenuSectionDescriptor*> EditorMenuRegistry::VisibleSections() const {
    std::vector<const EditorMenuSectionDescriptor*> result;
    for (const EditorMenuSectionDescriptor& section : sections_) {
        if (section.visible) {
            result.push_back(&section);
        }
    }
    return result;
}

std::vector<const EditorMenuItemDescriptor*> EditorMenuRegistry::VisibleItems(std::string_view sectionId) const {
    std::vector<const EditorMenuItemDescriptor*> result;
    for (const EditorMenuItemDescriptor& item : items_) {
        if (item.visible && item.sectionId == sectionId) {
            result.push_back(&item);
        }
    }
    return result;
}

void EditorMenuRegistry::Touch() {
    ++revision_;
}

void EditorToolModuleRegistry::Clear() {
    modules_.clear();
    diagnostics_.clear();
}

bool EditorToolModuleRegistry::Register(
    EditorToolModuleRegistration registration,
    EditorToolRegistry* diagnostics) {
    if (!FeatureAllowsRegistration(registration.descriptor.feature)) {
        return true;
    }
    if (!IsSupportedVersionValue(registration.descriptor.version)) {
        AddDiagnostic(
            EditorToolDiagnosticSeverity::Error,
            EditorToolDescriptorKind::Module,
            registration.descriptor.id,
            "Unsupported editor tool module descriptor version.",
            diagnostics);
        return false;
    }
    if (registration.descriptor.id.empty() || registration.descriptor.label.empty()) {
        AddDiagnostic(
            EditorToolDiagnosticSeverity::Error,
            EditorToolDescriptorKind::Module,
            registration.descriptor.id,
            "Tool module descriptor requires an id and label.",
            diagnostics);
        return false;
    }
    if (!registration.registerFrame) {
        AddDiagnostic(
            EditorToolDiagnosticSeverity::Error,
            EditorToolDescriptorKind::Module,
            registration.descriptor.id,
            "Tool module descriptor requires a frame registration callback.",
            diagnostics);
        return false;
    }

    const auto found = std::find_if(
        modules_.begin(),
        modules_.end(),
        [&registration](const EditorToolModuleRegistration& current) {
            return current.descriptor.id == registration.descriptor.id;
        });
    if (found != modules_.end()) {
        if (!registration.descriptor.allowReplace) {
            AddDiagnostic(
                EditorToolDiagnosticSeverity::Error,
                EditorToolDescriptorKind::Module,
                registration.descriptor.id,
                "Duplicate editor tool module id.",
                diagnostics);
            return false;
        }
        *found = std::move(registration);
    } else {
        modules_.push_back(std::move(registration));
    }
    std::stable_sort(
        modules_.begin(),
        modules_.end(),
        [](const EditorToolModuleRegistration& lhs, const EditorToolModuleRegistration& rhs) {
            return lhs.descriptor.loadOrder < rhs.descriptor.loadOrder;
        });
    return true;
}

void EditorToolModuleRegistry::RunStartupRegistrations(EditorToolModuleRegistrationContext context) {
    RunFrameRegistrations(context);
}

void EditorToolModuleRegistry::RunFrameRegistrations(EditorToolModuleRegistrationContext context) {
    for (EditorToolModuleRegistration& registration : modules_) {
        if (!registration.registerFrame) {
            continue;
        }
        try {
            registration.registerFrame(context);
        } catch (const std::exception& exception) {
            AddDiagnostic(
                EditorToolDiagnosticSeverity::Error,
                EditorToolDescriptorKind::Module,
                registration.descriptor.id,
                std::string("Tool module registration failed: ") + exception.what(),
                context.tools);
        } catch (...) {
            AddDiagnostic(
                EditorToolDiagnosticSeverity::Error,
                EditorToolDescriptorKind::Module,
                registration.descriptor.id,
                "Tool module registration failed with an unknown exception.",
                context.tools);
        }
    }
}

uint32_t EditorToolModuleRegistry::ErrorCount() const {
    uint32_t count = 0;
    for (const EditorToolRegistrationDiagnostic& diagnostic : diagnostics_) {
        if (diagnostic.severity == EditorToolDiagnosticSeverity::Error) {
            ++count;
        }
    }
    return count;
}

uint32_t EditorToolModuleRegistry::WarningCount() const {
    uint32_t count = 0;
    for (const EditorToolRegistrationDiagnostic& diagnostic : diagnostics_) {
        if (diagnostic.severity == EditorToolDiagnosticSeverity::Warning) {
            ++count;
        }
    }
    return count;
}

void EditorToolModuleRegistry::AddDiagnostic(
    EditorToolDiagnosticSeverity severity,
    EditorToolDescriptorKind kind,
    std::string_view id,
    std::string message,
    EditorToolRegistry* sink) {
    diagnostics_.push_back(
        EditorToolRegistrationDiagnostic{
            severity,
            kind,
            std::string(id),
            message});
    if (sink != nullptr) {
        sink->RecordDiagnostic(severity, kind, id, std::move(message));
    }
}

void EditorToolRegistry::BeginFrame() {
    diagnostics_.clear();
    toolbar_.Clear();
    menu_.Clear();
    assetProviders_.clear();
    propertyAccessors_.clear();
    validationAdapters_.clear();
    runtimeWatchProviders_.clear();
}

bool EditorToolRegistry::RegisterCommand(
    EditorCommandRegistrationDescriptor descriptor,
    EditorCommandRegistry& registry) {
    const std::string id = DescriptorId(descriptor.toolId, descriptor.command.id);
    if (!FeatureAllowsRegistration(descriptor.feature)) {
        return true;
    }
    if (!IsSupportedVersion(descriptor.version, EditorToolDescriptorKind::Command, id)) {
        return false;
    }
    if (descriptor.command.id.empty() || descriptor.command.displayName.empty()) {
        AddDiagnostic(
            EditorToolDiagnosticSeverity::Error,
            EditorToolDescriptorKind::Command,
            id,
            "Command descriptor requires a command id and display name.");
        return false;
    }
    if (!descriptor.allowReplace && registry.Find(descriptor.command.id) != nullptr) {
        AddDiagnostic(
            EditorToolDiagnosticSeverity::Error,
            EditorToolDescriptorKind::Command,
            descriptor.command.id,
            "Duplicate command descriptor id.");
        return false;
    }
    if (!descriptor.command.shortcut.empty()) {
        for (const EditorCommand& current : registry.Commands()) {
            if (current.id != descriptor.command.id &&
                current.shortcut == descriptor.command.shortcut) {
                AddDiagnostic(
                    EditorToolDiagnosticSeverity::Warning,
                    EditorToolDescriptorKind::Command,
                    descriptor.command.id,
                    "Command shortcut conflicts with " + current.id + ".");
            }
        }
    }
    return registry.Register(std::move(descriptor.command));
}

bool EditorToolRegistry::RegisterPanel(
    EditorPanelRegistrationDescriptor descriptor,
    EditorPanelRegistry& registry) {
    const std::string id = DescriptorId(descriptor.toolId, descriptor.panel.id);
    if (!FeatureAllowsRegistration(descriptor.feature)) {
        return true;
    }
    descriptor.panel.visible = descriptor.panel.visible && FeatureAllowsPresentation(descriptor.feature);
    if (!IsSupportedVersion(descriptor.version, EditorToolDescriptorKind::Panel, id)) {
        return false;
    }
    if (descriptor.panel.id.empty() || descriptor.panel.label.empty() || !descriptor.panel.draw) {
        AddDiagnostic(
            EditorToolDiagnosticSeverity::Error,
            EditorToolDescriptorKind::Panel,
            id,
            "Panel descriptor requires an id, label, and draw callback.");
        return false;
    }
    if (!descriptor.allowReplace) {
        for (const EditorPanelDescriptor& current : registry.AllPanels()) {
            if (current.id == descriptor.panel.id) {
                AddDiagnostic(
                    EditorToolDiagnosticSeverity::Error,
                    EditorToolDescriptorKind::Panel,
                    descriptor.panel.id,
                    "Duplicate panel descriptor id.");
                return false;
            }
        }
    }
    return registry.Register(std::move(descriptor.panel));
}

bool EditorToolRegistry::RegisterToolbarItem(EditorToolbarItemDescriptor descriptor) {
    return toolbar_.Register(std::move(descriptor), &diagnostics_);
}

bool EditorToolRegistry::RegisterMenuSection(EditorMenuSectionDescriptor descriptor) {
    return menu_.RegisterSection(std::move(descriptor), &diagnostics_);
}

bool EditorToolRegistry::RegisterMenuItem(EditorMenuItemDescriptor descriptor) {
    return menu_.RegisterItem(std::move(descriptor), &diagnostics_);
}

bool EditorToolRegistry::RegisterAssetProvider(EditorAssetProviderDescriptor descriptor) {
    const std::string id = descriptor.id;
    if (!FeatureAllowsRegistration(descriptor.feature)) {
        return true;
    }
    if (!IsSupportedVersion(descriptor.version, EditorToolDescriptorKind::AssetProvider, id)) {
        return false;
    }
    if (descriptor.id.empty() || descriptor.label.empty()) {
        AddDiagnostic(
            EditorToolDiagnosticSeverity::Error,
            EditorToolDescriptorKind::AssetProvider,
            id,
            "Asset provider descriptor requires an id and label.");
        return false;
    }
    const auto found = std::find_if(
        assetProviders_.begin(),
        assetProviders_.end(),
        [&descriptor](const EditorAssetProviderDescriptor& current) {
            return current.id == descriptor.id;
        });
    if (found != assetProviders_.end()) {
        if (!descriptor.allowReplace) {
            AddDiagnostic(
                EditorToolDiagnosticSeverity::Error,
                EditorToolDescriptorKind::AssetProvider,
                descriptor.id,
                "Duplicate asset provider descriptor id.");
            return false;
        }
        *found = std::move(descriptor);
        return true;
    }
    assetProviders_.push_back(std::move(descriptor));
    return true;
}

bool EditorToolRegistry::RegisterPropertyAccessor(
    EditorPropertyAccessorRegistrationDescriptor descriptor,
    EditorCompositePropertyAccessor& composite) {
    const std::string id = descriptor.id;
    if (!FeatureAllowsRegistration(descriptor.feature)) {
        return true;
    }
    if (!IsSupportedVersion(descriptor.version, EditorToolDescriptorKind::PropertyAccessor, id)) {
        return false;
    }
    if (descriptor.id.empty() || descriptor.accessor == nullptr) {
        AddDiagnostic(
            EditorToolDiagnosticSeverity::Error,
            EditorToolDescriptorKind::PropertyAccessor,
            id,
            "Property accessor descriptor requires an id and accessor.");
        return false;
    }
    const auto found = std::find_if(
        propertyAccessors_.begin(),
        propertyAccessors_.end(),
        [&descriptor](const EditorPropertyAccessorRegistrationDescriptor& current) {
            return current.id == descriptor.id;
        });
    if (found != propertyAccessors_.end()) {
        if (!descriptor.allowReplace) {
            AddDiagnostic(
                EditorToolDiagnosticSeverity::Error,
                EditorToolDescriptorKind::PropertyAccessor,
                descriptor.id,
                "Duplicate property accessor descriptor id.");
            return false;
        }
        *found = descriptor;
        composite.Add(descriptor.accessor);
        return true;
    }
    propertyAccessors_.push_back(descriptor);
    std::stable_sort(
        propertyAccessors_.begin(),
        propertyAccessors_.end(),
        [](const EditorPropertyAccessorRegistrationDescriptor& lhs,
           const EditorPropertyAccessorRegistrationDescriptor& rhs) {
            return lhs.priority < rhs.priority;
        });
    composite.Add(descriptor.accessor);
    return true;
}

bool EditorToolRegistry::RegisterValidationAdapter(
    EditorValidationAdapterRegistrationDescriptor descriptor,
    EditorValidationService& service) {
    const std::string id = descriptor.id;
    if (!FeatureAllowsRegistration(descriptor.feature)) {
        return true;
    }
    if (!IsSupportedVersion(descriptor.version, EditorToolDescriptorKind::ValidationAdapter, id)) {
        return false;
    }
    if (descriptor.id.empty() || descriptor.adapter == nullptr) {
        AddDiagnostic(
            EditorToolDiagnosticSeverity::Error,
            EditorToolDescriptorKind::ValidationAdapter,
            id,
            "Validation adapter descriptor requires an id and adapter.");
        return false;
    }
    const auto found = std::find_if(
        validationAdapters_.begin(),
        validationAdapters_.end(),
        [&descriptor](const EditorValidationAdapterRegistrationDescriptor& current) {
            return current.id == descriptor.id;
        });
    if (found != validationAdapters_.end()) {
        if (!descriptor.allowReplace) {
            AddDiagnostic(
                EditorToolDiagnosticSeverity::Error,
                EditorToolDescriptorKind::ValidationAdapter,
                descriptor.id,
                "Duplicate validation adapter descriptor id.");
            return false;
        }
        *found = descriptor;
        service.AddAdapter(descriptor.adapter);
        return true;
    }
    validationAdapters_.push_back(descriptor);
    std::stable_sort(
        validationAdapters_.begin(),
        validationAdapters_.end(),
        [](const EditorValidationAdapterRegistrationDescriptor& lhs,
           const EditorValidationAdapterRegistrationDescriptor& rhs) {
            return lhs.priority < rhs.priority;
        });
    service.AddAdapter(descriptor.adapter);
    return true;
}

bool EditorToolRegistry::RegisterRuntimeWatchProvider(EditorRuntimeWatchProviderDescriptor descriptor) {
    const std::string id = descriptor.id;
    if (!FeatureAllowsRegistration(descriptor.feature)) {
        return true;
    }
    if (!IsSupportedVersion(descriptor.version, EditorToolDescriptorKind::RuntimeWatchProvider, id)) {
        return false;
    }
    if (descriptor.id.empty() || !descriptor.build) {
        AddDiagnostic(
            EditorToolDiagnosticSeverity::Error,
            EditorToolDescriptorKind::RuntimeWatchProvider,
            id,
            "Runtime Watch provider descriptor requires an id and build callback.");
        return false;
    }
    const auto found = std::find_if(
        runtimeWatchProviders_.begin(),
        runtimeWatchProviders_.end(),
        [&descriptor](const EditorRuntimeWatchProviderDescriptor& current) {
            return current.id == descriptor.id;
        });
    if (found != runtimeWatchProviders_.end()) {
        if (!descriptor.allowReplace) {
            AddDiagnostic(
                EditorToolDiagnosticSeverity::Error,
                EditorToolDescriptorKind::RuntimeWatchProvider,
                descriptor.id,
                "Duplicate Runtime Watch provider descriptor id.");
            return false;
        }
        *found = std::move(descriptor);
        return true;
    }
    runtimeWatchProviders_.push_back(std::move(descriptor));
    std::stable_sort(
        runtimeWatchProviders_.begin(),
        runtimeWatchProviders_.end(),
        [](const EditorRuntimeWatchProviderDescriptor& lhs,
           const EditorRuntimeWatchProviderDescriptor& rhs) {
            return lhs.priority < rhs.priority;
        });
    return true;
}

bool EditorToolRegistry::ValidateToolbarCommands(const EditorCommandRegistry& registry) {
    bool valid = true;
    for (const EditorToolbarItemDescriptor& item : toolbar_.Items()) {
        if (!item.visible) {
            continue;
        }
        if (registry.Find(item.commandId) == nullptr) {
            AddDiagnostic(
                EditorToolDiagnosticSeverity::Error,
                EditorToolDescriptorKind::ToolbarItem,
                item.id,
                "Toolbar item references an unknown command: " + item.commandId + ".");
            valid = false;
        }
    }
    return valid;
}

bool EditorToolRegistry::ValidateMenuCommands(const EditorCommandRegistry& registry) {
    bool valid = true;
    for (const EditorMenuItemDescriptor& item : menu_.Items()) {
        if (!item.visible) {
            continue;
        }
        if (registry.Find(item.commandId) == nullptr) {
            AddDiagnostic(
                EditorToolDiagnosticSeverity::Error,
                EditorToolDescriptorKind::MenuItem,
                item.id,
                "Menu item references an unknown command: " + item.commandId + ".");
            valid = false;
        }
    }
    for (const EditorMenuItemDescriptor& item : menu_.Items()) {
        if (!item.visible) {
            continue;
        }
        const auto found = std::find_if(
            menu_.Sections().begin(),
            menu_.Sections().end(),
            [&item](const EditorMenuSectionDescriptor& section) {
                return section.visible && section.id == item.sectionId;
            });
        if (found == menu_.Sections().end()) {
            AddDiagnostic(
                EditorToolDiagnosticSeverity::Error,
                EditorToolDescriptorKind::MenuItem,
                item.id,
                "Menu item references an unknown or hidden section: " + item.sectionId + ".");
            valid = false;
        }
    }
    return valid;
}

void EditorToolRegistry::BuildRuntimeWatch(const EditorRuntimeWatchBuildInput& input) const {
    if (input.inspector == nullptr) {
        return;
    }
    input.inspector->Clear();
    for (const EditorRuntimeWatchProviderDescriptor& provider : runtimeWatchProviders_) {
        if (provider.build) {
            provider.build(input);
        }
    }
}

void EditorToolRegistry::RecordDiagnostic(
    EditorToolDiagnosticSeverity severity,
    EditorToolDescriptorKind kind,
    std::string_view id,
    std::string message) {
    AddDiagnostic(severity, kind, id, std::move(message));
}

uint32_t EditorToolRegistry::ErrorCount() const {
    uint32_t count = 0;
    for (const EditorToolRegistrationDiagnostic& diagnostic : diagnostics_) {
        if (diagnostic.severity == EditorToolDiagnosticSeverity::Error) {
            ++count;
        }
    }
    return count;
}

uint32_t EditorToolRegistry::WarningCount() const {
    uint32_t count = 0;
    for (const EditorToolRegistrationDiagnostic& diagnostic : diagnostics_) {
        if (diagnostic.severity == EditorToolDiagnosticSeverity::Warning) {
            ++count;
        }
    }
    return count;
}

bool EditorToolRegistry::IsSupportedVersion(
    EditorToolDescriptorVersion version,
    EditorToolDescriptorKind kind,
    std::string_view id) {
    if (IsSupportedVersionValue(version)) {
        return true;
    }
    AddDiagnostic(
        EditorToolDiagnosticSeverity::Error,
        kind,
        id,
        "Unsupported editor tool descriptor version.");
    return false;
}

void EditorToolRegistry::AddDiagnostic(
    EditorToolDiagnosticSeverity severity,
    EditorToolDescriptorKind kind,
    std::string_view id,
    std::string message) {
    diagnostics_.push_back(
        EditorToolRegistrationDiagnostic{
            severity,
            kind,
            std::string(id),
            std::move(message)});
}

bool RegisterEditorToolCommand(EditorContext& context, EditorCommand command, std::string_view toolId) {
    if (context.commands == nullptr) {
        return false;
    }
    if (context.tools != nullptr) {
        return context.tools->RegisterCommand(
            EditorCommandRegistrationDescriptor{
                {},
                DescriptorId(toolId, command.id),
                std::move(command),
                true,
                {}},
            *context.commands);
    }
    return context.commands->Register(std::move(command));
}

const char* ToString(EditorToolDescriptorKind kind) {
    switch (kind) {
    case EditorToolDescriptorKind::Command:
        return "Command";
    case EditorToolDescriptorKind::Panel:
        return "Panel";
    case EditorToolDescriptorKind::ToolbarItem:
        return "ToolbarItem";
    case EditorToolDescriptorKind::MenuSection:
        return "MenuSection";
    case EditorToolDescriptorKind::MenuItem:
        return "MenuItem";
    case EditorToolDescriptorKind::AssetProvider:
        return "AssetProvider";
    case EditorToolDescriptorKind::PropertyAccessor:
        return "PropertyAccessor";
    case EditorToolDescriptorKind::ValidationAdapter:
        return "ValidationAdapter";
    case EditorToolDescriptorKind::RuntimeWatchProvider:
        return "RuntimeWatchProvider";
    case EditorToolDescriptorKind::Module:
        return "Module";
    }
    return "Unknown";
}

const char* ToString(EditorToolDiagnosticSeverity severity) {
    switch (severity) {
    case EditorToolDiagnosticSeverity::Info:
        return "Info";
    case EditorToolDiagnosticSeverity::Warning:
        return "Warning";
    case EditorToolDiagnosticSeverity::Error:
        return "Error";
    }
    return "Unknown";
}

const char* ToString(EditorToolFeatureState state) {
    switch (state) {
    case EditorToolFeatureState::Production:
        return "Production";
    case EditorToolFeatureState::Experimental:
        return "Experimental";
    case EditorToolFeatureState::Hidden:
        return "Hidden";
    case EditorToolFeatureState::Disabled:
        return "Disabled";
    }
    return "Unknown";
}

void RegisterDefaultEditorRuntimeWatchProvider(EditorToolRegistry& registry) {
    registry.RegisterRuntimeWatchProvider(
        EditorRuntimeWatchProviderDescriptor{
            {},
            "editor.runtimeWatch.default",
            "Default Runtime Watch",
            0,
            [](const EditorRuntimeWatchBuildInput& input) {
                AppendDefaultEditorRuntimeWatch(input);
            },
            true,
            {}});
}

} // namespace editor

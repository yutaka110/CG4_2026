#pragma once

#include "EditorCommandRegistry.h"
#include "EditorPanelRegistry.h"
#include "EditorCompositePropertyAccessor.h"
#include "EditorRuntimeWatchBuilder.h"
#include "EditorValidationService.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

struct EditorContext;
class EditorDetailsSectionProviderRegistry;
class EditorPropertyRegistry;
class EditorToolRegistry;

struct EditorToolDescriptorVersion {
    uint16_t major = 1;
    uint16_t minor = 0;
};

enum class EditorToolDescriptorKind {
    Command,
    Panel,
    ToolbarItem,
    MenuSection,
    MenuItem,
    AssetProvider,
    PropertyAccessor,
    ValidationAdapter,
    RuntimeWatchProvider,
    Module,
};

enum class EditorToolDiagnosticSeverity {
    Info,
    Warning,
    Error,
};

enum class EditorToolFeatureState {
    Production,
    Experimental,
    Hidden,
    Disabled,
};

struct EditorToolFeatureGate {
    EditorToolFeatureState state = EditorToolFeatureState::Production;
    std::string flag;
    bool enabled = true;
};

struct EditorToolRegistrationDiagnostic {
    EditorToolDiagnosticSeverity severity = EditorToolDiagnosticSeverity::Info;
    EditorToolDescriptorKind kind = EditorToolDescriptorKind::Command;
    std::string id;
    std::string message;
};

struct EditorCommandRegistrationDescriptor {
    EditorToolDescriptorVersion version{};
    std::string toolId;
    EditorCommand command;
    bool allowReplace = true;
    EditorToolFeatureGate feature{};
};

struct EditorPanelRegistrationDescriptor {
    EditorToolDescriptorVersion version{};
    std::string toolId;
    EditorPanelDescriptor panel;
    bool allowReplace = true;
    EditorToolFeatureGate feature{};
};

struct EditorToolbarItemDescriptor {
    EditorToolDescriptorVersion version{};
    std::string id;
    std::string commandId;
    std::string label;
    int sortOrder = 0;
    bool separatorAfter = false;
    bool visible = true;
    bool allowReplace = true;
    EditorToolFeatureGate feature{};
    std::string contextualDocumentType;
    std::string compactLabel;
    bool requiresCoursePreview = false;
};

struct EditorMenuSectionDescriptor {
    EditorToolDescriptorVersion version{};
    std::string id;
    std::string label;
    int sortOrder = 0;
    bool visible = true;
    bool allowReplace = true;
    EditorToolFeatureGate feature{};
};

struct EditorMenuItemDescriptor {
    EditorToolDescriptorVersion version{};
    std::string id;
    std::string sectionId;
    std::string commandId;
    std::string label;
    int sortOrder = 0;
    bool separatorAfter = false;
    bool visible = true;
    bool allowReplace = true;
    EditorToolFeatureGate feature{};
    std::string contextualDocumentType;
};

struct EditorAssetProviderDescriptor {
    EditorToolDescriptorVersion version{};
    std::string id;
    std::string label;
    std::string category;
    bool allowReplace = true;
    EditorToolFeatureGate feature{};
};

struct EditorPropertyAccessorRegistrationDescriptor {
    EditorToolDescriptorVersion version{};
    std::string id;
    EditorPropertyAccessor* accessor = nullptr;
    int priority = 0;
    bool allowReplace = true;
    EditorToolFeatureGate feature{};
};

struct EditorValidationAdapterRegistrationDescriptor {
    EditorToolDescriptorVersion version{};
    std::string id;
    const EditorValidationAdapter* adapter = nullptr;
    int priority = 0;
    bool allowReplace = true;
    EditorToolFeatureGate feature{};
};

struct EditorRuntimeWatchProviderDescriptor {
    EditorToolDescriptorVersion version{};
    std::string id;
    std::string label;
    int priority = 0;
    std::function<void(const EditorRuntimeWatchBuildInput&)> build;
    bool allowReplace = true;
    EditorToolFeatureGate feature{};
};

struct EditorToolModuleDescriptor {
    EditorToolDescriptorVersion version{};
    std::string id;
    std::string label;
    int loadOrder = 0;
    bool allowReplace = true;
    EditorToolFeatureGate feature{};
};

struct EditorToolModuleRegistrationContext {
    EditorToolRegistry* tools = nullptr;
    EditorCommandRegistry* commands = nullptr;
    EditorPanelRegistry* panels = nullptr;
    EditorCompositePropertyAccessor* propertyAccessors = nullptr;
    EditorCompositePropertyAccessor* previewPropertyAccessors = nullptr;
    EditorValidationService* validation = nullptr;
    EditorPropertyRegistry* propertyRegistry = nullptr;
    EditorDetailsSectionProviderRegistry* detailsSectionProviders = nullptr;
};

struct EditorToolModuleRegistration {
    EditorToolModuleDescriptor descriptor;
    std::function<void(EditorToolModuleRegistrationContext&)> registerFrame;
};

class EditorToolbarRegistry {
public:
    void Clear();
    bool Register(EditorToolbarItemDescriptor descriptor, std::vector<EditorToolRegistrationDiagnostic>* diagnostics);

    const std::vector<EditorToolbarItemDescriptor>& Items() const { return items_; }
    std::vector<const EditorToolbarItemDescriptor*> VisibleItems() const;
    std::size_t Count() const { return items_.size(); }
    uint32_t Revision() const { return revision_; }

private:
    void Touch();

    std::vector<EditorToolbarItemDescriptor> items_;
    uint32_t revision_ = 0;
};

class EditorMenuRegistry {
public:
    void Clear();
    bool RegisterSection(EditorMenuSectionDescriptor descriptor, std::vector<EditorToolRegistrationDiagnostic>* diagnostics);
    bool RegisterItem(EditorMenuItemDescriptor descriptor, std::vector<EditorToolRegistrationDiagnostic>* diagnostics);

    const std::vector<EditorMenuSectionDescriptor>& Sections() const { return sections_; }
    const std::vector<EditorMenuItemDescriptor>& Items() const { return items_; }
    std::vector<const EditorMenuSectionDescriptor*> VisibleSections() const;
    std::vector<const EditorMenuItemDescriptor*> VisibleItems(std::string_view sectionId) const;
    std::size_t SectionCount() const { return sections_.size(); }
    std::size_t ItemCount() const { return items_.size(); }
    uint32_t Revision() const { return revision_; }

private:
    void Touch();

    std::vector<EditorMenuSectionDescriptor> sections_;
    std::vector<EditorMenuItemDescriptor> items_;
    uint32_t revision_ = 0;
};

class EditorToolModuleRegistry {
public:
    void Clear();
    bool Register(EditorToolModuleRegistration registration, EditorToolRegistry* diagnostics = nullptr);
    void RunStartupRegistrations(EditorToolModuleRegistrationContext context);
    void RunFrameRegistrations(EditorToolModuleRegistrationContext context);

    const std::vector<EditorToolModuleRegistration>& Modules() const { return modules_; }
    const std::vector<EditorToolRegistrationDiagnostic>& Diagnostics() const { return diagnostics_; }
    std::size_t Count() const { return modules_.size(); }
    uint32_t ErrorCount() const;
    uint32_t WarningCount() const;

private:
    void AddDiagnostic(
        EditorToolDiagnosticSeverity severity,
        EditorToolDescriptorKind kind,
        std::string_view id,
        std::string message,
        EditorToolRegistry* sink);

    std::vector<EditorToolModuleRegistration> modules_;
    std::vector<EditorToolRegistrationDiagnostic> diagnostics_;
};

class EditorToolRegistry {
public:
    void BeginFrame();

    bool RegisterCommand(EditorCommandRegistrationDescriptor descriptor, EditorCommandRegistry& registry);
    bool RegisterPanel(EditorPanelRegistrationDescriptor descriptor, EditorPanelRegistry& registry);
    bool RegisterToolbarItem(EditorToolbarItemDescriptor descriptor);
    bool RegisterMenuSection(EditorMenuSectionDescriptor descriptor);
    bool RegisterMenuItem(EditorMenuItemDescriptor descriptor);
    bool RegisterAssetProvider(EditorAssetProviderDescriptor descriptor);
    bool RegisterPropertyAccessor(
        EditorPropertyAccessorRegistrationDescriptor descriptor,
        EditorCompositePropertyAccessor& composite);
    bool RegisterValidationAdapter(
        EditorValidationAdapterRegistrationDescriptor descriptor,
        EditorValidationService& service);
    bool RegisterRuntimeWatchProvider(EditorRuntimeWatchProviderDescriptor descriptor);
    bool ValidateToolbarCommands(const EditorCommandRegistry& registry);
    bool ValidateMenuCommands(const EditorCommandRegistry& registry);
    void BuildRuntimeWatch(const EditorRuntimeWatchBuildInput& input) const;
    void RecordDiagnostic(
        EditorToolDiagnosticSeverity severity,
        EditorToolDescriptorKind kind,
        std::string_view id,
        std::string message);

    EditorToolbarRegistry& Toolbar() { return toolbar_; }
    const EditorToolbarRegistry& Toolbar() const { return toolbar_; }
    EditorMenuRegistry& Menu() { return menu_; }
    const EditorMenuRegistry& Menu() const { return menu_; }
    const std::vector<EditorAssetProviderDescriptor>& AssetProviders() const { return assetProviders_; }
    const std::vector<EditorPropertyAccessorRegistrationDescriptor>& PropertyAccessors() const { return propertyAccessors_; }
    const std::vector<EditorValidationAdapterRegistrationDescriptor>& ValidationAdapters() const { return validationAdapters_; }
    const std::vector<EditorRuntimeWatchProviderDescriptor>& RuntimeWatchProviders() const { return runtimeWatchProviders_; }

    const std::vector<EditorToolRegistrationDiagnostic>& Diagnostics() const { return diagnostics_; }
    uint32_t ErrorCount() const;
    uint32_t WarningCount() const;

private:
    bool IsSupportedVersion(
        EditorToolDescriptorVersion version,
        EditorToolDescriptorKind kind,
        std::string_view id);
    void AddDiagnostic(
        EditorToolDiagnosticSeverity severity,
        EditorToolDescriptorKind kind,
        std::string_view id,
        std::string message);

    EditorToolbarRegistry toolbar_{};
    EditorMenuRegistry menu_{};
    std::vector<EditorAssetProviderDescriptor> assetProviders_;
    std::vector<EditorPropertyAccessorRegistrationDescriptor> propertyAccessors_;
    std::vector<EditorValidationAdapterRegistrationDescriptor> validationAdapters_;
    std::vector<EditorRuntimeWatchProviderDescriptor> runtimeWatchProviders_;
    std::vector<EditorToolRegistrationDiagnostic> diagnostics_;
};

bool RegisterEditorToolCommand(EditorContext& context, EditorCommand command, std::string_view toolId = {});
void RegisterDefaultEditorRuntimeWatchProvider(EditorToolRegistry& registry);

const char* ToString(EditorToolDescriptorKind kind);
const char* ToString(EditorToolDiagnosticSeverity severity);
const char* ToString(EditorToolFeatureState state);

} // namespace editor

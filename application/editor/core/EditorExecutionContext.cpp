#include "EditorExecutionContext.h"

#include <algorithm>

namespace editor {

bool EditorExecutionContext::Register(
    IEditorExecutionService& service,
    EditorError* error) {
    ClearEditorError(error);
    const std::string_view serviceId = service.ServiceId();
    if (serviceId.empty()) {
        SetEditorError(
            error,
            EditorErrorCode::InvalidArgument,
            "Editor execution service id is empty.");
        return false;
    }
    if (Find(serviceId) != nullptr) {
        SetEditorError(
            error,
            EditorErrorCode::InvalidArgument,
            "Editor execution service is already registered.");
        return false;
    }
    services_.push_back(&service);
    return true;
}

void EditorExecutionContext::Unregister(std::string_view serviceId) {
    services_.erase(
        std::remove_if(
            services_.begin(),
            services_.end(),
            [&](const IEditorExecutionService* service) {
                return service == nullptr || service->ServiceId() == serviceId;
            }),
        services_.end());
}

void EditorExecutionContext::Clear() {
    services_.clear();
}

IEditorExecutionService* EditorExecutionContext::Find(
    std::string_view serviceId) const noexcept {
    const auto it = std::find_if(
        services_.begin(),
        services_.end(),
        [&](const IEditorExecutionService* service) {
            return service != nullptr && service->ServiceId() == serviceId;
        });
    return it != services_.end() ? *it : nullptr;
}

} // namespace editor

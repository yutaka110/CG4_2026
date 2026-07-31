#include "EditorGimmickRuntimeBehavior.h"

#include "EditorGimmickRuntimeFactory.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>
#include <utility>

namespace editor {
namespace {

void SetError(
    std::string* errorMessage,
    std::string message) {
    if (errorMessage != nullptr) {
        *errorMessage = std::move(message);
    }
}

bool ParseBoolean(
    const EditorGimmickRuntimeInstance& instance,
    std::string_view parameterId,
    bool& output,
    std::string* errorMessage) {
    const EditorGimmickParameterValue* parameter =
        instance.FindParameter(parameterId);
    if (parameter == nullptr) {
        SetError(
            errorMessage,
            "Runtime Gimmick Boolean parameter is missing: " +
                std::string(parameterId));
        return false;
    }
    if (parameter->value == "true" ||
        parameter->value == "1") {
        output = true;
    } else if (parameter->value == "false" ||
        parameter->value == "0") {
        output = false;
    } else {
        SetError(
            errorMessage,
            "Runtime Gimmick Boolean parameter is malformed: " +
                std::string(parameterId));
        return false;
    }
    return true;
}

bool ParseFloat(
    const EditorGimmickRuntimeInstance& instance,
    std::string_view parameterId,
    float& output,
    std::string* errorMessage) {
    const EditorGimmickParameterValue* parameter =
        instance.FindParameter(parameterId);
    if (parameter == nullptr) {
        SetError(
            errorMessage,
            "Runtime Gimmick Float parameter is missing: " +
                std::string(parameterId));
        return false;
    }
    std::istringstream input(parameter->value);
    input.imbue(std::locale::classic());
    double value = 0.0;
    if (!(input >> value) || !std::isfinite(value)) {
        SetError(
            errorMessage,
            "Runtime Gimmick Float parameter is malformed: " +
                std::string(parameterId));
        return false;
    }
    input >> std::ws;
    if (!input.eof() ||
        value < 0.0 ||
        value > static_cast<double>(
            (std::numeric_limits<float>::max)())) {
        SetError(
            errorMessage,
            "Runtime Gimmick Float parameter is out of range: " +
                std::string(parameterId));
        return false;
    }
    output = static_cast<float>(value);
    return true;
}

std::string ReferenceGuid(
    const EditorGimmickRuntimeInstance& instance,
    std::string_view parameterId) {
    const EditorGimmickEntityReferenceValue* reference =
        instance.FindEntityReference(parameterId);
    return reference != nullptr
        ? reference->entityGuid
        : std::string{};
}

class EditorPassiveGimmickRuntimeBehavior final
    : public IEditorGimmickRuntimeBehavior {
public:
    std::string_view BehaviorId() const noexcept override {
        return "behavior.gimmick.passive";
    }
    std::unique_ptr<IEditorGimmickRuntimeBehavior>
    Clone() const override {
        return std::make_unique<
            EditorPassiveGimmickRuntimeBehavior>(*this);
    }
    bool Initialize(
        EditorGimmickRuntimeInstance&,
        std::string* errorMessage) override {
        if (errorMessage != nullptr) errorMessage->clear();
        return true;
    }
    bool Reconcile(
        EditorGimmickRuntimeInstance&,
        std::string* errorMessage) override {
        if (errorMessage != nullptr) errorMessage->clear();
        return true;
    }
    void HandleCommand(
        const EditorGimmickRuntimeCommand& command,
        EditorGimmickRuntimeInstance& instance,
        EditorGimmickRuntimeCommandQueue&) override {
        switch (command.kind) {
        case EditorGimmickRuntimeCommandKind::Activate:
        case EditorGimmickRuntimeCommandKind::Toggle:
            if (instance.lifecycle.Activate()) {
                instance.lifecycle.FinishActivation();
            }
            break;
        case EditorGimmickRuntimeCommandKind::Deactivate:
            instance.lifecycle.Deactivate();
            break;
        case EditorGimmickRuntimeCommandKind::Reset:
            instance.lifecycle.Reset();
            break;
        case EditorGimmickRuntimeCommandKind::Enable:
        case EditorGimmickRuntimeCommandKind::Disable:
            break;
        }
    }
    void Update(
        float,
        EditorGimmickRuntimeInstance&,
        EditorGimmickRuntimeCommandQueue&) override {}
};

} // namespace

std::unique_ptr<IEditorGimmickRuntimeBehavior>
EditorDoorGimmickRuntimeBehavior::Clone() const {
    return std::make_unique<
        EditorDoorGimmickRuntimeBehavior>(*this);
}

bool EditorDoorGimmickRuntimeBehavior::ReadConfiguration(
    const EditorGimmickRuntimeInstance& instance,
    bool preserveRuntimeState,
    std::string* errorMessage) {
    bool startsOpen = false;
    bool locked = false;
    float openDistance = 0.0f;
    float travelSeconds = 0.0f;
    if (!ParseBoolean(
            instance,
            "startsOpen",
            startsOpen,
            errorMessage) ||
        !ParseBoolean(
            instance,
            "locked",
            locked,
            errorMessage) ||
        !ParseFloat(
            instance,
            "openDistance",
            openDistance,
            errorMessage) ||
        !ParseFloat(
            instance,
            "travelSeconds",
            travelSeconds,
            errorMessage) ||
        openDistance <= 0.0f ||
        travelSeconds <= 0.0f) {
        if (errorMessage != nullptr &&
            errorMessage->empty()) {
            *errorMessage =
                "Door Behavior requires positive distance and travel "
                "duration.";
        }
        return false;
    }
    startsOpen_ = startsOpen;
    locked_ = locked;
    openDistance_ = openDistance;
    travelSeconds_ = travelSeconds;
    if (!preserveRuntimeState) {
        targetOpen_ = startsOpen_;
        openFraction_ = startsOpen_ ? 1.0f : 0.0f;
    } else {
        openFraction_ =
            (std::clamp)(openFraction_, 0.0f, 1.0f);
    }
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool EditorDoorGimmickRuntimeBehavior::Initialize(
    EditorGimmickRuntimeInstance& instance,
    std::string* errorMessage) {
    if (!ReadConfiguration(
            instance, false, errorMessage)) {
        return false;
    }
    if (startsOpen_ &&
        instance.lifecycle.Activate() &&
        instance.lifecycle.OneShot()) {
        instance.lifecycle.FinishActivation();
    }
    return true;
}

bool EditorDoorGimmickRuntimeBehavior::Reconcile(
    EditorGimmickRuntimeInstance& replacement,
    std::string* errorMessage) {
    return ReadConfiguration(
        replacement, true, errorMessage);
}

void EditorDoorGimmickRuntimeBehavior::HandleCommand(
    const EditorGimmickRuntimeCommand& command,
    EditorGimmickRuntimeInstance& instance,
    EditorGimmickRuntimeCommandQueue&) {
    switch (command.kind) {
    case EditorGimmickRuntimeCommandKind::Activate:
        if (locked_) return;
        if (instance.lifecycle.State() ==
            EditorGimmickRuntimeState::Active) {
            targetOpen_ = true;
        } else if (instance.lifecycle.Activate()) {
            targetOpen_ = true;
        }
        break;
    case EditorGimmickRuntimeCommandKind::Deactivate:
        if (!targetOpen_ && openFraction_ <= 0.0f) {
            return;
        }
        if (instance.lifecycle.State() ==
                EditorGimmickRuntimeState::Active ||
            instance.lifecycle.Activate()) {
            targetOpen_ = false;
        }
        break;
    case EditorGimmickRuntimeCommandKind::Toggle:
        if (targetOpen_) {
            if (instance.lifecycle.State() ==
                    EditorGimmickRuntimeState::Active ||
                instance.lifecycle.Activate()) {
                targetOpen_ = false;
            }
        } else if (!locked_ &&
            (instance.lifecycle.State() ==
                 EditorGimmickRuntimeState::Active ||
             instance.lifecycle.Activate())) {
            targetOpen_ = true;
        }
        break;
    case EditorGimmickRuntimeCommandKind::Reset:
        instance.lifecycle.Reset();
        targetOpen_ = startsOpen_;
        openFraction_ = startsOpen_ ? 1.0f : 0.0f;
        if (startsOpen_) instance.lifecycle.Activate();
        break;
    case EditorGimmickRuntimeCommandKind::Enable:
    case EditorGimmickRuntimeCommandKind::Disable:
        break;
    }
}

void EditorDoorGimmickRuntimeBehavior::Update(
    float deltaTime,
    EditorGimmickRuntimeInstance& instance,
    EditorGimmickRuntimeCommandQueue&) {
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f ||
        instance.lifecycle.State() ==
            EditorGimmickRuntimeState::Disabled) {
        return;
    }
    const float direction = targetOpen_ ? 1.0f : -1.0f;
    const float previous = openFraction_;
    openFraction_ = (std::clamp)(
        openFraction_ + direction *
            (deltaTime / travelSeconds_),
        0.0f,
        1.0f);
    if (previous == openFraction_) return;
    if (targetOpen_ && openFraction_ >= 1.0f &&
        instance.lifecycle.OneShot()) {
        instance.lifecycle.FinishActivation();
    } else if (!targetOpen_ &&
        openFraction_ <= 0.0f) {
        instance.lifecycle.FinishActivation();
    }
}

std::unique_ptr<IEditorGimmickRuntimeBehavior>
EditorSwitchGimmickRuntimeBehavior::Clone() const {
    return std::make_unique<
        EditorSwitchGimmickRuntimeBehavior>(*this);
}

bool EditorSwitchGimmickRuntimeBehavior::ReadConfiguration(
    const EditorGimmickRuntimeInstance& instance,
    std::string* errorMessage) {
    bool toggleTarget = true;
    if (!ParseBoolean(
            instance,
            "toggle",
            toggleTarget,
            errorMessage)) {
        return false;
    }
    const std::string target =
        ReferenceGuid(instance, "target");
    if (target.empty()) {
        SetError(
            errorMessage,
            "Switch Behavior requires a resolved target Entity.");
        return false;
    }
    toggleTarget_ = toggleTarget;
    targetEntityGuid_ = target;
    nextGimmickEntityGuid_ =
        ReferenceGuid(instance, "nextGimmick");
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool EditorSwitchGimmickRuntimeBehavior::Initialize(
    EditorGimmickRuntimeInstance& instance,
    std::string* errorMessage) {
    dispatchCount_ = 0;
    return ReadConfiguration(instance, errorMessage);
}

bool EditorSwitchGimmickRuntimeBehavior::Reconcile(
    EditorGimmickRuntimeInstance& replacement,
    std::string* errorMessage) {
    return ReadConfiguration(replacement, errorMessage);
}

void EditorSwitchGimmickRuntimeBehavior::HandleCommand(
    const EditorGimmickRuntimeCommand& command,
    EditorGimmickRuntimeInstance& instance,
    EditorGimmickRuntimeCommandQueue& commands) {
    switch (command.kind) {
    case EditorGimmickRuntimeCommandKind::Activate:
    case EditorGimmickRuntimeCommandKind::Toggle:
        if (!instance.lifecycle.Activate()) return;
        commands.Enqueue(
            EditorGimmickRuntimeCommand{
                0,
                command.hopCount + 1,
                toggleTarget_
                    ? EditorGimmickRuntimeCommandKind::Toggle
                    : EditorGimmickRuntimeCommandKind::Activate,
                targetEntityGuid_,
                instance.entityGuid,
                {}},
            nullptr);
        if (!nextGimmickEntityGuid_.empty()) {
            commands.Enqueue(
                EditorGimmickRuntimeCommand{
                    0,
                    command.hopCount + 1,
                    EditorGimmickRuntimeCommandKind::Activate,
                    nextGimmickEntityGuid_,
                    instance.entityGuid,
                    {}},
                nullptr);
        }
        ++dispatchCount_;
        instance.lifecycle.FinishActivation();
        break;
    case EditorGimmickRuntimeCommandKind::Reset:
        instance.lifecycle.Reset();
        dispatchCount_ = 0;
        break;
    case EditorGimmickRuntimeCommandKind::Deactivate:
        instance.lifecycle.Deactivate();
        break;
    case EditorGimmickRuntimeCommandKind::Enable:
    case EditorGimmickRuntimeCommandKind::Disable:
        break;
    }
}

void EditorSwitchGimmickRuntimeBehavior::Update(
    float,
    EditorGimmickRuntimeInstance&,
    EditorGimmickRuntimeCommandQueue&) {}

std::unique_ptr<IEditorGimmickRuntimeBehavior>
CreateBuiltInEditorGimmickRuntimeBehavior(
    std::string_view definitionTypeId) {
    if (definitionTypeId == "gimmick.door") {
        return std::make_unique<
            EditorDoorGimmickRuntimeBehavior>();
    }
    if (definitionTypeId == "gimmick.switch") {
        return std::make_unique<
            EditorSwitchGimmickRuntimeBehavior>();
    }
    return std::make_unique<
        EditorPassiveGimmickRuntimeBehavior>();
}

} // namespace editor

#include "EditorCommandInputRouter.h"

#include "EditorContext.h"

#include "../../externals/imgui/imgui.h"

#include <cctype>
#include <string_view>

namespace editor {
namespace {

struct ParsedShortcut {
    bool valid = false;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
    bool super = false;
    ImGuiKey key = ImGuiKey_None;
};

std::string_view Trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1);
    }
    return value;
}

char ToUpperAscii(char value) {
    return static_cast<char>(std::toupper(static_cast<unsigned char>(value)));
}

bool EqualsIgnoreCase(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (ToUpperAscii(lhs[i]) != ToUpperAscii(rhs[i])) {
            return false;
        }
    }
    return true;
}

ImGuiKey ParseShortcutKey(std::string_view token) {
    token = Trim(token);
    if (token.size() == 1) {
        const char c = ToUpperAscii(token[0]);
        if (c >= 'A' && c <= 'Z') {
            return static_cast<ImGuiKey>(ImGuiKey_A + (c - 'A'));
        }
        if (c >= '0' && c <= '9') {
            return static_cast<ImGuiKey>(ImGuiKey_0 + (c - '0'));
        }
    }
    if (EqualsIgnoreCase(token, "Delete") || EqualsIgnoreCase(token, "Del")) {
        return ImGuiKey_Delete;
    }
    if (EqualsIgnoreCase(token, "Enter") || EqualsIgnoreCase(token, "Return")) {
        return ImGuiKey_Enter;
    }
    if (EqualsIgnoreCase(token, "Escape") || EqualsIgnoreCase(token, "Esc")) {
        return ImGuiKey_Escape;
    }
    if (EqualsIgnoreCase(token, "Space")) {
        return ImGuiKey_Space;
    }
    if (EqualsIgnoreCase(token, "Tab")) {
        return ImGuiKey_Tab;
    }
    return ImGuiKey_None;
}

ParsedShortcut ParseShortcut(std::string_view shortcut) {
    ParsedShortcut parsed;
    shortcut = Trim(shortcut);
    if (shortcut.empty()) {
        return parsed;
    }

    while (!shortcut.empty()) {
        const std::size_t separator = shortcut.find('+');
        const std::string_view token = Trim(shortcut.substr(0, separator));
        if (token.empty()) {
            return {};
        }

        if (EqualsIgnoreCase(token, "Ctrl") || EqualsIgnoreCase(token, "Control")) {
            parsed.ctrl = true;
        } else if (EqualsIgnoreCase(token, "Shift")) {
            parsed.shift = true;
        } else if (EqualsIgnoreCase(token, "Alt")) {
            parsed.alt = true;
        } else if (EqualsIgnoreCase(token, "Super") || EqualsIgnoreCase(token, "Win") || EqualsIgnoreCase(token, "Cmd")) {
            parsed.super = true;
        } else {
            if (parsed.key != ImGuiKey_None) {
                return {};
            }
            parsed.key = ParseShortcutKey(token);
            if (parsed.key == ImGuiKey_None) {
                return {};
            }
        }

        if (separator == std::string_view::npos) {
            break;
        }
        shortcut.remove_prefix(separator + 1);
    }

    parsed.valid = parsed.key != ImGuiKey_None;
    return parsed;
}

bool MatchesCurrentInput(const ParsedShortcut& shortcut) {
    if (!shortcut.valid) {
        return false;
    }

    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl != shortcut.ctrl ||
        io.KeyShift != shortcut.shift ||
        io.KeyAlt != shortcut.alt ||
        io.KeySuper != shortcut.super) {
        return false;
    }

    return ImGui::IsKeyPressed(shortcut.key, false);
}

} // namespace

EditorCommandInputDispatch EditorCommandInputRouter::Dispatch(
    EditorContext& context,
    const EditorCommandInputRouterOptions& options) {
    if (context.commands == nullptr) {
        return {};
    }
    return Dispatch(*context.commands, options);
}

EditorCommandInputDispatch EditorCommandInputRouter::Dispatch(
    EditorCommandRegistry& registry,
    const EditorCommandInputRouterOptions& options) {
    if (!options.enabled) {
        return {};
    }

    const ImGuiIO& io = ImGui::GetIO();
    if (options.ignoreWhenTextInputActive && io.WantTextInput) {
        return {};
    }

    for (const EditorCommand& command : registry.Commands()) {
        if (command.shortcut.empty()) {
            continue;
        }
        if (!registry.IsEnabled(command)) {
            continue;
        }

        const ParsedShortcut shortcut = ParseShortcut(command.shortcut);
        if (!MatchesCurrentInput(shortcut)) {
            continue;
        }

        EditorCommandInputDispatch dispatch;
        dispatch.handled = true;
        dispatch.commandId = command.id;
        dispatch.result = registry.Execute(command.id);
        lastDispatch_ = dispatch;
        return dispatch;
    }

    return {};
}

} // namespace editor

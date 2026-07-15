#pragma once

#include <cstddef>

#include "EditorError.h"

namespace editor {

class EditorTransactionMemoryBudget final {
public:
    static constexpr std::size_t kDefaultLimitBytes = 64u * 1024u * 1024u;

    bool SetLimitBytes(std::size_t bytes, EditorError* error = nullptr);
    std::size_t LimitBytes() const noexcept { return limitBytes_; }
    bool AcceptsSingleRecord(std::size_t bytes) const noexcept;
    bool ExceededBy(std::size_t historyBytes) const noexcept;

private:
    std::size_t limitBytes_ = kDefaultLimitBytes;
};

} // namespace editor

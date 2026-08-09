#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class Interner {
  public:
    using Sym = uint64_t Sym intern(std::string_view str) {
        if (const auto it = ids_.find(str); it != ids_.end()) {
            return it->second;
        }

        const Sym new_id = static_cast<Sym>(storage_.size());
        storage_.emplace_back(str);

        std::string_view stable_ref = storage_.back();
        ids_.emplace(stable_ref, new_id);
        labels_.push_back(stable_ref);

        return new_id;
    }

    std::optional<Sym> id(std::string_view str) const {
        if (const auto it = ids_.find(str); it != ids_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::string_view label(Sym id) const {
        return labels_[static_cast<size_t>(id)];
    }

    size_t size() const { return labels_.size(); }

  private:
    struct Hash {
        using is_transparent = void;
        size_t operator()(std::string_view s) const {
            return std::hash<std::string_view>{}(s);
        }
    };

    std::deque<std::string> storage_;
    std::unordered_map<std::string_view, Sym, Hash> ids_;
    std::vector<std::string_view> labels_;
};

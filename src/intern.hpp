#ifndef HOME_JM_SOFTWARE_CPP_BCHK_SRC_INTERN_HPP
#define HOME_JM_SOFTWARE_CPP_BCHK_SRC_INTERN_HPP

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
    using Sym = int32_t;
    Sym intern(std::string_view str) {
        if (const auto it = ids.find(str); it != ids.end()) {
            return it->second;
        }

        const Sym new_id = static_cast<Sym>(storage.size());
        storage.emplace_back(str);

        std::string_view stable_ref = storage.back();
        ids.emplace(stable_ref, new_id);
        canonical_names.push_back(stable_ref);
        display_names.emplace_back();

        return new_id;
    }

    Sym freshen(std::string_view canonical_name) {
        const Sym new_id = static_cast<Sym>(storage.size());
        storage.emplace_back(canonical_name);
        canonical_names.push_back(storage.back());
        display_names.emplace_back();
        return new_id;
    }

    std::optional<Sym> id(std::string_view str) const {
        if (const auto it = ids.find(str); it != ids.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    void set_display(Sym sym, std::string_view display) {
        if (sym >= 0 && static_cast<size_t>(sym) < display_names.size()) {
            display_names[static_cast<size_t>(sym)] = display;
        }
    }

    std::string_view lookup(Sym sym) const {
        if (sym < 0 || static_cast<size_t>(sym) >= canonical_names.size()) {
            return "";
        }
        std::string_view display = display_names[static_cast<size_t>(sym)];
        return display.empty() ? canonical_names[static_cast<size_t>(sym)]
                               : display;
    }

    size_t size() const { return canonical_names.size(); }

  private:
    struct Hash {
        using is_transparent = void;
        size_t operator()(std::string_view s) const {
            return std::hash<std::string_view>{}(s);
        }
    };

    std::deque<std::string> storage;
    std::unordered_map<std::string_view, Sym, Hash> ids;
    std::vector<std::string_view> canonical_names;
    std::vector<std::string> display_names;
};

#endif

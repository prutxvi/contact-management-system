// ============================================================================
//  Contact Management System  --  C++17 console application
//  Group 11 assignment: menu-driven CRUD over a personal contact list.
//
//  Data structures used (and why):
//    std::vector<Contact>          storage of record; dense, cache friendly
//    unordered_map<int, size_t>    id  -> slot        : O(1) locate by ID
//    unordered_map<string,size_t>  phone -> slot      : O(1) phone lookup + duplicate check
//    map<string,vector<size_t>>    lower(name) -> slots: O(log n) ordered/prefix name search
//    Trie (prefix tree)            name -> id         : O(p) autocomplete
//    std::stack<Action>            undo history (bounded)
//
//  No third-party dependencies. Build:
//      c++ -std=c++17 -O2 -Wall -Wextra -o cms src/contact_manager.cpp
// ============================================================================

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// 1. Small string / validation utilities
// ---------------------------------------------------------------------------
namespace util {

std::string trim(const std::string& s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string lower(const std::string& s) {
    std::string r = s;
    for (char& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

bool iequals(const std::string& a, const std::string& b) {
    return lower(a) == lower(b);
}

bool startsWithCi(const std::string& s, const std::string& p) {
    if (p.size() > s.size()) return false;
    for (std::size_t i = 0; i < p.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(s[i])) !=
            std::tolower(static_cast<unsigned char>(p[i]))) return false;
    return true;
}

bool containsCi(const std::string& hay, const std::string& needle) {
    if (needle.empty()) return true;
    if (needle.size() > hay.size()) return false;
    return lower(hay).find(lower(needle)) != std::string::npos;
}

// Collapse runs of whitespace and trim. "  Aarav   Sharma " -> "Aarav Sharma"
std::string squeezeSpaces(const std::string& s) {
    std::string out;
    bool pending = false;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) { pending = !out.empty(); continue; }
        if (pending) { out.push_back(' '); pending = false; }
        out.push_back(c);
    }
    return out;
}

std::string digitsOnly(const std::string& s) {
    std::string out;
    for (char c : s) if (std::isdigit(static_cast<unsigned char>(c))) out.push_back(c);
    return out;
}

// Accepts "+91 98765 43210", "098765-43210", "9876543210" -> "9876543210".
std::string normalizePhone(const std::string& raw) {
    std::string d = digitsOnly(raw);
    if (d.size() > 10 && d.compare(0, 2, "91") == 0) d = d.substr(2);
    if (d.size() > 10 && d[0] == '0') d = d.substr(1);
    return d;
}

// Indian numbering plan: 10 digits, first digit 2-9 (2-5 landline STD, 6-9 mobile).
bool isValidPhone(const std::string& n) {
    if (n.size() != 10) return false;
    return n[0] >= '2' && n[0] <= '9';
}

// Hand-written validator: deterministic, no regex backtracking, easy to justify.
bool isValidEmail(const std::string& e) {
    if (e.empty() || e.size() > 100) return false;
    std::size_t at = e.find('@');
    if (at == std::string::npos || at == 0 || at + 1 >= e.size()) return false;
    if (e.find('@', at + 1) != std::string::npos) return false;
    std::string domain = e.substr(at + 1);
    if (domain.find('.') == std::string::npos) return false;
    if (domain.front() == '.' || domain.back() == '.' || domain.back() == '-') return false;
    for (std::size_t i = 0; i < e.size(); ++i) {
        char c = e[i];
        if (std::isalnum(static_cast<unsigned char>(c))) continue;
        if (c == '@' || c == '.' || c == '_' || c == '-' || c == '+') continue;
        return false;
    }
    return true;
}

bool hasControlChar(const std::string& s) {
    for (char c : s) {
        unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x20 && c != '\t') return true;
    }
    return false;
}

std::string todayIso() {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tmv);
    return buf;
}

// Left-align into a fixed width, ellipsizing long values so the table stays aligned.
std::string fit(const std::string& s, std::size_t w) {
    if (s.size() == w) return s;
    if (s.size() < w) return s + std::string(w - s.size(), ' ');
    if (w <= 3) return s.substr(0, w);
    return s.substr(0, w - 3) + "...";
}

std::string fmtPhone(const std::string& p) {
    if (p.size() != 10) return p;
    return p.substr(0, 5) + " " + p.substr(5);
}

}  // namespace util

// ---------------------------------------------------------------------------
// 2. Edit distance (Damerau-Levenshtein, transposition aware)
//    Handles "Pruhtvi" -> "Pruthvi" in one edit instead of two.
//    Time O(|a|*|b|), space O(min(|a|,|b|)).
// ---------------------------------------------------------------------------
namespace fuzzy {

int damerauLevenshtein(const std::string& a, const std::string& b) {
    const std::size_t n = a.size(), m = b.size();
    if (n == 0) return static_cast<int>(m);
    if (m == 0) return static_cast<int>(n);
    std::vector<int> prev2(m + 1, 0), prev(m + 1, 0), cur(m + 1, 0);
    for (std::size_t j = 0; j <= m; ++j) prev[j] = static_cast<int>(j);
    for (std::size_t i = 1; i <= n; ++i) {
        cur[0] = static_cast<int>(i);
        for (std::size_t j = 1; j <= m; ++j) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            int best = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost});
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1])
                best = std::min(best, prev2[j - 2] + 1);
            cur[j] = best;
        }
        prev2.swap(prev);
        prev.swap(cur);
    }
    return prev[m];
}

}  // namespace fuzzy

// ---------------------------------------------------------------------------
// 3. Trie (prefix tree) for name autocomplete
//    Insert/erase O(|key|). Prefix enumeration O(|prefix| + nodes in subtree).
//    Each node keeps a std::map so DFS output is lexicographic and repeatable.
// ---------------------------------------------------------------------------
class Trie {
public:
    static std::string key(const std::string& name) {
        return util::lower(util::squeezeSpaces(util::trim(name)));
    }

    void insert(const std::string& k, int id) {
        Node* n = root_.get();
        for (char c : k) {
            auto it = n->children.find(c);
            if (it == n->children.end())
                it = n->children.emplace(c, std::make_unique<Node>()).first;
            n = it->second.get();
        }
        if (std::find(n->ids.begin(), n->ids.end(), id) == n->ids.end()) n->ids.push_back(id);
    }

    void erase(const std::string& k, int id) {
        eraseRec(root_.get(), k, 0, id);
    }

    std::vector<int> prefix(const std::string& p) const {
        const Node* n = root_.get();
        for (char c : p) {
            auto it = n->children.find(c);
            if (it == n->children.end()) return {};
            n = it->second.get();
        }
        std::vector<int> out;
        collect(n, out);
        return out;
    }

    std::size_t nodeCount() const { return nodeCountRec(root_.get()); }

private:
    struct Node {
        std::map<char, std::unique_ptr<Node>> children;
        std::vector<int> ids;  // contacts whose full key ends here
    };

    bool eraseRec(Node* n, const std::string& k, std::size_t d, int id) {
        if (d == k.size()) {
            auto it = std::find(n->ids.begin(), n->ids.end(), id);
            if (it != n->ids.end()) n->ids.erase(it);
        } else {
            auto it = n->children.find(k[d]);
            if (it == n->children.end()) return n->ids.empty() && n->children.empty();
            if (eraseRec(it->second.get(), k, d + 1, id)) n->children.erase(it);
        }
        return n->ids.empty() && n->children.empty();
    }

    static void collect(const Node* n, std::vector<int>& out) {
        out.insert(out.end(), n->ids.begin(), n->ids.end());
        for (const auto& kv : n->children) collect(kv.second.get(), out);
    }

    static std::size_t nodeCountRec(const Node* n) {
        std::size_t c = 1;
        for (const auto& kv : n->children) c += nodeCountRec(kv.second.get());
        return c;
    }

    std::unique_ptr<Node> root_ = std::make_unique<Node>();
};

// ---------------------------------------------------------------------------
// 4. CSV reader/writer that survives commas, quotes and embedded newlines
// ---------------------------------------------------------------------------
namespace csv {

std::string escape(const std::string& f) {
    bool need = f.empty();
    if (!need)
        for (char c : f)
            if (c == ',' || c == '"' || c == '\n' || c == '\r') { need = true; break; }
    if (!need && (std::isspace(static_cast<unsigned char>(f.front())) ||
                  std::isspace(static_cast<unsigned char>(f.back())))) need = true;
    if (!need) return f;
    std::string out = "\"";
    for (char c : f) {
        if (c == '"') out += "\"\"";
        else out.push_back(c);
    }
    out += "\"";
    return out;
}

std::string joinRecord(const std::vector<std::string>& fields) {
    std::string out;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i) out.push_back(',');
        out += escape(fields[i]);
    }
    return out;
}

// State machine: a quoted field may legally contain commas and newlines.
bool parse(const std::string& text, std::vector<std::vector<std::string>>& out, std::string& err) {
    std::vector<std::string> record;
    std::string field;
    bool inQuotes = false, sawAny = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < text.size() && text[i + 1] == '"') { field.push_back('"'); ++i; }
                else inQuotes = false;
            } else field.push_back(c);
        } else if (c == '"') {
            inQuotes = true;  // opening quote; tolerate quotes mid-field
        } else if (c == ',') {
            record.push_back(field); field.clear(); sawAny = true;
        } else if (c == '\r') {
            // swallow CR in CRLF
        } else if (c == '\n') {
            record.push_back(field); field.clear();
            out.push_back(record); record.clear(); sawAny = false;
        } else {
            field.push_back(c); sawAny = true;
        }
    }
    if (inQuotes) { err = "unterminated quoted field (file truncated?)"; return false; }
    if (sawAny || !field.empty() || !record.empty()) {
        record.push_back(field);
        out.push_back(record);
    }
    return true;
}

}  // namespace csv

// ---------------------------------------------------------------------------
// 5. The record itself
// ---------------------------------------------------------------------------
struct Contact {
    int         id = 0;
    std::string name;      // display name, spaces allowed
    std::string phone;     // always stored normalised: 10 digits, no separators
    std::string email;     // optional
    std::string group;     // Family / Friend / Work / General (free text, default General)
    std::string address;   // free text, may contain commas and quotes
    std::string created;   // ISO date the record was first added
};

// ---------------------------------------------------------------------------
// 6. ContactStore: one array of record + three lookup indexes + a trie
// ---------------------------------------------------------------------------
struct SortStats { long long comparisons = 0; };

struct CiLess {
    bool operator()(const std::string& a, const std::string& b) const {
        return util::lower(a) < util::lower(b);
    }
};

class ContactStore {
public:
    struct Result {
        bool ok;
        std::string msg;
    };
    enum class NameMode { Exact, Prefix, Contains, Fuzzy };
    enum class SortKey { Name, Phone, Id, Group, Created };

    // ---- size / introspection (used by the report screen) ------------------
    std::size_t size() const { return rows_.size(); }
    std::size_t indexEntries() const { return byId_.size() + byPhone_.size(); }
    std::size_t nameBuckets() const { return byName_.size(); }
    std::size_t trieNodes() const { return trie_.nodeCount(); }
    int nextId() const { return nextId_; }

    // ---- create -------------------------------------------------------------
    Result add(Contact c, int* outId = nullptr) {
        Result v = validate(c);
        if (!v.ok) return v;

        bool sameNameExists = byName_.find(c.name) != byName_.end();
        c.id = nextId_++;
        rows_.push_back(std::move(c));
        linkIndexes(rows_.back(), rows_.size() - 1);
        const Contact& stored = rows_.back();
        pushUndo(Action{Kind::Added, Contact{}, stored});
        if (outId) *outId = stored.id;

        std::string msg = "Added contact #" + std::to_string(stored.id) + " (" + stored.name + ")";
        if (sameNameExists)
            msg += "\n    note: another contact already has this name."
                   "\n    names are not unique; phone numbers are.";
        return {true, msg};
    }

    // ---- read ---------------------------------------------------------------
    const Contact* findById(int id) const {
        auto it = byId_.find(id);
        return it == byId_.end() ? nullptr : &rows_[it->second];
    }

    const Contact* findByPhone(const std::string& rawPhone) const {
        std::string p = util::normalizePhone(rawPhone);
        auto it = byPhone_.find(p);
        return it == byPhone_.end() ? nullptr : &rows_[it->second];
    }

    std::vector<const Contact*> searchByName(const std::string& qIn, NameMode mode) const {
        std::string q = util::squeezeSpaces(util::trim(qIn));
        if (q.empty()) return {};
        std::vector<const Contact*> out;

        if (mode == NameMode::Exact) {
            auto it = byName_.find(q);
            if (it != byName_.end())
                for (std::size_t pos : it->second) out.push_back(&rows_[pos]);
            return out;
        }
        if (mode == NameMode::Prefix) {
            // Ordered map -> one lower_bound, then walk while the prefix still matches.
            for (auto it = byName_.lower_bound(q); it != byName_.end(); ++it) {
                if (!util::startsWithCi(it->first, q)) break;
                for (std::size_t pos : it->second) out.push_back(&rows_[pos]);
            }
            return out;
        }
        if (mode == NameMode::Fuzzy) {
            std::vector<std::pair<int, const Contact*>> scored;
            std::string ql = util::lower(q);
            for (const Contact& c : rows_) {
                int d1 = fuzzy::damerauLevenshtein(ql, util::lower(c.name));
                int d2 = fuzzy::damerauLevenshtein(ql, util::lower(firstToken(c.name)));
                int d = std::min(d1, d2);
                if (d <= fuzzyLimit(q)) scored.emplace_back(d, &c);
            }
            std::stable_sort(scored.begin(), scored.end(),
                             [](const auto& a, const auto& b) { return a.first < b.first; });
            for (const auto& s : scored) out.push_back(s.second);
            return out;
        }
        for (const Contact& c : rows_)  // Contains: worst case, needs a full scan
            if (util::containsCi(c.name, q)) out.push_back(&c);
        return out;
    }

    std::vector<const Contact*> searchByPhone(const std::string& raw) const {
        std::vector<const Contact*> out;
        std::string digits = util::digitsOnly(raw);
        if (digits.empty()) return out;
        if (const Contact* exact = findByPhone(digits)) { out.push_back(exact); return out; }
        for (const Contact& c : rows_)  // partial / suffix match
            if (c.phone.find(digits) != std::string::npos) out.push_back(&c);
        return out;
    }

    std::vector<const Contact*> autocomplete(const std::string& prefixIn) const {
        std::string p = Trie::key(prefixIn);
        if (p.empty()) return {};
        // No sort step: DFS visits children in char order, so the trie already
        // emits matches in alphabetical order. That is a free bonus of the shape
        // of the tree, not extra work.
        std::vector<const Contact*> out;
        for (int id : trie_.prefix(p))
            if (const Contact* c = findById(id)) out.push_back(c);
        return out;
    }

    std::vector<const Contact*> allById() const {
        std::vector<const Contact*> out;
        out.reserve(rows_.size());
        for (const Contact& c : rows_) out.push_back(&c);
        std::sort(out.begin(), out.end(),
                  [](const Contact* a, const Contact* b) { return a->id < b->id; });
        return out;
    }

    // std::stable_sort keeps equal keys in the incoming (id) order.
    std::vector<const Contact*> sorted(SortKey key, bool ascending, SortStats* st) const {
        std::vector<const Contact*> v = allById();
        auto cmp = [key, ascending, st](const Contact* a, const Contact* b) {
            if (st) ++st->comparisons;
            int r = 0;
            switch (key) {
                case SortKey::Name:    r = cmpStr(util::lower(a->name), util::lower(b->name)); break;
                case SortKey::Phone:   r = cmpStr(a->phone, b->phone); break;
                case SortKey::Group:   r = cmpStr(util::lower(a->group), util::lower(b->group)); break;
                case SortKey::Created: r = cmpStr(a->created, b->created); break;
                case SortKey::Id:      r = (a->id < b->id) ? -1 : (a->id > b->id ? 1 : 0); break;
            }
            return ascending ? r < 0 : r > 0;
        };
        std::stable_sort(v.begin(), v.end(), cmp);
        return v;
    }

    // ---- update -------------------------------------------------------------
    // Fields left empty keep their current value; pass keepFields=false to blank them.
    Result update(int id, Contact nv) {
        auto it = byId_.find(id);
        if (it == byId_.end()) return {false, "No contact with ID " + std::to_string(id) + "."};
        std::size_t pos = it->second;

        Result v = validate(nv, id);
        if (!v.ok) return v;

        Contact before = rows_[pos];
        unlinkIndexes(rows_[pos], pos);
        rows_[pos] = std::move(nv);
        rows_[pos].id = id;
        rows_[pos].created = before.created;  // creation date is immutable
        linkIndexes(rows_[pos], pos);
        pushUndo(Action{Kind::Updated, before, rows_[pos]});
        return {true, "Updated contact #" + std::to_string(id) + " (" + rows_[pos].name + ")"};
    }

    // ---- delete -------------------------------------------------------------
    Result remove(int id) {
        auto it = byId_.find(id);
        if (it == byId_.end()) return {false, "No contact with ID " + std::to_string(id) + "."};
        std::size_t pos = it->second;
        Contact removed = rows_[pos];
        eraseAt(pos);
        pushUndo(Action{Kind::Deleted, removed, Contact{}});
        return {true, "Deleted contact #" + std::to_string(id) + " (" + removed.name +
                      "). Use Undo (menu 9) to restore."};
    }

    // ---- undo ---------------------------------------------------------------
    bool undo(std::string& what) {
        if (undo_.empty()) { what = "Nothing to undo."; return false; }
        Action a = undo_.back();
        undo_.pop_back();
        switch (a.kind) {
            case Kind::Added: {
                auto it = byId_.find(a.after.id);
                if (it == byId_.end()) { what = "Undo skipped: contact already gone."; return false; }
                eraseAt(it->second);
                what = "Undid add of #" + std::to_string(a.after.id) + " (" + a.after.name + ")";
                return true;
            }
            case Kind::Updated: {
                auto it = byId_.find(a.before.id);
                if (it == byId_.end()) { what = "Undo skipped: contact was deleted."; return false; }
                if (byPhone_.count(a.before.phone) && byPhone_[a.before.phone] != it->second) {
                    what = "Undo refused: old phone " + a.before.phone + " is now used by another contact.";
                    return false;
                }
                std::size_t pos = it->second;
                unlinkIndexes(rows_[pos], pos);
                rows_[pos] = a.before;
                linkIndexes(rows_[pos], pos);
                what = "Undid update of #" + std::to_string(a.before.id) + " (" + a.before.name + ")";
                return true;
            }
            case Kind::Deleted: {
                if (byId_.count(a.before.id)) { what = "Undo skipped: ID already in use."; return false; }
                if (byPhone_.count(a.before.phone)) {
                    what = "Undo refused: phone " + a.before.phone + " was re-added as another contact.";
                    return false;
                }
                rows_.push_back(a.before);
                linkIndexes(rows_.back(), rows_.size() - 1);
                what = "Restored #" + std::to_string(a.before.id) + " (" + a.before.name + ")";
                return true;
            }
        }
        what = "Unknown action.";
        return false;
    }

    bool canUndo() const { return !undo_.empty(); }

    // ---- index audit --------------------------------------------------------
    // A self-check that every index agrees with the record array. This is what
    // proves the O(1) delete repaired the right entries: a stale index would show
    // up here as a mismatch instead of as a record that silently vanished.
    struct Audit {
        bool ok = true;
        std::size_t rows = 0, idEntries = 0, phoneEntries = 0, nameSlots = 0, trieHits = 0;
        std::vector<std::string> problems;
        void note(const std::string& p) { ok = false; if (problems.size() < 10) problems.push_back(p); }
    };

    Audit audit() const {
        Audit a;
        a.rows = rows_.size();
        a.idEntries = byId_.size();
        a.phoneEntries = byPhone_.size();
        if (byId_.size() != rows_.size()) a.note("id index holds " + std::to_string(byId_.size()) +
                                                 " entries for " + std::to_string(rows_.size()) + " records");
        if (byPhone_.size() != rows_.size()) a.note("phone index holds " + std::to_string(byPhone_.size()) +
                                                    " entries for " + std::to_string(rows_.size()) + " records");

        std::vector<int> slots(rows_.size(), 0);
        for (std::size_t pos = 0; pos < rows_.size(); ++pos) {
            const Contact& c = rows_[pos];
            auto iit = byId_.find(c.id);
            if (iit == byId_.end() || iit->second != pos)
                a.note("record #" + std::to_string(c.id) + " at slot " + std::to_string(pos) +
                       " is not found at that slot in the id index");
            auto pit = byPhone_.find(c.phone);
            if (pit == byPhone_.end() || pit->second != pos)
                a.note("phone " + c.phone + " of record #" + std::to_string(c.id) +
                       " does not point back to slot " + std::to_string(pos));
            auto nit = byName_.find(c.name);
            if (nit == byName_.end()) {
                a.note("name '" + c.name + "' of record #" + std::to_string(c.id) + " is missing from the name index");
            } else {
                std::size_t seen = 0;
                for (std::size_t slot : nit->second) if (slot == pos) ++seen;
                if (seen != 1)
                    a.note("name '" + c.name + "' of record #" + std::to_string(c.id) + " appears " +
                           std::to_string(seen) + " times in its name bucket (expected exactly 1)");
            }
            std::vector<int> hits = trie_.prefix(Trie::key(c.name));
            bool inTrie = std::find(hits.begin(), hits.end(), c.id) != hits.end();
            if (!inTrie)
                a.note("record #" + std::to_string(c.id) + " (" + c.name + ") is not reachable in the trie");
            else
                ++a.trieHits;
        }
        for (const auto& kv : byPhone_) {
            if (kv.second >= rows_.size() || rows_[kv.second].phone != kv.first)
                a.note("phone index key " + kv.first + " points at a record that does not own it");
        }
        for (const auto& kv : byName_) {
            a.nameSlots += kv.second.size();
            for (std::size_t slot : kv.second) {
                if (slot >= rows_.size()) { a.note("name index bucket '" + kv.first + "' points past the end"); continue; }
                if (!util::iequals(rows_[slot].name, kv.first))
                    a.note("name index bucket '" + kv.first + "' points at record #" +
                           std::to_string(rows_[slot].id) + " named '" + rows_[slot].name + "'");
                ++slots[slot];
            }
        }
        if (a.nameSlots != rows_.size())
            a.note("name index holds " + std::to_string(a.nameSlots) + " slots for " +
                   std::to_string(rows_.size()) + " records");
        for (std::size_t pos = 0; pos < slots.size(); ++pos)
            if (slots[pos] != 1)
                a.note("slot " + std::to_string(pos) + " (record #" + std::to_string(rows_[pos].id) +
                       ") is listed " + std::to_string(slots[pos]) + " times across the name index");
        return a;
    }

    // Test hook. Used by --inject-fault and by tests/test_cms.py to prove the audit
    // above detects damage instead of always reporting success.
    void injectFaultForTest(int which) {
        if (rows_.empty()) return;
        switch (which) {
            case 1: byPhone_.erase(rows_.front().phone); break;
            case 2: byId_.erase(rows_.front().id); break;
            case 3: byName_[rows_.back().name].push_back(0); break;
            case 4: trie_.erase(Trie::key(rows_.front().name), rows_.front().id); break;
            case 5: byPhone_["9999999999"] = 0; break;
            default: break;
        }
    }

    // ---- CSV persistence ----------------------------------------------------
    Result save(const std::string& path) const {
        std::vector<const Contact*> v = allById();
        std::string tmp = path + ".tmp";
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f) return {false, "Cannot open '" + tmp + "' for writing (check the directory and permissions)."};
            f << "id,name,phone,email,group,address,created\n";
            for (const Contact* c : v) {
                f << csv::joinRecord({std::to_string(c->id), c->name, c->phone, c->email, c->group,
                                      c->address, c->created})
                  << "\n";
            }
            f.flush();
            if (!f.good()) return {false, "Write failed part way through; '" + path + "' was left untouched."};
        }
        if (std::rename(tmp.c_str(), path.c_str()) != 0)
            return {false, "Could not replace '" + path + "' (rename failed). Data is in '" + tmp + "'."};
        return {true, "Saved " + std::to_string(v.size()) + " contact(s) to " + path};
    }

    Result load(const std::string& path, bool* existed = nullptr) {
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            if (existed) *existed = false;
            return {true, "No existing file at " + path + "; starting with an empty list."};
        }
        if (existed) *existed = true;
        std::stringstream ss;
        ss << f.rdbuf();
        std::vector<std::vector<std::string>> records;
        std::string err;
        if (!csv::parse(ss.str(), records, err))
            return {false, "Parsing " + path + " failed: " + err};

        std::vector<Contact> staged;
        std::vector<std::string> header;
        std::size_t skipped = 0, dupes = 0;
        if (!records.empty()) { header = records.front(); records.erase(records.begin()); }
        auto col = [&header](const std::string& n) -> std::optional<std::size_t> {
            for (std::size_t i = 0; i < header.size(); ++i)
                if (util::iequals(util::trim(header[i]), n)) return i;
            return std::nullopt;
        };
        auto cell = [](const std::vector<std::string>& r, std::optional<std::size_t> i) {
            return (i && *i < r.size()) ? r[*i] : std::string();
        };
        std::optional<std::size_t> cId = col("id"), cName = col("name"), cPhone = col("phone");
        std::optional<std::size_t> cEmail = col("email"), cGroup = col("group");
        std::optional<std::size_t> cAddr = col("address"), cCreated = col("created");
        if (!cName || !cPhone)
            return {false, "Header must contain at least 'name' and 'phone' columns (found: " +
                               (header.empty() ? std::string("none") : header.front()) + " ...)"};

        // Only now is it safe to discard the live state: the file parsed and the
        // header is usable. Keeping this before the staging loop also keeps the
        // duplicate check in validate() from seeing stale index entries.
        rows_.clear();
        byId_.clear();
        byPhone_.clear();
        byName_.clear();
        trie_ = Trie{};
        undo_.clear();
        nextId_ = 1;

        std::unordered_map<std::string, std::size_t> seenPhones;
        int maxId = 0;
        for (const auto& r : records) {
            if (r.size() == 1 && util::trim(r[0]).empty()) continue;  // blank line
            Contact c;
            c.name = util::squeezeSpaces(util::trim(cell(r, cName)));
            c.phone = util::normalizePhone(cell(r, cPhone));
            c.email = util::trim(cell(r, cEmail));
            c.group = util::trim(cell(r, cGroup));
            c.address = util::trim(cell(r, cAddr));
            c.created = util::trim(cell(r, cCreated));
            std::string idTxt = util::trim(cell(r, cId));
            c.id = idTxt.empty() ? 0 : std::atoi(idTxt.c_str());
            if (c.created.empty()) c.created = util::todayIso();
            Result v = validate(c, 0);
            if (!v.ok) { ++skipped; continue; }
            if (seenPhones.count(c.phone)) { ++dupes; continue; }
            seenPhones[c.phone] = staged.size();
            if (c.id <= 0) c.id = ++maxId;
            maxId = std::max(maxId, c.id);
            staged.push_back(std::move(c));
        }

        for (const Contact& c : staged) {
            rows_.push_back(c);
            linkIndexes(rows_.back(), rows_.size() - 1);
            nextId_ = std::max(nextId_, c.id + 1);
        }
        std::string msg = "Loaded " + std::to_string(rows_.size()) + " contact(s) from " + path;
        if (dupes) msg += "; skipped " + std::to_string(dupes) + " duplicate phone number(s)";
        if (skipped) msg += "; skipped " + std::to_string(skipped) + " invalid record(s)";
        return {true, msg};
    }

private:
    enum class Kind { Added, Updated, Deleted };
    struct Action {
        Kind kind;
        Contact before;
        Contact after;
    };

    static int cmpStr(const std::string& a, const std::string& b) {
        return a < b ? -1 : (a > b ? 1 : 0);
    }

    static std::string firstToken(const std::string& s) {
        std::size_t sp = s.find(' ');
        return sp == std::string::npos ? s : s.substr(0, sp);
    }

    // Typo tolerance scales with query length: short names must match almost exactly.
    static int fuzzyLimit(const std::string& q) {
        if (q.size() <= 3) return 1;
        if (q.size() <= 7) return 2;
        return 3;
    }

    // Shared validation for add() and update(). `selfId` != 0 means "ignore this
    // contact when checking for a duplicate phone".
    Result validate(Contact& c, int selfId = 0) {
        c.name = util::squeezeSpaces(util::trim(c.name));
        c.email = util::trim(c.email);
        c.group = util::trim(c.group);
        c.address = util::trim(c.address);

        if (c.name.empty()) return {false, "Name cannot be empty."};
        if (c.name.size() > 60) return {false, "Name is too long (max 60 characters)."};
        if (util::hasControlChar(c.name)) return {false, "Name contains control characters."};
        if (util::digitsOnly(c.name) == c.name) return {false, "Name cannot be all digits."};

        std::string ph = util::normalizePhone(c.phone);
        if (ph.empty()) {
            if (util::trim(c.phone).empty()) return {false, "Phone number cannot be empty."};
            return {false, "Invalid phone '" + util::trim(c.phone) + "': no digits found."};
        }
        if (!util::isValidPhone(ph))
            return {false, "Invalid phone '" + c.phone + "'."
                               "\n    need exactly 10 digits starting 2-9; spaces, dashes, +91 and a "
                               "leading 0 are stripped for you."};
        auto pit = byPhone_.find(ph);
        if (pit != byPhone_.end()) {
            std::size_t at = pit->second;
            bool same = (selfId != 0 && rows_[at].id == selfId);
            if (!same)
                return {false, "Duplicate: phone " + util::fmtPhone(ph) + " already belongs to #" +
                                   std::to_string(rows_[at].id) + " (" + rows_[at].name + ")."};
        }
        c.phone = ph;

        if (!c.email.empty() && !util::isValidEmail(c.email))
            return {false, "Invalid email '" + c.email + "'."};
        if (c.address.size() > 200) return {false, "Address is too long (max 200 characters)."};
        if (c.group.empty()) c.group = "General";
        if (c.group.size() > 24) return {false, "Group name is too long (max 24 characters)."};
        if (c.created.empty()) c.created = util::todayIso();
        return {true, "ok"};
    }

    void linkIndexes(const Contact& c, std::size_t pos) {
        byId_[c.id] = pos;
        byPhone_[c.phone] = pos;
        byName_[c.name].push_back(pos);
        trie_.insert(Trie::key(c.name), c.id);
    }

    void unlinkIndexes(const Contact& c, std::size_t pos) {
        byId_.erase(c.id);
        auto pit = byPhone_.find(c.phone);
        if (pit != byPhone_.end() && pit->second == pos) byPhone_.erase(pit);
        auto nit = byName_.find(c.name);
        if (nit != byName_.end()) {
            auto& v = nit->second;
            v.erase(std::remove(v.begin(), v.end(), pos), v.end());
            if (v.empty()) byName_.erase(nit);
        }
        trie_.erase(Trie::key(c.name), c.id);
    }

    // O(1) delete: move the last record into the hole, then repair only the two
    // index entries that pointed at the old last slot. Cost is independent of n.
    void eraseAt(std::size_t pos) {
        unlinkIndexes(rows_[pos], pos);
        std::size_t last = rows_.size() - 1;
        if (pos != last) {
            Contact moved = std::move(rows_[last]);
            rows_.pop_back();
            unlinkIndexes(moved, last);
            rows_[pos] = std::move(moved);
            linkIndexes(rows_[pos], pos);
        } else {
            rows_.pop_back();
        }
    }

    void pushUndo(Action a) {
        undo_.push_back(std::move(a));
        while (undo_.size() > kUndoDepth) undo_.pop_front();  // forget the oldest
    }

    std::vector<Contact> rows_;
    std::unordered_map<int, std::size_t> byId_;
    std::unordered_map<std::string, std::size_t> byPhone_;
    std::map<std::string, std::vector<std::size_t>, CiLess> byName_;
    Trie trie_;
    int nextId_ = 1;
    std::deque<Action> undo_;
    static const std::size_t kUndoDepth = 20;
};

// ---------------------------------------------------------------------------
// 7. Console presentation helpers
//
// Input state lives in an object, not in a global variable. The style guides
// used to mark this kind of work explicitly forbid non-constant globals.
// ---------------------------------------------------------------------------
class Console {
public:
    bool eof() const { return eof_; }

    std::string line(const std::string& label) {
        std::cout << label;
        std::cout.flush();
        std::string s;
        if (!std::getline(std::cin, s)) {
            eof_ = true;
            std::cout << "\n(input ended)\n";
            return {};
        }
        return s;
    }

    // Reads a whole number and rejects anything that is not exactly one number,
    // so "12abc", "1.5" and out-of-range input do not sneak through.
    bool integer(const std::string& label, long long lo, long long hi, long long& out) {
        std::string s = util::trim(line(label));
        if (eof_) return false;
        if (s.empty()) { std::cout << "  ! nothing entered.\n"; return false; }
        errno = 0;
        char* end = nullptr;
        long long v = std::strtoll(s.c_str(), &end, 10);
        if (errno == ERANGE || end == s.c_str() || *end != '\0') {
            std::cout << "  ! '" << s << "' is not a whole number.\n";
            return false;
        }
        if (v < lo || v > hi) {
            std::cout << "  ! " << v << " is outside the allowed range " << lo << ".." << hi << ".\n";
            return false;
        }
        out = v;
        return true;
    }

    bool yesNo(const std::string& label) {
        std::string s = util::lower(util::trim(line(label)));
        if (eof_) return false;
        return s == "y" || s == "yes";
    }

private:
    bool eof_ = false;
};

void printRule(std::size_t n = 88) { std::cout << std::string(n, '-') << "\n"; }

void printContacts(const std::vector<const Contact*>& v, bool withAddress = true) {
    if (v.empty()) {
        std::cout << "  (no matching contacts)\n";
        return;
    }
    std::cout << util::fit("ID", 4) << " " << util::fit("Name", 22) << " " << util::fit("Phone", 12)
              << " " << util::fit("Group", 10) << " " << util::fit("Email", 24) << " ";
    if (withAddress) std::cout << util::fit("Address", 26) << " ";
    std::cout << util::fit("Added", 10) << "\n";
    printRule(withAddress ? 115 : 90);
    for (const Contact* c : v) {
        std::cout << util::fit(std::to_string(c->id), 4) << " " << util::fit(c->name, 22) << " "
                  << util::fit(util::fmtPhone(c->phone), 12) << " " << util::fit(c->group, 10) << " "
                  << util::fit(c->email, 24) << " ";
        if (withAddress) std::cout << util::fit(c->address, 26) << " ";
        std::cout << util::fit(c->created, 10) << "\n";
    }
    std::cout << "  " << v.size() << " record(s).\n";
}

void printOne(const Contact& c) {
    std::cout << "  ID      : " << c.id << "\n"
              << "  Name    : " << c.name << "\n"
              << "  Phone   : " << util::fmtPhone(c.phone) << "\n"
              << "  Email   : " << (c.email.empty() ? "(none)" : c.email) << "\n"
              << "  Group   : " << c.group << "\n"
              << "  Address : " << (c.address.empty() ? "(none)" : c.address) << "\n"
              << "  Added   : " << c.created << "\n";
}

// ---------------------------------------------------------------------------
// 8. Report: structure sizes, group split, duplicate analysis
// ---------------------------------------------------------------------------
void reportStats(Console& con, const ContactStore& store, const std::string& dataPath) {
    (void)con;  // read-only screen: no input needed
    std::vector<const Contact*> all = store.allById();
    printRule();
    std::cout << "STATISTICS REPORT\n";
    printRule();
    std::cout << "  Contacts stored          : " << store.size() << "\n";
    std::cout << "  Next ID to be issued     : " << store.nextId() << "\n";
    std::cout << "  vector capacity x sizeof : " << store.size() << " x " << sizeof(Contact)
              << " = " << store.size() * sizeof(Contact) << " bytes (payload)\n";
    std::cout << "  idIndex entries          : " << store.indexEntries() << " (id + phone maps)\n";
    std::cout << "  nameIndex buckets        : " << store.nameBuckets() << " distinct names\n";
    std::cout << "  Trie nodes               : " << store.trieNodes() << " (one per distinct name prefix)\n";
    std::cout << "  Undo available           : " << (store.canUndo() ? "yes" : "no") << "\n";
    std::cout << "  CSV file                 : " << dataPath << "\n\n";

    if (all.empty()) {
        std::cout << "  No data yet: add a contact (menu 1) or load the sample set (menu 13).\n";
        printRule();
        return;
    }

    // Group split: std::map keeps the keys sorted for free.
    std::map<std::string, int> groups;
    std::map<char, int> prefixes;
    int noEmail = 0, noAddress = 0;
    const Contact* longest = all.front();
    for (const Contact* c : all) {
        groups[c->group]++;
        if (!c->phone.empty()) prefixes[c->phone[0]]++;
        if (c->email.empty()) ++noEmail;
        if (c->address.empty()) ++noAddress;
        if (c->name.size() > longest->name.size()) longest = c;
    }
    std::cout << "  By group:\n";
    for (const auto& kv : groups)
        std::cout << "    " << util::fit(kv.first, 16) << " " << kv.second << "\n";
    std::cout << "  Leading digit of phone (operator series):\n";
    for (const auto& kv : prefixes)
        std::cout << "    " << kv.first << " : " << kv.second << "\n";
    std::cout << "  Missing data: email " << noEmail << ", address " << noAddress << "\n";
    std::cout << "  Longest name: " << longest->name << " (" << longest->name.size() << " chars)\n\n";

    // Exact duplicate names are allowed; report them so the user can clean up.
    std::map<std::string, std::vector<std::string>> byName;
    for (const Contact* c : all) byName[util::lower(c->name)].push_back(std::to_string(c->id));
    bool anyDup = false;
    for (const auto& kv : byName) {
        if (kv.second.size() > 1) {
            if (!anyDup) std::cout << "  Contacts sharing a name (phones are still unique):\n";
            anyDup = true;
            std::cout << "    " << util::fit(kv.first, 24) << " ids ";
            for (const std::string& s : kv.second) std::cout << s << " ";
            std::cout << "\n";
        }
    }
    if (!anyDup) std::cout << "  No contacts share a name.\n";

    // Near-duplicate names: O(n^2 * L^2). Capped so the demo never hangs.
    const std::size_t kPairCap = 200;
    if (all.size() > kPairCap) {
        std::cout << "  Near-duplicate name scan skipped (" << all.size() << " > " << kPairCap
                  << " records; the pairwise scan is O(n^2)).\n";
    } else {
        std::cout << "\n  Near-duplicate names (edit distance <= 2, likely typos):\n";
        bool found = false;
        for (std::size_t i = 0; i < all.size(); ++i)
            for (std::size_t j = i + 1; j < all.size(); ++j) {
                int d = fuzzy::damerauLevenshtein(util::lower(all[i]->name), util::lower(all[j]->name));
                if (d > 0 && d <= 2) {
                    found = true;
                    std::cout << "    distance " << d << " : #" << all[i]->id << " " << all[i]->name
                              << "  <->  #" << all[j]->id << " " << all[j]->name << "\n";
                }
            }
        if (!found) std::cout << "    none\n";
    }
    printRule();
}

// ---------------------------------------------------------------------------
// 9. Sorting experiment: std::sort vs std::stable_sort, with real counters
// ---------------------------------------------------------------------------
void sortExperiment(Console& con, const ContactStore& store) {
    (void)con;  // run from stored data, no questions asked
    std::vector<const Contact*> base = store.allById();
    printRule();
    std::cout << "SORTING EXPERIMENT (key = name, case-insensitive)\n";
    printRule();
    if (base.empty()) { std::cout << "  Nothing to sort yet.\n"; printRule(); return; }

    auto key = [](const Contact* a, const Contact* b) { return util::lower(a->name) < util::lower(b->name); };

    SortStats s1, s2;
    std::vector<const Contact*> unstable = base;
    std::sort(unstable.begin(), unstable.end(),
              [&](const Contact* a, const Contact* b) { ++s1.comparisons; return key(a, b); });
    std::vector<const Contact*> stable = base;
    std::stable_sort(stable.begin(), stable.end(),
                     [&](const Contact* a, const Contact* b) { ++s2.comparisons; return key(a, b); });

    std::size_t n = base.size();
    double nlogn = (n > 1) ? static_cast<double>(n) * std::log2(static_cast<double>(n)) : 1.0;
    std::cout << "  records                     : " << n << "\n";
    std::cout << "  n * log2(n)                 : " << static_cast<long long>(nlogn) << " (lower bound for comparison sorts)\n";
    std::cout << "  std::sort       comparisons : " << s1.comparisons << "\n";
    std::cout << "  std::stable_sort comparisons: " << s2.comparisons << "\n";
    std::cout << "  both copied the source first, so input was already in ID order.\n\n";

    std::cout << "  Result order (ID : Name)\n";
    std::cout << "    sort        :";
    for (const Contact* c : unstable) std::cout << " " << c->id;
    std::cout << "\n    stable_sort :";
    for (const Contact* c : stable) std::cout << " " << c->id;
    std::cout << "\n";
    std::size_t tiePairs = 0;
    for (std::size_t i = 0; i < base.size(); ++i)
        for (std::size_t j = i + 1; j < base.size(); ++j)
            if (util::lower(base[i]->name) == util::lower(base[j]->name)) ++tiePairs;
    std::cout << "  records sharing a name here: " << tiePairs << " pair(s)\n";
    if (stable == unstable)
        std::cout << "  Note: both orders matched this time. That is luck, not a guarantee:\n"
                     "        std::sort may reorder records that share a name. See the stress test.\n";
    else
        std::cout << "  Note: outputs differ. Equal keys keep ID order only under std::stable_sort.\n";
    // A second run on synthetic data with many repeated names, so the stability
    // difference is visible instead of being a claim in a slide.
    {
        const char* pool[] = {"Ravi Kumar", "Anita Rao", "Suresh Nair", "Kiran Das",
                              "Neha Bose", "Amit Jain", "Lata Menon", "Prakash Reddy"};
        std::vector<const Contact*> tieSet;
        std::vector<Contact> owner;
        owner.reserve(64);
        // Interleave the names so equal keys are not already adjacent on input.
        for (int round = 0; round < 8; ++round)
            for (int k = 0; k < 8; ++k) {
                Contact c;
                c.id = static_cast<int>(owner.size()) + 1;
                c.name = pool[(k * 3 + round) % 8];
                c.phone = "9000000000";
                owner.push_back(c);
            }
        for (const Contact& c : owner) tieSet.push_back(&c);

        auto countBrokenTies = [](const std::vector<const Contact*>& v) {
            long long broken = 0;
            for (std::size_t i = 0; i < v.size(); ++i)
                for (std::size_t j = i + 1; j < v.size(); ++j)
                    if (util::lower(v[i]->name) == util::lower(v[j]->name) && v[i]->id > v[j]->id)
                        ++broken;
            return broken;
        };
        std::vector<const Contact*> u = tieSet, t = tieSet, tot = tieSet;
        std::sort(u.begin(), u.end(), [](const Contact* a, const Contact* b) {
            return util::lower(a->name) < util::lower(b->name);
        });
        std::stable_sort(t.begin(), t.end(), [](const Contact* a, const Contact* b) {
            return util::lower(a->name) < util::lower(b->name);
        });
        std::cout << "\n  Stress test on " << tieSet.size()
                  << " synthetic records with 8 repeated names:\n";
        std::cout << "    std::sort        reordered " << countBrokenTies(u)
                  << " pair(s) of records that share a name\n";
        std::cout << "    std::stable_sort reordered " << countBrokenTies(t)
                  << " pair(s) of records that share a name\n";
        // The practical alternative to relying on stability: make the comparator a
        // total order by breaking ties on the ID.
        std::sort(tot.begin(), tot.end(), [](const Contact* a, const Contact* b) {
            std::string x = util::lower(a->name), y = util::lower(b->name);
            if (x != y) return x < y;
            return a->id < b->id;
        });
        std::cout << "    std::sort + (name, id) key  reordered " << countBrokenTies(tot)
                  << " pair(s) of records that share a name\n";
        std::cout << "    -> equal keys swap position under std::sort, so records can appear to\n"
                     "       jump around between runs. Two fixes: std::stable_sort, or break the\n"
                     "       tie in the comparator so the key is a total order. Both give 0.\n";
    }

    std::cout << "  Sorted by name:\n";
    for (const Contact* c : stable)
        std::cout << "    #" << util::fit(std::to_string(c->id), 3) << " " << util::fit(c->name, 22)
                  << " " << util::fmtPhone(c->phone) << "\n";
    printRule();
}

// ---------------------------------------------------------------------------
// 10. Sample data used for the demonstration
// ---------------------------------------------------------------------------
void loadSamples(ContactStore& store) {
    struct Seed { const char* name; const char* phone; const char* email; const char* group; const char* addr; };
    const Seed seeds[] = {
        {"Aarav Sharma",   "9876543210",      "aarav.sharma@example.com", "Family", "12, MG Road, Bengaluru"},
        {"Ananya Iyer",    "9812345678",      "ananya@example.com",       "Work",   "Flat 3B, \"Sunrise\" Apartments, Pune"},
        {"Ananya Iyer",    "+91 9900112233",  "ananya.iyer@work.in",      "Work",   "Same office, Hyderabad"},
        {"Rohan Verma",    "+91 90000 11111", "",                         "Friend", "Sector 21, Noida"},
        {"Priya Nair",     "9445566778",      "priya.nair@example.com",   "Friend", "Kochi"},
        {"Vikram Singh",   "0755-4000123",    "",                         "Work",   "Bhopal"},
        {"Meera Krishnan", "8899776655",      "meera.k@example.com",      "Family", "Chennai"},
        {"Arjun Reddy",    "7012345678",      "",                         "Friend", "Hyderabad"},
        {"Kavya Menon",    "9955667788",      "kavya@example.com",        "Family", ""},
        {"Sanjay Gupta",   "9632587410",      "sanjay.g@example.com",     "General","Kolkata"},
    };
    int added = 0, refused = 0;
    for (const Seed& s : seeds) {
        Contact c;
        c.name = s.name; c.phone = s.phone; c.email = s.email; c.group = s.group; c.address = s.addr;
        ContactStore::Result r = store.add(c);
        if (r.ok) ++added; else ++refused;
    }
    std::cout << "\n  Sample data: " << added << " contact(s) added";
    if (refused) std::cout << ", " << refused << " refused as duplicates";
    std::cout << ".\n  Note the deliberate pair of contacts both named 'Ananya Iyer'.\n";
}

// ---------------------------------------------------------------------------
// 11. Menu handlers
// ---------------------------------------------------------------------------
void menuAdd(Console& con, ContactStore& store) {
    std::cout << "--- 1. Add contact (leave optional fields blank to skip) ---\n";
    Contact c;
    c.name  = con.line("  Name        : ");
    if (con.eof()) return;
    c.phone = con.line("  Phone       : ");
    if (con.eof()) return;
    c.email = con.line("  Email       : ");
    if (con.eof()) return;
    c.group = con.line("  Group       : ");
    if (con.eof()) return;
    c.address = con.line("  Address     : ");
    if (con.eof()) return;
    int id = 0;
    ContactStore::Result r = store.add(c, &id);
    std::cout << "\n" << (r.ok ? "  OK: " : "  REJECTED: ") << r.msg << "\n";
}

void menuDisplay(Console& con, ContactStore& store) {
    std::cout << "--- 2. Display all contacts ---\n";
    if (store.size() == 0) {
        std::cout << "  The list is empty. Add a contact first (menu 1).\n";
        return;
    }
    std::cout << "  Sort by: 1 Name  2 Phone  3 ID  4 Group  5 Date added\n";
    long long k = 0;
    if (!con.integer("  Choice [1-5, default 1]: ", 0, 5, k)) {
        if (con.eof()) return;
        k = 1;
    }
    if (k == 0) k = 1;
    std::string ord = util::lower(util::trim(con.line("  Order: a ascending / d descending [default a]: ")));
    if (con.eof()) return;
    bool asc = (ord != "d" && ord != "desc" && ord != "descending");
    ContactStore::SortKey key = ContactStore::SortKey::Name;
    const char* keyName = "name";
    switch (k) {
        case 2: key = ContactStore::SortKey::Phone;   keyName = "phone";     break;
        case 3: key = ContactStore::SortKey::Id;      keyName = "id";        break;
        case 4: key = ContactStore::SortKey::Group;   keyName = "group";     break;
        case 5: key = ContactStore::SortKey::Created; keyName = "date";      break;
        default: key = ContactStore::SortKey::Name;   keyName = "name";      break;
    }
    SortStats st;
    std::vector<const Contact*> v = store.sorted(key, asc, &st);
    std::cout << "\n  " << v.size() << " record(s), sorted by " << keyName << " "
              << (asc ? "ascending" : "descending") << "; " << st.comparisons
              << " comparisons performed.\n";
    printContacts(v);
}

void menuSearchName(Console& con, ContactStore& store) {
    std::cout << "--- 3. Search by name ---\n";
    if (store.size() == 0) { std::cout << "  The list is empty; nothing to search.\n"; return; }
    std::string q = con.line("  Search text: ");
    if (con.eof()) return;
    if (util::trim(q).empty()) { std::cout << "  ! Empty search text; nothing done.\n"; return; }
    std::cout << "  Mode: 1 exact (O(log n))  2 prefix (O(log n + k))  3 contains (O(n))  4 fuzzy typo-tolerant\n";
    long long m = 0;
    if (!con.integer("  Choice [1-4, default 3]: ", 0, 4, m)) { if (con.eof()) return; m = 3; }
    if (m == 0) m = 3;
    const char* label = "contains";
    ContactStore::NameMode mode = ContactStore::NameMode::Contains;
    switch (m) {
        case 1: mode = ContactStore::NameMode::Exact;    label = "exact";    break;
        case 2: mode = ContactStore::NameMode::Prefix;   label = "prefix";   break;
        case 4: mode = ContactStore::NameMode::Fuzzy;    label = "fuzzy";    break;
        default: break;
    }
    std::vector<const Contact*> v = store.searchByName(q, mode);
    std::cout << "\n  " << v.size() << " match(es) for " << label << " search '" << util::trim(q) << "':\n";
    printContacts(v, false);
    if (mode == ContactStore::NameMode::Fuzzy)
        std::cout << "  Fuzzy mode scores every record with Damerau-Levenshtein, so it is O(n * L^2).\n";
}

void menuSearchPhone(Console& con, ContactStore& store) {
    std::cout << "--- 4. Search by phone number ---\n";
    if (store.size() == 0) { std::cout << "  The list is empty; nothing to search.\n"; return; }
    std::string q = con.line("  Phone (full or partial): ");
    if (con.eof()) return;
    if (util::trim(q).empty()) { std::cout << "  ! Empty search text; nothing done.\n"; return; }
    std::vector<const Contact*> v = store.searchByPhone(q);
    bool exact = (v.size() == 1 && v[0]->phone == util::normalizePhone(q));
    std::cout << "\n  " << v.size() << " match(es). " << (exact ? "Exact hash hit, O(1)." : "Partial match required a linear scan, O(n).") << "\n";
    printContacts(v, false);
}

void menuUpdate(Console& con, ContactStore& store) {
    std::cout << "--- 5. Update contact details ---\n";
    if (store.size() == 0) { std::cout << "  The list is empty; nothing to update.\n"; return; }
    long long id = 0;
    if (!con.integer("  ID to update: ", 1, 1000000000LL, id)) return;
    const Contact* cur = store.findById(static_cast<int>(id));
    if (!cur) { std::cout << "  ! No contact with ID " << id << ".\n"; return; }
    std::cout << "  Current record:\n";
    printOne(*cur);
    Contact nv = *cur;
    std::cout << "  Enter a new value, or press Enter to keep the current one.\n"
                 "  For email, group and address, enter '-' to clear the field.\n";
    auto ask = [&con](const std::string& label, std::string& field, bool clearable) {
        std::string shown = field.empty() ? "(empty)" : field;
        std::string s = util::trim(con.line("  " + label + " [" + shown + "]: "));
        if (con.eof()) return false;
        if (s.empty()) return true;
        if (clearable && s == "-") { field.clear(); return true; }
        field = s;
        return true;
    };
    if (!ask("Name   ", nv.name, false)) return;
    if (!ask("Phone  ", nv.phone, false)) return;
    if (!ask("Email  ", nv.email, true)) return;
    if (!ask("Group  ", nv.group, true)) return;
    if (!ask("Address", nv.address, true)) return;
    ContactStore::Result r = store.update(static_cast<int>(id), nv);
    std::cout << "\n" << (r.ok ? "  OK: " : "  REJECTED: ") << r.msg << "\n";
}

void menuDelete(Console& con, ContactStore& store) {
    std::cout << "--- 6. Delete contact ---\n";
    if (store.size() == 0) { std::cout << "  The list is empty; nothing to delete.\n"; return; }
    long long id = 0;
    if (!con.integer("  ID to delete: ", 1, 1000000000LL, id)) return;
    const Contact* cur = store.findById(static_cast<int>(id));
    if (!cur) { std::cout << "  ! No contact with ID " << id << ".\n"; return; }
    std::cout << "  About to delete:\n";
    printOne(*cur);
    if (!con.yesNo("  Confirm delete? (y/N): ")) { std::cout << "  Cancelled; nothing removed.\n"; return; }
    ContactStore::Result r = store.remove(static_cast<int>(id));
    std::cout << "\n" << (r.ok ? "  OK: " : "  REJECTED: ") << r.msg << "\n";
}

void menuAutocomplete(Console& con, const ContactStore& store) {
    std::cout << "--- 8. Autocomplete a name (trie) ---\n";
    if (store.size() == 0) { std::cout << "  The list is empty.\n"; return; }
    std::string p = con.line("  Type a prefix: ");
    if (con.eof()) return;
    std::vector<const Contact*> v = store.autocomplete(p);
    std::cout << "\n  " << v.size() << " suggestion(s) for prefix '" << util::trim(p) << "':\n";
    std::size_t shown = 0;
    for (const Contact* c : v) {
        if (shown++ == 10) { std::cout << "  ... " << (v.size() - 10) << " more\n"; break; }
        std::cout << "    " << util::fit(c->name, 22) << " " << util::fmtPhone(c->phone) << "  (#"
                  << c->id << ")\n";
    }
    if (v.empty() && !util::trim(p).empty())
        std::cout << "  (trie has no path for this prefix; that is an O(|prefix|) negative result)\n";
}

void menuAudit(Console& con, const ContactStore& store) {
    (void)con;
    std::cout << "--- 14. Validate indexes ---\n";
    if (store.size() == 0) { std::cout << "  The list is empty; nothing to validate.\n"; return; }
    ContactStore::Audit a = store.audit();
    std::cout << "  records " << a.rows << ", id entries " << a.idEntries
              << ", phone entries " << a.phoneEntries << ", name slots " << a.nameSlots
              << ", trie hits " << a.trieHits << "\n";
    if (a.ok) {
        std::cout << "  RESULT: every index agrees with the record array.\n"
                     "  This is the check that proves the O(1) delete repaired the right entries.\n";
        return;
    }
    std::cout << "  RESULT: " << a.problems.size() << " problem(s) detected.\n";
    for (const std::string& p : a.problems) std::cout << "    - " << p << "\n";
}

void menuUndo(Console& con, ContactStore& store) {
    (void)con;
    std::cout << "--- 9. Undo last change ---\n";
    std::string what;
    bool ok = store.undo(what);
    std::cout << "\n  " << (ok ? "OK: " : "  ") << what << "\n";
}

#ifndef CMS_NO_MAIN
int main(int argc, char** argv) {
    std::string dataPath = "contacts.csv";
    bool wantSeed = false, auditAndExit = false;
    int injectFault = 0;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--data" && i + 1 < argc) dataPath = argv[++i];
        else if (a == "--seed") wantSeed = true;
        else if (a == "--inject-fault" && i + 1 < argc) injectFault = std::atoi(argv[++i]);
        else if (a == "--audit") auditAndExit = true;
        else if (a == "-h" || a == "--help") {
            std::cout << "usage: cms [--data FILE] [--seed] [--audit] [--inject-fault N]\n"
                         "  --data FILE      CSV file to load at start and save into (default contacts.csv)\n"
                         "  --seed           add the built-in sample contacts if the list is empty\n"
                         "  --audit          run the index self-check, print the result and exit\n"
                         "  --inject-fault N  testing hook, corrupt index N (1-5) after loading, then audit\n";
            return 0;
        } else {
            std::cout << "Ignoring unknown option '" << a << "'\n";
        }
    }

    Console con;
    ContactStore store;
    bool existed = false;
    ContactStore::Result lr = store.load(dataPath, &existed);
    std::cout << "=============================================================\n";
    std::cout << " CONTACT MANAGEMENT SYSTEM  (Group 11, C++17)\n";
    std::cout << "=============================================================\n";
    std::cout << (lr.ok ? "  " : "  ERROR: ") << lr.msg << "\n";
    if (!lr.ok) {
        std::cout << "  Starting with an empty in-memory list; the file was not modified.\n";
        store = ContactStore{};
    }
    if (wantSeed && store.size() == 0) loadSamples(store);
    if (injectFault != 0) {
        store.injectFaultForTest(injectFault);
        std::cout << "  TEST HOOK: index fault " << injectFault << " injected on purpose.\n";
    }
    if (auditAndExit) {
        ContactStore::Audit a = store.audit();
        if (a.ok) std::cout << "  AUDIT PASSED: " << a.rows << " records, " << a.idEntries
                            << " id entries, " << a.phoneEntries << " phone entries, " << a.nameSlots
                            << " name slots, " << a.trieHits << " trie hits, all consistent.\n";
        else {
            std::cout << "  AUDIT FAILED with " << a.problems.size() << " problem(s):\n";
            for (const std::string& p : a.problems) std::cout << "    - " << p << "\n";
        }
        return a.ok ? 0 : 1;
    }
    std::cout << "  Structures in memory: vector<Contact> " << store.size()
              << " rows, id map " << store.size()
              << " keys, phone map " << store.size()
              << " keys, name map " << store.nameBuckets()
              << " buckets, trie " << store.trieNodes() << " nodes.\n";

    bool running = true, savedOnExit = false;
    while (running && !con.eof()) {
        std::cout << "\n";
        printRule(60);
        std::cout << "  1  Add contact\n"
                     "  2  Display all contacts (choose sort)\n"
                     "  3  Search by name\n"
                     "  4  Search by phone number\n"
                     "  5  Update contact details\n"
                     "  6  Delete contact\n"
                     "  7  Sort alphabetically / comparison experiment\n"
                     "  8  Autocomplete a name (trie)\n"
                     "  9  Undo last change\n"
                     " 10  Statistics report\n"
                     " 11  Save to file\n"
                     " 12  Reload from file\n"
                     " 13  Load sample data\n"
                     " 14  Validate indexes (self-check)\n"
                     "  0  Exit\n";
        printRule(60);
        long long choice = -1;
        if (!con.integer("Choice: ", 0, 14, choice)) continue;
        switch (choice) {
            case 1: menuAdd(con, store); break;
            case 2: menuDisplay(con, store); break;
            case 3: menuSearchName(con, store); break;
            case 4: menuSearchPhone(con, store); break;
            case 5: menuUpdate(con, store); break;
            case 6: menuDelete(con, store); break;
            case 7: sortExperiment(con, store); break;
            case 8: menuAutocomplete(con, store); break;
            case 9: menuUndo(con, store); break;
            case 10: reportStats(con, store, dataPath); break;
            case 11: { ContactStore::Result r = store.save(dataPath);
                       std::cout << "\n  " << (r.ok ? "OK: " : "ERROR: ") << r.msg << "\n"; } break;
            case 12: { bool ex = false; ContactStore::Result r = store.load(dataPath, &ex);
                       std::cout << "\n  " << (r.ok ? "OK: " : "ERROR: ") << r.msg << "\n"; } break;
            case 13: loadSamples(store); break;
            case 14: menuAudit(con, store); break;
            case 0:
                running = false;
                savedOnExit = true;
                if (store.size() > 0) {
                    ContactStore::Result r = store.save(dataPath);
                    std::cout << "  " << (r.ok ? "OK: " : "ERROR: ") << r.msg << "\n";
                }
                std::cout << "  Goodbye.\n";
                break;
            default: std::cout << "  ! Option not handled.\n"; break;
        }
    }
    // The input stream ended (Ctrl-D, or a script ran out) instead of the user
    // choosing Exit. Save anyway, so ending a session never silently discards work.
    if (!savedOnExit && store.size() > 0) {
        ContactStore::Result r = store.save(dataPath);
        std::cout << "\n  Input ended. " << (r.ok ? "OK: " : "ERROR: ") << r.msg << "\n";
    }
    return 0;
}
#endif  // CMS_NO_MAIN

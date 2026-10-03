# Contact Management System

## A menu-driven C++ application demonstrating data structures, searching and sorting

**Term Project Report**

| | |
|---|---|
| **Project title** | Contact Management System |
| **Group** | Group 11 |
| **Language** | C++ (ISO C++17) |
| **Repository** | `contact-management-system` |
| **Document** | `docs/report.md` |

---

## Abstract

This project implements a menu-driven contact management application in C++17. It stores personal
contacts and supports the seven operations required by the brief: add, delete, search by name,
search by phone number, update, display and sort.

The design decision that shapes the whole project is that a contact list is queried in four
different ways, and no single data structure answers all four efficiently. A plain array answers
"show me everything" cheaply but costs O(n) for every lookup. A hash table answers exact lookups in
O(1) but has no ordering, so it cannot answer "all names beginning with An". A balanced search tree
answers range and prefix questions in O(log n) but is slower than a hash table for exact lookups.
A trie answers prefix questions in time proportional to the prefix length.

The implemented design therefore keeps **one array of records as the storage of record, and four
indexes maintained over it**: an identity index, a phone index, an ordered name index and a prefix
trie. Every mutation passes through two functions that are the only code that touches the indexes,
which keeps the indexes consistent by construction. An index audit screen verifies the central
invariant, and a fault-injection hook proves the audit can actually fail.

The project also measures itself. A separate benchmark program times each chosen structure against
its obvious alternatives on the same generated data at 1,000, 10,000 and 100,000 records, and
writes the results into a report. On the development machine, an exact phone lookup costs about
94 nanoseconds through the hash index against about 69,974 nanoseconds for a linear scan at
100,000 records, and deleting a record is about 46 times faster than the naive array deletion.

**Keywords:** data structures, hash table, balanced tree, trie, complexity analysis, editing
distance, stable sorting, C++17.

---

## Table of contents

1. Introduction
2. Tools and environment
3. System design
4. Data structure selection and justification
5. Algorithms and pseudocode
6. Implementation
7. Testing
8. Results and performance analysis
9. Edge cases handled
10. Requirement traceability
11. Limitations and future scope
12. Conclusion
13. References
- Appendix A: sample run
- Appendix B: viva questions

---

## 1. Introduction

### 1.1 Problem statement

As given in the project brief:

> "Your team is required to design and implement a C++ application for contact management system.
> The application should be menu-driven and should allow the user to perform the required
> operations through clear options. The implementation must demonstrate appropriate use of the DSA
> concepts taught in class."

The brief further requires that students "explain why each selected data structure was used", and
that the demonstration show at least two edge cases.

### 1.2 Objectives

1. Implement all seven mandatory contact operations behind a clear text menu.
2. Use structures, strings, STL containers, searching and sorting as the brief requires.
3. Choose each data structure for a stated reason, with a complexity cost attached.
4. Handle the five failure classes named in the brief: empty records, invalid IDs, duplicate
   entries, unavailable resources and invalid input.
5. Produce measured evidence so that every performance claim can be verified rather than asserted.

### 1.3 Scope

The application is a single-user, single-threaded console program. It holds its records in memory
for the duration of the run and persists them to a CSV file on exit. It has no network, database
or graphical interface, because the brief specifies a menu-driven application.

### 1.4 Why a contact manager is a good data-structure exercise

A contact list looks trivial and is not. The same collection of records is reached through four
different keys (position, identity, phone number, name prefix), it must reject duplicates on one
key while permitting them on another, it must survive text containing the delimiter used for
storage, and it must remain consistent while records are added and removed. Each of those
requirements maps onto a standard data structure, which is what makes the exercise useful.

---

## 2. Tools and environment

| Item | Value |
|---|---|
| Language | C++17 (`-std=c++17`) |
| Compiler | Apple clang 21.0.0 (arm64), verified warning-clean |
| Build flags | `-O2 -Wall -Wextra -Wpedantic -Werror` |
| Build system | `make` (plus a single-command manual build) |
| Test harness | Python 3 standard library only |
| Libraries | C++ standard library only; no third-party dependencies |
| Platform calls | None. No `system()`, no `conio.h`, no Windows-only headers |
| Editor / IDE | Any |

Hardware: any machine capable of running a C++17 compiler. The benchmarks in Section 8 were taken
on an Apple Silicon machine; the shape of the measured curves does not depend on the machine, only
the absolute timings do.

---

## 3. System design

### 3.1 Data model

The record is a structure. This is deliberate: the alternative, parallel arrays of names and phone
numbers, links the fields only by convention, so sorting or deleting from one array without the
other silently corrupts the data.

```cpp
struct Contact {
    int         id;        // monotonic, assigned by the store, never reused
    std::string name;      // display name, spaces preserved, internal runs collapsed
    std::string phone;     // stored canonical: exactly 10 digits, no separators
    std::string email;     // optional
    std::string group;     // Family / Friend / Work / General, defaults to General
    std::string address;   // free text, may contain commas, quotes and newlines
    std::string created;   // ISO date, immutable after creation
};
```

Two choices deserve comment.

**The phone number is a string, not an integer.** Every Indian mobile number starts with a digit
from 6 to 9, so a 10-digit number such as 9876543210 exceeds the range of a signed 32-bit integer
and silently becomes 2147483647. A number beginning with 0 would also lose its leading zero.
Storing the phone as a string removes both failure modes and permits digit-level validation.

**The name is not unique.** Two people may share a name, and a household may share a landline. The
identity index uses the assigned integer ID, the phone index enforces uniqueness because a phone
number identifies a line, and the name index maps one name to a list of positions rather than a
single position.

### 3.2 Architecture

One array of records, and four indexes over it.

```
                    ┌──────────────────────────────────────────┐
                    │  rows_   std::vector<Contact>            │
                    │  the storage of record, dense and        │
                    │  contiguous: [0] [1] [2] [3] ... [n-1]   │
                    └──────────────────────────────────────────┘
                          ▲      ▲          ▲            ▲
        position (size_t) │      │          │            │
                          │      │          │            │
   ┌──────────────────────┴──┐ ┌─┴────────┐ ┌┴──────────┐ ┌┴─────────────────────┐
   │ byId_                   │ │ byPhone_ │ │ byName_   │ │ trie_                │
   │ unordered_map<int,size_t>│ │ unord_map│ │ std::map< │ │ Trie (prefix tree)   │
   │ id  -> position         │ │ phone->  │ │ name ->   │ │ name prefix -> ids   │
   │ O(1) average            │ │ position │ │ vector<   │ │ O(prefix length)     │
   │                         │ │ O(1) avg │ │ size_t>   │ │                      │
   │                         │ │          │ │ O(log n)  │ │                      │
   └─────────────────────────┘ └──────────┘ └───────────┘ └──────────────────────┘

   ┌─────────────────────────────────────────────────────────────────────────────┐
   │ undo_  std::deque<Action>   last 20 changes, newest at the back             │
   └─────────────────────────────────────────────────────────────────────────────┘
```

The indexes store **positions (indices), never pointers**. The vector reallocates its memory when
it grows, and the delete strategy moves records between slots; a stored pointer would dangle, while
a position remains meaningful as long as the invariant below is maintained.

### 3.3 The central invariant

> For every record at position *p*, every index that contains that record maps back to *p*. No
> index contains a key that no record owns.

Every operation must leave this true. It is checked directly by the audit screen (menu option 14),
which walks every record and every index entry in both directions.

### 3.4 Module design

The code is organised into eleven labelled sections in one file.

| Section | Responsibility |
|---|---|
| 1. Utilities | string trimming, folding, comparison; phone normalisation and validation; email validation; table formatting |
| 2. Edit distance | Damerau-Levenshtein with O(min(m,n)) space |
| 3. Trie | node type, insert, erase with pruning, prefix enumeration, node count |
| 4. CSV | field escaping, record joining, and a state-machine parser |
| 5. Record | `struct Contact` |
| 6. `ContactStore` | the five containers and every operation on them |
| 7. Console | input reading and validation, table printing |
| 8. Report | statistics, group histogram, duplicate and near-duplicate analysis |
| 9. Sorting experiment | comparison counters, stability stress test |
| 10. Sample data | ten seed records |
| 11. Menu handlers | one function per menu option, plus `main` |

One function per major operation, as the brief requires. No non-constant global variables: input
state lives in a `Console` object that is passed explicitly.

---

## 4. Data structure selection and justification

This section is the direct answer to the brief's requirement that each choice be explained.

### 4.1 Which structure answers which question

| Question asked of the data | Structure | Cost | Why not the alternatives |
|---|---|---|---|
| "Show me all contacts" | `std::vector<Contact>` | O(n) scan, O(1) amortised append | A `std::list` would scatter records across the heap and give up contiguous iteration; sorting and printing both walk memory linearly, so density is valuable |
| "Give me the record with ID 7" | `std::unordered_map<int, size_t>` | O(1) average | Linear search is O(n); a `std::map` would cost O(log n) for no benefit, since IDs are never needed in order |
| "Who owns 9876543210?" and "Is this number already stored?" | `std::unordered_map<std::string, size_t>` | O(1) average | A sorted array with binary search costs O(log n); a scan costs O(n). The duplicate check runs on every add and every update, so it must be O(1) |
| "All names beginning with An" | `std::map<std::string, std::vector<size_t>, CiLess>` | O(log n + k) | A hash table **cannot** answer a prefix question at all, because hashing destroys order. This is the single clearest illustration of why both a hash table and an ordered tree appear in the same program |
| "Suggest a name from the letters Sa" | Trie | O(prefix length + k) | A trie is independent of how many keys exist. It also emits matches in alphabetical order without a sort, because children are visited in character order |
| "Undo my last change" | `std::deque<Action>` | O(1) | Last-in first-out is a stack, but a `std::stack` cannot drop its oldest entry, and the history is deliberately bounded to 20 |

### 4.2 Complexity of every operation

*n* = number of contacts, *k* = number of matches returned, *p* = prefix length, *L* = name length,
*q* = query length.

| Operation | Time | Space | Structure responsible |
|---|---|---|---|
| Add contact | O(1) amortised + O(log n) + O(L) | O(1) amortised | vector append, name-index insert, trie insert |
| Find by ID | O(1) average | O(1) | id hash index |
| Find by phone, exact | O(1) average | O(1) | phone hash index |
| Find by phone, partial | O(n·L) | O(1) | linear scan (a partial key cannot use a hash) |
| Find by name, exact | O(log n + k) | O(k) | ordered name index |
| Find by name, prefix | O(log n + k) | O(k) | ordered name index, one `lower_bound` |
| Find by name, substring | O(n·L) | O(k) | linear scan (no index helps an arbitrary substring) |
| Find by name, typo-tolerant | O(n·L²) | O(L) | Damerau-Levenshtein per record |
| Autocomplete | O(p + k) | O(k) | trie |
| Update contact | O(1) + O(L) | O(1) | locate by ID, repair two index entries |
| Delete contact | O(1) + O(L) | O(1) | swap-with-last, repair one record |
| Display, sorted | O(n log n) | O(n) | `std::stable_sort` over n positions |
| Save to file | O(n) | O(n) | CSV writer |
| Load from file | O(n·L + n log n) | O(n) | CSV parser plus index rebuild |
| Index audit | O(n·(L + log n)) | O(n) | walks every record and index entry |
| Undo | O(1) + O(L) | O(1) | deque action |

### 4.3 The delete strategy, and why it is not `vector::erase`

`std::vector::erase` is O(n) because every element after the erased position must be moved one slot
left. The implementation instead moves the **last** record into the hole and pops the tail. One
record moved, so exactly one record needs re-indexing: unlink it from the old last slot, relink it
at the new slot.

Measured at 100,000 records: 50.72 ms against 2330.77 ms for `vector::erase`, a factor of 46.0.
At 1,000 records the swap strategy is slightly slower (0.23 ms against 0.19 ms) because index
repair is a fixed cost per delete while the memmove is tiny at that size. Reporting that honestly
is more useful than claiming a uniform win.

Storage order becomes arbitrary after such a delete. That is acceptable because order is a property
of the **view**, not of the array: every display sorts by a chosen key, and IDs are monotonic, so
insertion order is always recoverable by sorting on ID.

### 4.4 Rejected alternatives, with reasons

| Alternative | Why it was rejected |
|---|---|
| Parallel arrays for name and phone | No compiler-enforced link between the arrays. Sorting one without the other attaches phone numbers to the wrong names, silently |
| A single `std::map<string, Contact>` keyed on name | Names are not unique, so the key cannot be the identity; it also cannot answer phone lookups |
| A single `std::unordered_map` for everything | Cannot answer prefix or ordered queries at all |
| A `std::list` for storage | Constant-time deletion at a known iterator is attractive, but the code already achieves O(1) deletion with a vector, and a list destroys the contiguous iteration that sorting and printing rely on |
| `std::multimap` for the name index | It permits duplicate keys, which is correct, but it can only collect a name's records through `equal_range`, and prefix ranges still require the ordered traversal that `std::map` already provides |
| Storing the record inside each index | Tripling the memory and creating three copies to keep in sync. Indexes hold positions; the record exists once |
| A `std::set<Contact>` with the whole record as the key | Any field update requires erasing and reinserting the whole record, and duplicates would be impossible even where they are legitimate |

### 4.5 Sorting: stability and the lower bound

Comparison sorting requires at least O(n log n) comparisons. The program prints that bound beside
the measured comparison count, so the claim is checked rather than asserted.

`std::sort` is not stable. Two contacts with equal keys may exchange positions, so two people both
named "Ananya Iyer" can appear in a different order between runs. Two fixes exist:

1. `std::stable_sort`, which preserves the input order of equal keys and is documented as costing
   O(n log² n) comparisons unless spare memory is available.
2. Break the tie inside the comparator so the key `(lower(name), id)` is a **total order**. Then
   any sort is deterministic.

The program implements and measures both. On 64 synthetic records with 8 repeated names,
`std::sort` reordered 95 pairs of records sharing a name, while `std::stable_sort` and
`std::sort` with the total-order key each reordered 0.

### 4.6 Memory cost of the design

The indexes duplicate keys, not records. At 100,000 records the payload is 14.50 MB of `Contact`
structures, and the trie holds 336,799 nodes, roughly 3.4 nodes per name. The trie is the most
expensive structure per record, which is the honest reason it is one menu option rather than the
primary name index.

---

## 5. Algorithms and pseudocode

Numbers in brackets are the line ranges in `src/contact_manager.cpp`.

### 5.1 Normalise a phone number

```
function NORMALISE(input):
    digits <- keep only characters 0-9 from input
    if length(digits) > 10 and digits starts with "91":
        digits <- digits without the first 2 characters
    if length(digits) > 10 and digits starts with "0":
        digits <- digits without the first character
    return digits

function VALID(digits):
    return length(digits) == 10 and digits[0] is between '2' and '9'
```

Normalisation runs **before** the duplicate check. Otherwise `098765-43210` and `9876543210` would
be accepted as two different contacts.

### 5.2 Add a contact

```
function ADD(raw):
    name <- squeezeSpaces(trim(raw.name))
    if name is empty or length(name) > 60 or contains a control character: return REJECT
    if name consists only of digits: return REJECT
    phone <- NORMALISE(raw.phone)
    if not VALID(phone): return REJECT
    if phone exists in byPhone_: return REJECT naming the existing owner
    if raw.email is not empty and not VALID_EMAIL(raw.email): return REJECT
    if raw.group is empty: raw.group <- "General"

    record.id      <- nextId; nextId <- nextId + 1
    record.created <- today
    rows_.pushBack(record)
    LINK(record, position = rows_.size - 1)
    undo_.pushBack(Action{Added, record})
    return ACCEPT
```

### 5.3 Link and unlink an index entry

These two functions are the **only** code in the program that touches the indexes, which is what
makes index consistency a property of the design rather than of reviewer discipline.

```
function LINK(record, position):
    byId_[record.id]        <- position
    byPhone_[record.phone]  <- position
    byName_[record.name].append(position)
    trie_.insert(lower(record.name), record.id)

function UNLINK(record, position):
    remove record.id from byId_
    if byPhone_[record.phone] == position: remove it
    remove position from byName_[record.name]; delete the bucket if it becomes empty
    trie_.erase(lower(record.name), record.id)
```

### 5.4 Delete a contact in O(1)

```
function REMOVE(id):
    position <- byId_[id]                 ; O(1)
    if not found: return REJECT
    removed <- rows_[position]            ; keep a copy for undo
    ERASE_AT(position)
    undo_.pushBack(Action{Deleted, removed})
    return ACCEPT

function ERASE_AT(position):
    UNLINK(rows_[position], position)
    last <- rows_.size - 1
    if position != last:
        moved <- rows_[last]; rows_.popBack()
        UNLINK(moved, last)                ; erase the entries still pointing at the old last slot
        rows_[position] <- moved
        LINK(rows_[position], position)     ; relink the one record that moved
    else:
        rows_.popBack()
```

Work performed is independent of n: one unlink, one move, one unlink, one link.

### 5.5 Update a contact

```
function UPDATE(id, changed):
    position <- byId_[id]; if not found: return REJECT
    VALIDATE(changed, selfId = id)          ; duplicate check ignores this record
    before <- rows_[position]
    UNLINK(rows_[position], position)
    rows_[position] <- changed
    rows_[position].created <- before.created     ; creation date is immutable
    LINK(rows_[position], position)
    undo_.pushBack(Action{Updated, before, rows_[position]})
    return ACCEPT
```

The `selfId` parameter is essential. Without it, saving a record without changing its phone number
would be rejected as a duplicate of itself.

### 5.6 Search by name, four modes

```
function SEARCH_NAME(query, mode):
    query <- squeezeSpaces(trim(query))
    if query is empty: return REJECT          ; an empty query must not match everything

    if mode == EXACT:
        return every record at every position in byName_[query]          ; O(log n + k)

    if mode == PREFIX:
        result <- empty
        for iterator <- byName_.lowerBound(query); iterator != end; iterator++:
            if iterator.key does not start with query (case-insensitively): break
            append every record at every position in iterator.value
        return result                                                    ; O(log n + k)

    if mode == CONTAINS:
        return every record whose name contains query, case-insensitively ; O(n * L)

    if mode == FUZZY:
        limit <- 1 if length(query) <= 3 else 2 if length(query) <= 7 else 3
        candidates <- empty
        for each record:
            d1 <- DAMERAU_LEVENSHTEIN(lower(query), lower(record.name))
            d2 <- DAMERAU_LEVENSHTEIN(lower(query), lower(first word of record.name))
            d  <- min(d1, d2)
            if d <= limit: candidates.append((d, record))
        sort candidates by d, keeping equal distances in input order
        return candidates                                                ; O(n * L^2)
```

### 5.7 Damerau-Levenshtein edit distance

Ordinary Levenshtein counts insertion, deletion and substitution. Damerau-Levenshtein adds
**transposition**, so `Pruhtvi` differs from `Pruthvi` by one edit instead of two. Plain
Levenshtein would also fail to treat `Ananaya` as a near match to `Ananya`.

```
function DAMERAU_LEVENSHTEIN(a, b):
    if b is shorter than a: swap a and b
    previous2 <- array of length |b|+1, filled with 0
    previous  <- array of length |b|+1, where previous[j] = j
    current   <- array of length |b|+1
    for i from 1 to |a|:
        current[0] <- i
        for j from 1 to |b|:
            cost <- 0 if a[i-1] == b[j-1] else 1
            best <- min(previous[j] + 1,            ; deletion
                        current[j-1] + 1,           ; insertion
                        previous[j-1] + cost)       ; substitution
            if i > 1 and j > 1 and a[i-1] == b[j-2] and a[i-2] == b[j-1]:
                best <- min(best, previous2[j-2] + 1)   ; transposition
            current[j] <- best
        shift: previous2 <- previous; previous <- current
    return previous[|b|]
```

Time O(|a|·|b|), space O(min(|a|,|b|)) because only three rows are retained.

### 5.8 Search by phone number

```
function SEARCH_PHONE(input):
    digits <- keep only 0-9 from input
    if digits is empty: return REJECT
    if digits exists in byPhone_: return that single record        ; O(1) average
    return every record whose phone contains digits                ; O(n * L) fallback
```

The distinction matters: an exact key uses the hash index, a partial key cannot, so the program
prints which path was taken. This is a small piece of honesty that makes the complexity claim
visible to the user.

### 5.9 Trie autocomplete

```
function TRIE_INSERT(key, id):
    node <- root
    for each character c in key:
        if node has no child c: create one
        node <- node.child[c]
    if id is not in node.ids: node.ids.append(id)

function TRIE_ERASE(key, id):
    recursive descent removing id from the terminal node,
    then pruning every node that becomes childless and id-less

function TRIE_PREFIX(prefix):
    node <- root
    for each character c in prefix:
        if node has no child c: return empty        ; O(|prefix|) negative result
        node <- node.child[c]
    return every id in the subtree below node, in child order
```

Because children are stored in a character-ordered container and visited in that order, the
subtree walk yields names in alphabetical order. **No sort step is required for autocomplete**,
which is a direct consequence of the shape of the tree.

Independent benchmark finding, reported here because it is a genuine counter-result: in this
implementation the trie beats a linear scan of every name by about 5 to 7 times, but it is about
2 times **slower** than the ordered map's `lower_bound` at these dataset sizes, because both
structures chase pointers and the trie pays more per node. The trie is asymptotically the right
structure for prefix completion and it wins for long keys and for incremental suggestions; at a few
thousand in-memory records the ordered map is competitive. Both numbers are printed.

### 5.10 Sorting with stability

```
function SORTED(key, ascending):
    view <- every record, ordered by id                 ; insertion order
    comparisons <- 0
    stableSort(view, comparator that counts comparisons)
    return view

comparator(a, b):
    comparisons <- comparisons + 1
    result <- compare the chosen key of a and b, case-insensitively
    return ascending ? result < 0 : result > 0
```

### 5.11 Undo

```
function UNDO():
    if undo_ is empty: report "nothing to undo"; return
    action <- undo_.popBack()
    if action.kind == Added:
        find action.after.id; ERASE_AT its position
    if action.kind == Updated:
        if the old phone now belongs to a different record:
            report why the undo is refused; return
        UNLINK(current); rows_[position] <- action.before; LINK(rows_[position], position)
    if action.kind == Deleted:
        if the id is in use, or the phone now belongs to someone else:
            report why the undo is refused; return
        rows_.pushBack(action.before); LINK at the new position
```

The history is bounded to 20 entries by `pop_front` on the deque.

### 5.12 CSV parsing as a state machine

A CSV field may legally contain the delimiter, a double quote, or a newline. A `getline`-per-row
reader breaks on the last case; a naive comma split breaks on the first two.

```
function ESCAPE(field):
    if field contains , or " or newline, or has leading/trailing space, or is empty:
        return '"' + field with every " replaced by "" + '"'
    return field

function PARSE(text):
    record <- empty; field <- empty; insideQuotes <- false
    for each character c in text:
        if insideQuotes:
            if c == '"' and next character is also '"': append '"'; skip next
            else if c == '"': insideQuotes <- false
            else: append c
        else if c == '"':      insideQuotes <- true
        else if c == ',' :     record.append(field); field <- empty
        else if c == '\r':     ignore
        else if c == '\n':     record.append(field); output.append(record); record <- empty
        else:                  append c
    if insideQuotes: report an unterminated quoted field
    flush any trailing field and record
```

`std::quoted` is **not** a CSV facility: its default escape character is a backslash rather than a
doubled quote, and its input operator is a whitespace-skipping stream extractor with no notion of
fields or records. A purpose-built pair of functions is the correct tool.

### 5.13 Atomic save

```
function SAVE(path):
    open path + ".tmp" for writing
    if it cannot be opened: return ERROR, leaving the old file untouched
    write the header row, then one escaped record per line, ordered by id
    flush
    if the stream is not good: return ERROR, leaving the old file untouched
    rename path + ".tmp" to path
```

A failure part-way through the write therefore cannot destroy previously saved data. This is the
brief's "unavailable resources" requirement, implemented rather than asserted.

### 5.14 The index audit

```
function AUDIT():
    problems <- empty
    if count(byId_) != count(rows_): report
    if count(byPhone_) != count(rows_): report
    nameSlots <- 0
    for each position p:
        check byId_[rows_[p].id] == p
        check byPhone_[rows_[p].phone] == p
        check rows_[p] appears exactly once under its name in byName_
        check trie_.prefix(lower(rows_[p].name)) contains rows_[p].id
    for each entry in byPhone_:
        check entry.value is a valid position and rows_[entry.value].phone == entry.key
    for each bucket in byName_:
        check every position is valid and its record's name matches the bucket key
        accumulate nameSlots; check exactly one slot per position
    if nameSlots != count(rows_): report
    return problems
```

A self-check that cannot fail is worthless, so the program also accepts `--inject-fault 1..5`,
which deliberately corrupts one index after loading. The five faults are: a missing phone entry, a
missing identity entry, a name bucket claiming the wrong record, a trie entry removed, and a phone
key that no record owns. The test suite asserts that the audit passes on clean data and reports
each of the five faults.

---

## 6. Implementation

The entire application is one file, `src/contact_manager.cpp`, 1,456 lines, so that it can be
submitted as a single artifact. The benchmark is a second file, `bench/bench.cpp`, which includes
the application and replaces `main` through a preprocessor guard.

Build:

```sh
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -o cms src/contact_manager.cpp
```

Run:

```sh
./cms --data contacts.csv          # or: make run
./cms --data contacts.csv --seed   # start with the ten sample contacts
```

| Command-line flag | Effect |
|---|---|
| `--data FILE` | CSV file to load at start and save into (default `contacts.csv`) |
| `--seed` | add the ten sample contacts if the list is empty |
| `--audit` | run the index self-check, print the result and exit 0 or 1 |
| `--inject-fault N` | testing hook: corrupt index N (1-5) after loading |
| `--help` | usage |

The project compiles warning-free under `-Wall -Wextra -Wpedantic -Werror`, which is checked in
continuous integration.

---

## 7. Testing

### 7.1 Method

`tests/test_cms.py` drives the compiled binary by piping a script into it and asserting on its
standard output. No test framework is required. Running the real binary means the tests exercise
exactly the code path the demonstration uses, and it forced the input design to consume a
predictable number of lines.

```sh
python3 tests/test_cms.py      # or: make test
```

Result on the development machine: **64 assertions, 14 test functions, 0 failures.**

### 7.2 Test inventory

| Test | Property verified |
|---|---|
| `test_empty_store_is_safe` | every menu option behaves correctly on an empty list; exit code 0 |
| `test_validation_rules` | letters-only phone, short phone, zero-leading phone, all-digit name, malformed email, and a duplicate expressed in a different notation |
| `test_whitespace_is_squeezed` | `   Pruthvi    Raj   Toganti  ` is stored as `Pruthvi Raj Toganti` |
| `test_delete_keeps_indexes_consistent` | after deleting a middle record, every surviving record is still reachable by ID, by name and by phone, and the deleted one is gone |
| `test_undo_paths` | undo of a delete, undo of an add, and undo with an empty history |
| `test_sorting_is_monotonic` | the output really is in ascending order, and the comparison count is plausible |
| `test_csv_round_trip_survives_punctuation` | a two-line address containing a comma and a quoted word survives load, display, save and reload byte for byte |
| `test_csv_duplicate_and_bad_rows_are_reported` | bad rows are counted and skipped, not silently dropped |
| `test_missing_required_column_is_reported` | a wrong header is refused with a message naming the required columns |
| `test_unwritable_destination_fails_cleanly` | saving into a missing directory reports the error, does not crash, and leaves the in-memory data intact |
| `test_update_protects_duplicate_phone_and_immutable_id` | taking another record's phone is refused; renaming is accepted |
| `test_autocomplete_order_and_prefix_misses` | two matches for the prefix `An`, and a clear negative result for a prefix with no path |
| `test_large_batch_insert_and_lookup` | 200 inserts, statistics correct, exact hash path used, reload preserves all 200 |
| `test_index_audit_passes_and_detects_every_fault` | the audit passes on clean data and reports each of the five injected faults with a non-zero exit code |

### 7.3 Verification that the delete is correct

The most important test is `test_delete_keeps_indexes_consistent`. It loads six records, deletes
the third through the menu, and then asks the program for the record that was **last** in the
array. Because the delete moves that record into the vacated slot, a correct implementation finds
it under its original ID, name and phone; an implementation that forgets to repair an index finds
nothing, or finds a ghost. The test then asserts that the number of records displayed is five, and
that the moved record appears exactly once.

---

## 8. Results and performance analysis

All figures in this section were produced by `bench/bench.cpp` on the development machine
(Apple clang 21, Apple Silicon, `-O2`) and are reproducible with `make measure`. They are measured,
not quoted. Absolute times depend on the machine; the growth of the curves does not.

### 8.1 Exact phone lookup

| n | hash index O(1) | sorted vector + binary search O(log n) | linear scan O(n) | scan / hash |
|---|---:|---:|---:|---:|
| 1,000 | 40 ns | 62 ns | 715 ns | 18.0x |
| 10,000 | 37 ns | 86 ns | 7,305 ns | 197.2x |
| 100,000 | 94 ns | 139 ns | 69,974 ns | 748.0x |

**Interpretation.** The hash index is flat as n grows, which is the O(1) behaviour being claimed.
The linear scan grows with n, reaching a factor of 748 at 100,000 records. The sorted vector loses
at every size, but only by about 1.6 to 2.5 times rather than by the factor the O(log n) bound
suggests, because each binary-search step touches contiguous memory. This is the honest reason a
sorted array remains a serious alternative in practice, and the reason the ordered name index in
this project is a tree rather than a hash table: trees pay for ordering, and ordering is exactly
what the name queries need.

### 8.2 Prefix search

| n | trie | ordered map `lower_bound` | linear scan | scan / trie | trie / map |
|---|---:|---:|---:|---:|---:|
| 1,000 | 11.68 ms | 8.47 ms | 60.23 ms | 5.2x | 1.38x |
| 10,000 | 8.65 ms | 4.70 ms | 61.19 ms | 7.1x | 1.84x |
| 100,000 | 12.65 ms | 6.11 ms | 62.58 ms | 4.9x | 2.07x |

**Interpretation.** The trie beats the linear scan by 4.9 to 7.1 times and does not grow with n,
which is the property being claimed. It is, however, about twice as slow as the ordered map at
these sizes. Reporting this counter-result is deliberate: the trie is asymptotically correct for
prefix completion and becomes the better choice for long keys, very large key sets, and ranked or
incremental suggestion, but at a few thousand in-memory records the ordered map is competitive.
The measurement contradicts the naive expectation and is reported as measured.

### 8.3 Deletion

| n | swap-with-last + index repair | `vector::erase` | speed-up |
|---|---:|---:|---:|
| 1,000 | 0.23 ms | 0.19 ms | 0.8x |
| 10,000 | 3.72 ms | 23.41 ms | 6.3x |
| 100,000 | 50.72 ms | 2,330.77 ms | 46.0x |

**Interpretation.** The advantage grows with n, as an O(1) strategy should against an O(n) one, and
it inverts at the smallest size, where the fixed cost of repairing the index entries dominates a
tiny memmove. Both the win and the loss are reported.

### 8.4 Sorting

| n | n log2(n) bound | `std::sort` comparisons | `std::stable_sort` comparisons | `std::sort` | `std::stable_sort` |
|---|---:|---:|---:|---:|---:|
| 1,000 | 9,966 | 11,403 | 20,414 | 0.46 ms | 0.77 ms |
| 10,000 | 132,877 | 147,532 | 185,207 | 6.25 ms | 7.34 ms |
| 100,000 | 1,660,964 | 1,842,179 | 3,530,302 | 75.38 ms | 130.74 ms |

**Interpretation.** Both sorts sit just above the theoretical lower bound, which is what a good
introsort does. `std::stable_sort` performs about 1.9 times as many comparisons and takes about
twice the wall time; that is the price of the guarantee that records with equal keys keep their
input order. The counting comparator itself adds an increment per comparison, which inflates both
figures and penalises the sort that compares more often.

Stability stress test on 64 synthetic records containing 8 repeated names:

| Sort | Pairs of same-name records reordered |
|---|---:|
| `std::sort` | **95** |
| `std::stable_sort` | **0** |
| `std::sort` with the key `(name, id)` | **0** |

### 8.5 Typo-tolerant search

| n | time for one query |
|---|---:|
| 1,000 | 0.32 ms |
| 10,000 | 3.13 ms |
| 100,000 | 30.58 ms |

**Interpretation.** Cost grows linearly with n, as expected for a structure that scores every
record. This path is deliberately the slow one; it is offered because it is genuinely useful, and
its cost is stated rather than hidden. A BK-tree would reduce the number of edit-distance
computations, at the cost of a considerably more complex structure.

### 8.6 Memory

| n | `Contact` payload | trie nodes |
|---|---:|---:|
| 1,000 | 0.14 MB | 5,922 |
| 10,000 | 1.45 MB | 35,985 |
| 100,000 | 14.50 MB | 336,799 |

The trie holds about 3.4 nodes per name and is the most expensive structure relative to the data it
serves. The hash indexes and the name index store keys and positions, not copies of records.

---

## 9. Edge cases handled

The brief names five failure classes. Each is handled with a specific message rather than a crash
or a silent failure.

| Case | Behaviour |
|---|---|
| Empty records | display, search, update, delete, sort and autocomplete each print their own message; the search message differs from "no matches" |
| Invalid IDs | `No contact with ID 99.` |
| Duplicate entries | phone numbers are unique and enforced on both add and update, across formatting variants, naming the existing owner |
| Unavailable resources | a missing file is reported and an empty list is started; an unwritable target is reported and the in-memory data is preserved; a partial write leaves the previous file intact |
| Invalid input | non-numeric, partially numeric and out-of-range menu input is rejected and the menu repeats |
| Empty search text | refused, rather than matching every record |
| Repeated name | permitted, and listed by the statistics screen, because names are not unique |
| Self-update | changing a record without changing its phone is accepted, because the duplicate check excludes the record under edit |
| Delete with the confirmation declined | nothing changes |
| Undo of a delete whose phone was since reused | refused, with the reason stated |
| Non-monotonic storage order after a delete | harmless, because every view sorts and IDs are monotonic |
| Menus with 0 or 1 record | sort and report handle both without special-casing |
| Input stream ending mid-prompt | the program exits cleanly rather than looping |

Additional edge cases are listed with the exact expected output in `docs/viva_qa.md`, Part C.

---

## 10. Requirement traceability

| Brief requirement | Implementation | Demonstrated by |
|---|---|---|
| Add contact | `ContactStore::add`, menu 1 | demo step 8-9, validation tests |
| Delete contact | `ContactStore::remove`, menu 6 | demo step 12, `test_delete_keeps_indexes_consistent` |
| Search by name | `searchByName`, menu 3 | demo steps 3, 6, 7 |
| Search by phone number | `searchByPhone`, menu 4 | demo steps 4, 5 |
| Update contact details | `update`, menu 5 | demo steps 10, 11 |
| Display all contacts | `menuDisplay`, menu 2 | demo step 2 |
| Sort alphabetically | `sorted`, menu 2 and 7 | demo steps 2, 15 |
| Menu-driven | `main`, options 0-14 | `demo/session.log` |
| Separate functions | one handler per option, one method per operation | Section 3.4 |
| Structures | `Contact`, `Action`, `CiLess`, `Trie::Node` | Section 3.1 |
| Strings | trimming, folding, comparison, digit extraction, parsing | Sections 5.1, 5.6, 5.12 |
| Arrays / STL containers | `vector`, `unordered_map`, `map`, `deque` | Section 4.1 |
| Searching | hash index, ordered range, linear scan, edit distance, trie walk | Section 5 |
| Sorting | `std::stable_sort`, comparison counters, stability stress test | Sections 4.5, 8.4 |
| Explain the structures | Section 4 | this report |

---

## 11. Limitations and future scope

### Limitations, stated plainly

1. Records are held in memory for the run; the CSV file is the whole persistence layer. There is
   no incremental or transactional update to the file.
2. The program is single-user and single-threaded. Concurrency was not required and would need
   locking around every mutation.
3. Table columns assume single-width characters. Very long values are ellipsized in the table view
   and shown in full by the update screen.
4. The near-duplicate name scan is pairwise and O(n²), so it is skipped above 200 records and says
   so, rather than appearing to hang.
5. Typo-tolerant search is linear in the number of records.
6. The trie is measurably slower than the ordered map at small dataset sizes, as reported in
   Section 8.2.
7. Search is case-insensitive but not accent-insensitive, and there is no transliteration.

### Future scope

1. **BK-tree for fuzzy search**, reducing the edit-distance computations per query, with the
   benchmark extended to measure the improvement.
2. **Union-Find (disjoint set union)** to cluster probable duplicates and merge them in one
   command, building on the near-duplicate detection already present.
3. **Group graph with breadth-first search** to answer "how do I know this person?" through shared
   groups.
4. **Least-recently-used cache** for repeated lookups, implemented as a hash table plus a doubly
   linked list.
5. **Persistent on-disk index** so that loading does not rebuild every index.
6. **vCard (RFC 6350) import and export** as a second interchange format, alongside CSV.
7. **Command-line batch mode**, so the operations can be scripted without the menu.

---

## 12. Conclusion

The seven mandatory features are present and working. The substance of the project is the design
decision behind them.

A contact list is reached through four different keys, and no single structure serves all four
cheaply. Hashing gives O(1) exact lookup and destroys order; a balanced tree preserves order and
costs O(log n); a vector provides contiguous storage and O(1) append; a trie turns prefix lookup
into a function of the prefix length. The implementation therefore keeps one array of records and
four indexes over it, with every mutation funnelled through two functions so that index consistency
is a property of the design rather than of care taken at each call site.

Correctness is then verified rather than assumed. An audit screen checks the central invariant in
both directions, and a fault-injection hook proves the audit can fail. Performance is measured
rather than asserted: a separate benchmark times each chosen structure against its obvious
alternatives at three dataset sizes and writes the results into a report.

The most useful outcome of the project is not the contact manager. It is the demonstration that a
design claim is only worth making once a number can be attached to it, and that a claim which
survives measurement unchanged is more persuasive than one that is never tested. Two of the
measurements here contradicted the naive expectation: a trie lost to an ordered map at small
sizes, and the O(1) delete lost to a plain array erase at 1,000 records. Reporting both is what
makes the remaining claims credible.

---

## 13. References

1. ISO/IEC 14882:2017 — Programming Languages: C++.
2. cppreference.com. `std::vector`, `std::unordered_map`, `std::map`, `std::stable_sort`,
   `std::sort`, `std::deque`. Used for the standardised complexity guarantees quoted in Section 4.
3. Y. Shafranovich. *RFC 4180: Common Format and MIME Type for Comma-Separated Values (CSV) Files*,
   2005. Basis for the escaping rules in Section 5.12.
4. S. Perreault. *RFC 6350: vCard Format Specification*, 2011. Reference for the multi-valued
   telephone field in the data model.
5. F. J. Damerau. "A technique for computer detection and correction of spelling errors."
   *Communications of the ACM*, 7(3), 1964. V. I. Levenshtein, 1965. Basis for Section 5.7.
6. C. A. R. Hoare, "Quicksort", 1961; D. R. Musser, "Introspective Sorting and Selection
   Algorithms", 1997. Context for the comparison-sort lower bound and for `std::sort`.
7. D. E. Knuth. *The Art of Computer Programming, Volume 3: Sorting and Searching*. Addison-Wesley.
8. T. H. Cormen, C. E. Leiserson, R. L. Rivest, C. Stein. *Introduction to Algorithms*,
   3rd edition. MIT Press, 2009. Standard reference for hash tables, balanced trees and tries.
9. R. Sedgewick and K. Wayne. *Algorithms*, 4th edition. Addison-Wesley, 2011.

---

## Appendix A: sample run

A complete scripted run is saved at `demo/session.log` (535 lines). The keystrokes are in
`demo/steps/in.txt`, and the purpose of each of the twenty steps is in
`demo/steps/intent.txt`. The run can be replayed with `make demo`.

Representative output:

```
ID   Name                   Phone        Group      Email                    Address                    Added
1    Aarav Sharma           98765 43210  Family     aarav.sharma@example.com 12, MG Road, Bengaluru     2026-10-03
2    Ananya Iyer            98123 45678  Work       ananya@example.com       Flat 3B, "Sunrise" Apar... 2026-10-03
3    Ananya Iyer            99001 12233  Work       ananya.iyer@work.in      Same office, Hyderabad     2026-10-03
  10 record(s).
```

```
  Phone (full or partial):   1 match(es). Exact hash hit, O(1).
  Phone (full or partial):   1 match(es). Partial match required a linear scan, O(n).
  Choice [1-4, default 3]:   2 match(es) for fuzzy search 'Ananaya':
  Choice [1-5, default 1]:   10 record(s), sorted by name ascending; 22 comparisons performed.
  Name        :   Phone       :   Email       :   Group       :   Address     :   REJECTED: Duplicate: phone 94455 66778 already belongs to #5 (Priya Nair).
  Confirm delete? (y/N):   OK: Deleted contact #5 (Priya Nair). Use Undo (menu 9) to restore.
  OK: Restored #5 (Priya Nair)
  Type a prefix:   1 suggestion(s) for prefix 'Sa':
  records 10, id entries 10, phone entries 10, name slots 10, trie hits 10
  RESULT: every index agrees with the record array.
```

## Appendix B: demonstration script

The twenty steps, in order, with the edge case each one proves:

| Step | Action | Proves |
|---|---|---|
| 1 | Load sample data (13) | repeatable starting state, including a deliberate duplicate name |
| 2 | Display sorted by name (2, 1, a) | required feature 83, plus a live comparison count |
| 3 | Prefix search "Ananya" (3, mode 2) | ordered range query |
| 4 | Full phone search (4) | O(1) hash path |
| 5 | Partial phone search (4) | the scan path, and that the program says which path ran |
| 6 | Typo search "Ananaya" (3, mode 4) | edit distance |
| 7 | Lower-case substring "kavya" (3, mode 3) | case-insensitive comparison |
| 8 | Add with an existing phone in another notation (1) | duplicate detection after normalisation, message names the owner |
| 9 | Add with a bad phone (1) | the validation rule is stated |
| 10 | Update into another record's phone (5) | rejection on update |
| 11 | Update the name only (5) | the self-exclusion rule |
| 12 | Delete with confirmation (6) | required feature 78 |
| 13 | Undo the delete (9) | the record returns with its original ID |
| 14 | Autocomplete "Sa" (8) | trie prefix results in alphabetical order |
| 15 | Sorting experiment (7) | the lower bound, and 95 reordered pairs against 0 |
| 16 | Statistics (10) | group histogram and duplicate-name alert |
| 17 | Validate indexes (14) | the invariant holds after deletes, updates and an undo |
| 18 | Save (11) | persistence |
| 19 | Reload (12) | the CSV round trip |
| 20 | Exit (0) | the program saves on the way out |

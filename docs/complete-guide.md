# The complete guide

Everything about this project in one document: the basics of the topic, what the project is, the
full tech stack, how it works, how to run and test it, what was measured, and where it is
published.

Written for someone who has not seen the project before. No prior C++ or data-structures knowledge
is assumed. Technical terms are defined the first time they appear, and collected again in the
glossary at the end.

---

## How to read this

| If you want to... | Read |
|---|---|
| Understand the assignment and the theory behind it | Part 1 |
| Know what the program does when you run it | Part 2 |
| Know every tool, version and library involved | Part 3 |
| Understand the design decision that earns the marks | Part 4 |
| Check the cost of every operation | Part 5 |
| Know what each file is for | Part 6 |
| Build, run, test, package, or edit it | Part 7 |
| Know how correctness and performance are verified | Part 8 |
| See the measured numbers | Part 9 |
| Submit it, publish it, or prepare for the viva | Part 10 |

**In one paragraph.** This is a college term project: build a menu-driven C++ contact manager that
demonstrates data structures, searching and sorting. The program works, but the real content is
that a contact list is queried in four different ways, no single data structure answers all four
efficiently, so the design keeps one array of records plus four indexes over it — and a benchmark
program measures each index against its obvious alternatives so that every claim comes with a
number. An index audit checks the design's central invariant, and a fault-injection hook proves the
audit can fail.

---

# Part 1 — The basics of the topic

## 1.1 What "DSA" means

**DSA = Data Structures and Algorithms.**

- A **data structure** is a way of arranging data in memory so that some operations become cheap.
- An **algorithm** is a step-by-step method that does something with that data.

The two words are joined because they trade off against each other. The same ten phone numbers can
be stored in several ways, and each way makes some operations fast and others slow. Choosing well
is the skill the assignment is testing.

A simple analogy: a wardrobe. Clothes hung by colour take longer to put away and much less time to
find. Clothes in a pile are instant to put away and slow to search. Same clothes, different
structure, different costs. Data structures are that choice, made deliberately.

## 1.2 The brief, decoded

The assignment brief says:

> "Your team is required to design and implement a C++ application for contact management system.
> The application should be menu-driven and should allow the user to perform the required
> operations through clear options. The implementation must demonstrate appropriate use of the DSA
> concepts taught in class."

Three requirements are hidden in those three sentences:

1. **Language: C++.** Not Python, not Java, not JavaScript. C++ specifically.
2. **Menu-driven.** No windows, no buttons, no website. A text menu: type `1`, it adds a contact.
3. **Must demonstrate DSA.** Real, named structures and algorithms, not just one array and hope.

Then seven mandatory features: add contact, delete contact, search by name, search by phone
number, update contact details, display all contacts, sort contacts alphabetically.

Then the expected DSA concepts: structures, strings, arrays/STL containers, searching, sorting.

Then the implementation rules: separate functions per operation, and handle empty records, invalid
IDs, duplicate entries, unavailable resources and invalid input.

And then the sentence that decides the marks:

> "Students should be prepared to explain why each selected data structure was used."

**This is the whole assignment.** Two teams can write the same seven features and get very
different marks:

- Average answer: "We used an array because it is simple."
- Top answer: "We used an array of records for storage because appending is O(1) amortised and it
  is contiguous memory. We added a hash map for phone numbers because phone lookup must be O(1)
  average and a scan is O(n). We used a balanced tree for names because we need prefix and ordered
  queries, which a hash map cannot answer at all. And here are the measured numbers."

The second answer is the project.

## 1.3 Data structures, from zero

### The record (`struct`)

Imagine storing contacts in two separate arrays:

```
names  = [ "Aarav", "Rohan", "Priya" ]
phones = [ "98765...", "90000...", "94455..." ]
```

Position 0 in `names` and position 0 in `phones` are *supposed* to describe the same person.
Nothing enforces that. Sort `names` and forget `phones`, and Aarav's row now shows Rohan's number.
The program still runs; the data is silently wrong.

A **struct** fixes this by making the person the unit:

```cpp
struct Contact {
    int         id;
    std::string name;
    std::string phone;
    std::string email;
    std::string group;
    std::string address;
    std::string created;
};
```

Now sorting one array of `Contact` moves everything together. The compiler enforces the link. This
is why the brief lists "structures" first, and why parallel arrays are the classic mistake.

### The array and `std::vector`

An array is one contiguous block of memory, one item after another. `std::vector` is C++'s
growable array: same idea, but it manages its own size.

| Operation | Cost | Why |
|---|---|---|
| read item `k` | O(1) | the address is arithmetic: base + k × size |
| append at the end | O(1) amortised | occasionally it reallocates and copies, spread over many cheap appends |
| insert or delete in the middle | O(n) | every later element must slide left or right |
| search for a value | O(n) | nothing knows your keys, so it must look at each item |

The O(n) middle-delete is the problem this project solves with a trick (Part 4.4).

### The hash table (`std::unordered_map`)

You give it a key, it computes `hash(key)`, and it jumps straight to a slot. Lookup is O(1)
average. The costs: more memory, no order at all, and a poor hash function degrades it toward O(n).

Crucially, **a hash table cannot answer "give me every key starting with An"**. Hashing deliberately
destroys the relationship between similar keys. This single limitation is why the project needs
both a hash table *and* a tree.

### The balanced tree (`std::map`)

Keys kept in sorted order inside a balanced binary search tree. Lookup is O(log n) — worse than a
hash. In exchange:

- iterating gives you every key in sorted order,
- one `lower_bound` call jumps to the start of any range,
- so "all names beginning with An" costs O(log n + matches).

`std::map` is usually implemented as a red-black tree. That is the structure the name index uses.

### The trie (prefix tree)

A tree where each edge is one character. `Aarav` is the path A → a → r → a → v. Looking up a prefix
costs the length of the prefix, not the number of names stored.

Two properties make it attractive:

- cost is independent of how many keys exist,
- because children are visited in character order, everything below a node comes out **already
  alphabetised**, with no sort step.

The cost is memory: a node per distinct prefix, which is more per record than any other structure
here.

### The stack and the deque

A **stack** is last-in, first-out: the last thing pushed is the first thing popped. That is exactly
what undo needs — the most recent change is the first one reversed.

`std::stack` cannot drop its oldest entry, but the undo history is deliberately bounded to 20
entries. `std::deque` (double-ended queue) can push at the back and pop from the front, so it is the
right container here.

## 1.4 Big-O, with real numbers

Big-O describes how the work grows as the amount of data grows. It ignores constant factors and
small inputs. It answers: "if I make the list ten times bigger, how much slower does this get?"

| Notation | Name | 1,000 records | 100,000 records | Used in this project for |
|---|---|---|---|---|
| O(1) | constant | 1 step | 1 step | phone lookup, ID lookup |
| O(log n) | logarithmic | ~10 | ~17 | name lookup in the tree |
| O(n) | linear | 1,000 | 100,000 | substring search, partial phone match |
| O(n log n) | linearithmic | ~10,000 | ~1.7 million | sorting |
| O(n²) | quadratic | 1 million | 10 billion | the near-duplicate pair scan (capped on purpose) |

Why this matters, in one measured comparison from this project. At 100,000 contacts, one phone
lookup costs **87 nanoseconds** through the hash index and **69,005 nanoseconds** through a linear
scan. That is a factor of **792**. Both are "just a lookup" — the structure decides the cost.

**The honest half of the lesson:** at 1,000 records the same comparison is only 12.6×, and a sorted
array with binary search is competitive with a hash map. Constant factors dominate at small sizes.
Saying that out loud is what separates understanding from recitation, and the program prints both
numbers.

## 1.5 The three operations that decide the design

**Searching** has three standard options, and each exists for a reason:

| Method | Cost | Requires | Fails when |
|---|---|---|---|
| Linear search | O(n) | nothing | the list is large |
| Binary search | O(log n) | **data sorted on the searched key** | you need prefix or substring matching |
| Hash lookup | O(1) average | exact key, a good hash | you need any ordering |

The classic viva question "why can't you use binary search here?" has a precise answer: binary
search needs the data sorted on the key being searched, and this program must also answer prefix
and substring questions, which no single ordering serves.

**Deleting** from the middle of an array costs O(n) because everything after the gap must move. That
is the specific cost this project avoids.

**Sorting** by comparison costs at least O(n log n) — a proven lower bound, not an implementation
detail. Two further facts matter:

- `std::sort` is **not stable**: two records with equal keys may swap positions. So two contacts
  both named "Ananya Iyer" can appear in a different order between runs.
- `std::stable_sort` keeps equal keys in their original relative order, and pays for the guarantee.
- The cheaper alternative is to make the key a **total order** by including the ID, so no two
  records are ever truly equal.

## 1.6 Invariants

An **invariant** is a condition that must always be true.

This project's central invariant:

> For every record in the array, every index points back at that record. No index contains a key
> that no record owns.

Every operation must leave that true. A delete that forgets to repair one index breaks it
*silently*: the record becomes unfindable, or a ghost entry appears. The program checks the
invariant directly (menu option 14), which is how correctness becomes verifiable instead of hoped
for.

---

# Part 2 — What this project is

## 2.1 One paragraph

A single-file C++17 console application that stores personal contacts and supports the seven
required operations behind a text menu. Underneath, it keeps one array of records and four indexes
over it, because a contact list is queried four different ways and no single structure serves all
four. A separate benchmark program measures each index against its obvious alternatives at 1,000,
10,000 and 100,000 records and writes the results into a report. A self-check verifies the index
invariant, and a fault-injection hook proves the self-check can fail. 64 automated tests drive the
compiled binary.

## 2.2 What running it looks like

```sh
make run
```

```
  1  Add contact
  2  Display all contacts (choose sort)
  3  Search by name
  4  Search by phone number
  5  Update contact details
  6  Delete contact
  7  Sort alphabetically / comparison experiment
  8  Autocomplete a name (trie)
  9  Undo last change
 10  Statistics report
 11  Save to file
 12  Reload from file
 13  Load sample data
 14  Validate indexes (self-check)
  0  Exit
Choice:
```

Type `13` to load ten sample contacts, then `2`, `1`, `a` to see them sorted by name:

```
ID   Name                   Phone        Group      Email                    Address                    Added
--------------------------------------------------------------------------------------------------------------------
1    Aarav Sharma           98765 43210  Family     aarav.sharma@example.com 12, MG Road, Bengaluru     2026-10-03
2    Ananya Iyer            98123 45678  Work       ananya@example.com       Flat 3B, "Sunrise" Apar... 2026-10-03
3    Ananya Iyer            99001 12233  Work       ananya.iyer@work.in      Same office, Hyderabad     2026-10-03
8    Arjun Reddy            70123 45678  Friend                              Hyderabad                  2026-10-03
9    Kavya Menon            99556 67788  Family     kavya@example.com                                   2026-10-03
7    Meera Krishnan         88997 76655  Family     meera.k@example.com      Chennai                    2026-10-03
5    Priya Nair             94455 66778  Friend     priya.nair@example.com   Kochi                      2026-10-03
4    Rohan Verma            90000 11111  Friend                              Sector 21, Noida           2026-10-03
10   Sanjay Gupta           96325 87410  General    sanjay.g@example.com     Kolkata                    2026-10-03
6    Vikram Singh           75540 00123  Work                                Bhopal                     2026-10-03
  10 record(s).
```

That is the whole user experience: type numbers, read text, type more numbers.

## 2.3 The seven required features, and where they live

| Brief | Menu | Code |
|---|---|---|
| Add contact | 1 | `ContactStore::add` |
| Delete contact | 6 | `ContactStore::remove` |
| Search by name | 3 | `ContactStore::searchByName` (four modes) |
| Search by phone number | 4 | `ContactStore::searchByPhone` |
| Update contact details | 5 | `ContactStore::update` |
| Display all contacts | 2 | `menuDisplay` |
| Sort contacts alphabetically | 2 and 7 | `ContactStore::sorted` |
| Exit | 0 | `main` |

## 2.4 Beyond the brief

None of these were required. Each exists because it gives the team something concrete to explain.

1. **Phone normalisation.** `+91 98765 43210`, `098765-43210` and `9876543210` all become one
   stored value, so a duplicate cannot hide behind formatting.
2. **Four search modes** — exact, prefix, substring, and typo-tolerant. Typing `Ananaya` still
   finds `Ananya`, via Damerau-Levenshtein edit distance.
3. **Trie autocomplete.** Type `Sa` and it suggests `Sanjay Gupta`, already in alphabetical order.
4. **Undo.** The last 20 changes are reversible, including a delete.
5. **CSV that survives real data.** A proper escape routine and a character-level state machine
   handle commas, double quotes and embedded newlines. Saving writes a temp file and renames it, so
   a failed write cannot destroy the previous file.
6. **Header-driven loading.** Columns are matched by name, so a reordered or extended CSV still
   loads. Bad rows and duplicate phones are skipped and *counted* in the load message.
7. **Statistics report.** Group histogram, missing-field counts, repeated names, and near-duplicate
   names at edit distance 1 or 2 — which is how a typo becomes two contacts.
8. **Index audit with fault injection.** Menu 14 verifies the invariant; `--inject-fault 1..5`
   deliberately corrupts an index so the audit can be shown to fail.
9. **Measured evidence.** A benchmark times every design choice against its alternatives and counts
   comparisons. `docs/benchmarks.md` is generated from the run.
10. **64 automated tests** that drive the compiled binary through stdin.

## 2.5 The one idea that makes it score

Your contact list is asked four different questions, and no single storage box is fast at all four:

1. "Show me everything" → needs a plain list.
2. "Give me the record with ID 7" → needs a fast exact lookup.
3. "Who owns 9876543210? Is this a duplicate?" → needs a fast lookup by phone.
4. "Show me everyone whose name starts with An" → needs something that keeps names *in order*.

So the program keeps **one list of contacts plus four small lookup tables pointing into it** — like
the index at the back of a textbook. The pages are not rearranged; you just jump straight to the
right one.

That is the answer to "explain why each selected data structure was used": a specific reason for
every box, with a measured cost.

---

# Part 3 — The tech stack

Every tool, version and library involved, and why each one is there.

## 3.1 Language and standard

| | |
|---|---|
| Language | **C++**, standard **C++17** (`-std=c++17`) |
| Source files | 2 (one application, one benchmark) |
| Third-party dependencies | **none** |
| Lines of C++ | 1,464 (application) + 366 (benchmark) |

**Why C++17 and not C++20 or C++23.** C++17 is the newest standard that is safe to assume on any
college machine or lab image. The project uses C++17 features deliberately (`std::optional`,
structured bindings, `std::string_view`-free by choice, `if constexpr`-free by choice) but nothing
that requires a newer compiler. C++20's `std::erase_if` and C++23's `std::flat_map` would be
nicer in places, but a submission that does not compile on the examiner's laptop is worth nothing.

## 3.2 Compiler and build flags

```sh
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o cms src/contact_manager.cpp
```

| Flag | Meaning | Why it is here |
|---|---|---|
| `-std=c++17` | select the language standard | portability |
| `-O2` | optimisation level 2 | the benchmarks are only meaningful with optimisation on |
| `-Wall -Wextra` | enable the standard warning sets | catches real bugs before the demo |
| `-Wpedantic` | reject non-standard extensions | proves the code is standard C++ |
| `-Werror` | treat warnings as errors | a warning can never be ignored; CI enforces this |

**Verified on two compilers.** Apple clang 21.0.0 on macOS, and GCC on Ubuntu (through GitHub
Actions). The project builds warning-free under `-Werror` on both. That portability is not assumed
— CI caught a real GCC failure once (Part 8.4).

## 3.3 The C++ standard library surface

Eighteen headers, all standard. Nothing exotic.

| Header | Used for |
|---|---|
| `<algorithm>` | `sort`, `stable_sort`, `find`, `find_if`, `remove`, `min`, `max`, `reverse` |
| `<cctype>` | `isspace`, `isdigit`, `isalnum`, `tolower` for string handling |
| `<cerrno>` | `errno` and `ERANGE`, to detect integer overflow in input parsing |
| `<cmath>` | `log2`, to print the comparison-sort lower bound next to the measured count |
| `<cstdio>` | `snprintf` for formatting, `std::rename` for the atomic save |
| `<cstdlib>` | `strtoll` for strict integer parsing, `atoi` in the CSV loader |
| `<ctime>` | `time`, `localtime_r`, `strftime` for the ISO creation date |
| `<deque>` | the bounded undo history |
| `<fstream>` | `ofstream` / `ifstream` for the CSV file |
| `<iostream>` | `std::cin` / `std::cout` |
| `<map>` | the ordered name index, and the trie's ordered child map |
| `<memory>` | `unique_ptr` / `make_unique` for trie nodes |
| `<optional>` | "column found?" results while mapping CSV headers |
| `<sstream>` | `stringstream` for reading the whole file, `ostringstream` for the report |
| `<string>` | all text |
| `<unordered_map>` | the ID index and the phone index |
| `<utility>` | `std::move`, `std::pair` |
| `<vector>` | the storage of record |

Deliberately **not** used: `<regex>` (hand-written validation is faster and easier to defend),
`<thread>` / `<mutex>` (single-user console program), `<random>` in the application (only the
benchmark needs it), `<iomanip>`, `<limits>` and `<stack>` (added early, found unused, removed).

## 3.4 The containers actually used

Counted from the source:

| Container | Occurrences | Role |
|---|---:|---|
| `std::string` | 125 | every text field; phone numbers are strings on purpose |
| `std::vector` | 46 | storage of record, and every result list |
| `std::map` | 7 | the ordered name index; the trie's child maps; the group histogram |
| `std::unordered_map` | 3 | the ID index and the phone index |
| `std::optional` | 5 | "is this CSV column present?" |
| `std::unique_ptr` | 2 | trie node ownership |
| `std::deque` | 1 | bounded undo history |

Total: **one vector + three hash maps/trees + one trie + one deque**.

## 3.5 Build system

| | |
|---|---|
| Tool | **GNU Make 3.81** (the version macOS ships) |
| File | `Makefile`, 45 lines |

```sh
make            # build the application
make run        # run it
make test       # build, then run the 64 behaviour tests
make measure    # rebuild docs/benchmarks.md on this machine
make bench      # build the benchmark binary
make demo       # replay the 20-step demo into demo/session.log
make package    # write dist/Group11_ContactManagementSystem.zip
make clean
```

The Makefile is a convenience, not a requirement. The single build command in 3.2 is all you need.
The variables `CXX` and `CXXFLAGS` are overridable, so `make CXX=g++-13` works.

## 3.6 Persistence format

**CSV, per RFC 4180.** Chosen because it is human-readable, diff-able, and can be opened in Excel
or LibreOffice for the report appendix.

| Aspect | Decision |
|---|---|
| Encoding | plain ASCII/UTF-8 text |
| Header | `id,name,phone,email,group,address,created` |
| Quoting | a field is quoted only when it contains a comma, a quote, a newline, or leading/trailing space |
| Inner quotes | doubled (`"Sunrise"` → `""Sunrise""`) |
| Reading | columns matched **by header name**, not position, so column order can change |
| Writing | to `contacts.csv.tmp`, then `rename` — so a failed write cannot destroy the old file |

`std::quoted` was rejected: its default escape is a backslash rather than a doubled quote, and its
input operator is a whitespace-skipping stream extractor with no concept of fields or records.

## 3.7 Test harness

| | |
|---|---|
| Language | **Python 3** (3.14.7 on the development machine) |
| Libraries | standard library only (`subprocess`, `re`, `tempfile`, `shutil`) |
| Framework | none — a plain script |
| File | `tests/test_cms.py`, 325 lines |
| Assertions | 64, across 14 test functions |

Each test pipes a script of keystrokes into the **compiled binary** and asserts on its stdout. No
test framework is needed, and the tests exercise exactly the code path the demo uses. That choice
also forced the input design: a scripted stream only behaves predictably when the program consumes
a predictable number of lines.

## 3.8 Benchmark harness

| | |
|---|---|
| File | `bench/bench.cpp`, 366 lines |
| Timing | `std::chrono::steady_clock`, wall-clock milliseconds |
| Data | generated deterministically from a seeded PRNG, so runs are comparable |
| Output | markdown to stdout **and** to `docs/benchmarks.md` |
| Sizes | 1,000 / 10,000 / 100,000 records (overridable: `./bench_bin 5000 50000`) |

It measures six things: exact phone lookup (hash vs sorted vector vs scan), prefix search (trie vs
ordered map vs scan), deletion (swap-with-last vs `vector::erase`), sorting (`std::sort` vs
`std::stable_sort`, with comparison counters), fuzzy search cost, and memory footprint. It reports
the actual compiler and platform in the generated file.

## 3.9 Screenshot pipeline

| | |
|---|---|
| File | `scripts/make_screenshots.py`, 228 lines |
| Library | **Pillow 12.3.0** (the only non-standard-library Python dependency in the project) |
| Font | `SFNSMono.ttf` on macOS, with Menlo / Andale Mono / DejaVu Sans Mono fallbacks for Linux |
| Output | 7 PNGs in `docs/images/` |

It runs the real binary, captures the real output, and renders it into macOS-style terminal
windows with a title bar and syntax colouring (red for rejections, green for passes, cyan for
section headers). **Nothing in the screenshots is typed by hand.** Each render is sanity-checked
programmatically: the ink bounding box must stay inside the canvas, so nothing is ever clipped.

## 3.10 Documentation and presentation

| | |
|---|---|
| Format | **Markdown** (GitHub-flavoured) |
| Diagrams | ASCII art inside fenced code blocks — no external image tooling needed |
| Badges | **shields.io** static badges (license, C++17, tests, dependencies, flags) |
| CI badge | the live GitHub Actions workflow badge |
| Report | `docs/report.md`, 989 lines, structured for a term-project submission |

ASCII diagrams were chosen over generated images deliberately: they survive copy-paste into a Word
document, they diff in version control, and they never go stale.

## 3.11 Version control and CI

| | |
|---|---|
| VCS | **git** 2.54.0 |
| Host | **GitHub**, public repository `prutxvi/contact-management-system` |
| CI | **GitHub Actions**, `.github/workflows/ci.yml`, 42 lines |
| Runners | `ubuntu-latest` **and** `macos-latest` |
| Licence | **MIT** |

The CI job on every push:

1. builds with `-Wall -Wextra -Wpedantic -Werror` (a warning fails the build),
2. runs the 64 behaviour tests,
3. loads sample data and runs the index audit, which must pass,
4. corrupts an index with `--inject-fault 3` and asserts the audit **fails**,
5. builds and runs the benchmark binary.

Step 4 is the unusual one, and it is the point: a self-check that cannot fail is worthless, so the
build is designed to break if the check ever becomes a rubber stamp.

## 3.12 Development machine

| | |
|---|---|
| OS | macOS 27.0, arm64 |
| CPU | Apple M5 |
| Compiler | Apple clang 21.0.0 |
| Python | 3.14.7 |
| Package manager | `uv` 0.12.5 (used only to install Pillow for the screenshot script) |

All benchmark figures in Part 9 were taken on this machine. Absolute times are machine dependent;
the *shape* of the curves is not.

## 3.13 What is deliberately not used, and why

| Not used | Reason |
|---|---|
| Any third-party C++ library | a submission that needs a package manager is a submission that might not build |
| A database | the brief asks for a menu-driven console program; a CSV file is the right size of solution |
| A GUI or web front-end | the brief specifies menu-driven, and a half-finished GUI scores worse than a finished console app |
| `system("cls")` / `system("pause")` | non-portable and it spawns a shell process |
| Threads or concurrency | single user, single process; adding locking would add risk and no marks |
| `std::regex` | hand-written validation is deterministic, has no backtracking cost, and is easier to explain in a viva |
| `using namespace std;` | name collisions and unclear origin; would be qualified in production |
| Non-constant global variables | a documented style violation, and it makes code untestable |
| Pointers into the record array | the array reallocates and records move; indexes store positions instead |
| Magic numbers | every limit (60-character name, 200-character address, 20-entry undo) is named or explained |

---

# Part 4 — How it works

## 4.1 One record, four indexes

```
                 ┌───────────────────────────────────────────────┐
                 │ rows_   std::vector<Contact>                  │
                 │ storage of record — dense, contiguous         │
                 └───────────────────────────────────────────────┘
                    ▲          ▲            ▲              ▲
   position (size_t)│          │            │              │
        ┌───────────┴──┐  ┌────┴─────┐  ┌───┴───────┐  ┌───┴──────────────┐
        │ byId_        │  │ byPhone_ │  │ byName_   │  │ trie_            │
        │ unordered_map│  │ unord_map│  │ std::map  │  │ Trie             │
        │ id → pos     │  │ phone→pos│  │ name →    │  │ prefix → ids     │
        │ O(1) average │  │ O(1) avg │  │ vector    │  │ O(prefix length) │
        │              │  │          │  │ O(log n)  │  │                  │
        └──────────────┘  └──────────┘  └───────────┘  └──────────────────┘

        ┌──────────────────────────────────────────────────────────────┐
        │ undo_   std::deque<Action>   last 20 changes, newest at back │
        └──────────────────────────────────────────────────────────────┘
```

| Structure | Answers | Cost | Why not something simpler |
|---|---|---|---|
| `rows_` (vector) | "show me all contacts" | scan O(n), append O(1) amortised | a `std::list` scatters records across the heap and gives up contiguous iteration |
| `byId_` (hash) | "the record with ID 7" | O(1) average | a scan is O(n); a tree costs O(log n) for an ordering nobody needs |
| `byPhone_` (hash) | "who owns this number", "is it a duplicate" | O(1) average | a sorted array costs O(log n), a scan O(n); the duplicate check runs on every add |
| `byName_` (tree) | "all names starting with An" | O(log n + k) | **a hash table cannot do this at all** |
| `trie_` | "suggest a name from these letters" | O(prefix length + k) | independent of how many keys exist, and already alphabetical |
| `undo_` (deque) | "undo my last change" | O(1) | LIFO is a stack, but a `std::stack` cannot drop its oldest entry |

Two details that make this maintainable:

- **The value in `byName_` is a vector of positions, not one position**, because two people can
  share a name. Deliberate, not an oversight.
- **Indexes store positions, never pointers.** The vector reallocates when it grows and records
  move during a delete. A position survives both; a pointer would dangle.

## 4.2 The two functions that make it safe

```cpp
void linkIndexes(const Contact& c, std::size_t pos);    // add this record to all four indexes
void unlinkIndexes(const Contact& c, std::size_t pos);  // remove it from all four
```

**These are the only functions in the program that touch an index.** That is the design rule that
makes index consistency a property of the architecture rather than of care taken at each call site.
If you ever catch yourself writing `byPhone_[...] = ...` somewhere else, that is the bug.

## 4.3 Trace: adding a contact

You type: name `Aarav   Sharma`, phone `+91 98765 43210`, group blank.

1. `menuAdd` collects the raw text and calls `ContactStore::add`.
2. `validate` runs, in order:
   - `squeezeSpaces` on the name → `Aarav Sharma`
   - reject empty, over 60 characters, or containing control characters
   - reject a name that is all digits
   - `normalizePhone`: strip non-digits, strip a leading `91`, strip a leading `0`
     → `+91 98765 43210` becomes `9876543210`
   - look up that number in `byPhone_`. If found, refuse and name the existing owner. One hash
     lookup, O(1).
   - validate the email if one was given
   - default the group to `General`
3. `add` assigns `nextId_`, pushes the record onto `rows_`, and calls `linkIndexes`, which writes
   all four index entries.
4. An `Action` of kind `Added` is pushed onto the undo deque.

Note the order: **normalisation happens before the duplicate check.** Without that, `098765-43210`
and `9876543210` would both be accepted as different numbers.

## 4.4 Trace: deleting a contact — the O(1) trick

`vector::erase` is O(n) because every element after the hole must slide left. Instead:

1. `remove` finds the position through `byId_` — O(1) — and copies the record out for the undo entry.
2. `eraseAt(pos)`:
   - `unlinkIndexes(rows_[pos], pos)` — remove this record from all four indexes
   - let `last = rows_.size() - 1`
   - if `pos != last`: move `rows_[last]` into a local, `pop_back()`, then `unlinkIndexes(moved, last)`
     to erase the entries still pointing at the old last slot, then `rows_[pos] = moved` and
     `linkIndexes(rows_[pos], pos)`
   - if `pos == last`: just `pop_back()`
3. Push an `Action` of kind `Deleted`.

**Exactly one record moved, so exactly one record needs repair.** The cost does not grow with the
list length. Measured: **46× faster** than `vector::erase` at 100,000 records.

The catch, which you should volunteer: storage order becomes arbitrary. That is acceptable because
order is a property of the **view**, not of the array — every display sorts by a chosen key, and IDs
are monotonic, so insertion order is always recoverable by sorting on ID.

## 4.5 Trace: updating a contact — the subtle bug

```cpp
Result validate(Contact& c, int selfId = 0);
```

`update` runs the same validation as `add`, **except** that the duplicate-phone check ignores a hit
when the slot it found holds the same record being edited. Without `selfId`, saving a record without
touching its phone would be rejected as a duplicate of itself. This is the kind of bug that appears
the first time a user edits anything, and it is worth showing in the demo.

`created` is copied from the old record, so the creation date is immutable even though every other
field can change.

## 4.6 Trace: the four search modes

| Mode | Path | Cost | Note |
|---|---|---|---|
| Exact | one `byName_.find` | O(log n + k) | needs an exact full-name match |
| Prefix | one `byName_.lower_bound`, then walk while the prefix matches | O(log n + k) | this is what an ordered tree buys you |
| Contains | scan every record with a case-insensitive substring test | O(n · L) | no index can help an arbitrary substring |
| Fuzzy | score every record with Damerau-Levenshtein, sort by distance | O(n · L²) | deliberately the slow path |

Phone search: `normalizePhone`, then one `byPhone_` hash hit for an exact key. A **partial** number
cannot use a hash, so it falls back to a scan — and the program prints which path ran, which makes
the complexity claim visible to the user.

## 4.7 Trace: autocomplete

A trie stores names character by character. `Sa` walks two edges to a node, then collects every ID
in the subtree below it. Because children are held in a character-ordered map and visited in that
order, the results come out **already alphabetised** — no sort step. A prefix with no path returns
empty in O(prefix length), which is a fast, honest negative.

## 4.8 Undo

Every change records an `Action { kind, before, after }`:

| Action | Undo does |
|---|---|
| `Added` | delete that record |
| `Deleted` | push the record back with its original ID |
| `Updated` | unlink the current version, restore `before`, relink |

Undo **refuses and explains** when it cannot be correct — for example, restoring a deleted contact
whose phone number has since been given to someone else. The history is bounded to 20 entries.

## 4.9 The audit and the fault-injection hook

`audit()` walks every record and checks the invariant **in both directions**:

- the ID index for this record points back at this slot,
- the phone index for this record points back at this slot,
- this slot appears exactly once across the name index, under a case-insensitively matching key,
- the trie still reaches this record,
- and the reverse: no index key points at a record that does not own it.

`injectFaultForTest(1..5)` deliberately breaks one index:

| Fault | Corruption |
|---|---|
| 1 | drop a phone index entry |
| 2 | drop an ID index entry |
| 3 | add a name-bucket entry claiming the wrong record |
| 4 | remove a name from the trie |
| 5 | insert a phone key that no record owns |

```sh
./cms --data contacts.csv --audit                    # AUDIT PASSED, exit 0
./cms --data contacts.csv --inject-fault 3 --audit    # AUDIT FAILED, exit 1
```

## 4.10 CSV: why it is not a `getline` loop

A field may contain a comma (`12, MG Road`), a double quote (`Flat 3B, "Sunrise"`), or a newline (a
two-line address). A naive splitter breaks on the first two; a `getline`-per-row reader breaks on
the third.

- `csv::escape` writes quotes only when needed and doubles inner quotes.
- `csv::parse` is a small state machine tracking whether it is inside quotes, so a quoted field may
  legally span lines.
- `save` writes `contacts.csv.tmp` and only then renames, so a failure part-way cannot destroy the
  existing file.
- `load` maps columns **by header name**, so a reordered or extended CSV still loads. Invalid rows
  and duplicate phones are skipped and counted.

---

# Part 5 — Algorithms and complexity

## 5.1 Cost of every operation

*n* = contacts, *k* = matches returned, *p* = prefix length, *L* = name length, *q* = query length.

| Operation | Time | Space | Structure responsible |
|---|---|---|---|
| Add contact | O(1) amortised + O(log n) + O(L) | O(1) amortised | vector append, name-index insert, trie insert |
| Find by ID | O(1) average | O(1) | ID hash index |
| Find by phone, exact | O(1) average | O(1) | phone hash index |
| Find by phone, partial | O(n · L) | O(1) | linear scan — a partial key cannot use a hash |
| Find by name, exact | O(log n + k) | O(k) | ordered name index |
| Find by name, prefix | O(log n + k) | O(k) | ordered name index, one `lower_bound` |
| Find by name, substring | O(n · L) | O(k) | linear scan — no index helps an arbitrary substring |
| Find by name, typo-tolerant | O(n · L²) | O(L) | Damerau-Levenshtein per record |
| Autocomplete | O(p + k) | O(k) | trie |
| Update contact | O(1) + O(L) | O(1) | locate by ID, repair two index entries |
| Delete contact | O(1) + O(L) | O(1) | swap-with-last, repair one record |
| Display, sorted | O(n log n) | O(n) | `std::stable_sort` over n positions |
| Save to file | O(n) | O(n) | CSV writer |
| Load from file | O(n·L + n log n) | O(n) | CSV parser plus index rebuild |
| Index audit | O(n·(L + log n)) | O(n) | walks every record and index entry |
| Undo | O(1) + O(L) | O(1) | deque action |

## 5.2 The fourteen algorithms in the source

Each has full pseudocode in `docs/report.md` Section 5.

| # | Algorithm | Line | Idea |
|---|---|---|---|
| 1 | Phone normalisation | ~110 | strip separators, country code and leading zero, then validate |
| 2 | Add contact | 367 | validate → append → link four indexes → record undo |
| 3 | Link / unlink an index entry | 816 / 823 | the only two functions that touch an index |
| 4 | Delete in O(1) | 836 | move the last record into the hole, repair one record |
| 5 | Update | 488 | with self-exclusion from the duplicate check |
| 6 | Search by name, four modes | 398 | hash-free exact, `lower_bound` range, scan, edit distance |
| 7 | Damerau-Levenshtein | 159 | dynamic programming, three rolling rows, transposition aware |
| 8 | Search by phone | 436 | hash hit, else scan |
| 9 | Trie insert / erase / prefix | 189 | character walk; pruning on erase; subtree collection |
| 10 | Sorting with counters | 468 | `std::stable_sort` plus a comparison counter |
| 11 | Undo | 519 | three action kinds, with refusals explained |
| 12 | CSV escape and parse | 261 | state machine for quoted fields spanning lines |
| 13 | Atomic save | 651 | temp file then rename |
| 14 | Index audit | 574 | both-direction invariant check |

## 5.3 Sorting and stability

`std::sort` is not stable: records with equal keys can swap. The program implements and measures
**three** approaches on the same data:

| Approach | Same-name pairs reordered (64 records, 8 repeated names) |
|---|---:|
| `std::sort` | **95** |
| `std::stable_sort` | **0** |
| `std::sort` with the key `(lower(name), id)` — a total order | **0** |

The total-order key is what a production system would prefer: then any sort is deterministic and
you do not depend on a stability guarantee. `std::stable_sort` is documented as costing
O(n log² n) comparisons unless spare memory is available, which the measurements confirm — about
1.9× the comparisons and 2× the wall time of `std::sort` here.

The program also prints the `n · log2(n)` lower bound beside the measured comparison count, so the
claim "this sort is O(n log n)" is checked rather than asserted.

---

# Part 6 — Every file, explained

```
contact-management-system/
├── src/contact_manager.cpp       the whole application — 1,464 lines, 11 marked sections
├── bench/bench.cpp               the benchmark — 366 lines
├── tests/test_cms.py             64 behaviour tests — 325 lines
├── scripts/make_screenshots.py   renders docs/images/ from real output — 228 lines
├── Makefile                      8 targets — 45 lines
├── README.md                     the front page — the pitch, screenshots, results
├── LICENSE                       MIT
├── .gitignore                    keeps binaries, local data and generated files out
├── .github/workflows/ci.yml      build + test + audit on Linux and macOS
├── demo/
│   ├── steps/in.txt              the 20 demonstration keystrokes
│   ├── steps/intent.txt          what each step proves, and which edge case
│   ├── run_demo.sh               replays the demo and saves the transcript
│   └── session.log               a saved full run, 535+ lines
└── docs/
    ├── complete-guide.md         ← this document
    ├── explained.md              from-zero walkthrough plus edit recipes
    ├── report.md                 the term-project report, 989 lines
    ├── viva_qa.md                viva questions with prepared answers
    ├── idea_book.md              23 further ideas, ranked, with the DSA concept each adds
    ├── assignment.md             the brief, transcribed and mapped to the implementation
    ├── benchmarks.md             generated by `make measure`
    ├── submission_note.md        the build/run note inside the submission zip
    └── images/                   7 screenshots, generated from real output
```

### The application, section by section

| Line | Section | Contents |
|---|---|---|
| 37 | 1. Utilities | trim, lowercase, case-insensitive compare, space squeezing, phone normalisation and validation, email validation, table formatting |
| 159 | 2. Edit distance | Damerau-Levenshtein, O(n·m) time, O(min) space |
| 189 | 3. Trie | `class Trie`, `Node`, insert, erase with pruning, prefix enumeration, node count |
| 261 | 4. CSV | `escape`, `joinRecord`, `parse` state machine |
| 327 | 5. Record | `struct Contact` |
| 340 | 6. `ContactStore` | the heart: five structures, every operation, validation, index maintenance, audit, persistence |
| 866 | 7. Console | `class Console` (validated input), `printContacts`, `printOne`, `printRule` |
| 951 | 8. Report | statistics, group histogram, duplicate and near-duplicate detection |
| 1034 | 9. Sorting experiment | three sorts, comparison counters, 64-record stability stress test |
| 1140 | 10. Sample data | the ten seed contacts |
| 1169 | 11. Menu handlers | one `menuXxx` per option, plus `main` at 1348 |

### Key methods inside `ContactStore`

| Line | Method | Job |
|---|---|---|
| 367 | `add` | validate, assign ID, append, link indexes, record undo |
| 389 / 394 | `findById` / `findByPhone` | the two O(1) lookups |
| 398 | `searchByName` | four modes |
| 436 | `searchByPhone` | hash hit, else scan |
| 446 | `autocomplete` | trie prefix walk |
| 460 / 470 | `allById` / `sorted` | materialise, then sort by a chosen key |
| 488 | `update` | with self-exclusion |
| 507 | `remove` | the O(1) delete |
| 519 | `undo` | three action kinds |
| 574 | `audit` | the invariant check |
| 638 | `injectFaultForTest` | the testing hook |
| 653 / 673 | `save` / `load` | atomic write, header-driven read |
| 775 | `validate` | every input rule in one place |
| 816 / 823 | `linkIndexes` / `unlinkIndexes` | the only two functions that touch an index |
| 836 | `eraseAt` | swap-with-last delete |
| 850 | `pushUndo` | bounded history |

---

# Part 7 — Commands and workflows

## 7.1 Build and run

```sh
cd /Users/pruthvi/Projects/contact-management-system

make            # compile ./cms
make run        # run it against ./contacts.csv
```

Manual build, no make needed:

```sh
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -o cms src/contact_manager.cpp
./cms --data contacts.csv --seed
```

## 7.2 Command-line flags

| Flag | Effect |
|---|---|
| `--data FILE` | CSV file to load at start and save into (default `contacts.csv`) |
| `--seed` | add the ten sample contacts if the list is empty |
| `--audit` | run the index self-check, print the result, exit 0 or 1 — usable in a script |
| `--inject-fault N` | testing hook: corrupt index N (1–5) after loading |
| `--help` | usage |

## 7.3 What each menu option does inside

| # | Option | Internally |
|---|---|---|
| 1 | Add | validate → normalise phone → duplicate check → append → link four indexes → record undo |
| 2 | Display | pick sort key and direction → `sorted()` → table → comparison count |
| 3 | Search name | four modes: exact, prefix, contains, fuzzy |
| 4 | Search phone | hash hit or linear scan; the message says which ran |
| 5 | Update | field by field; blank keeps, `-` clears; `selfId` excludes the record |
| 6 | Delete | confirm → `eraseAt` → repair one index entry |
| 7 | Sort experiment | three sorts, counters, stability stress test |
| 8 | Autocomplete | trie prefix, up to 10 suggestions, already alphabetical |
| 9 | Undo | pop one action and reverse it |
| 10 | Statistics | structure sizes, group histogram, missing fields, duplicate names |
| 11 / 12 | Save / Reload | atomic write; header-driven read with skipped-row counts |
| 13 | Sample data | ten seeds, including a deliberate duplicate name |
| 14 | Validate indexes | the audit |

## 7.4 The demonstration

`demo/steps/in.txt` is the exact keystroke list; `demo/steps/intent.txt` says what each of the
twenty steps proves. `make demo` replays it and writes `demo/session.log`.

| Step | Action | Proves |
|---|---|---|
| 1 | Load sample data | repeatable start state, with a deliberate duplicate name |
| 2 | Display sorted by name | required feature, plus a live comparison count |
| 3 | Prefix search "Ananya" | ordered range query |
| 4 | Full phone search | the O(1) hash path |
| 5 | Partial phone search | the scan path, and that the program says which ran |
| 6 | Typo search "Ananaya" | edit distance |
| 7 | Lower-case "kavya" | case-insensitive comparison |
| 8 | Add a duplicate in another notation | normalisation before the duplicate check |
| 9 | Add a bad phone | the validation rule is stated |
| 10 | Update into another record's phone | rejection on update |
| 11 | Update the name only | the self-exclusion rule |
| 12 | Delete with confirmation | required feature |
| 13 | Undo the delete | the record returns with its original ID |
| 14 | Autocomplete "Sa" | trie results already alphabetical |
| 15 | Sorting experiment | the lower bound, and 95 vs 0 vs 0 |
| 16 | Statistics | group histogram and duplicate-name alert |
| 17 | Validate indexes | the invariant holds after deletes, updates and an undo |
| 18 | Save | persistence |
| 19 | Reload | the CSV round trip |
| 20 | Exit | the program saves on the way out |

## 7.5 The edit loop — run this every time

```
1. edit src/contact_manager.cpp
2. make                              -> zero warnings
3. make test                         -> "64 passed, 0 failed"
4. ./cms --seed --data /tmp/x.csv    -> click through the change by hand
5. ./cms --data /tmp/x.csv --audit   -> "AUDIT PASSED"      <- do not skip this
6. make measure                      -> only if you changed a structure or algorithm
7. make demo                         -> only if you changed the menu or demo steps
8. update README.md and docs/        -> if the change is user-visible
```

Step 5 catches index bugs immediately, which is why it exists.

## 7.6 Recipes for changing the code

### Add a field to `Contact` (example: `birthday`) — 9 places

| Place | Line | Change |
|---|---|---|
| `struct Contact` | 327 | add the member |
| `validate` | 775 | validate and default it |
| `printContacts` | 920 | add a column and widen the rule |
| `printOne` | 940 | add a line to the single-record view |
| `menuAdd` | 1171 | prompt for it |
| `menuUpdate` | 1259 | prompt for it, and decide blank/clear behaviour |
| `save` | 651 | add it to the header string **and** to the `joinRecord` list, in the same order |
| `load` | 671 | add a `col(...)` and a `cell(...)` call |
| `loadSamples` | 1140 | add it to the `Seed` struct and the table |

If you forget: save/load is the dangerous pair. A missing column silently writes a field into the
wrong column.

### Add a menu option — 4 places

| Place | Line | Change |
|---|---|---|
| the handler | near 1171 | `void menuFoo(Console& con, ContactStore& store)` |
| menu text | 1406 | add a printed line |
| range check | 1423 | widen `con.integer("Choice: ", 0, 14, choice)` |
| dispatch | 1424 | add `case 15: menuFoo(con, store); break;` |

### Add a new index — 6 places

1. Add the container as a private member of `ContactStore`.
2. Add its maintenance to **both** `linkIndexes` and `unlinkIndexes` — nowhere else.
3. Add its checks to `audit()`, in both directions.
4. Add a fault case to `injectFaultForTest`, and an expectation to the audit test.
5. Clear it in `load` before the staging loop.
6. Add it to the statistics report so its size is visible.

### The two rules that keep it safe

1. **Only `linkIndexes` and `unlinkIndexes` touch an index.**
2. **Never store a pointer or iterator into `rows_`.** Store the position.

---

# Part 8 — Verification

## 8.1 The test suite

```sh
make test        # or: python3 tests/test_cms.py
# 64 passed, 0 failed
```

14 test functions, 64 assertions, driving the compiled binary through stdin.

| Test | Property verified |
|---|---|
| `test_delete_keeps_indexes_consistent` | after deleting a middle record, every survivor is reachable by ID, name and phone, and the deleted one is gone |
| `test_index_audit_passes_and_detects_every_fault` | the audit passes clean and reports all five injected faults |
| `test_csv_round_trip_survives_punctuation` | a two-line address with a comma and a quote survives load → save → reload byte for byte |
| `test_validation_rules` | letters-only phone, short phone, zero-leading phone, all-digit name, bad email, duplicate in another notation |
| `test_unwritable_destination_fails_cleanly` | a failed save reports, does not crash, preserves in-memory data |
| `test_update_protects_duplicate_phone_and_immutable_id` | self-exclusion works; renaming is allowed |
| `test_empty_store_is_safe` | every option behaves correctly on an empty list |
| `test_undo_paths` | delete-undo, add-undo, empty history |
| `test_sorting_is_monotonic` | output really is sorted, comparison count plausible |
| `test_csv_duplicate_and_bad_rows_are_reported` | skips are counted, not silent |
| `test_missing_required_column_is_reported` | a wrong header is refused with the required columns named |
| `test_autocomplete_order_and_prefix_misses` | two hits for `An`, and a clear negative result |
| `test_large_batch_insert_and_lookup` | 200 inserts, statistics correct, exact hash path, reload intact |
| `test_whitespace_is_squeezed` | `   Pruthvi    Raj   Toganti  ` is stored as `Pruthvi Raj Toganti` |

The most important one is `test_delete_keeps_indexes_consistent`. It loads six records, deletes the
third, then asks for the record that was **last** in the array. A correct implementation finds it
under its original ID, name and phone; an implementation that forgets to repair an index finds
nothing, or finds a ghost.

## 8.2 Why the audit is a real check

A self-check that always passes is worthless, so the program can be told to break itself:

```sh
./cms --data contacts.csv --audit                    # AUDIT PASSED ... exit 0
./cms --data contacts.csv --inject-fault 1 --audit    # phone entry missing
./cms --data contacts.csv --inject-fault 2 --audit    # ID entry missing
./cms --data contacts.csv --inject-fault 3 --audit    # name bucket claims the wrong record
./cms --data contacts.csv --inject-fault 4 --audit    # trie entry removed
./cms --data contacts.csv --inject-fault 5 --audit    # stale phone key
```

All five are asserted in the test suite, and the CI build fails if any of them ever passes.

## 8.3 Continuous integration

`.github/workflows/ci.yml` runs on every push, on **Ubuntu and macOS**:

1. build with `-Wall -Wextra -Wpedantic -Werror`,
2. run the 64 behaviour tests,
3. load sample data, run the audit, require it to pass,
4. corrupt an index and require the audit to fail,
5. build and run the benchmark.

Current status: **passing on both runners.**

## 8.4 What CI actually caught

On the first push, macOS passed and Linux failed on the benchmark build:

```
bench/bench.cpp:67:11: error: '__clang_major__' was not declared in this scope
```

The benchmark hardcoded `__clang_major__` and the string `"Apple Silicon"`. Neither exists under
GCC. It would have broken on any Linux machine, including a college lab PC.

Fixed by deriving the toolchain description from `__clang__` / `__GNUC__`, `__APPLE__` / `__linux__`
/ `_WIN32` and the architecture, so the generated report describes the machine it actually ran on.
This is the concrete reason to run CI on more than one platform: the bug was invisible on the
development machine.

## 8.5 Other problems found during development

| Found | Fix |
|---|---|
| Exiting with Ctrl-D discarded the session; only option 0 saved | every exit path now saves |
| Two messages were 207 columns wide — wider than a normal terminal | both now wrap inside 100 columns |
| In a piped transcript, results were glued onto the last prompt line | result messages start on their own line |
| `load()` validated staged rows against the live indexes, so every row was rejected as a duplicate | the store is cleared after the file parses and before staging |
| `-Wall -Wextra` reported unused includes (`<iomanip>`, `<limits>`, `<stack>`) | removed |
| The file header described the undo history as a `std::stack` | corrected to `std::deque` |

---

# Part 9 — Measured results

All figures produced by `bench/bench.cpp` on the development machine (Apple M5, Apple clang 21,
`-O2`) and reproducible with `make measure`. They are measured, not quoted. Absolute times depend
on the machine; the shape of the curves does not.

## 9.1 Exact phone lookup

| n | hash index O(1) | sorted vector + binary search O(log n) | linear scan O(n) | scan / hash |
|---|---:|---:|---:|---:|
| 1,000 | 78 ns | 110 ns | 982 ns | 12.6× |
| 10,000 | 37 ns | 83 ns | 6,853 ns | 183.5× |
| 100,000 | 87 ns | 148 ns | 69,005 ns | **792×** |

**Interpretation.** The hash index stays flat as n grows — that is the O(1) claim, measured. The
linear scan grows with n, reaching a factor of 792. The sorted vector loses at every size, but only
by 1.4× to 2.2× rather than the factor the O(log n) bound suggests, because each binary-search step
touches contiguous memory. This is the honest reason a sorted array remains a serious alternative
in practice — and the reason the ordered name index here is a tree rather than a hash map: trees pay
for ordering, and ordering is exactly what the name queries need.

## 9.2 Prefix search

| n | trie | ordered map `lower_bound` | linear scan | scan / trie | trie / map |
|---|---:|---:|---:|---:|---:|
| 1,000 | 11.82 ms | 8.50 ms | 64.96 ms | 5.5× | 1.39× |
| 10,000 | 8.77 ms | 4.79 ms | 64.40 ms | 7.3× | 1.83× |
| 100,000 | 12.63 ms | 6.32 ms | 65.15 ms | 5.2× | 2.00× |

## 9.3 Deletion

| n | swap-with-last + index repair | `vector::erase` | speed-up |
|---|---:|---:|---:|
| 1,000 | 0.25 ms | 0.19 ms | 0.8× |
| 10,000 | 3.37 ms | 23.36 ms | 6.9× |
| 100,000 | 50.77 ms | 2,338.27 ms | **46.1×** |

## 9.4 Sorting

| n | n·log₂n bound | `std::sort` comparisons | `std::stable_sort` comparisons | `std::sort` | `std::stable_sort` |
|---|---:|---:|---:|---:|---:|
| 1,000 | 9,966 | 11,403 | 20,414 | 0.47 ms | 0.70 ms |
| 10,000 | 132,877 | 147,532 | 185,207 | 6.22 ms | 7.38 ms |
| 100,000 | 1,660,964 | 1,842,179 | 3,530,302 | 70.21 ms | 126.50 ms |

Both sorts sit just above the theoretical lower bound, which is what a good introsort does.
`std::stable_sort` performs about 1.9× the comparisons and takes about twice the wall time — the
price of the guarantee that records with equal keys keep their input order.

## 9.5 Typo-tolerant search

| n | time for one query |
|---|---:|
| 1,000 | 0.31 ms |
| 10,000 | 3.02 ms |
| 100,000 | 30.54 ms |

Linear in n, as expected for a path that scores every record. Deliberately the slow one.

## 9.6 Memory

| n | `Contact` payload | trie nodes |
|---|---:|---:|
| 1,000 | 0.14 MB | 5,922 |
| 10,000 | 1.45 MB | 35,985 |
| 100,000 | 14.50 MB | 336,799 |

The trie holds about 3.4 nodes per name and is the most expensive structure relative to the data it
serves. The hash indexes and the name index store keys and positions, not copies of records.

## 9.7 Two counter-results, reported against ourselves

1. **The trie is ~2× slower than the ordered map's `lower_bound`** at 1k–100k in-memory records,
   even though it beats a linear scan by 5.2–7.3×. Both structures chase pointers; the trie pays
   more per node. It is asymptotically the right structure for prefix completion and wins for long
   keys, very large key sets, and incremental or ranked suggestion — but at this scale the map is
   competitive. Independent research for this project measured the same effect on a different
   implementation (a trie about 4× slower than `lower_bound` over a sorted vector for 20,000 prefix
   queries), so the effect is real and not a quirk of this code.
2. **The O(1) delete loses at n = 1,000.** Index repair is a fixed cost per delete; the memmove it
   avoids is tiny at that size. The advantage only appears as n grows.

A claim that survives measurement is worth more than one that is never tested.

---

# Part 10 — Publishing, submission and the viva

## 10.1 Where it is published

| | |
|---|---|
| Repository | https://github.com/prutxvi/contact-management-system |
| Visibility | public |
| Default branch | `main` |
| Licence | MIT |
| CI | passing on Ubuntu and macOS |

One consideration: because the repository is public, classmates can copy it. If the college treats
that as a problem, make it private until the grades are out, then publish.

## 10.2 Submitting it as a term project

```sh
make package     # writes dist/Group11_ContactManagementSystem.zip
```

The archive contains:

| File | Why |
|---|---|
| `Group11_ContactManagementSystem.cpp` | the whole program, one file, standard library only |
| `Group11_Report.md` | the full report — design, justification, pseudocode, results, references |
| `Group11_sample_output.txt` | a saved 20-step run, so there is output even if the live demo fails |
| `HOW_TO_RUN.md` | one-command build and run |

For a Word or PDF submission, open `docs/report.md`, paste into Word, export PDF. If the college
provides a cover-page template, use it.

Verify the package the way an examiner would — extract it somewhere clean, build, run:

```sh
cd /tmp && rm -rf verify && mkdir verify && cd verify
unzip -q /Users/pruthvi/Projects/contact-management-system/dist/Group11_ContactManagementSystem.zip
cd Group11_ContactManagementSystem
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -o cms Group11_ContactManagementSystem.cpp
./cms --data t.csv --seed
```

## 10.3 The viva, in short

`docs/viva_qa.md` has 30+ prepared answers. The five questions that decide the outcome:

| Question | The answer that wins |
|---|---|
| "Why a structure and not parallel arrays?" | Parallel arrays are linked only by convention. Sorting one without the other attaches phone numbers to the wrong names, and the compiler cannot catch it. |
| "Why four structures? Is that not wasteful?" | Each answers a different question. A hash table cannot answer "names starting with An" at all; a tree cannot answer "the record with this phone" in O(1). The cost is paid once at insert and returned on every later lookup. |
| "Why is the phone a string?" | `9876543210` overflows a 32-bit `int` and silently becomes `2147483647`. A leading zero would be lost. |
| "Why does delete not use `vector::erase`?" | It is O(n). We move the last record into the hole and repair one index entry — O(1), measured 46× faster at 100,000 records. |
| "Is your trie actually faster?" | Against a linear scan, yes, 5–7×. Against the ordered map's `lower_bound`, no — about 2× slower at this scale. It is asymptotically right for prefix completion and wins for long keys. Here are both numbers. |

That last answer is the one to rehearse. Volunteering your own counter-result is what makes the rest
of your claims believable.

## 10.4 Roadmap

- BK-tree for fuzzy search — cut the O(n·L²) scan to a tree walk, and extend the benchmark to prove it
- Union-Find to cluster and merge probable duplicates (the statistics screen already finds them)
- Group graph plus BFS — "how do I know this person?" through shared groups
- LRU cache for repeated lookups, with a measured hit rate
- On-disk index so loading does not rebuild everything
- vCard (RFC 6350) import/export alongside CSV
- `--query` batch mode so operations can be scripted without the menu

---

# Glossary

| Term | Meaning |
|---|---|
| **algorithm** | a step-by-step method for doing something with data |
| **amortised O(1)** | an occasional expensive operation spread over many cheap ones, so the average stays constant |
| **audit** | a self-check that verifies an invariant across all data |
| **big-O** | notation for how work grows as input grows, ignoring constant factors |
| **binary search** | halve the search range each step; requires data sorted on the searched key; O(log n) |
| **CI** | continuous integration — an automated build and test that runs on every push |
| **comparison sort** | a sort that only compares pairs; proven to need at least O(n log n) comparisons |
| **CSV** | comma-separated values; a plain-text table format, standardised in RFC 4180 |
| **data structure** | a way of arranging data in memory so some operations become cheap |
| **Damerau-Levenshtein** | edit distance that also counts a swap of two adjacent characters as one edit |
| **deque** | double-ended queue; push and pop at both ends |
| **edit distance** | the number of single-character edits needed to turn one string into another |
| **fault injection** | deliberately breaking something to prove a check can detect it |
| **hash table** | maps a key to a slot via a hash function; O(1) average; no ordering |
| **heap** | a tree-shaped region of memory used for dynamically allocated objects |
| **index** | a lookup table that points into the main storage; like the index of a book |
| **invariant** | a condition that must always remain true |
| **introsort** | the hybrid sort used by `std::sort`: quicksort, then heapsort, then insertion sort |
| **LIFO** | last in, first out; the rule a stack and an undo history follow |
| **linear search** | check items one by one; O(n); the only option for substrings |
| **lower bound** | a proven minimum; e.g. no comparison sort can beat O(n log n) |
| **mutate** | to change stored data |
| **node** | one element of a linked or tree structure |
| **O(1) / O(log n) / O(n)** | constant / logarithmic / linear growth in work |
| **prefix tree** | see trie |
| **record** | a struct; several named fields treated as one unit |
| **red-black tree** | a self-balancing binary search tree; what `std::map` usually is |
| **regression** | a change that breaks something that used to work |
| **RFC 4180** | the document that defines the CSV format |
| **stable sort** | a sort that keeps records with equal keys in their original relative order |
| **state machine** | code that moves between a fixed set of states while reading input |
| **STL** | the C++ Standard Template Library — the containers and algorithms in the standard library |
| **string** | a sequence of characters |
| **struct** | a user-defined type grouping several fields |
| **substring** | a contiguous run of characters inside a string |
| **swap-with-last** | delete by moving the final element into the hole, then shrinking |
| **time complexity** | how the running time grows with input size |
| **total order** | an ordering in which no two distinct items compare equal |
| **transposition** | swapping two adjacent characters in a string |
| **trie** | a tree where each edge is one character; lookup costs the key length |
| **undo stack** | the recorded history that lets a change be reversed |
| **unordered_map** | C++ hash table |
| **vector** | C++ growable array |
| **viva** | the spoken examination where you defend the work |

# Start here: everything explained, from zero

Read this before you edit anything. It covers four things in order:

1. What you are actually being asked to do.
2. The basics you need to understand the code.
3. How this project is built, piece by piece.
4. Every workflow, and how to change the code without breaking it.

---

# Part 1 - What the assignment is really asking

## The plain-words version

Write a C++ program that keeps a list of contacts and lets a user do seven things through a text
menu: add, delete, search by name, search by phone, update, show all, sort by name.

That part is easy. It is a beginner exercise.

## The part that decides your marks

The brief has one sentence that changes everything:

> "Students should be prepared to explain why each selected data structure was used."

And the demo requirement says you must "explain the data structures and algorithms used".

So the assignment is not testing whether you can store ten phone numbers. It is testing whether you
can **choose** a way to store them and **defend that choice with a cost**.

Two teams can write the same seven features. One gets an average mark, the other gets a top mark,
and the difference is a single sentence:

- Average: "We used an array because it is simple."
- Top: "We used an array of records for storage because appending is O(1) amortised and it is
  contiguous memory. We added a hash map for phone numbers because looking up a phone must be O(1)
  average, and a scan is O(n). We rejected a single structure because no single structure answers
  all four questions cheaply. Here are the measured numbers."

That second answer is the whole assignment. Everything in this project exists to make that answer
true and provable.

## What "DSA" means here

DSA = Data Structures and Algorithms.

- A **data structure** is a way of organising data in memory so that some operations become cheap.
- An **algorithm** is a sequence of steps that does something with that data.

The two words are joined because they trade off against each other. A structure makes some
algorithms fast and others slow. Choosing well is the skill.

The brief names five DSA topics it expects: structures, strings, arrays/STL containers, searching,
sorting. This project uses all five, plus four more that go beyond the brief (hash tables, tries,
edit distance, and self-checks).

---

# Part 2 - The basics you need

## 2.1 A record (struct) versus parallel arrays

Imagine you store contacts in two separate arrays:

```
names  = [ "Aarav", "Rohan", "Priya" ]
phones = [ "98765...", "90000...", "94455..." ]
```

Position 0 in `names` and position 0 in `phones` are supposed to describe the same person. Nothing
enforces that. If you sort `names` and forget to sort `phones`, then Aarav's row now shows Rohan's
number. The program still runs. The data is silently wrong.

A **struct** fixes this by making the person the unit:

```cpp
struct Contact {
    int         id;
    std::string name;
    std::string phone;
    // ...
};
```

Now you sort one array of `Contact` and everything moves together. The compiler enforces the link.
This is why the brief lists "structures" first, and it is why parallel arrays are the classic
mistake to avoid.

## 2.2 Big-O: how we compare two ways of doing the same thing

Big-O says how the work grows as the amount of data grows. It ignores constant factors and small
inputs. It answers: "if I make the list ten times bigger, how much slower does this get?"

| Notation | Name | For 1,000 records | For 100,000 records | In this project |
|---|---|---|---|---|
| O(1) | constant | 1 step | 1 step | phone lookup, ID lookup |
| O(log n) | logarithmic | ~10 steps | ~17 steps | name lookup in the ordered map |
| O(n) | linear | 1,000 steps | 100,000 steps | substring search, partial phone search |
| O(n log n) | linearithmic | ~10,000 steps | ~1.7 million steps | sorting |
| O(n^2) | quadratic | 1 million steps | 10 billion steps | the near-duplicate pair scan (capped, on purpose) |

The gap between O(1) and O(n) is the entire reason this project has four lookup paths instead of
one array. At 100,000 records, one extra lookup in the array costs about 70,000 nanoseconds. The
same lookup in the hash map costs about 97 nanoseconds. You can see that in `docs/benchmarks.md`.

**The honest part:** for 1,000 records, the "worse" structure is often fine, because constant
factors dominate at small sizes. Saying that out loud makes you sound like you understand the
theory instead of reciting it.

## 2.3 The five structures this project uses

Learn these five and you can read the whole program.

**Array / `std::vector`** - one block of memory, one item after another.
- Reading item number `k` is O(1) and very fast, because the address is just arithmetic.
- Adding at the end is O(1) amortised.
- Deleting from the middle is O(n), because everything after the gap must slide left.
- It does not sort itself and it does not know your keys.

**Record / `struct`** - several named variables bound into one type. Covered above.

**Hash table / `std::unordered_map`** - you give it a key, it computes `hash(key)`, and jumps
straight to a slot. O(1) average. Memory cost is higher, order is meaningless, and a bad hash
function degrades it toward O(n). It cannot answer "give me all keys starting with An", because
that is not what hashing does.

**Balanced tree / `std::map`** - keys kept in sorted order in a tree. Lookup is O(log n), which is
worse than a hash. In exchange, iterating gives you everything in key order, and one
`lower_bound` call jumps to the start of any range. It can answer "all names that start with An"
in O(log n + matches). This is why the project has a `map` for names and an `unordered_map` for
phones: they answer different questions.

**Trie (prefix tree)** - a tree where each edge is one character. "Aarav" is a path A-a-r-a-v.
- Looking up a prefix costs the length of the prefix, not the number of names.
- Everything under a node is every name with that prefix, and walking the children in character
  order gives them out **already alphabetised**. No sort needed.

Also used, in passing: **`std::deque`** as a bounded stack for undo. `std::stack` is last-in
first-out, but it cannot drop the oldest entry; a deque can pop from the front.

## 2.4 The three operations that decide the design

**Searching.** Three ways, each with a reason:
- *Linear search* - check one by one. O(n). Works on unsorted data. Only option for a substring.
- *Binary search* - check the middle, throw away half. O(log n). **Requires sorted data on the
  key you search.** This is the answer to the classic viva question "why can't you use binary
  search here?" - because we also need prefix and substring matches, and no ordering serves those.
- *Hash lookup* - O(1) average. Exact keys only, and it gives up all ordering.

**Deleting.** From an array, deleting position 5 means moving positions 6..n one slot left. That is
the O(n) the project avoids. The trick used instead is described in Part 3.

**Sorting.** Comparison sorting costs at least O(n log n) comparisons; that is a proven lower
bound, not an implementation detail. Two extra facts matter:
- `std::sort` is not **stable**: two records with equal keys may swap places. So two contacts both
  named "Ananya Iyer" can appear in a different order between runs.
- `std::stable_sort` keeps equal keys in their original relative order, and pays for it.
- The alternative fix is to make the key a **total order** by including the ID, so no two records
  are truly equal. Then even `std::sort` is deterministic.

## 2.5 Invariants: the concept the audit depends on

An **invariant** is a condition that must always be true.

This project's central invariant is: *for every record in the array, all four indexes point back
to that record, and no index points at a record that does not own it.*

Every operation must leave that invariant true. A delete that forgets to repair one index breaks
it silently - the record becomes unfindable, or a ghost entry appears. The audit screen checks the
invariant directly, which is how we prove the delete is correct instead of hoping.

---

# Part 3 - How this project is built

## 3.1 The core idea

One record, five structures, each answering one question:

| Structure | Holds | Answers | Cost |
|---|---|---|---|
| `std::vector<Contact> rows_` | the actual records | "what are my contacts" | append O(1), scan O(n) |
| `std::unordered_map<int, size_t> byId_` | id -> row number | "record with ID 7" | O(1) average |
| `std::unordered_map<std::string, size_t> byPhone_` | phone -> row number | "who owns this number", "is this a duplicate" | O(1) average |
| `std::map<std::string, std::vector<size_t>, CiLess> byName_` | name -> row numbers | "all records named X", "all names starting with An" | O(log n + matches) |
| `Trie trie_` | name prefixes -> IDs | "suggest a name from these letters" | O(prefix length) |

Plus `std::deque<Action> undo_` holding the last 20 changes.

The value in `byName_` is a **vector** of row numbers, not a single one, because two people can
share a name. That is a deliberate design decision, not an oversight.

Why `size_t` and not pointers? Because the array can move its memory when it grows. A row number
survives that; a pointer into the array would not.

## 3.2 A worked trace: adding a contact

You type: name `Aarav   Sharma`, phone `+91 98765 43210`, group blank.

1. `menuAdd` collects the raw text and calls `ContactStore::add`.
2. `validate` does, in order:
   - `squeezeSpaces` on the name: `Aarav Sharma`.
   - Reject empty, over 60 characters, or containing control characters.
   - Reject a name that is all digits.
   - `normalizePhone`: strip non-digits, strip a leading `91`, strip a leading `0`.
     `+91 98765 43210` becomes `9876543210`.
   - Check `byPhone_` for that number. If found, refuse and name the existing owner.
     This is one hash lookup, O(1).
   - Validate the email if one was given.
   - Default the group to `General`.
3. `add` assigns `nextId_`, pushes the record onto `rows_`, then calls `linkIndexes`:
   - `byId_[id] = newSlot`
   - `byPhone_[phone] = newSlot`
   - `byName_[name].push_back(newSlot)`
   - `trie_.insert(lowername, id)`
4. An `Action` of kind `Added` is pushed onto the undo deque.

Note that normalisation happens **before** the duplicate check. Without that order, `098765-43210`
and `9876543210` would both be accepted as different numbers.

## 3.3 A worked trace: deleting a contact

This is the most interesting code in the project. `vector::erase` would be O(n). Instead:

1. `remove` finds the row number through `byId_` (O(1)), copies the record out for the undo entry.
2. `eraseAt(pos)`:
   - `unlinkIndexes(rows_[pos], pos)` - remove this record from all four indexes.
   - Let `last = rows_.size() - 1`.
   - If `pos != last`: move `rows_[last]` into a local, `pop_back()`, then
     `unlinkIndexes(moved, last)` to erase the index entries that still point at the old last slot,
     then `rows_[pos] = moved` and `linkIndexes(rows_[pos], pos)`.
   - If `pos == last`: just `pop_back()`.
3. Push an `Action` of kind `Deleted`.

Exactly one record moved, so exactly one record needs repair. The cost does not grow with the list
length. Measured: 43.7x faster than `vector::erase` on 100,000 records.

**The catch to admit out loud:** storage order is now arbitrary. That is fine because order is a
property of the *view*, not of the array. Every display sorts, and IDs are monotonic so insertion
order is always recoverable by sorting on ID.

## 3.4 A worked trace: updating a contact

1. `update` finds the slot through `byId_`.
2. `validate(nv, selfId)` runs the same checks as add, **but the duplicate-phone check ignores a
   hit when the slot it found holds the same record**. Without `selfId`, saving a record without
   touching its phone would be rejected as a duplicate of itself.
3. `unlinkIndexes` the old version, assign the new fields, `linkIndexes` the new version.
4. `created` is copied from the old record, so the creation date is immutable even though every
   other field can change.

## 3.5 Undo

Every change records an `Action { kind, before, after }`.

- Undo an `Added` -> delete that record.
- Undo a `Deleted` -> push the record back with its original ID.
- Undo an `Updated` -> unlink the current version, restore `before`, relink.

Undo refuses when it cannot be correct, and says why. Example: restoring a deleted contact whose
phone number has since been given to someone else.

## 3.6 The audit and the fault-injection hook

`ContactStore::audit()` walks every record and checks the invariant in both directions:
- the ID index for this record points back at this slot,
- the phone index for this record points back at this slot,
- this slot appears exactly once across the name index, under a case-insensitively matching key,
- the trie still reaches this record,
- and the reverse: no index key points at a record that does not own it.

`injectFaultForTest(1..5)` deliberately breaks one index. The five faults are: drop a phone entry,
drop an ID entry, add a bogus name-bucket entry, drop a trie entry, insert a phone key no record
owns. `--inject-fault N --audit` is how you show the audit can fail.

This matters because a self-check that always passes is worthless. The test suite asserts all five
faults are detected and that clean data passes.

## 3.7 CSV: why it is not a `getline` loop

A field may contain a comma (`12, MG Road`), a double quote (`Flat 3B, "Sunrise"`), or a newline
(a two-line address). A naive splitter breaks on the first two, and a `getline`-per-row reader
breaks on the third.

So `csv::escape` writes quotes only when needed and doubles inner quotes, and `csv::parse` is a
small state machine that tracks whether it is inside quotes. `save` writes to `contacts.csv.tmp`
and only then renames, so a failure halfway through cannot destroy your existing file.

Loading maps columns **by header name**, so a reordered or extended CSV still loads. Invalid rows
and duplicate phones are skipped and counted in the load message.

---

# Part 4 - The code, top to bottom

`src/contact_manager.cpp` is one file, 1,456 lines, in eleven marked sections.

| Line | Section | What is in it |
|---|---|---|
| 40 | 1. Utilities | string trim/lower/compare, phone normalisation and validation, email validation, table formatting |
| 162 | 2. Edit distance | Damerau-Levenshtein DP, O(n*m) time, O(min) space |
| 192 | 3. Trie | `class Trie`, `Node`, insert, erase with pruning, prefix enumeration, node count |
| 264 | 4. CSV | `escape`, `joinRecord`, `parse` state machine |
| 330 | 5. The record | `struct Contact` |
| 343 | 6. ContactStore | the heart: five structures, add, search x4, update, remove, undo, audit, save, load, validate, link/unlink, `eraseAt` |
| 873 | 7. Console helpers | `class Console` (input), `printContacts`, `printOne`, `printRule` |
| 955 | 8. Report | statistics screen, group histogram, duplicate and near-duplicate detection |
| 1036 | 9. Sorting experiment | `std::sort` vs `std::stable_sort` vs total-order key, with comparison counters and a 64-record stress test |
| 1142 | 10. Sample data | the ten seed contacts |
| 1171 | 11. Menu handlers | one `menuXxx` function per option, plus `main` at line 1348 |

Key methods inside `ContactStore`, with line numbers so you can jump straight there:

| Line | Method | Job |
|---|---|---|
| 370 | `add` | validate, assign ID, append, link indexes, record undo |
| 389 / 394 | `findById` / `findByPhone` | O(1) lookups |
| 400 | `searchByName` | four modes: exact, prefix, contains, fuzzy |
| 438 | `searchByPhone` | exact hash hit, else linear scan |
| 448 | `autocomplete` | trie prefix walk |
| 460 / 470 | `allById` / `sorted` | materialise, then sort by a chosen key |
| 490 | `update` | with the `selfId` self-exclusion |
| 509 | `remove` | O(1) delete |
| 521 | `undo` | three action kinds |
| 576 | `audit` | the invariant check |
| 640 | `injectFaultForTest` | testing hook |
| 653 / 673 | `save` / `load` | atomic write, header-driven read |
| 777 | `validate` | all input rules in one place |
| 816 / 823 | `linkIndexes` / `unlinkIndexes` | the only two places that touch indexes |
| 838 | `eraseAt` | swap-with-last delete |
| 852 | `pushUndo` | bounded history |

Two design rules worth noticing, because they are what make the code safe:
- **All mutations go through one path.** `linkIndexes` and `unlinkIndexes` are the only functions
  that touch the indexes. There is no second place to forget.
- **Nothing stores a pointer into the array.** Indexes store row numbers, so a reallocation cannot
  leave a dangling pointer.

---

# Part 5 - Every workflow

## 5.1 Build and run

```sh
cd /Users/pruthvi/Projects/contact-management-system

make            # compile ./cms
make run        # run it against ./contacts.csv
make test       # compile, then run the 64 behaviour tests
make measure    # rebuild docs/benchmarks.md on this machine
make demo       # replay demo/steps/in.txt -> demo/session.log
make clean      # remove the binaries and data files
make bench      # compile the benchmark binary as ./bench_bin
```

The compile command the Makefile runs, if you want it by hand:

```sh
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -o cms src/contact_manager.cpp
```

Command-line flags:

| Flag | Effect |
|---|---|
| `--data FILE` | which CSV to load at start and save into (default `contacts.csv`) |
| `--seed` | add the ten sample contacts if the list is empty |
| `--audit` | run the index self-check, print the result, exit 0 or 1 |
| `--inject-fault N` | testing hook: corrupt index N (1-5) after loading |
| `--help` | usage |

## 5.2 The user workflow: what each menu option does inside

| Option | Internally |
|---|---|
| 1 Add contact | prompts five fields, `validate`, `add`, links four indexes, records undo |
| 2 Display all contacts | asks a sort key and direction, `sorted()`, prints a table, reports the comparison count |
| 3 Search by name | asks the query and one of four modes: exact (map), prefix (map range), contains (scan), fuzzy (edit distance) |
| 4 Search by phone | `normalizePhone`, `byPhone_` hash hit, else a linear scan; the message says which path ran |
| 5 Update contact | shows the record, asks each field (blank keeps, `-` clears), `update` with `selfId` |
| 6 Delete contact | shows the record, confirms, `remove` -> `eraseAt` -> index repair |
| 7 Sort experiment | runs three sorts on real data, plus a 64-record stress test that shows stability |
| 8 Autocomplete | `trie_.prefix`, prints up to 10 suggestions |
| 9 Undo | pops one `Action` and reverses it |
| 10 Statistics | structure sizes, group histogram, missing fields, repeated names, near-duplicate names |
| 11 Save | atomic write to the CSV |
| 12 Reload | re-parse the CSV and rebuild every index |
| 13 Load sample data | ten seed contacts, including a deliberate duplicate name |
| 14 Validate indexes | runs `audit()` and reports |

## 5.3 The demo workflow

`demo/steps/in.txt` is the exact list of keystrokes. `demo/steps/intent.txt` says what each step is
for, in order. `demo/run_demo.sh` replays it and writes `demo/session.log`.

To change the demo: edit the two files in `demo/steps/`, then run `make demo`. If you rerun the
demo it starts from a clean `scratch/demo.csv`, so it is repeatable.

The demonstration ends with the audit (option 14). That is deliberate: it is the sentence that
closes the questioning.

## 5.4 The test workflow

```sh
python3 tests/test_cms.py     # or: make test
```

Each test pipes a script into the compiled binary and checks its stdout. That means the tests
exercise the same code path your demo does. Fourteen tests:

| Test | What it guards |
|---|---|
| `test_empty_store_is_safe` | every option on an empty list prints a message instead of crashing |
| `test_validation_rules` | bad phones, all-digit names, bad email, duplicates across notations |
| `test_whitespace_is_squeezed` | `  Aarav   Sharma ` becomes `Aarav Sharma` |
| `test_delete_keeps_indexes_consistent` | after deleting a middle record, every survivor is still reachable by ID, name and phone |
| `test_undo_paths` | delete-undo, add-undo, empty history |
| `test_sorting_is_monotonic` | the sort is actually sorted and counts comparisons |
| `test_csv_round_trip_survives_punctuation` | commas, quotes, two-line addresses, byte-stable save/reload |
| `test_csv_duplicate_and_bad_rows_are_reported` | skips are counted, not silent |
| `test_missing_required_column_is_reported` | wrong header is refused with a useful message |
| `test_unwritable_destination_fails_cleanly` | save into a missing directory reports, does not crash, keeps memory data |
| `test_update_protects_duplicate_phone_and_immutable_id` | self-exclusion works, rename allowed |
| `test_autocomplete_order_and_prefix_misses` | two hits for "An", and a clear negative result |
| `test_large_batch_insert_and_lookup` | 200 inserts, statistics, exact hash hit, reload |
| `test_index_audit_passes_and_detects_every_fault` | audit passes clean and fails on all five injected faults |

Run this after every edit. It is your safety net.

## 5.5 The benchmark workflow

```sh
make measure       # rebuilds and reruns, writes docs/benchmarks.md
./bench_bin        # same, but only the sizes you pass
./bench_bin 5000 50000
```

It measures, at 1,000 / 10,000 / 100,000 records:
- exact phone lookup: hash map vs sorted vector + `binary_search` vs linear scan,
- prefix search: trie vs ordered map `lower_bound` vs linear scan,
- deleting 10% of records: swap-with-last vs `vector::erase`,
- sorting: `std::sort` vs `std::stable_sort`, with comparison counts against the n log2 n bound,
- fuzzy search: one query over all records,
- memory: vector payload and trie node count.

Absolute times depend on the machine. The shape of the curves does not.

## 5.6 The edit workflow: the loop to follow every time

```
1. Edit src/contact_manager.cpp
2. make                     -> must build with zero warnings
3. make test                -> must print "64 passed, 0 failed"
4. ./cms --seed --data /tmp/x.csv     -> click through the change by hand
5. ./cms --data /tmp/x.csv --audit    -> must print AUDIT PASSED
6. make measure             -> only if you touched a structure or an algorithm
7. make demo                -> only if you changed the menu or the demo steps
8. Update README.md / docs   -> if the change is user-visible
```

Step 5 is the one people skip and regret. It catches index bugs immediately.

---

# Part 6 - How to change the code without breaking it

Each recipe lists every place you must touch. The "if you forget" column is the failure you will
actually see.

## Recipe A - add a new field to `Contact` (example: `birthday`)

| Place | Line | Change |
|---|---|---|
| `struct Contact` | 332 | add `std::string birthday;` |
| `validate` | 777 | validate it, and default it if empty |
| `printContacts` | 922 | add a column and widen the rule length |
| `printOne` | 942 | add a line to the single-record view |
| `menuAdd` | 1173 | prompt for it |
| `menuUpdate` | 1261 | prompt for it, and decide blank/clear behaviour |
| `save` | 653 | add it to the header string **and** to the `joinRecord` list, in the same order |
| `load` | 673 | add a `col("birthday")` and a `cell(...)` call |
| `loadSamples` | 1144 | add it to the `Seed` struct and the seed table |

If you forget: `save`/`load` is the dangerous pair. A missing header column silently writes an
extra field into the wrong column, and a missing `cell` reads an empty string. Always run
`make test` (the round-trip test) plus `--audit` after this change.

## Recipe B - add a new menu option

| Place | Line | Change |
|---|---|---|
| write the handler | near 1171 | `void menuFoo(Console& con, ContactStore& store)` |
| menu text | 1408 | add a line to the printed menu |
| range check | 1425 | widen `con.integer("Choice: ", 0, 14, choice)` |
| dispatch | 1426 | add `case 15: menuFoo(con, store); break;` inside the `switch` |

If you forget the range check, the option is unreachable. If you forget the dispatch, it prints
"Option not handled."

## Recipe C - add a new sort key

| Place | Change |
|---|---|
| `enum class SortKey` | 360 | add the key |
| `sorted` | 470 | add a `case` to the comparator switch |
| `menuDisplay` | 1191 | add the option and its `keyName` label |

## Recipe D - add a new index

1. Add the container as a private member of `ContactStore`.
2. Add its maintenance to **both** `linkIndexes` and `unlinkIndexes`. Never anywhere else.
3. Add its checks to `audit()`, in both directions.
4. Add a fault case to `injectFaultForTest` and an expectation to the audit test.
5. Clear it in `load` before the staging loop.
6. Add it to the statistics report so its size is visible.

If you skip step 2 for any one path, the index will drift and only the audit will tell you.

## Recipe E - change the phone rule

`util::normalizePhone` strips separators and country code. `util::isValidPhone` enforces
"exactly 10 digits, first digit 2-9". `validate` calls both, and the error message names the rule.

If you change the length or the country, update the message text too, and update
`test_validation_rules`, which asserts on that text.

## The two rules that keep this codebase safe

1. **Indexes are only touched by `linkIndexes` and `unlinkIndexes`.** If you catch yourself writing
   `byPhone_[...] = ...` anywhere else, stop and add a method instead.
2. **Never store a pointer or iterator into `rows_`.** Store the row number. `eraseAt` moves
   records and `push_back` can reallocate; both invalidate pointers.

---

# Part 7 - Where to go next

| Question | File |
|---|---|
| What should I build next, ranked? | `docs/idea_book.md` |
| What will the panel ask, and what do I answer? | `docs/viva_qa.md` |
| Where did these numbers come from? | `docs/benchmarks.md`, `bench/bench.cpp` |
| What exactly did the brief say? | `docs/assignment.md` |
| What does this program do, structurally? | `README.md` |
| What does a full run look like? | `demo/session.log` |

If you only read one thing before the demo, read the two honest paragraphs: the trie is
asymptotically right but measures about 2x slower than the ordered map at this scale, and the fuzzy
search is the slow path on purpose. Volunteering both makes every other claim you make credible.

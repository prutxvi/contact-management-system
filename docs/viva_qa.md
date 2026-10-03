# Viva preparation

Three kinds of question come up in a practical exam. Prepare all three.

- **Part A** is the definition layer. Every group member must answer these instantly, out loud,
  without looking at the code.
- **Part B** is about this project. The panel will point at a line and ask why it is there.
- **Part C** is the live edge-case demand: "show me what happens when ...".

---

## Part A - the definition layer

Short answers only. These are the standard questions in published data-structures practical
question banks.

| Question | Answer |
|---|---|
| Define searching. | Finding the location of a record inside a collection of records. |
| How many types of searching are there? | Two: linear (sequential) and binary. This project also uses hashing-based lookup and a trie walk, which are further search methods. |
| Why is binary search faster than linear search? | Each comparison discards half of the remaining range, so it needs about log2(n) comparisons instead of n. |
| Why can binary search not be used on our data? | Binary search needs the data sorted by the key being searched. We also need substring and prefix matches, which no ordering can serve, so those paths scan or use an index instead. |
| What is the worst case of linear search? | The record is last, or absent: n comparisons. |
| How many comparisons if the record is first? | One. |
| Define time complexity. | How the number of basic operations grows as the input size grows. |
| Difference between O(n) and O(log n) in practice | On 100,000 records a linear scan needs about 100,000 steps, a binary search about 17. Measured numbers are in `docs/benchmarks.md`. |
| What is a structure? | A user-defined type that groups variables of possibly different types under one name. |
| How is a structure different from an array? | A structure is heterogeneous and its members are named; an array is homogeneous and its elements are reached by an index. |
| Can we make an array of structures? | Yes. We use `std::vector<Contact>`, which is an array of structures with a managed size. |
| Operations on an array? | Traversal, insertion, deletion, searching, sorting and merging. |
| Limitations of an array? | Fixed size, insertion and deletion cost O(n) because elements must shift. We avoid the second limit for deletion with the swap-with-last trick. |
| Difference between call by value and call by reference. | By value copies the argument; by reference passes the same object, so changes are visible to the caller. Our `add` takes `Contact` by value because it wants its own copy to normalise; `undo` takes a `std::string&` because it wants to report back. |
| What is a stack used for here? | Undo history: the last change is the first one undone, which is last-in first-out. |
| What is a queue used for here? | Nothing in the current build. It would suit a "send reminder" work list. We use `std::deque` only as a bounded history with removal from the front. |
| What is a hash table? | A structure that maps a key to a slot using a hash function, giving average O(1) lookup, at the cost of order and extra memory. |
| What is a collision? | Two keys hashing to the same slot. The standard library resolves it for us, typically with chaining. |
| Difference between `std::map` and `std::unordered_map`? | `std::map` is a balanced search tree, so operations are O(log n) and iteration is in key order. `std::unordered_map` is a hash table: O(1) average, no order. We use both, each where its property is what we need. |
| What is a trie? | A tree where every edge is a character and every node is a prefix, so a lookup costs the length of the key. |
| What is stability in sorting? | A stable sort keeps records with equal keys in their original relative order. `std::stable_sort` guarantees it; `std::sort` does not. |
| Difference between `std::sort` and `std::stable_sort`? | Same big-O class, but `stable_sort` keeps equal keys in order and may use extra memory. We measure both. |
| What is a vector's size versus its capacity? | `size()` is how many records exist; `capacity()` is how much space is reserved. Exceeding capacity triggers a reallocation, which is why `push_back` is amortised O(1), not always O(1). |
| What is amortised O(1)? | An occasional expensive operation spread over many cheap ones, so the average per operation stays constant. |
| What is a string? | A sequence of characters. In C++ `std::string` manages its own memory. |
| Difference between a character array and `std::string`? | A character array has a fixed size and needs manual bounds handling; `std::string` grows, knows its length, and supports `find`, `substr` and comparison operators. |
| What is a file? | A named block of data on disk that survives the program ending. We use it so the contact list is not lost at exit. |
| What is the difference between text and binary file handling? | Text mode is for human-readable data and may translate line endings; binary mode copies bytes exactly. Our CSV file is text. |
| What are the file modes? | `ios::in` read, `ios::out` write, `ios::app` append, `ios::trunc` discard existing content. We open the CSV with `ios::in` to read and `ios::trunc` to write. |
| What is a sentinel? | A value that marks the end of data. We do not need one: the container knows its own size. |
| What is the difference between `#include <string>` and `#include "file.h"`? | Angle brackets search the system include path, quotes search the local directory first. |

---

## Part B - questions about this project

### "Why a structure and not parallel arrays?"

Parallel arrays are two arrays that must always be kept in the same order by hand. Sorting one
without the other, or deleting from one and not the other, silently attaches the wrong phone to
the wrong name, and the compiler cannot catch it. Printing the two arrays before and after such a
sort is a good live demonstration: Ann 111, Bob 222, Zed 333 becomes Ann 111, Bob 222, Zed 333
with the phones attached to the wrong names once one array is sorted. An array of structures
cannot desynchronise, because the record is the unit.

### "Why `std::vector<Contact>` and not `Contact contacts[100]`?"

A fixed array needs a hand-kept count. That count is a second source of truth, and any bug in it
either hides records or reads past the end. `std::vector` knows its own size, grows on demand,
and cannot desynchronise from its contents. There is also no arbitrary 100-contact ceiling.

### "Why are there three indexes? Is that not wasteful?"

Each index answers a different question, and each is measured to be worth its memory.

- `byId_` (`unordered_map<int, size_t>`): update and delete must start in O(1) from an ID.
- `byPhone_` (`unordered_map<string, size_t>`): phone search and the duplicate check are O(1).
- `byName_` (`map<string, vector<size_t>>`): gives ordered traversal and a prefix range in
  O(log n + k), and gives the alphabetical display without a sort step.

A single structure cannot do all three: a hash table cannot answer "all names starting with An",
and a tree cannot answer "the record with this phone" in O(1). The trade is paid once at insert
time and returned on every later lookup, which is exactly what the benchmarks show.

### "Why is the phone number a string and not an integer?"

`9876543210` overflows a 32-bit `int` and silently becomes `2147483647`. Every Indian mobile
number starting 6-9 has this problem. A number with a leading zero would also lose it. On top of
correctness, we store a normalised and displayable value, and we need digit-level validation
rather than arithmetic.

### "Why does delete not use `vector::erase`?"

`vector::erase` is O(n): every element after the erased one moves. We move the last record into
the hole and pop the last slot instead. One record moved, so one record needs re-indexing: unlink
it from the old slot, relink it at the new one. Deletion becomes O(1) of work plus the trie
update, independent of n. Measured on 100,000 records it is about 46x faster than `erase`
(`docs/benchmarks.md`).

### "Does swap-with-last not destroy the ordering?"

It destroys *storage* order, which we never promised. Order is a property of the view, not of the
array: every display sorts by ID, name, phone, group or date, and IDs are monotonic, so "insertion
order" is always recoverable. Keeping the array dense instead is worth more, because printing and
sorting then walk contiguous memory.

### "How do you stop a duplicate phone number?"

`byPhone_` holds every phone number, so the duplicate check is one hash lookup in `add` and in
`update`. Normalisation happens first, so `+91 98765 43210` cannot hide behind a different
notation from `9876543210`. Names are *not* forced unique, because two people can genuinely share
a name; the statistics report lists those records instead.

### "How does update avoid rejecting a contact that keeps its own phone?"

`validate` takes a `selfId`. A duplicate hit is ignored when the slot it found holds the record
being edited. Without that, saving a record without touching its phone would be rejected.

### "What does the trie add that the `std::map` name index does not?"

Three things, and it is worth being precise because this question is a trap if answered carelessly.

- Against a **linear scan** it wins by 5.6x to 7.2x, in the same measured run. That is the
  comparison most students make and it is not the interesting one.
- Against the **ordered map's `lower_bound`**, at a few thousand to a hundred thousand in-memory
  records, the trie is actually about **2x slower** in this build. Both structures chase pointers,
  and the trie pays more per node. Say this before the panel finds it.
- Where the trie genuinely wins is asymptotic, not constant: O(prefix length) regardless of how
  many keys exist, matches returned in alphabetical order with no extra sort, and it is the
  natural structure for ranked or incremental suggestions as the user keeps typing.

The correct summary is: the trie is the right structure for this job on the merits, the map is
faster in this particular build at this particular scale, and we kept both so the trade can be
demonstrated rather than asserted. Independent research for this project measured the same effect
on a different implementation (a trie was about 4x slower than `lower_bound` over a sorted vector
for 20,000 prefix queries), so the effect is real and not a quirk of this code.

### "Then why keep the trie at all?"

Because the brief asks us to demonstrate data structures and to explain the choice, and because
the asymptotic argument is the correct one for a contact store that is expected to grow. Keeping
both, with the numbers on screen, is stronger than claiming a win that the measurement does not
support. If the panel prefers the simpler answer, the fallback is one sentence: "the map alone is
sufficient for the required features; the trie is there for autocomplete specifically."

### "What is `std::stable_sort` doing that matters?"

Records that share a name can swap position under `std::sort`, so two contacts named "Ananya
Iyer" can appear in a different order from one run to the next. `std::stable_sort` keeps them in
ID order. The demonstration presses this: on 64 records with 8 repeated names, `std::sort`
reordered 95 pairs, `std::stable_sort` reordered 0, and a `std::sort` with the tie broken on ID
also reordered 0.

Follow-up to have ready: **which fix is better?** Breaking the tie in the comparator, because then
any sort can be used and the order does not depend on a stability guarantee. `std::stable_sort` is
also documented as costing O(N log^2 N) comparisons unless it has spare memory, so the total-order
key is cheaper as well as more predictable. We demonstrate stability because it is a syllabus
point, and we demonstrate the total-order key because it is the answer a senior engineer gives.

### "How does undo work?"

Every change records an `Action` with the record before and after it. Add undoes by deleting,
delete undoes by reinserting with the original ID, update undoes by restoring the previous fields
and re-indexing. The history is bounded to 20 entries, and undo refuses honestly when it cannot
be applied, for example when the deleted contact's phone has since been given to someone else.

### "Why is the CSV parser not just `getline` with commas?"

Because a field may legally contain a comma, a double quote or a newline. `Flat 3B, "Sunrise"
Apartments` breaks a naive splitter, and an address spanning two lines breaks a `getline`-per-row
reader. We use a small state machine that tracks whether it is inside quotes, and we write the
matching escape routine. `std::quoted` handles quotes but not the CSV comma convention, so a
purpose-built pair is the correct answer here.

### "What happens if the file cannot be written?"

`save` writes to `contacts.csv.tmp` and renames it only after the write succeeded. If the file
cannot be opened or the write fails part way, the previous file is untouched, the error is
reported, and the in-memory contact list keeps working. This is the "unavailable resources" case
in the brief.

### "How do you know the delete did not corrupt an index?"

Menu option 14 runs an audit: it walks every record and checks that the ID index points back to
that slot, that the phone index points back to that slot, that the record appears exactly once
across the name index and under the right key, and that the trie still reaches it. It also checks
the reverse direction, so a stale key that no record owns is caught.

The fair question is "what makes you think the audit works?". A testing hook corrupts one index
on purpose (`--inject-fault 1..5`) and the test suite asserts that each of the five corruptions is
reported. A clean run says `AUDIT PASSED` and exits 0; a corrupted one names the broken mapping
and exits 1.

### "How do your tests run a console program?"

`tests/test_cms.py` pipes a script into the compiled binary and reads its stdout, so the tests
exercise exactly the code path the demonstration uses. That is also why the program reads whole
lines instead of using `cin >>`: a scripted stream only behaves predictably when the program
consumes a predictable number of lines.

### "Do you use global variables?"

Only constants. The input state lives in a `Console` object, not in a global, because a mutable
global is a documented style violation and a bug magnet: any function can change it, and nothing
tracks who did.

### "Why `getline` everywhere instead of `cin >>`?"

`cin >>` stops at the first space, so "Ravi Kumar" would be stored as "Ravi". It also leaves the
newline in the buffer, so the next `getline` returns an empty string. Reading whole lines and
validating afterwards avoids both traps, and it is why the menu can survive non-numeric input.

### "What is the complexity of each operation?"

The table is in `README.md` and reproduced in the report screen. Numbers to have ready: add O(1)
amortised plus O(log n) name-index insert; find by ID or phone O(1) average; find by name O(log n
+ k); autocomplete O(p + k); update and delete O(1) plus the name length; sort O(n log n);
fuzzy search O(n * L^2).

---

## Part C - live edge cases to demonstrate

Show the before and after list state, not just the message.

1. **Duplicate phone in another notation.** Add `098765-43210` when `9876543210` exists. Expect
   rejection that names the existing owner, then display the list and show it is unchanged.
2. **Bad phone number.** Add `12345`, then `0123456789`. Both rejected, and the message states the
   rule that failed. `0123456789` is ten digits, so it proves the leading-digit rule is enforced.
3. **Name with messy spacing.** Add `   Pruthvi    Raj   Toganti  ` and show the stored value is
   `Pruthvi Raj Toganti`.
4. **Self-update.** Update a record, change only the name, and leave the phone alone. It must be
   accepted, which proves the duplicate check excludes the record being edited.
5. **Update into someone else's phone.** Rejected, with the owner named.
6. **Delete the record in the middle, then search for the record that was last.** This is the
   index-repair demonstration. The last record moved into the hole and must still be found by ID,
   by name and by phone.
7. **Delete then undo.** The record returns with its original ID.
8. **Empty list.** Run search, update, delete, sort and autocomplete before adding anything. Each
   prints its own message; the search message differs from "no match", and nothing crashes.
9. **Empty search text.** Rejected rather than matching every record.
10. **Sort stability.** Show the 64-record stress test: `std::sort` reorders 95 shared-name pairs,
    `std::stable_sort` reorders none.
11. **CSV with a comma, a quote and a two-line address.** Load it, display it, save it, and show
    the file is byte-for-byte identical, which proves the round trip.
12. **CSV with a duplicate phone row and a junk row.** Load message counts both skips.
13. **CSV with the wrong header.** Rejected with the required column names in the message.
14. **Save into a directory that does not exist.** Error reported, program stays alive, data still
    in memory.
15. **Non-numeric menu input.** Type letters at the menu. Rejected, menu repeats, no infinite loop.
16. **Repeated name.** Two contacts named `Ananya Iyer` coexist, and the statistics report lists it.

---

## Part D - anti-patterns avoided, and why

| Anti-pattern | Why it costs marks | What this project does instead |
|---|---|---|
| Parallel arrays | silent data corruption, as shown above | one `std::vector<Contact>` |
| Non-constant global variables | documented style violation; untracked state | `Console` object, constants only |
| No input validation | the overflow and getline bugs above are silent | `Console::integer`, `validate` |
| Bubble sort as the only sort with no complexity statement | O(n^2) with no justification | `std::stable_sort`, O(n log n), with measured comparison counts |
| `system("cls")` / `system("pause")` | non-portable, spawns a shell | none; plain `std::cin`/`std::cout` |
| Commenting every line | noise that hides the code | comments explain *why*, not *what* |
| One giant `main` | cannot be tested or reviewed | one handler per menu option, one method per operation |
| Duplicated add and edit logic | the two copies drift apart | `add` and `update` share one `validate` |
| `char name[50]` with `strcpy`/`strcmp` | arbitrary limit, buffer risk, painful case handling | `std::string` with `find`, `substr`, comparison |
| Storing a phone in `int` | silent overflow | `std::string` |
| Reusing the add path for update | rejects a record keeping its own phone | `selfId` parameter |
| Stale index after delete | a record becomes unfindable, or a ghost stays | swap-with-last re-indexes exactly the record that moved |

One caution about a common claim: `using namespace std;` is a frequent viva *question* ("name
collisions, unclear origin, would be qualified in production"), but it is not a rubric line in the
marking schemes found during research. Do not present it as a mark loss; present it as a style
choice. This project does not use it.

---

## Part E - the two-minute demonstration

Run `make demo`, or type the sequence in `demo/steps/intent.txt`. The order is chosen so every
claim is shown before it is explained.

1. Load the sample data (option 13). Ten contacts, including two deliberately named "Ananya Iyer"
   and two phone numbers written in `+91` and `0` notation.
2. Display sorted by name (option 2). Point out that the display is alphabetical with no separate
   sort step in the user's flow, and that the count of comparisons is printed.
3. Search by name prefix "Ananya" (option 3, mode 2). Two hits, O(log n + k).
4. Search by full phone (option 4). "Exact hash hit, O(1)". Then search a partial number and point
   out the message changes to a linear scan, because a partial key cannot use a hash.
5. Search "Ananaya" with mode 4. The typo still finds "Ananya Iyer" through edit distance.
6. Search "kavya" in lower case with mode 3. It finds "Kavya Menon", which shows the comparisons
   are case-insensitive.
7. Add a contact with an existing phone number (option 1). Rejected, and the message names the
   owner. Add one with `12345`. Rejected with the rule.
8. Update "Rohan Verma" and try to give it "Ananya Iyer"'s phone (option 5). Rejected. Then change
   only its name. Accepted, which proves the self-exclusion.
9. Delete "Priya Nair" (option 6), then undo (option 9). The record returns with ID 5.
10. Autocomplete the prefix "Sa" (option 8). One suggestion from the trie.
11. Open the sorting experiment (option 7). Read out the comparison counts against the n*log2(n)
    bound, then the stress test: 95 reordered pairs under `std::sort`, 0 under `std::stable_sort`.
12. Open the statistics report (option 10). Show the structure sizes, the group split, and the
    duplicate-name alert.
13. Save (option 11), reload (option 12), and exit (option 0). Mention that the CSV handles
    embedded commas, quotes and newlines, and that a save is atomic.

Close with the one sentence the panel is listening for: **every structure in this program is
there for a named reason, and each reason has a number attached to it in
`docs/benchmarks.md`.**

---

## Part F - sources and honesty notes

- Marking split for laboratory courses with viva weightage: VTU Regulations 2021, Annexure-I rubric
  table (CIE 50 marks; results and viva-voce carry 60% weightage in the laboratory test split).
  The PDF was retrieved and read during this research.
- Model viva question banks for a data-structures laboratory: government polytechnic practical
  question bank PDF, retrieved and read. Questions such as "define searching", "how many types of
  searching", "how many comparisons when the record is at the first position" come from it.
- Coding style rules used for Part D (`-Wall -Wextra` clean, no non-constant globals, no
  `system("pause")`/`system("cls")`): a university C++ grading style guide, retrieved and read.
- Contact-manager specific review points (names and phones are not unique; put validation on the
  record; return all matches; do not duplicate add and edit): a public code review of a contact
  manager, retrieved and read.
- Overflow, buffer, stability and case-sensitivity claims were reproduced locally by compiling and
  running small C++ programs, not taken on trust.
- Not verified, so do not quote it: any specific percentage split for a *mini-project* (as opposed
  to a laboratory experiment) was not found. Treat the viva as important because the standard
  laboratory rubric gives it real weight, not because a specific mini-project split was located.

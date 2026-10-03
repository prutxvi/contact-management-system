# Idea book

Ideas for turning "Group 11 - Contact Management System" from a working CRUD console app into the
project the panel remembers, without leaving the syllabus.

Everything marked **built** already exists in this repository and runs today. Everything marked
**idea** is a concrete, sized proposal. Nothing here is a suggestion to abandon the required
console application: the console application is the deliverable, and every idea below is a way to
make it score higher.

---

## 1. The thesis in one line

Everyone in the class will submit the same seven features. The marks are not in the seven
features, they are in the sentence the brief writes in bold: *"Students should be prepared to
explain why each selected data structure was used."*

So the winning move is not "more features". It is: **one contact record, four lookup paths, each
chosen for a named cost, each cost printed on screen as a measured number.**

That is the whole strategy. The rest of this file is how to execute it.

## 2. What the brief is actually grading

Read the demo requirement again. It asks for four things, and only two of them are code:

| What is asked | What it really tests |
|---|---|
| "explain the problem" | did the team understand the requirement, or just start typing |
| "show the major features" | does it run, live, on the panel's input |
| "demonstrate at least two edge cases" | does the team know where its own code breaks |
| "explain the data structures and algorithms used" | can each member defend a choice, with a cost |

Standard laboratory rubrics give the oral defence real weight, so this is not a formality. A
team that shows a flawless happy path and then says "we used an array because it is easy" loses to
a team that shows a rejected duplicate with a clear message and says "that check is one hash
lookup, O(1) average, and here is why it cannot be a scan".

**The reliable separator between a top answer and an average answer**, from the marking schemes
and code reviews read during this research:

- Average: names the container, passes the happy path.
- Top: attaches a complexity number *and a rejected alternative* to every choice, and shows
  before/after list state for each edge case.

"We used `std::vector` for storage, `unordered_map` for phone lookup because exact lookup should be
O(1) and a scan is O(n), `std::map` for names because we need sorted traversal and prefix ranges,
a trie for autocomplete because it is O(prefix length), and we rejected `std::list` because
contiguous storage makes display and sort much faster in practice."

That paragraph is the project. Everything below serves it.

## 3. Three tiers

| Tier | Purpose | Budget | Status |
|---|---|---|---|
| Tier 1 | meet the brief, correctly, with validation | the required work | built |
| Tier 2 | the differentiators that win the comparison | about one day of work | mostly built |
| Tier 3 | extra structures that make the panel lean forward | optional, pick one or two | not built |

Do not skip Tier 1 to reach Tier 3. A beautiful BK-tree on top of a program that crashes on
`cin >>` scores worse than a plain program that never crashes.

---

## 4. Tier 1 - the brief, done properly

These are the ideas that make the required features defensible rather than merely present.

### Idea 1 - One record, four lookup paths (**built**)

- **What:** one `std::vector<Contact>` for storage; an `unordered_map` for ID; an `unordered_map`
  for phone; a `map` for lower-cased name.
- **DSA concept:** hashing, balanced search trees, arrays, the trade between order and speed.
- **Why it lands:** this *is* the "explain why" answer. It also creates the natural question "why
  three indexes?" which the team can answer with a table.
- **Effort:** medium.

### Idea 2 - O(1) delete by moving the last record into the hole (**built**)

- **What:** `vector::erase` is O(n) because everything after the hole shifts. Move the last record
  into the hole instead, pop the tail, and repair only the index entries for the one record that
  moved.
- **DSA concept:** array deletion, index invalidation, amortised reasoning.
- **Why it lands:** it is a genuine insight, it is measurable (about 46x on 100,000 records), and
  it invites the follow-up "does that not break your order?" which has a clean answer: order is a
  property of the view, and every view sorts.
- **Effort:** medium. It is the single best line in the whole project.

### Idea 3 - Strict input reading instead of `cin >>` (**built**)

- **What:** read whole lines, then parse. Reject "12abc", reject out-of-range, never leave the
  stream in a failed state.
- **DSA concept:** parsing and validation, not data structures, but it is where the brief's
  "invalid input" requirement is actually satisfied.
- **Why it lands:** `cin >>` fails in two silent ways, both reproducible in ten seconds live.
  `cin >> name` truncates "Ravi Kumar" to "Ravi"; and `cin >>` leaves the newline behind so the
  next `getline` returns an empty name and a blank record is stored. Showing those two failures
  and then showing the fix is a two-minute demo by itself.
- **Effort:** small.

### Idea 4 - Phone numbers are strings, normalised, uniquely indexed (**built**)

- **What:** strip spaces, dashes, a leading `0` and a `+91`; validate ten digits starting 2-9;
  enforce uniqueness on that normalised value.
- **DSA concept:** strings, canonicalisation, hash-key equivalence.
- **Why it lands:** three separate talking points in one feature.
  1. `9876543210` does not fit in a 32-bit `int` and silently becomes `2147483647`; the team can
     prove it live in three lines.
  2. A duplicate check is meaningless until the key is canonical, otherwise `098765-43210` hides
     from `9876543210`.
  3. It shows the team thought about data quality, not just data storage.
- **Effort:** small.

### Idea 5 - Name and phone are treated differently on purpose (**built**)

- **What:** phone is unique and enforced. Name is not unique, and repeated names are reported
  instead of blocked.
- **DSA concept:** the difference between a key and a field.
- **Why it lands:** two real people can share a name, and a family can share a landline. A team
  that can say "we refused to make the name a primary key because that is a data-model mistake"
  is demonstrating judgement, which is exactly what "explain why each structure" is testing.
- **Effort:** small.

### Idea 6 - Handle the failure paths the brief names (**built**)

- **What:** empty list, invalid ID, duplicate entry, unreadable file, unwritable file, partial
  write, invalid input. Each has its own message, and no path exits silently.
- **Why it lands:** the brief lists these five cases by name. Gambling that the panel will not test
  them is a bad bet, and "it did not crash, it told me why" is easy to demonstrate.
- **Effort:** small, spread across the code.

---

## 5. Tier 2 - the differentiators

These are what separate this submission from the other groups.

### Idea 7 - Trie for autocomplete, as its own menu option (**built**)

- **What:** a prefix tree over lower-cased names; typing "Sa" suggests "Sanjay Gupta".
- **DSA concept:** trie, the structure most likely to be on the syllabus and the least likely to
  appear in a classmate's submission.
- **Why it lands:** it is a recognisable "real product" feature, and there is a free bonus to point
  out: because the children are visited in character order, the trie returns matches already in
  alphabetical order, with no extra sort.
- **The honest part, and this is what makes it land harder:** measured in this build the trie beats
  a linear scan by 5.6x-7.2x, but it is about 2x *slower* than the ordered map's `lower_bound` at
  a few thousand to a hundred thousand in-memory records. Independent research for this project
  measured the same effect elsewhere (a trie about 4x slower than `lower_bound` over a sorted
  vector for 20,000 prefix queries). So the trie is asymptotically right and constant-factor
  wrong at this scale. Volunteer that before the panel finds it: "the map is sufficient for the
  required features, the trie is here for autocomplete specifically, and here are both numbers."
  A team that reports its own counter-example is trusted on everything else it says.
- **Effort:** medium.

### Idea 8 - Typo-tolerant search with Damerau-Levenshtein (**built**)

- **What:** searching "Ananaya" still finds "Ananya".
- **DSA concept:** dynamic programming over strings, edit distance; the transposition case
  (Damerau rather than plain Levenshtein) is what makes "Pruhtvi" one edit instead of two.
- **Why it lands:** it is visibly impressive, it is honest about its cost (O(n * L^2) because it
  scores every record), and it sets up the best Tier 3 idea: a BK-tree removes that scan.
- **Effort:** small once the recurrence is right, and the recurrence is four lines.
- **Honesty note to keep ready:** this is the slow path on purpose. Do not claim it is fast.

### Idea 9 - Proven sorting, with counters, live (**built**)

- **What:** sort with `std::stable_sort`, print the number of comparisons performed, print
  `n * log2(n)` next to it, and run `std::sort` on the same data for comparison.
- **DSA concept:** comparison sorting, lower bounds, stability.
- **Why it lands:** most teams write bubble sort and say "it works". This one shows the lower
  bound and lands next to it. Then the stability demo is decisive: on 64 records with 8 repeated
  names, `std::sort` reordered 95 pairs of records that share a name, while `std::stable_sort`
  reordered 0 *and* a `std::sort` with the tie broken on ID also reordered 0. Showing both fixes,
  and saying which one a production system would prefer, is the difference between remembering a
  fact and understanding one.
- **Effort:** small.

### Idea 10 - CSV that survives real data (**built**)

- **What:** a proper escape routine and a character-level parser, so an address like
  `Flat 3B, "Sunrise" Apartments` followed by a newline round-trips exactly. Save writes a
  temporary file and renames it, so a failed write cannot destroy the previous file. Loading maps
  columns by header name, so a reordered or extended file still loads.
- **DSA concept:** parsing as a state machine, and failure atomicity.
- **Why it lands:** it is the part of the project that behaves like software rather than homework,
  and it gives an unbreakable edge-case demo: `diff` the file before and after.
- **Effort:** medium.

### Idea 11 - Undo with a bounded history (**built**)

- **What:** the last 20 changes are reversible, including a delete.
- **DSA concept:** stack, LIFO, bounded memory.
- **Why it lands:** delete becomes safe to demonstrate live. The panel can ask for a random
  deletion and the team can reverse it in front of them. The undo also refuses honestly when it
  cannot apply (the deleted contact's phone was given to someone else), which is a nice
  demonstration of thinking about invariants.
- **Effort:** small.

### Idea 12 - Measured evidence instead of asserted complexity (**built**)

- **What:** `bench/bench.cpp` times hash lookup against a sorted array with `binary_search` and
  against a linear scan, the trie against a linear scan, swap-delete against `vector::erase`, and
  `std::sort` against `std::stable_sort`, on 1,000 / 10,000 / 100,000 records, and writes
  `docs/benchmarks.md`.
- **DSA concept:** empirical algorithm analysis; the difference between big-O and real time.
- **Why it lands:** this is the highest-value idea in the file. It converts every claim into a
  number, and the panel can watch it run on their own machine in about a minute. It also produces
  the honest subtlety that impresses examiners: on 1,000 records the constant factors still matter,
  and the asymptotically worse structure can win. Saying that out loud is worth more than any
  extra feature.
- **Effort:** medium, and it is mostly copy-paste once one measurement works.

### Idea 13 - Behaviour tests that drive the real binary (**built**)

- **What:** `tests/test_cms.py` runs the compiled program through stdin and checks its output, 64
  assertions, including the index repair after a swap-delete and a byte-stable CSV round trip.
- **DSA concept:** none directly; it is engineering discipline.
- **Why it lands:** "how do you know the delete does not corrupt an index?" is a question that
  ends most projects. "There is a test that deletes the middle record and then looks up every
  survivor through all three indexes" is an answer that ends the questioning.
- **Effort:** medium, and it pays for itself the first time a refactor breaks something.

### Idea 14a - An index audit with a fault-injection hook (**built**)

- **What:** menu option 14 walks every record and checks that all four indexes agree with it, in
  both directions, and names the broken mapping when they do not. `--audit` runs the same check
  headlessly. `--inject-fault 1..5` deliberately corrupts one index so the check can be shown to
  fail rather than merely shown to pass.
- **DSA concept:** invariants, index consistency, the cost of keeping derived structures in sync.
- **Why it lands:** "how do you know the delete did not corrupt an index?" is the question that
  ends most projects. This answers it, and it answers the follow-up "how do you know your check
  works?" with five measured cases. It also converts the O(1) delete from a claim into a verified
  property.
- **Effort:** small, and it is the best value-per-line addition in the whole project.

### Idea 14 - Report the structures, not just the data (**built**)

- **What:** the statistics screen prints the vector row count, both hash-map key counts, the
  name-index bucket count and the trie node count, next to the group histogram, the missing-field
  counts, the repeated-name list and a near-duplicate name scan.
- **DSA concept:** aggregation with a map; introspection of your own structures.
- **Why it lands:** it makes the invisible visible. The panel can see the four indexes holding
  the same records, which makes the "why four structures" conversation start itself.
- **Effort:** small.

---

## 6. Tier 3 - extra structures, if the team wants to be remembered

Pick one. Do not pick three; a half-finished extra structure is worse than none.

### Idea 15 - BK-tree for fuzzy search

- **What:** index names in a Burkhard-Keller tree so a fuzzy query walks only nearby branches
  instead of scoring every record.
- **DSA concept:** metric trees, triangle inequality.
- **Why it lands:** it directly answers the weak point in Idea 8 with a second structure, and it
  is the kind of thing a panel has not seen from an undergraduate group. The benchmark already
  exists to prove the improvement, which makes the claim safe.
- **Effort:** large. Do it only if the fuzzy search is already working.

### Idea 16 - Union-Find for merging duplicate clusters

- **What:** group contacts that are probably the same person (same name and an edit distance of 1
  on the phone, or a near-identical name) and let the user merge a cluster with one command.
- **DSA concept:** disjoint set union, path compression, union by rank.
- **Why it lands:** disjoint set union is a syllabus structure almost nobody applies to a CRUD
  app, and "clean up my contacts" is a genuinely useful feature, not a toy. The statistics screen
  already detects the candidate pairs; Union-Find is the natural next step.
- **Effort:** medium.

### Idea 17 - Group graph and BFS "how do I know this person?"

- **What:** treat groups as edges between contacts, then answer "what is the shortest chain from
  Rohan to Meera?" with BFS.
- **DSA concept:** graphs, adjacency lists, breadth-first search, shortest path in an unweighted
  graph.
- **Why it lands:** the brief's expected concepts do not include graphs, so adding one deliberately
  signals that the team went past the syllabus. It is also a genuinely interesting demo question.
- **Effort:** medium.

### Idea 18 - LRU cache for recent lookups

- **What:** keep the last N viewed contacts in a hash map plus a doubly linked list, and show the
  cache hit rate in the report.
- **DSA concept:** hash map combined with a linked list, the classic O(1) LRU design.
- **Why it lands:** it is a famous interview structure, it is defensible ("repeated phone lookups
  are the common case"), and it produces a measurable hit rate from the demo's own activity.
- **Effort:** medium.

### Idea 19 - Top-K with a heap

- **What:** a "recently updated" or "upcoming birthday" list maintained with a priority queue.
- **DSA concept:** binary heap, `std::priority_queue`, partial sorting in O(n log k) instead of
  O(n log n).
- **Why it lands:** it is small, it is visibly useful, and it gives a clean comparison:
  full sort O(n log n) against heap O(n log k).
- **Effort:** small.

### Idea 20 - Bloom filter to skip negative phone lookups

- **What:** a small bit array in front of the phone index that answers "definitely not present"
  without touching the hash map.
- **DSA concept:** probabilistic data structures, false positives, space versus time.
- **Why it lands:** it is memorable, and it comes with an honest caveat that is itself a mark
  earner: it has false positives, never false negatives, and it is only worth the extra bit array
  when misses dominate. The benchmark can be extended to measure that.
- **Effort:** medium.

### Idea 21 - In-memory benchmark screen inside the app

- **What:** a menu option that times O(1) phone lookup against a linear scan on the user's own
  loaded data and prints both numbers.
- **DSA concept:** empirical complexity on real data.
- **Why it lands:** it takes Idea 12 and makes it a live demo instead of a pre-generated file.
  The panel watches the app prove its own design claim on data they just typed past.
- **Effort:** small, if `bench/bench.cpp` is already written and the timing helper is factored out.

### Idea 22 - Batch mode

- **What:** `./cms --query "phone:9876543210"` or a small script file of operations, so the app
  can be driven without a human at the menu.
- **DSA concept:** none directly; engineering.
- **Why it lands:** it is how the test suite drives the program, and it makes the tests and the
  demo share one code path.
- **Effort:** small.

### Idea 23 - Complexities printed from the running program

- **What:** a menu option that prints the operation complexity table, generated from the code's
  own counts rather than typed into a slide.
- **Why it lands:** it means every member can open the app and answer the complexity question
  without a script.
- **Effort:** small.

---

## 7. Two signature combinations

If the team wants one clear story, pick one of these two and commit to it.

**Combination A - "the data-structure showcase"**
Ideas 1, 2, 7, 8, 9, 12. Four lookup paths, an O(1) delete, a trie, edit distance, proven
stability, and measured evidence. Broad, safe, and every claim already runs.

**Combination B - "the data-quality project"**
Ideas 4, 5, 10, 11, 14, plus Idea 16. Canonical phone keys, names that are deliberately not
unique, a CSV that survives real punctuation, undo, a report that finds duplicate clusters, and
Union-Find to merge them. Narrower, and it frames the project as solving a real problem rather
than storing records. Choose this if the panel rewards problem framing more than structure count.

**Do not** attempt Combination A plus Tier 3. The demo is a few minutes long, and a shallow pass
over eleven structures reads worse than a deep pass over six.

---

## 8. Dividing the work across the group

The brief says "your team". The defence is individual, so the work should be split so each member
owns one thing they can defend alone.

| Member | Owns | Must be able to answer alone |
|---|---|---|
| A | record, validation, phone canonicalisation, CSV | why strings for phones, why the normalisation order, what a state-machine parser does |
| B | indexes, delete strategy, undo | why four structures, what the O(1) delete does, what a stale index looks like |
| C | search modes, trie, sorting, benchmarks | search costs, why a hash cannot do prefixes, what stability means, how the benchmark was run |

Rotate for the presentation so that the person who built a piece explains it, and prepare each
member to answer at least one question from another member's area, because the panel does not ask
in order.

---

## 9. The presentation plan

1. **30 seconds, the problem.** Ten contacts in a notebook, and the three questions people
   actually ask: "what is Ravi's number", "who is in my work group", "which Ravi".
2. **30 seconds, the shape.** One record, three indexes, one trie. Draw four boxes. Do not walk
   through the code.
3. **Four minutes, live.** The scripted steps in `demo/steps/intent.txt`. Feature, then the edge
   case that proves the feature is real. End with the index audit (option 14), which is the
   sentence that closes the questioning.
4. **Two minutes, the numbers.** Open `docs/benchmarks.md` or run `./bench_bin`. Read out the
   lookup table, the trie-versus-map row, and the stability stress test. Say the honest parts: the
   hash map won at every size but the sorted vector stayed within 1.6x-2.5x, and the trie is
   asymptotically right but about 2x slower than the ordered map at this scale. Those sentences
   are the difference between reciting and understanding.
5. **One minute, the limits.** Say what the program does not do: in-memory only, single-threaded,
   ASCII column widths, a pairwise duplicate scan that is skipped above 200 records, and a fuzzy
   search that is the slow path by design. Volunteering limits is what makes the rest of the
   claims credible.

Keep a fallback: the transcript is saved at `demo/session.log`, so if the live run fails, the
demonstration continues from the saved output.

---

## 10. What not to do

| Avoid | Why |
|---|---|
| Parallel arrays | they desynchronise silently; the brief's own expected concept is structures |
| A mutable global for the menu state or the count | documented style violation, and it makes the code untestable |
| `cin >>` for names | truncates at the first space and leaves the newline behind |
| A phone number in an `int` | silent overflow for every mobile number |
| Bubble sort as the only sort | O(n^2) with no justification loses to `std::sort` with a stated bound |
| `system("cls")` or `system("pause")` | non-portable, and it spawns a shell process |
| Commenting every line | reads as noise; comment the reason, not the syntax |
| Adding a GUI or a web page *instead of* finishing the console app | the brief asks for a menu-driven console program; polish the required artifact first |
| Claiming the fuzzy search is fast | it is O(n * L^2) and the honest answer is stronger |
| Storing the password or the whole file in memory without saying so | say the limits first, then the panel stops hunting for them |

---

## 11. Where everything lives

| Path | Contents |
|---|---|
| `src/contact_manager.cpp` | the whole application, one submittable file |
| `bench/bench.cpp` | the measurements behind every complexity claim |
| `tests/test_cms.py` | 64 behaviour tests driving the compiled binary |
| `docs/benchmarks.md` | measured numbers, regenerated by `make measure` |
| `docs/viva_qa.md` | viva questions with prepared answers and the sources used |
| `docs/assignment.md` | the brief, transcribed, mapped to the implementation |
| `demo/steps/intent.txt` | the twenty demonstration steps in order |
| `demo/session.log` | the saved transcript of a full run |
| `README.md` | build, run, structure rationale, edge cases |

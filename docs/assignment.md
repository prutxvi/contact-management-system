# The assignment brief, as given

Transcribed from the screenshot of the brief (`C++ project context.jpeg`) by optical character
recognition. Numbers below are reproduced exactly as they appear in the image, including the
numbering that starts at 77.

---

**Group 11 - Contact Management System**

Create a phone/contact management application for storing and managing personal contacts.

## Problem Statement

Your team is required to design and implement a C++ application for contact management system.
The application should be menu-driven and should allow the user to perform the required operations
through clear options. The implementation must demonstrate appropriate use of the DSA concepts
taught in class.

## Mandatory Features

77. Add contact
78. Delete contact
79. Search by name
80. Search by phone number
81. Update contact details
82. Display all contacts
83. Sort contacts alphabetically

## DSA Concepts Expected

- Structures
- Strings
- Arrays / STL containers
- Searching
- Sorting

## Implementation Expectations

Use separate functions for major operations. Handle cases such as empty records, invalid IDs,
duplicate entries, unavailable resources, and invalid input wherever applicable. Students should
be prepared to explain why each selected data structure was used.

## Suggested Menu

- 1. Add / Create record
- 2. Display records
- 3. Search
- 4. Update / Process
- 5. Delete / Cancel where applicable
- 6. Sort / Report where applicable
- 7. Exit

## Demo Requirement

During the demonstration, the group should explain the problem, show the major features,
demonstrate at least two edge cases, and explain the data structures and algorithms used.

---

## How this build answers it

| Brief line | Answer |
|---|---|
| 77 Add contact | menu 1, `ContactStore::add` |
| 78 Delete contact | menu 6, `ContactStore::remove`, undo at menu 9 |
| 79 Search by name | menu 3, four modes: exact, prefix, substring, typo-tolerant |
| 80 Search by phone number | menu 4, O(1) exact hash hit, linear scan for a partial key |
| 81 Update contact details | menu 5, field-by-field, blank keeps the current value |
| 82 Display all contacts | menu 2, six sort keys, both directions |
| 83 Sort alphabetically | menu 2 and menu 7, `std::stable_sort` with printed comparison counts |
| Structures | `Contact`, `Action`, `CiLess`, `Trie::Node` |
| Strings | `std::string` throughout; phone stored as a string on purpose |
| Arrays / STL containers | `vector`, `unordered_map`, `map`, `deque` |
| Searching | hash lookup, ordered range scan, linear scan, edit distance, trie walk |
| Sorting | `std::stable_sort`, plus a measured `std::sort` comparison |
| Separate functions | one `menuXxx` handler per option, one method per operation |
| Empty records | every handler has its own message |
| Invalid IDs | `No contact with ID n.` |
| Duplicate entries | unique phone enforced on add and update |
| Unavailable resources | CSV open/write/rename failures reported, memory state preserved |
| Invalid input | strict integer reader, phone and email validators |
| Explain the structures | `README.md`, `docs/viva_qa.md`, `docs/benchmarks.md` |
| Demo, at least two edge cases | `demo/steps/intent.txt`, 20 steps, 16 edge cases listed |

# Group 11 - Contact Management System

## Build (one command, no dependencies)

```sh
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -o cms Group11_ContactManagementSystem.cpp
```

## Run

```sh
./cms --data contacts.csv --seed
```

Type a menu number and press Enter. Start with `2` to display the list, or `13` to load sample
contacts. Choose `0` to exit; the list is saved to the CSV file.

## Files

| File | What it is |
|---|---|
| `Group11_ContactManagementSystem.cpp` | the complete program, one file, standard library only |
| `Group11_Report.md` | the report: design, data-structure justification, algorithms, measured results |
| `Group11_sample_output.txt` | a saved full run, 20 demonstration steps |

## Verify these two things first

```sh
./cms --data /tmp/v.csv --seed > /dev/null
./cms --data /tmp/v.csv --audit          # must print AUDIT PASSED and exit 0
./cms --data /tmp/v.csv --inject-fault 3 --audit   # must print AUDIT FAILED and exit 1
```

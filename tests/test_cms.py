#!/usr/bin/env python3
"""Behaviour tests for the contact manager.

Run from the project root:  python3 tests/test_cms.py
Each test drives the real compiled binary through stdin and checks its stdout,
so the tests exercise exactly what the demo will show.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(ROOT, "cms")
FAILS = []
PASSES = []


def run(script, data_path, cwd=None):
    """Feed `script` (a list of lines) to the binary and return its full output."""
    p = subprocess.run([BIN, "--data", data_path], input="\n".join(str(x) for x in script) + "\n",
                       capture_output=True, text=True, cwd=cwd or ROOT)
    return p.stdout + p.stderr, p.returncode


def run_argv(argv, script=None, cwd=None):
    """Run the binary with explicit arguments, optionally feeding stdin."""
    p = subprocess.run([BIN] + argv, input=("\n".join(str(x) for x in script) + "\n") if script else "",
                       capture_output=True, text=True, cwd=cwd or ROOT)
    return p.stdout + p.stderr, p.returncode


def check(name, cond, detail=""):
    if cond:
        PASSES.append(name)
        print("PASS  " + name)
    else:
        FAILS.append((name, detail))
        print("FAIL  " + name + ("  -> " + detail if detail else ""))


def tmpdir():
    return tempfile.mkdtemp(prefix="cms_test_")


def test_empty_store_is_safe():
    d = tmpdir()
    out, rc = run([2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0],
                  os.path.join(d, "c.csv"))
    check("empty store: exit code 0", rc == 0, "rc=%d" % rc)
    check("empty store: display warns", "The list is empty" in out, out[-300:])
    check("empty store: search name refuses", "The list is empty; nothing to search" in out)
    check("empty store: search phone refuses", "The list is empty; nothing to search" in out)
    check("empty store: update refuses", "The list is empty; nothing to update" in out)
    check("empty store: delete refuses", "The list is empty; nothing to delete" in out)
    check("empty store: undo says nothing to undo", "Nothing to undo." in out)
    check("empty store: sort experiment handles zero rows", "Nothing to sort yet." in out)
    check("empty store: autocomplete refuses", "The list is empty." in out)
    check("empty store: exit is clean", "Goodbye." in out)
    shutil.rmtree(d)


def test_validation_rules():
    d = tmpdir()
    script = [
        1, "Duplicate Guy", "abc", "", "", "",          # bad phone
        1, "Duplicate Guy", "12345", "", "", "",        # too short
        1, "Duplicate Guy", "0123456789", "", "", "",   # leading 0 and only 10 digits -> invalid
        1, "99999", "9812345678", "", "", "",           # name all digits
        1, "Real Name", "9812345678", "bad@@mail", "", "",  # bad email
        1, "Real Name", "9812345678", "good@mail.com", "", "",  # accepted
        1, "Copy Cat", "098123-45678", "", "", "",       # duplicate in another notation
        1, "Phone With Dash", "+91 91234 56789", "", "", "",  # accepted after normalising
        0,
    ]
    out, rc = run(script, os.path.join(d, "c.csv"))
    check("validation: exit 0", rc == 0)
    check("validation: letters-only phone rejected",
          "Invalid phone 'abc': no digits found." in out)
    check("validation: short phone rejected", "Invalid phone '12345'" in out)
    check("validation: 0-leading 10-digit rejected", "Invalid phone '0123456789'" in out)
    check("validation: all-digit name rejected", "Name cannot be all digits." in out)
    check("validation: bad email rejected", "Invalid email 'bad@@mail'." in out)
    check("validation: duplicate rejected across notations",
          re.search(r"REJECTED: Duplicate: phone 98123 45678 already belongs to #\d+ \(Real Name\)", out) is not None)
    csv_text_pre = open(os.path.join(d, "c.csv")).read() if os.path.exists(os.path.join(d, "c.csv")) else ""
    check("validation: +91 and spaces normalised to 10 digits",
          "9123456789" in csv_text_pre, csv_text_pre)
    csv_text = open(os.path.join(d, "c.csv")).read() if os.path.exists(os.path.join(d, "c.csv")) else ""
    check("validation: only 2 valid rows persisted", csv_text.count("\n") == 3, csv_text)
    shutil.rmtree(d)


def test_whitespace_is_squeezed():
    d = tmpdir()
    out, _ = run([1, "   Pruthvi    Raj   Toganti  ", "98765 00000", "", "Work", "Hyderabad", 0],
                 os.path.join(d, "c.csv"))
    check("whitespace: name squeezed", "Pruthvi Raj Toganti" in out, out[-400:])
    shutil.rmtree(d)


def test_delete_keeps_indexes_consistent():
    """Delete a middle record, then verify every remaining record is still
    reachable by ID, by name and by phone. This is the swap-with-last repair."""
    d = tmpdir()
    csv = os.path.join(d, "c.csv")
    lines = ["id,name,phone,email,group,address,created"]
    people = [("Aarav Sharma", "9000000001"), ("Bhavna Rao", "9000000002"),
              ("Chirag Mehta", "9000000003"), ("Divya Joshi", "9000000004"),
              ("Esha Pillai", "9000000005"), ("Farhan Bhatt", "9000000006")]
    for i, (nm, ph) in enumerate(people, 1):
        lines.append("%d,%s,%s,,General,,2026-01-0%d" % (i, nm, ph, i))
    open(csv, "w").write("\n".join(lines) + "\n")

    script = [6, 3, "y", 0]   # delete id 3 (the last record moves into slot 3)
    out, _ = run(script, csv)

    # Now probe every surviving record through each index.
    probe = [4, "9000000006", 4, "9000000001", 0]
    out2, _ = run(probe, csv)
    check("delete: record that moved is found by phone",
          "Farhan Bhatt" in out2 and "90000 00006" in out2, out2[-500:])
    check("delete: untouched record still found",
          "Aarav Sharma" in out2, out2[-500:])
    check("delete: deleted record is gone",
          "Chirag Mehta" not in out2, out2[-500:])
    check("delete: no stale duplicate row", out2.count("90000 00006") == 1, out2[-500:])

    # Display count and the alphabet check
    out3, _ = run([2, 1, "a", 0], csv)
    check("delete: five records remain", "5 record(s)" in out3, out3[-600:])
    ids = re.findall(r"^(\d+)\s+\w", out3, re.M)
    check("delete: no phantom rows in display", len(ids) == 5, str(ids))
    shutil.rmtree(d)


def test_undo_paths():
    d = tmpdir()
    csv = os.path.join(d, "c.csv")
    # delete then undo
    out, _ = run([13, 6, 1, "y", 9, 4, "9876543210", 0], csv)
    check("undo: delete restored", "Restored #" in out, out[-500:])
    check("undo: restored record is searchable",
          "Aarav Sharma" in out.split("9. Undo last change")[-1], out[-500:])
    # add then undo
    shutil.rmtree(d)
    d = tmpdir()
    csv = os.path.join(d, "c.csv")
    out, _ = run([13, 1, "Temp Person", "9111111111", "", "", "", 9, 4, "9111111111", 0], csv)
    check("undo: add removed", "Undid add of" in out, out[-500:])
    check("undo: removed record no longer found", "(no matching contacts)" in out.split("Undid add")[-1])
    # undo with empty history
    shutil.rmtree(d)
    d = tmpdir()
    out, _ = run([9, 0], os.path.join(d, "c.csv"))
    check("undo: empty history message", "Nothing to undo." in out)
    shutil.rmtree(d)


def test_sorting_is_monotonic():
    d = tmpdir()
    csv = os.path.join(d, "c.csv")
    out, _ = run([13, 7, 0], csv)
    check("sort: comparison count reported for std::sort",
          re.search(r"std::sort\s+comparisons : (\d+)", out) is not None)
    m = re.search(r"std::sort\s+comparisons : (\d+)", out)
    check("sort: comparisons are plausible for 10 items", m and 15 <= int(m.group(1)) <= 40,
          m.group(1) if m else "no match")
    block = out.split("Sorted by name:")[-1].split("---")[0]
    names = [re.sub(r"^.*?#\d+\s+(.{22})\s.*$", r"\1", ln).strip().lower()
             for ln in block.strip().splitlines() if ln.strip().startswith("#")]
    check("sort: output is in ascending name order", names == sorted(names), str(names))
    shutil.rmtree(d)


def test_csv_round_trip_survives_punctuation():
    d = tmpdir()
    csv = os.path.join(d, "c.csv")
    nasty = 'Flat 3B, "Sunrise" Apartments, 2nd Cross\nBehind the temple, Pune'
    payload = ("id,name,phone,email,group,address,created\n"
               '1,Aarav Sharma,9000000001,aarav@example.com,Family,"' + nasty + '",2026-01-01\n'
               '2,"Nair, Priya",9000000002,,Work,"plain",2026-01-02\n')
    open(csv, "w").write(payload)
    out, _ = run([5, 1, "", "", "", "", "", 2, 1, "a", 11, 12, 0], csv)
    check("csv: quoted multi-line address parsed",
          "Behind the temple, Pune" in out, out[-800:])
    check("csv: comma inside a quoted name parsed",
          "Nair, Priya" in out, out[-800:])
    # Save and reload, then compare the file byte for byte.
    before = open(csv).read()
    out2, _ = run([11, 12, 0], csv)
    after = open(csv).read()
    check("csv: save/reload is byte stable", before == after,
          "BEFORE:\n" + before + "\nAFTER:\n" + after)
    check("csv: reload reports both records", "Loaded 2 contact(s)" in out2, out2[-400:])
    shutil.rmtree(d)


def test_csv_duplicate_and_bad_rows_are_reported():
    d = tmpdir()
    csv = os.path.join(d, "c.csv")
    open(csv, "w").write("id,name,phone,email,group,address,created\n"
                         "1,A One,9000000001,,General,,2026-01-01\n"
                         "2,B Two,9000000001,,General,,2026-01-01\n"
                         "3,C Three,not-a-phone,,General,,2026-01-01\n")
    out, _ = run([0], csv)
    check("csv: duplicate phone row skipped", "skipped 1 duplicate phone number(s)" in out, out[:600])
    check("csv: invalid row skipped", "skipped 1 invalid record(s)" in out, out[:600])
    check("csv: valid row kept", "Loaded 1 contact(s)" in out, out[:600])
    shutil.rmtree(d)


def test_missing_required_column_is_reported():
    d = tmpdir()
    csv = os.path.join(d, "c.csv")
    open(csv, "w").write("id,fullname,telephone\n1,A One,9000000001\n")
    out, rc = run([0], csv)
    check("csv: wrong header rejected with a clear message",
          "Header must contain at least 'name' and 'phone' columns" in out, out[:600])
    check("csv: bad header does not crash", rc == 0, "rc=%d" % rc)
    shutil.rmtree(d)


def test_unwritable_destination_fails_cleanly():
    d = tmpdir()
    bad = os.path.join(d, "no_such_dir", "c.csv")
    out, rc = run([13, 11, 0], bad)
    check("io: save into a missing directory reports an error",
          "Cannot open" in out, out[-600:])
    check("io: failure is not a crash", rc == 0, "rc=%d" % rc)
    check("io: in-memory data survives a failed save",
          "10" in out and "REJECTED" not in out, out[-600:])
    shutil.rmtree(d)


def test_update_protects_duplicate_phone_and_immutable_id():
    d = tmpdir()
    csv = os.path.join(d, "c.csv")
    # update id 1 phone to id 2's phone -> rejected; then a legal rename
    out, _ = run([13, 5, 1, "", "9812345678", "", "", "", 5, 1, "Aarav S Sharma", "", "", "", "", 0], csv)
    check("update: duplicate phone rejected",
          "REJECTED: Duplicate: phone 98123 45678" in out, out[-800:])
    check("update: legal rename accepted", "Updated contact #1 (Aarav S Sharma)" in out, out[-800:])
    shutil.rmtree(d)


def test_autocomplete_order_and_prefix_misses():
    d = tmpdir()
    csv = os.path.join(d, "c.csv")
    out, _ = run([13, 8, "An", 8, "Zzz", 0], csv)
    part = out.split("8. Autocomplete a name (trie)")[1]
    ids = re.findall(r"#(\d+)\)", part)
    check("trie: two Ananya matches", len(ids) == 2, str(ids))
    check("trie: negative prefix reported",
          "no path for this prefix" in out.split("Zzz")[-1] or "(trie has no path" in out, out[-400:])
    shutil.rmtree(d)


def test_large_batch_insert_and_lookup():
    d = tmpdir()
    csv = os.path.join(d, "c.csv")
    script = []
    for i in range(200):
        script += [1, "Person Number %d" % i, "9%09d" % i, "", "Bulk", ""]
    script += [10, 4, "9000000123", 11, 12, 2, 3, "a", 0]
    out, rc = run(script, csv)
    check("bulk: 200 inserts succeed", out.count("OK: Added contact") == 200,
          str(out.count("OK: Added contact")))
    check("bulk: statistics show 200 rows", "Contacts stored          : 200" in out)
    check("bulk: bulk lookup is an exact hash hit", "Exact hash hit, O(1)." in out)
    check("bulk: reload after save keeps 200 rows", "Loaded 200 contact(s)" in out, out[-400:])
    check("bulk: exit 0", rc == 0)
    shutil.rmtree(d)


def test_index_audit_passes_and_detects_every_fault():
    """The index audit must be able to fail. Five deliberate corruptions are
    injected through a testing hook and each one must be reported."""
    d = tmpdir()
    csv = os.path.join(d, "c.csv")
    out, _ = run([13, 11, 0], csv)
    check("audit: sample data saved for the audit", "Saved 10 contact(s)" in out, out[-400:])

    out, rc = run_argv(["--data", csv, "--audit"])
    check("audit: clean index set passes", "AUDIT PASSED" in out and rc == 0,
          out[-400:] + " rc=%d" % rc)

    expectations = {
        1: "phone index holds 9 entries",
        2: "id index holds 9 entries",
        3: "points at record #1 named 'Aarav Sharma'",
        4: "is not reachable in the trie",
        5: "points at a record that does not own it",
    }
    for fault, expect in expectations.items():
        out, rc = run_argv(["--data", csv, "--inject-fault", str(fault), "--audit"])
        check("audit: detects injected fault %d" % fault,
              "AUDIT FAILED" in out and expect in out and rc == 1,
              out[-500:] + " rc=%d" % rc)

    # The same self-check through the menu, after real mutations.
    out, rc = run([13, 6, 3, "y", 5, 1, "Aarav S Sharma", "", "", "", "", 6, 1, "y", 9, 14, 11, 12, 14, 0], csv)
    check("audit: passes through the menu after deletes, an update and an undo",
          out.count("every index agrees with the record array") == 2, out[-700:])
    check("audit: menu option exists", "14  Validate indexes" in out, out[:900])
    shutil.rmtree(d)


def main():
    if not os.path.exists(BIN):
        print("Build first: c++ -std=c++17 -O2 -o cms src/contact_manager.cpp")
        return 2
    tests = [v for k, v in sorted(globals().items()) if k.startswith("test_") and callable(v)]
    for t in tests:
        t()
    print("\n%d passed, %d failed" % (len(PASSES), len(FAILS)))
    for name, detail in FAILS:
        print("  FAILED: " + name)
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())

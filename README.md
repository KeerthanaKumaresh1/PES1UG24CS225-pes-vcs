# PES-VCS — A Version Control System from Scratch

**Name:** Keerthana Kumaresh  
**SRN:** PES1UG24CS225  
**Repository:** PES1UG24CS225-pes-vcs  
**Platform:** Ubuntu 22.04

\---

## Objective

Build a local version control system that tracks file changes, stores snapshots efficiently, and supports commit history. Every component maps directly to operating system and filesystem concepts.

\---

## Prerequisites

```bash
sudo apt update \&\& sudo apt install -y gcc build-essential libssl-dev
```

\---

## Building

```bash
make          # Build the pes binary
make all      # Build pes + test binaries
make clean    # Remove all build artifacts
```

\---

## Author Configuration

```bash
export PES\_AUTHOR="Keerthana Kumaresh <PES1UG24CS225>"
```

\---

## File Inventory

|File|Role|Task|
|-|-|-|
|`pes.h`|Core data structures and constants|Do not modify|
|`object.c`|Content-addressable object store|Implement `object\_write`, `object\_read`|
|`tree.h`|Tree object interface|Do not modify|
|`tree.c`|Tree serialization and construction|Implement `tree\_serialize`, `tree\_parse`, `tree\_from\_index`|
|`index.h`|Staging area interface|Do not modify|
|`index.c`|Staging area (text-based index file)|Implement `index\_load`, `index\_save`, `index\_add`|
|`commit.h`|Commit object interface|Do not modify|
|`commit.c`|Commit creation and history|Implement `commit\_create`, `head\_read`, `head\_update`|
|`pes.c`|CLI entry point and command dispatch|Implement `cmd\_commit`|
|`test\_objects.c`|Phase 1 test program|Do not modify|
|`test\_tree.c`|Phase 2 test program|Do not modify|
|`test\_sequence.sh`|End-to-end integration test|Do not modify|
|`Makefile`|Build system|Do not modify|

\---

## Phase 1: Object Storage Foundation

**Filesystem Concepts:** Content-addressable storage, directory sharding, atomic writes, hashing for integrity

**Files:** `object.c`

### What I Implemented

**`object\_write`** — Stores data in the object store by prepending a type header (`blob`, `tree`, or `commit`), computing the SHA-256 hash of the full object, and writing atomically using a temp-file-then-rename pattern. Objects are sharded into subdirectories by the first 2 hex characters of the hash to avoid huge flat directories.

**`object\_read`** — Retrieves an object, parses the header to extract type and size, recomputes the SHA-256 hash and compares it against the filename to verify integrity. Returns only the data portion after the null byte.

### Testing

```bash
make test\_objects
./test\_objects
```

### Screenshot 1A — `./test\_objects` passing

!\[Screenshot 1A](https://raw.githubusercontent.com/KeerthanaKumaresh1/PES1UG24CS225-pes-vcs/main/1.png)

### Screenshot 1B — Sharded object directory structure

!\[Screenshot 1B](https://raw.githubusercontent.com/KeerthanaKumaresh1/PES1UG24CS225-pes-vcs/main/2.png)

\---

## Phase 2: Tree Objects

**Filesystem Concepts:** Directory representation, recursive structures, file modes and permissions

**Files:** `tree.c`

### What I Implemented

**`tree\_parse`** — Parses raw binary tree data into a `Tree` struct. Each entry has the format `"<mode> <name>\\0<32-byte-raw-hash>"`. The mode is ASCII octal, name is a null-terminated string, and hash is 32 raw bytes.

**`tree\_serialize`** — Serializes a `Tree` struct back to binary format. Entries are sorted by name before serialization so that identical directory contents always produce the same hash (deterministic hashing).

**`tree\_from\_index`** — Recursively builds a tree hierarchy from the staging area. Handles nested paths like `src/main.c` by creating subtrees bottom-up, writing each tree object to the object store before its parent.

### Testing

```bash
make test\_tree
./test\_tree
```

### Screenshot 2A — `./test\_tree` passing

!\[Screenshot 2A](https://raw.githubusercontent.com/KeerthanaKumaresh1/PES1UG24CS225-pes-vcs/main/3.png)

### Screenshot 2B — Raw binary tree object (xxd)

!\[Screenshot 2B](https://raw.githubusercontent.com/KeerthanaKumaresh1/PES1UG24CS225-pes-vcs/main/4.png)

\---

## Phase 3: The Index (Staging Area)

**Filesystem Concepts:** File format design, atomic writes, change detection using metadata

**Files:** `index.c`

### What I Implemented

**`index\_load`** — Reads the text-based `.pes/index` file into an `Index` struct. A missing index file is not an error — it simply means the staging area is empty. Each line has the format `<mode> <hash-hex> <mtime> <size> <path>`.

**`index\_save`** — Writes the index atomically: sorts entries by path, writes to a temp file, calls `fsync()` to flush kernel buffers to disk, then renames the temp file over the real index file.

**`index\_add`** — Reads a file's contents, writes it as a blob to the object store, then updates or inserts the corresponding entry in the index with the blob hash, mode, mtime, and size.

### Testing

```bash
make pes
./pes init
echo "hello" > file1.txt
echo "world" > file2.txt
./pes add file1.txt file2.txt
./pes status
cat .pes/index
```

### Screenshot 3A — `pes init` → `pes add` → `pes status`

!\[Screenshot 3A](https://raw.githubusercontent.com/KeerthanaKumaresh1/PES1UG24CS225-pes-vcs/main/5.png)

### Screenshot 3B — `cat .pes/index` showing text-format index

!\[Screenshot 3B](https://raw.githubusercontent.com/KeerthanaKumaresh1/PES1UG24CS225-pes-vcs/main/6.png)

\---

## Phase 4: Commits and History

**Filesystem Concepts:** Linked structures on disk, reference files, atomic pointer updates

**Files:** `commit.c`, `pes.c`

### What I Implemented

**`head\_read`** — Reads the commit hash that HEAD points to. If HEAD contains a symbolic ref like `ref: refs/heads/main`, it follows it to the branch file and reads the hash from there. Supports both symbolic and detached HEAD states.

**`head\_update`** — Atomically updates the current branch pointer to a new commit hash using the temp-file-then-rename pattern, ensuring the pointer swing is atomic.

**`commit\_create`** — Builds a tree from the staged index using `tree\_from\_index()`, reads the current HEAD as the parent commit (absent for the first commit), assembles the commit object with author and timestamp, writes it to the object store, then updates HEAD.

**`cmd\_commit`** in `pes.c` — Parses `-m <message>` from CLI arguments, calls `commit\_create`, and prints the short hash and message on success.

### Testing

```bash
./pes init
echo "Hello" > hello.txt
./pes add hello.txt
./pes commit -m "Initial commit"

echo "World" >> hello.txt
./pes add hello.txt
./pes commit -m "Add world"

echo "Goodbye" > bye.txt
./pes add bye.txt
./pes commit -m "Add farewell"

./pes log
make test-integration
```

### Screenshot 4A — `pes log` showing three commits

!\[Screenshot 4A](https://raw.githubusercontent.com/KeerthanaKumaresh1/PES1UG24CS225-pes-vcs/main/7.png)

### Screenshot 4B — `find .pes -type f | sort` showing object growth

!\[Screenshot 4B](https://raw.githubusercontent.com/KeerthanaKumaresh1/PES1UG24CS225-pes-vcs/main/8.png)

### Screenshot 4C — `cat .pes/refs/heads/main` and `cat .pes/HEAD`

!\[Screenshot 4C](https://raw.githubusercontent.com/KeerthanaKumaresh1/PES1UG24CS225-pes-vcs/main/9.png)

### Screenshot — Full integration test

!\[Integration Test](https://raw.githubusercontent.com/KeerthanaKumaresh1/PES1UG24CS225-pes-vcs/main/10.png)

!\[Integration Test](https://raw.githubusercontent.com/KeerthanaKumaresh1/PES1UG24CS225-pes-vcs/main/11.png)

\---

## Phase 5: Branching and Checkout (Analysis)

### Q5.1 — How would you implement `pes checkout <branch>`?

To implement `pes checkout <branch>`, the following steps are needed:

**Files that must change in `.pes/`:**

* `HEAD` must be updated to contain `ref: refs/heads/<branch>` pointing to the new branch.
* If the branch doesn't exist yet, a new file must be created at `.pes/refs/heads/<branch>` containing the current commit hash.

**What must happen to the working directory:**

1. Read the target branch's commit hash from `.pes/refs/heads/<branch>`.
2. Read the commit object to get its tree hash.
3. Recursively traverse the target tree and compare each file's blob hash against the current working directory.
4. For every file that differs, overwrite the working directory file with the blob contents from the object store.
5. For files that exist in the current tree but not in the target tree, delete them.
6. For files that exist in the target tree but not the current tree, create them.
7. Update the index to match the new tree exactly.

**What makes this complex:**
The main complexity is safely handling the working directory update. If a file was modified by the user but not staged, and the same file differs between branches, overwriting it would silently destroy the user's work. The operation must detect these conflicts and refuse to proceed. Additionally, files that exist in one branch but not the other must be carefully created or deleted without affecting untracked files. The entire operation must also be atomic — a partial checkout that crashes halfway leaves the repo in a broken state.

\---

### Q5.2 — How would you detect a "dirty working directory" conflict?

Using only the index and the object store, conflict detection works as follows:

1. For each file tracked in the current index, compare the file's current `mtime` and `size` on disk against the values stored in the index entry. If they differ, the file has been modified since it was last staged — this is an unstaged change.
2. For each file that differs between the current branch's tree and the target branch's tree, check if that same file has an unstaged change (detected in step 1).
3. If a file both differs between branches AND has unstaged changes in the working directory, checkout must refuse with an error like: `error: your local changes to 'file.txt' would be overwritten by checkout`.
4. If a file differs between branches but has no unstaged changes, it is safe to overwrite — the working directory version matches the index, which matches the current commit.

This approach avoids rehashing every file by using `mtime` and `size` as a fast heuristic, only falling back to a full hash comparison when the metadata suggests a change.

\---

### Q5.3 — What happens in detached HEAD state? How to recover?

**What happens when you commit in detached HEAD state:**

In detached HEAD, `HEAD` contains a raw commit hash instead of a branch reference like `ref: refs/heads/main`. When `head\_update` writes the new commit, it writes the new hash directly into `HEAD` itself rather than into a branch file. This means the new commits are reachable only through `HEAD`. The moment you run `checkout` to switch to any branch, `HEAD` is overwritten with the branch reference, and the commits you made in detached state have no branch pointing to them — they become unreachable and invisible to `pes log`.

**How to recover those commits:**

1. Before switching away, note the commit hash shown after each `pes commit` (the short hash printed on success).
2. Create a new branch pointing to that hash: create the file `.pes/refs/heads/recovery` containing the full commit hash.
3. Switch HEAD to point to that branch: write `ref: refs/heads/recovery` into `.pes/HEAD`.
4. The commits are now reachable through the `recovery` branch and will appear in `pes log`.

If you already switched away and lost the hash, the commits still exist as objects in `.pes/objects/` but have no reference pointing to them. You would need to scan all objects, parse each one as a commit, and find ones not reachable from any branch — essentially a manual garbage collection traversal in reverse.

\---

## Phase 6: Garbage Collection (Analysis)

### Q6.1 — Algorithm to find and delete unreachable objects

**Algorithm (mark-and-sweep):**

1. **Mark phase — find all reachable objects:**

   * Start from every branch ref in `.pes/refs/heads/` and collect their commit hashes into a set called `reachable`.
   * For each commit hash in the set: read the commit object, add its tree hash to `reachable`, add its parent hash (if any) to a queue.
   * For each tree hash: read the tree object, add every blob hash and subtree hash it contains to `reachable`.
   * Repeat until the queue is empty (BFS or DFS traversal).
2. **Sweep phase — delete unreachable objects:**

   * Enumerate every file under `.pes/objects/XX/YYY...`.
   * Reconstruct each object's full hash from its directory and filename.
   * If the hash is not in `reachable`, delete the file.

**Data structure:** A hash set (e.g. a hash table or a sorted array of 32-byte hashes) for O(1) lookup during the sweep phase.

**Estimate for 100,000 commits and 50 branches:**

* Starting from 50 branch tips, the traversal visits all 100,000 commits.
* Each commit points to one tree. Assuming an average project of \~200 files across \~20 directories, each commit adds roughly 220 objects (1 root tree + \~20 subtrees + \~200 blobs), but most blobs are shared between commits.
* In the worst case (all files change every commit) you visit up to 100,000 × 221 ≈ 22 million objects.
* In a typical repo where most files are unchanged, shared blobs are visited once each — realistically closer to 500,000–2,000,000 unique objects total.

\---

### Q6.2 — Race condition between GC and a concurrent commit

**The race condition:**

1. A commit operation begins. It calls `tree\_from\_index()` which writes several blob objects and a tree object to `.pes/objects/`. These objects exist on disk but HEAD has not yet been updated — no branch points to them yet.
2. Concurrently, GC runs its mark phase. It traverses all branches and marks reachable objects. Since HEAD hasn't been updated yet, the new blobs and tree are not reachable from any branch — GC does not mark them.
3. GC runs its sweep phase and deletes the unmarked objects, including the blobs and tree that the commit just wrote.
4. The commit operation calls `head\_update()` to swing HEAD to the new commit. The commit object references the tree that GC just deleted — the repository is now corrupt.

**How Git's real GC avoids this:**

Git uses a grace period: objects newer than a configurable threshold (default 2 weeks) are never deleted by GC, even if they appear unreachable. This means a commit in progress always has time to finish and update the branch pointer before GC can touch its newly written objects. Git also writes a `FETCH\_HEAD` or lock file during operations to signal that a transaction is in progress. Additionally, Git's `gc --auto` is never run concurrently with write operations — it checks for lock files before starting.

\---

## Submission Checklist

|Phase|ID|Screenshot|
|-|-|-|
|1|1A|`./test\_objects` all tests passing|
|1|1B|`find .pes/objects -type f` sharded structure|
|2|2A|`./test\_tree` all tests passing|
|2|2B|`xxd` of raw tree object|
|3|3A|`pes init` → `pes add` → `pes status`|
|3|3B|`cat .pes/index` text-format index|
|4|4A|`pes log` with three commits|
|4|4B|`find .pes -type f \| sort` object growth|
|4|4C|`cat .pes/refs/heads/main` and `cat .pes/HEAD`|
|Final|—|`make test-integration` passing|

\---

## Further Reading

* **Git Internals** (Pro Git book): https://git-scm.com/book/en/v2/Git-Internals-Plumbing-and-Porcelain
* **Git from the inside out**: https://codewords.recurse.com/issues/two/git-from-the-inside-out
* **The Git Parable**: https://tom.preston-werner.com/2009/05/19/the-git-parable.html



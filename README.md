# DAA Project — Divide & Conquer

A C++17 project implementing closest pair of points and Karatsuba integer multiplication. It includes input generation, a console solver, result verification, and a Raylib graphical interface.

## Team Members

| Name | Roll No. |
|---|---|
| Abdul Majid | 24K-0895 |
| Muhammad Hammad | 24K-0544 |
| Abdullah | 24K-0859 |

## Requirements

- A C++17-compatible GCC/MinGW compiler with `g++` available in the terminal.
- Raylib for the GUI. The commands below assume it is installed at `C:/raylib/raylib/src`.

Run all commands from the project folder. Close the running GUI before rebuilding `gui.exe`.

## Project files

| File or folder | Purpose |
|---|---|
| `src/gen.cpp` | Generates the 20 input datasets and `seeds.txt`. |
| `src/solver.cpp` | Runs the algorithms from the console and exports benchmark results. |
| `src/gui.cpp` | Draws the interface and provides manual algorithm-step navigation. |
| `src/closest_pair.hpp` | Closest-pair algorithm, custom merge sort, and step recording. |
| `src/karatsuba.hpp` | Large-integer arithmetic, Karatsuba multiplication, and step recording. |
| `src/check.hpp` | Input readers and answer-verification functions. |
| `data/` | Input datasets and the seed manifest. |
| `results.csv` | Results exported by the console batch run. |
| `Makefile` | Optional compilation shortcuts. |

## Compile and run

### 1. Compile the generator and solver

```powershell
g++ -O2 -std=c++17 -o gen.exe src/gen.cpp
g++ -O2 -std=c++17 -o solver.exe src/solver.cpp
```

### 2. Generate the input files

```powershell
.\gen.exe
```

The current generator always writes to `data/`. It creates the folder if necessary and writes `cp01.txt` through `cp10.txt`, `im01.txt` through `im10.txt`, and `seeds.txt`. Existing files with these names are overwritten.

The previously used command `.\gen.exe data` also works, but the current code ignores command-line arguments; it does not support selecting another output folder.

### 3. Run all datasets and export results

```powershell
.\solver.exe all data
```

This runs the available datasets, checks their answers, and creates or overwrites `results.csv` in the current working folder. Missing datasets are reported and skipped.

CSV columns:

```text
file,input size,time_ms,verified,answer
```

The GUI does not generate this CSV.

### 4. Compile and open the GUI

```powershell
g++ -O2 -std=c++17 src/gui.cpp -o gui.exe -I C:/raylib/raylib/src -L C:/raylib/raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm

.\gui.exe
```

Change the Raylib paths if it is installed elsewhere. Recompile after editing the source; opening an existing executable does not compile the changes.

To open inputs from a different folder:

```powershell
.\gui.exe other_folder
```

## Run individual inputs

```powershell
.\solver.exe cp data/cp04.txt
.\solver.exe im data/im04.txt
```

An individual multiplication run also writes the full product to a file such as `data/im04.txt.product.txt`.

Use the small demonstration inputs to print recorded steps:

```powershell
.\solver.exe cp data/cp10.txt --steps
.\solver.exe im data/im10.txt --steps
```

## Input formats

All inputs are plain text. Input sizes are read at runtime.

### Closest pair

The first line contains the number of points, followed by one space-separated `x y` pair per line:

```text
3
1.5 2.0
4.0 6.5
1.6 2.1
```

### Integer multiplication

Exactly two lines, each containing one non-negative integer in decimal digits. Do not add a blank line between them, labels, commas, or spaces within the numbers.

```text
123456789
987654321
```

## Generated datasets

### Closest pair of points

| File | Points | Content |
|---|---:|---|
| cp01 | 1,000 | Uniform random coordinates in [0, 1,000,000]. |
| cp02 | 10,000 | Uniform random coordinates in [0, 1,000,000]. |
| cp03 | 100,000 | Uniform random coordinates in [0, 1,000,000]. |
| cp04 | 1,000,000 | Uniform random coordinates in [0, 1,000,000]. |
| cp05 | 1,000,000 | Clusters around 10 random centers. |
| cp06 | 1,000,000 | Small-range integer coordinates with duplicate points. |
| cp07 | 100,000 | Points on one vertical line. |
| cp08 | 100,000 | Equally spaced grid with tied closest pairs. |
| cp09 | 1,000,000 | A very close pair placed across the middle dividing line. |
| cp10 | 101 | Small step-by-step demonstration. |

### Integer multiplication

| File | Digits in the two numbers | Content |
|---|---|---|
| im01 | 1,000 and 1,000 | Random digits without leading zeros. |
| im02 | 10,000 and 10,000 | Random digits without leading zeros. |
| im03 | 100,000 and 100,000 | Random digits without leading zeros. |
| im04 | 1,000,000 and 1,000,000 | Random digits without leading zeros. |
| im05 | 1,000,000 and 10 | Very different lengths. |
| im06 | 100,000 and 100,000 | One number consists entirely of 9s. |
| im07 | 99,999 and 99,999 | Odd lengths with many zeros. |
| im08 | 1,000,000 and 1,000,000 | Both numbers are powers of 10. |
| im09 | 1 and 1,000,000 | One number is zero. |
| im10 | 101 and 101 | Small step-by-step demonstration. |

The generator also prints file sizes and indicates which files are smaller than 1,000,000 bytes.

## Seeds

The generator uses fixed seeds with `mt19937_64`: `cpNN` uses `1000 + NN`, and `imNN` uses `2000 + NN`. Fixed seeds allow repeatable generation with the same program and environment. Some special datasets are constructed directly rather than randomly.

The generated `data/seeds.txt` contains only file identifiers, seeds, and sizes, without descriptions:

```text
cp01  seed=1001  n=1000
im01  seed=2001  digits=1000 and 1000
```

## GUI usage

Select a file from the left sidebar or drag a correctly formatted `.txt` file onto the window. The interface detects its type from its contents.

- The window title is **DAA Project**, and the heading is **Divide & Conquer**.
- The file list scrolls; the **Exit** button sits below the drag-and-drop hint.
- Small inputs use step mode: up to 300 points or up to 256 digits per number.
- Large inputs show the result, algorithm runtime, and verification status.
- Closest-pair results include a sampled scatter plot and a zoomed view of the closest pair.
- Multiplication results show a product preview. **Save full product** writes the full answer beside the input file.
- A blue animated loading bar remains responsive while computation runs in the background. It indicates activity, not a measured completion percentage.
- An unrecognized-file message appears for approximately 3 seconds.

### Step controls

| Control | Action |
|---|---|
| Reset | Return to the first step. |
| Prev | Move backward by the selected number of steps. |
| Next | Move forward by the selected number of steps. |
| End | Jump to the last step. |
| Speed − / + | Select 1, 2, 4, or 8 steps per click. |
| Click a step-log row | Jump directly to that step. |
| Scroll over the step log | View additional recorded steps. |

There is no automatic playback. The progress bar displays the current position. Prev and Next stop at the first or last step.

## Algorithms

### Closest pair of points

Points are initially sorted by x using a custom bottom-up merge sort. The algorithm divides the points into two halves, solves each half, merges by y, and checks nearby points in the dividing strip. Subproblems with at most three points use brute force.

```text
T(n) = 2T(n/2) + O(n)
Time complexity: O(n log n)
```

The closest-pair algorithm does not use built-in sorting. Built-in sorting is used separately for verification and the GUI file list.

### Karatsuba multiplication

Numbers are stored as base-10^9 limbs, with nine decimal digits per limb. No external big-integer library is used.

```text
x = x1 * B^m + x0
y = y1 * B^m + y0

z0 = x0 * y0
z2 = x1 * y1
z1 = (x0 + x1)(y0 + y1) - z0 - z2

x * y = z2 * B^(2m) + z1 * B^m + z0
```

Ordinary schoolbook multiplication is used when the shorter operand has at most 40 limbs. Step demonstrations use base 10 and a four-digit cutoff to make the splits readable.

For similarly sized operands:

```text
T(n) = 3T(n/2) + O(n)
Time complexity: O(n^log2(3)), approximately O(n^1.585)
```

## Verification and timing

Closest-pair distances are compared with a separate sort-and-sweep checker.

Multiplication results are checked modulo three primes. When both inputs have at most 20,000 digits, the entire result is also compared with schoolbook multiplication. Modular checks can detect many errors but are not a proof of equality for arbitrary large integers.

**MATCH** means the implemented checks passed; **MISMATCH** means a discrepancy was found.

Displayed algorithm timings exclude file reading and answer verification. Loading can therefore take longer than the reported algorithm time, and timings vary between machines and runs.

## Optional Makefile

The Makefile is optional if you use the full compilation commands above.

- `make` builds the generator and solver.
- `make gen` builds the generator.
- `make solver` builds the solver.
- `make gui` builds the GUI when Raylib's paths are configured.

These commands compile the programs; they do not generate datasets, run benchmarks, or open the GUI.

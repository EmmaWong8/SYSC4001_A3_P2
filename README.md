# SYSC4001 – Assignment 3 Part 2  

Author: Eshal Kashif (101297950) & Emma Wong (101297761)

TA Exam Marking Simulation (Shared Memory + Semaphores)

This repository contains the solution for Part 2 of Assignment 3.  
The assignment required building a concurrent “marking system” where several TA
processes mark student exams using shared memory. Part 2 has two programs:

- **part2a** – shared memory only (no synchronization)
- **part2b** – shared memory + semaphores

Part 2b fixes the race conditions from Part 2a by coordinating access to shared
data such as the rubric, question states, and current exam index.

## Files Included
- `part2a_101297950_101297761.c`  
- `part2b_101297950_101297761.c`  
- `rubric.txt`  
- `exams_list.txt`  
- `exams/` directory containing exam1.txt to exam20.txt  
- `reportPartC.pdf`  
- `designDiscussion.txt`

## How to Compile
These programs must be compiled on Linux or WSL because they use System V
shared memory and POSIX semaphores.

### Part 2a
```bash
gcc part2a_101297950_101297761.c -o part2a
```

### Part 2b
```bash
gcc part2b_101297950_101297761.c -o part2b
```

## How to Run
Both programs take one argument: the number of TA processes.

Example runs with 3 TAs:
```bash
./part2a 3
./part2b 3
```

Various test cases were created by changing the argument.

## Required Input Files
`exams_list.txt` lists all exam paths, one per line:
```bash
exams/exam1.txt
exams/exam2.txt
...
exams/exam20.txt
```
Each exam file contains just the student number (e.g., 0005). 
The very last file must contain the student number 9999

`rubric.txt` contains 5 lines corresponding to an answer per question number.
Both programs read and update this rubric as they run.

## What to Expect When Running

### Part 2a
TAs run simultaneously with no synchronization, so:
- more than one TA may mark the same question,
- multiple TAs may load the next exam at the same time,
- rubric updates can overwrite each other.

This behaviour is intentional for Part 2a.

### Part 2b
Every critical section is protected with semaphores.
You should see:
- each question marked exactly once,
- only one TA loading each new exam,
- rubric updates happening one at a time.

The output still looks interleaved because the TAs run concurrently.
Both programs finish by loading the final exam (9999), marking it, and
exiting cleanly.

# Design Discussion – Critical Section Requirements

This part of the assignment asked us to redesign the Part 2a program so that
shared memory accesses were done safely. The goal was to show how the solution
fits the three classic requirements of the critical-section problem:
**Mutual Exclusion**, **Progress**, and **Bounded Waiting**.

Below is a summary of how the design in Part 2b meets these requirements.

## 1. Mutual Exclusion

Several pieces of shared data must never be changed by more than one TA at a
time, such as the question states, the current exam index, and the rubric
entries.

To ensure mutual exclusion, we used two binary semaphores:

- **`mutex`**: protects the shared exam state  
  This semaphore is used whenever a TA:
  - selects the next question to mark,
  - updates `question_state[]`,
  - loads the next exam,
  - updates `current_exam_index` or `finished`.

  Only one TA can enter this section at a time, which prevents two TAs from
  selecting the same question or accidentally loading the same exam twice.

- **`rubric_mutex`**: protects rubric modifications  
  Since each TA may decide to “correct” a rubric entry, this semaphore ensures
  that rubric updates and writes to `rubric.txt` are done one at a time.  
  This keeps the rubric consistent and prevents overlapping file writes.


## 2. Progress

The program must ensure that if a TA wants to access the critical section,
and no one else is inside it, the TA should not be delayed forever.

In this design:

- Critical sections are kept short.
- TAs never hold a semaphore during the random delays (0.5–2 seconds).
- As soon as a TA finishes updating shared memory, it immediately releases the
  semaphore.

Because of this, the program keeps moving forward. After all five questions
are marked, one TA loads the next exam, and eventually the final exam
(`9999`) is reached. All TAs exit once they detect the final exam, showing that
the program does not stall.

## 3. Bounded Waiting

Every TA that wants to access shared memory should eventually get a turn.  
The design satisfies this because:

- Each semaphore is binary, so no TA can “hog” it.
- There are no nested semaphore acquisitions, so circular waiting cannot form.
- Critical sections are brief, which prevents any TA from waiting too long.
- The TAs loop in a regular pattern (check rubric, pick question, mark, 
  check for next exam), so no process gets starved.

In practice, the output from the program confirms this. Every TA marks multiple
questions and participates throughout the run, and no TA is stuck waiting for
access.

## Summary

The redesigned Part 2b program meets all three requirements of the
critical-section problem:

- **Mutual Exclusion:** shared memory updates are protected with semaphores.
- **Progress:** TAs always move forward, and the system reaches the sentinel.
- **Bounded Waiting:** no TA can be postponed indefinitely.

The result is a synchronized version of the marking system where each question
is only marked once and the exam order is preserved, fixing the race conditions
that appeared in Part 2a.

# Comparing RCU to other lock-based concurrency algorithms 

## Group Info: Gustavo Silvera

### Project Github: https://github.com/cmu15740-gsilvera/final-project 

### Project Description: 
“Read, Copy, Update” (RCU) is a lock-free synchronization technique that is optimized for read-mostly situations. This is a technique that allows reads to be executed concurrently with updates without locks nor atomics which are costly for performance. RCU ensures that reads are coherent by copying and maintaining multiple versions of objects while there is potential read-write conflict and restarting sequences (see Restartable Sequences) when a critical section is interrupted via a thread migration or signal or thread preemption. This requires OS-assisted system calls to know exactly when a critical section is being worked on. Since readers do not need any synchronization their access is extremely cheap (significantly more so than locks or even atomics) and writers will be guaranteed that their execution will not be interrupted mid-way, it will always entirely succeed or fail (and restart). This should provide an optimized lock-free way to access large data structures. My project will be interested in investigating the performance of this technique compared to traditional concurrency control algorithms such as locks and atomics on various machines (x86 and arm64) with various OSs (MacOS, Linux) and on various benchmarks (parallel bumps, many-readers-few-writers, many-writers-few-readers, etc.). 

| % Goal | Estimated project accomplishments | 
| --- | --- |
| 75% goal | Use RCU on Linux in a simple user-space program and compare with other concurrency algorithms | 
| 100% goal | Use RCU on Linux and MacOS in several simple user-space programs (with different reader-writer patterns) and compare with other concurrency algorithms |
| 125% goal | Perform the 100% goal and compare against various architectures (x86 vs arm64) and see if there is a performance difference |



## Logistics:
Plan of Attack & Schedule:

| Week | Estimated progress & goals |
| --- | --- |
| 9/18-9/24 | Submit this proposal| 
| 9/24-10/1 | Receive feedback on proposal and begin coding |
| 10/2-10/8 | Begin implementing a simple program that uses RCU either on MacOS or Linux |
| 10/9-10/15 | Begin implementing benchmarks (RCU) |
| 10/16-10/23 | Continue implementing benchmarks (locks) |
| 10/23-10/30 | Continue implementing benchmarks (atomics) |
| 10/30-11/5 | Continue implementing benchmarks (other?) |
| 11/6-11/12 | Perform experiments on all configurations |
| 11/13-11/19 | Perform analysis on experiment results |
| 11/20-11/26 | Begin writeup for final report |
| 11/27-12/3 | Continue writeup for final report |
| 12/4-12/9 | Finalize and present writeup for final report |


## Milestone:
By the milestone date, I plan to compare and contrast the performance (and correctness) of RCU against other concurrency control algorithms (locks, semaphores, atomics) across various problem types such as global increments (bumps), large data structure access, many-readers-few-writers, many-writers-few-readers, and other configurations. Ideally I can get userspace RCU working on both Linux and MacOS so I can compare the OS-system-call performance as well (since it could be a factor in the RCU performance)

## Literature Search:
- Restartable sequences: https://www.efficios.com/blog/2019/02/08/linux-restartable-sequences/ 
- What is RCU? https://www.kernel.org/doc/html/latest/RCU/whatisRCU.html
- Also documentation for other concurrency algorithm implementations in C++
- https://martong.github.io/high-level-cpp-rcu_informatics_2017.pdf 

## Resources Needed:
- My laptops should be sufficient to perform these experiments. 
- Namely a modern arm64 (M1) and x86 (Intel) machine. 
- RCU on MacOS userspace: https://formulae.brew.sh/formula/userspace-rcu
- RCU on Linux userspace: https://github.com/compudj/librseq
- RCU on Userspace: https://liburcu.org/ 

## Getting Started:
Haven’t done anything for this yet. Will need to begin researching if I can actually install a userspace RCU library on MacOS since traditionally this has been a Linux-kernel-specific technology.


## Installing `userspace-rcu`
Using git submodule of [`userspace-rcu`](userspace-rcu) you can do the following:

```bash
cd userspace-rcu/
./bootstrap
./configure
make -j10
sudo make install
```

Note that userspace-rcu is installed (On MacOS) if you can see the following files in `/usr/local/include/`
```
urcu urcu-bp.h urcu-call-rcu.h urcu-defer.h urcu-flavor.h urcu-pointer.h urcu-qsbr.h urcu.h
```
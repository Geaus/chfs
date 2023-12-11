# Lab 4: Map Reduce on Distributed Filesystem

## Introduction

In this lab, you are asked to build a MapReduce framework on top of your Distributed Filesystem implemented in previous labs.

You will implement a worker process that calls Map and Reduce functions and handles reading and writing files, and a coordinator process that hands out tasks to workers and copes with failed workers.

You can refer to [the MapReduce paper](https://www.usenix.org/legacy/events/osdi04/tech/full_papers/dean/dean.pdf) for more details (Note that this lab uses "coordinator" instead of the paper's "master").

There are six files added for this part:

- `src/include/map_reduce/protocol.h`: defines the basic data structures and interfaces needed in this lab
- `src/map_reduce/basic_mr.cc`: implement of basic  `Map` function and `Reduce` function
- `src/map_reduce/mr_sequential.cc`: implement of sequential map reduce in part1
- `src/map_reduce/mr_coordinator.cc`: implement of coordinator in part2
- `src/map_reduce/mr_worker.cc`: implement of worker in part2
- `test/map_reduce/mr_test.cc`: the test file we use for scoring.

You are free to modify the first five files and prohibited from modifying the last file.

Note: you are required to use the **distributed file system** that you implemented in lab2 to read and store any file in your map reduce implement, and prohibited from using your local file system. The files required for testing have been pre-stored in the distributed file system. For example, if you want to get the fd of a pre-stored file named `being_ernest.txt`, you can use the following code:

```cpp
auto res_lookup = chfs_client->lookup(1, "being_ernest.txt");
auto inode_id = res_lookup.unwrap();
```

See `test/map_reduce/mr_test.cc` for detail of the test environment.

## Get the Source Code

- Pull the source code of lab2 from our GitLab:

```sh
cd chfs && git pull
```

- Change it to the lab4 branch

```sh
git checkout -b lab4
```

- Merge with the code of lab3 and resolve any conflicts on your own if there are any.

```sh
git merge lab3
```

- Update submodules if needed:

```sh
git submodule update --init --recursive
```

## Part 1: Sequential MapReduce (40')

In this part, you are required to implement a sequential map reduce, running Map and Reduce once at a time within a single process.

Your task is to implement the Mapper and Reducer for Word Count in `src/map_reduce/basic_mr.cc`, and the backbone logic in `src/map_reduce/mr_sequential.cc`.

After correctly implementing Part1, you will pass `MapReduceTest.SequentialMapReduce` in `test/map_reduce/mr_test.cc`. Use the following commands to run the tests in this lab:

```sh
cd build
cmake ..
make
make run_mr_test
```

## Part2: Distributed MapReduce (60')

Your task in part2 is to implement a distributed MapReduce, consisting of two programs, `mr_coordinator.cc` and `mr_worker.cc`. There will be only one coordinator process, but one or more worker processes executing concurrently. The workers should talk to the coordinator via RPC. Each worker process will ask the coordinator for a task, read the task's input from one or more files, execute the task, and write the task's output to one or more files. The coordinator should notice if a worker hasn't completed its task in a reasonable amount of time, and give the same task to a different worker.

You are free to modify the return value type of the `Coordinator::askTask(int)` function based on your own protocol.

After correctly implementing Part2, you will pass `MapReduceTest.DistributedMapReduce` in `test/map_reduce/mr_test.cc`.

This lab has a time limit. Please ensure that the execution time of distributed map reduce is less than three times the execution time of sequential map reduce, otherwise a certain score will be deducted.

#### Hints

- The basic loop of one worker is the following: ask one task (Map or Reduce) from the coordinator, do the task and write the intermediate key-value into a file, then submit the task to the coordinator in order to hint a completion.
- The basic loop of the coordinator is the following: assign the Map tasks first; when all Map tasks are done, then assign the Reduce tasks; when all Reduce tasks are done, the `Done()` loop returns true indicating that all tasks are completely finished.
- Workers sometimes need to wait, e.g. reduces can't start until the last map has finished. One possibility is for workers to periodically ask the coordinator for work, sleeping between each request. Another possibility is for the relevant RPC handler in the coordinator to have a loop that waits.
- The coordinator, as an RPC server, should be concurrent; hence please don't forget to lock the shared data.
- A reasonable naming convention for intermediate files is mr-X-Y, where X is the Map task number, and Y is the reduce task number. The worker's map task code will need a way to store intermediate key/value pairs in files in a way that can be correctly read back during reduce tasks.

## Handin Procedure

After all above done, handin your code as before. You can use the scripts at `scripts/lab4/handin.sh`.

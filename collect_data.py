import numpy as np
import os
import sys
from typing import Tuple


def run_benchmark(num_readers: int, num_writers: int, mode: int) -> Tuple[float, float]:
    RD_OUTER_LOOP = 2000
    RD_INNER_LOOP = 10000
    WR_OUTER_LOOP = 10
    WR_INNER_LOOP = 200

    benchmark_cmd: str = f"./benchmark.out {num_readers} {num_writers} {mode} {RD_OUTER_LOOP} {RD_INNER_LOOP} {WR_OUTER_LOOP} {WR_INNER_LOOP} quiet"
    out = os.popen(benchmark_cmd).read()
    time_read = None
    time_write = None
    if num_readers == 0 and num_writers == 0:
        pass  # don't have any data... kinda useless
    elif num_readers == 0:  # only have data for writers
        time_write = float(out)
    elif num_writers == 0:  # only have data for readers
        time_read = float(out)
    else:  # have data for both read & write
        time_read, time_write, _ = out.split("\n")  # last is lingering \n
        time_read = float(time_read)
        time_write = float(time_write)
    return time_read, time_write


def main():
    sync_modes = {"RCU": 0, "LOCK": 1, "ATOMIC": 2, "NONE": 3}
    mode = sync_modes["RCU"]
    data = {}
    i = 0
    MAX_READERS = 10
    MAX_WRITERS = 10
    total = MAX_READERS * MAX_WRITERS
    for num_readers in range(0, MAX_READERS):
        for num_writers in range(0, MAX_WRITERS):
            value = run_benchmark(
                num_readers=num_readers, num_writers=num_writers, mode=mode
            )
            key = (num_readers, num_writers, mode)
            data[key] = value
            i += 1
            print(f"Done {i}/{total} ({100 * i / total}%)", end="\r", flush=True)
    print(data)


if __name__ == "__main__":
    main()

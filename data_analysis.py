import numpy as np
import os
import sys
from typing import Tuple
import matplotlib.pyplot as plt

sync_modes = {"RCU": 0, "LOCK": 1, "ATOMIC": 2, "RACE": 3}
results: str = "results"
os.makedirs(results, exist_ok=True)


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


def data_collection(datafile: str = "data.log"):
    data_all = {}
    MAX_READERS = 10
    MAX_WRITERS = 10
    total = MAX_READERS * MAX_WRITERS
    i = 0
    for mode in sync_modes.keys():
        data = {}
        data_all[mode] = data
        for num_readers in range(0, MAX_READERS):
            for num_writers in range(0, MAX_WRITERS):
                value = run_benchmark(
                    num_readers=num_readers, num_writers=num_writers, mode=mode
                )
                key = (num_readers, num_writers, mode)
                data[key] = value
                i += 1
                print(
                    f"({mode}) Done {i}/{total} ({100 * i / total}%)",
                    end="\r",
                    flush=True,
                )
        print(f"({mode}) Done {total}/{total} (100.0%)")
    # save to file
    with open(datafile, "w+") as f:
        f.write(str(data_all))
    print("Done!")


def _num_elems(data: dict, idx: int) -> int:
    return len(set([keys[idx] for keys in data.keys()]))


def _get_time_data(mode: str, data: dict, anchor: int, idx: int):
    mode_int: int = sync_modes[mode]
    max_elems = _num_elems(data, idx)  # num unique keys at idx
    out = []
    for iterator in range(max_elems):
        if idx == 0:
            key: tuple = (iterator, anchor, mode_int)
        elif idx == 1:
            key: tuple = (anchor, iterator, mode_int)
        else:
            raise NotImplementedError
        out.append(data[key][idx])
    return out


def get_reader_data(mode: str, data: dict, writers: int):
    return _get_time_data(mode, data, writers, 0)


def get_writer_data(mode: str, data: dict, readers: int):
    return _get_time_data(mode, data, readers, 1)


def plot_for_mode(mode: str, data: dict) -> None:
    print(f"Plotting data for mode: {mode}")
    readers = range(_num_elems(data, 0))
    writers = range(_num_elems(data, 1))
    reader_data = [get_reader_data(mode, data, writers=w) for w in writers]
    writer_data = [get_writer_data(mode, data, readers=r) for r in readers]

    def plot_data(
        thread_type: str,
        second_thread_type: str,
        data,
        thread_range,
        second_thread_range,
    ) -> None:
        fig, ax = plt.subplots(1, 1)
        ax_plots = []  # for the legends
        for th in second_thread_range:
            (ax_plot,) = ax.plot(
                thread_range,
                data[th],
                linewidth=3,
                label=f"{second_thread_type} threads = {th}",
            )
            ax_plots.append(ax_plot)
        ax.legend(handles=ax_plots)
        ax.set_ylabel("CPU Cycles (nanoseconds)")
        ax.set_xlabel(f"Number of {thread_type} threads")
        plt.title(f"Cycles per {thread_type} in {mode} mode")
        sub_dir: str = "cmp_same"
        os.makedirs(os.path.join(results, sub_dir), exist_ok=True)
        filepath: str = os.path.join(results, sub_dir, f"{mode}_{thread_type}.jpg")
        print(f"saving figure to {filepath}")
        fig.savefig(filepath)
        plt.close()

    plot_data(
        "Read", "Write", reader_data, thread_range=readers, second_thread_range=[1, 8]
    )
    plot_data(
        "Write", "Read", writer_data, thread_range=writers, second_thread_range=[1, 8]
    )


def data_analysis(datafile: str):
    data_dict = {}
    with open(datafile, "r") as f:
        data_dict = eval(f.read())
    
    # plot individually per mode
    for mode in data_dict.keys():
        plot_for_mode(mode, data_dict[mode])
    
    # plot comparatively across modes
    # plot_cmp(num_readers=8, num_writers=2, data_dict)


if __name__ == "__main__":
    datafile = "data.log"
    # data_collection(datafile)
    data_analysis(datafile)

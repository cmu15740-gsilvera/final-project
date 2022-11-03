import numpy as np
import os
import sys
from typing import Tuple
import matplotlib.pyplot as plt

sync_modes = {"RCU": 0, "LOCK": 1, "ATOMIC": 2, "RACE": 3}
results: str = "results"
os.makedirs(results, exist_ok=True)


def run_benchmark(num_readers: int, num_writers: int, mode: int) -> Tuple[float, float]:
    RD_OUTER_LOOP = 2000 if mode != sync_modes["LOCK"] else 200
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


def data_collection(datafile: str):
    MAX_READERS = 10
    MAX_WRITERS = 10
    total = MAX_READERS * MAX_WRITERS
    data_all = np.zeros(shape=(len(sync_modes), MAX_READERS, MAX_WRITERS, 2))
    for mode in sync_modes.keys():
        i = 0
        for num_readers in range(0, MAX_READERS):
            for num_writers in range(0, MAX_WRITERS):
                value = run_benchmark(
                    num_readers=num_readers,
                    num_writers=num_writers,
                    mode=sync_modes[mode],
                )
                data_all[sync_modes[mode], num_readers, num_writers, :] = value
                i += 1
                print(
                    f"({mode}) Done {i}/{total} ({100 * i / total}%)",
                    end="\r",
                    flush=True,
                )
        print(f"({mode}) Done {total}/{total} (100.0%)")
    np.save(datafile, data_all)
    print("Done!")


def plot_for_mode(mode: str, data: np.ndarray) -> None:
    print(f"Plotting data for mode: {mode}")

    m, nR, nW, d = data.shape
    assert m == len(sync_modes)
    assert d == 2  # only tracking reads (0) and writes (1)

    def plot_data(
        RW_IDX: int,
        rd_th_range: np.ndarray,
        wr_th_range: np.ndarray,
    ) -> None:
        assert RW_IDX == 0 or RW_IDX == 1  # reads or writes (last dim)
        thread_ranges = (rd_th_range, wr_th_range)
        x_axis = thread_ranges[RW_IDX]  # X axis (number of threads in this dim)
        num_lines = thread_ranges[1 - RW_IDX]  # which threads are overlaid
        types = ("Read", "Write")
        primary_type = types[RW_IDX]
        secondary_type = types[1 - RW_IDX]  # the other one
        fig, ax = plt.subplots(1, 1, figsize=(10, 7))
        ax_plots = []  # for the legends
        for th in num_lines:
            if RW_IDX == 0:
                cycle_time = data[sync_modes[mode], rd_th_range, th, RW_IDX]
            else:
                cycle_time = data[sync_modes[mode], th, wr_th_range, RW_IDX]
            assert cycle_time.shape == x_axis.shape
            (ax_plot,) = ax.plot(
                x_axis[np.isfinite(cycle_time)],
                cycle_time[np.isfinite(cycle_time)],  # ignore plotting None's
                linewidth=3,
                label=f"{secondary_type} threads = {th}",
            )
            ax_plots.append(ax_plot)
        ax.legend(handles=ax_plots)
        ax.set_ylabel("CPU Cycles (nanoseconds)")
        ax.set_xlabel(f"Number of {primary_type} threads")
        plt.title(f"Cycles per {primary_type} in {mode} mode")
        sub_dir: str = "cmp_same"
        os.makedirs(os.path.join(results, sub_dir), exist_ok=True)
        filepath: str = os.path.join(results, sub_dir, f"{mode}_{primary_type}.jpg")
        print(f"saving figure to {filepath}")
        fig.savefig(filepath)
        plt.close()

    plot_data(
        RW_IDX=0,  # plotting on reader threads primarily
        rd_th_range=np.arange(nR),  # all readers
        wr_th_range=np.array([1, 3, 8]),
    )
    plot_data(
        RW_IDX=1,  # plotting on writer threads primarily
        rd_th_range=np.array([1, 3, 8]),
        wr_th_range=np.arange(nW),  # all writers
    )


def plot_cmp(
    data: np.ndarray,
    num_readers: int,
    num_writers: int,
    modes: list = None,
    y_scale=lambda x: x,
) -> None:
    if modes is None:
        modes = list(sync_modes.keys())  # all of them

    cmp_data = np.zeros(shape=(len(modes), 2))
    for m in modes:
        cmp_data[sync_modes[m], :] = data[sync_modes[m], num_readers, num_writers, :]

    def plot_data(op_type: str) -> None:
        fig, ax = plt.subplots(1, 1, figsize=(10, 7))
        idx = 0 if op_type == "Read" else 1
        raw_ideal = cmp_data[sync_modes["RCU"], idx]
        for m in modes:
            raw_ht: float = cmp_data[sync_modes[m], idx]
            height: float = y_scale(raw_ht)
            ax.bar(
                x=m,
                height=height,
                width=0.4,
                color="r",
            )
            speedup: float = raw_ht / raw_ideal
            ax.text(x=m, ha="center", y=height / 2, s=f"{speedup:.3f}x", color="black")
        ax.set_ylabel("(log10) CPU Cycles (nanoseconds)")
        ax.set_xlabel(f"Type of concurrency control/synchronization method")
        plt.title(
            f"{op_type} performance across modes for {num_readers} readers threads & {num_writers} writer threads"
        )
        sub_dir: str = "cmp_diff"
        os.makedirs(os.path.join(results, sub_dir), exist_ok=True)
        filepath: str = os.path.join(
            results, sub_dir, f"cmp_{op_type.lower()}_{num_readers}_{num_writers}.png"
        )
        print(f"saving figure to {filepath}")
        fig.savefig(filepath)
        plt.close()

    plot_data("Read")
    plot_data("Write")


def data_analysis(datafile: str):
    data = np.load(datafile)

    # plot individually per mode
    for mode in sync_modes.keys():
        plot_for_mode(mode, data)

    # plot comparatively across modes
    plot_cmp(data, num_readers=8, num_writers=2, y_scale=lambda x: np.log10(x))
    plot_cmp(data, num_readers=9, num_writers=1, y_scale=lambda x: np.log10(x))
    plot_cmp(data, num_readers=8, num_writers=9, y_scale=lambda x: np.log10(x))


if __name__ == "__main__":
    datafile = "data.npy"
    # data_collection(datafile)
    data_analysis(datafile)

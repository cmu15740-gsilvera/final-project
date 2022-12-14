import numpy as np
import os
import glob
from typing import Tuple
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from mpl_toolkits.mplot3d import Axes3D
import time

# all the available modes
RCU = "RCU"
RWLOCK = "RWLOCK"
LOCK = "LOCK"
ATOMIC = "ATOMIC"
RACE = "RACE"
sync_modes = [RCU, RWLOCK, LOCK, ATOMIC, RACE]
_sync_modes_idx = {key: i for i, key in enumerate(sync_modes)}

# other metadata
results: str = "results"
OUT = "out"
BIN_SUFFIX = "out"  # convention is to end in .out

# op types must match the executables made from the makefile
ATOMIC_STR = "atomic-str"
ATOMIC_VEC = "atomic-vec"
BUMP_COUNTER = "bump-counter"
STRUCT_ABC = "struct-abc"
ops = [BUMP_COUNTER, STRUCT_ABC, ATOMIC_STR, ATOMIC_VEC]


def is_slow(op: str):
    if op == BUMP_COUNTER:
        return False  # atomic implemented in hardware
    return True  # atomic implemented using just locks


def run_benchmark(
    num_readers: int, num_writers: int, mode: str, op: str
) -> Tuple[float, float]:
    slow: list = (
        [LOCK, RWLOCK] if not is_slow(op) else [ATOMIC, LOCK, RWLOCK]
    )  # atomic is slow
    RD_OUTER_LOOP = 1000 if mode not in slow else 100
    RD_INNER_LOOP = 2000 if mode not in slow else 200

    binary: str = os.path.join(OUT, f"{op}.{BIN_SUFFIX}")
    benchmark_cmd: str = f"{binary} {num_readers} {num_writers} {mode} {RD_OUTER_LOOP} {RD_INNER_LOOP} quiet"
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
        try:
            time_read, time_write, _ = out.split("\n")  # last is lingering \n
            time_read = float(time_read)
            time_write = float(time_write)
        except ValueError as v:
            print(
                f"Error in mode {mode} on op {op} with {num_readers} readera & {num_writers} writers: {v}"
            )
    return time_read, time_write


def data_collection(datafile: str, op: str):
    MAX_READERS = 30
    MAX_WRITERS = 30
    total = MAX_READERS * MAX_WRITERS
    data_all = np.zeros(shape=(len(sync_modes), MAX_READERS, MAX_WRITERS, 2))
    for mode in sync_modes:
        i = 0
        start_t: float = time.time()
        for num_readers in range(0, MAX_READERS):
            for num_writers in range(0, MAX_WRITERS):
                value = run_benchmark(
                    num_readers=num_readers,
                    num_writers=num_writers,
                    mode=mode,
                    op=op,
                )
                data_all[_sync_modes_idx[mode], num_readers, num_writers, :] = value
                i += 1
                print(
                    f"({op} {mode}) Done {i}/{total} ({100 * i / total:.2f}%) @ {time.time() - start_t:.2f}s",
                    end="\r",
                    flush=True,
                )
        print(
            f"({op} {mode}) Done {total}/{total} (100.0%) @ {time.time() - start_t:.2f}s"
        )
    np.save(datafile, data_all)
    print("Done!")


def plot_for_mode(mode: str, data: np.ndarray) -> None:
    # print(f"Plotting data for mode: {mode}")

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
        fig, ax = plt.subplots(1, 1)
        ax_plots = []  # for the legends
        for th in num_lines:
            if RW_IDX == 0:
                cycle_time = data[_sync_modes_idx[mode], rd_th_range, th, RW_IDX]
            else:
                cycle_time = data[_sync_modes_idx[mode], th, wr_th_range, RW_IDX]
            assert cycle_time.shape == x_axis.shape
            (ax_plot,) = ax.plot(
                x_axis[np.isfinite(cycle_time)],
                cycle_time[np.isfinite(cycle_time)],  # ignore plotting None's
                linewidth=3,
                label=f"{secondary_type} threads = {th}",
            )
            ax_plots.append(ax_plot)
        ax.legend(handles=ax_plots)
        ax.set_ylabel("CPU Cycles (ns)")
        ax.set_xlabel(f"Number of {primary_type} threads")
        plt.title(f"Cycles per {primary_type} in {mode} mode")
        plt.tight_layout()
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


def plot_perf_mountain(
    mode: str, data: np.ndarray, execution: str = "read", scale=lambda x: x
) -> None:
    m, nR, nW, d = data.shape
    assert m == len(sync_modes)
    assert d == 2  # only tracking reads (0) and writes (1)

    fig = plt.figure()
    ax = fig.add_subplot(111, projection="3d")
    x = np.arange(nR)
    y = np.arange(nW)
    last_dim = 0 if (execution.lower() == "read") else 1
    z = scale(data[_sync_modes_idx[mode], :, :, last_dim])  # skips plotting nans

    ax.invert_xaxis()
    ax.set_title(f"Performance mountain of {mode} {execution}s")
    ax.set_xlabel("Num Readers")
    ax.set_ylabel("Num Writers")
    ax.set_zlabel("cycles (ns)")
    plt.tight_layout()
    x1, y1 = np.meshgrid(x, y)
    ax.plot_trisurf(x1.flatten(), y1.flatten(), z.flatten(), cmap=cm.Blues_r)
    # plt.show()

    sub_dir: str = "3d_plot"
    os.makedirs(os.path.join(results, sub_dir), exist_ok=True)
    filepath: str = os.path.join(results, sub_dir, f"plot_{mode}_{execution}.jpg")
    print(f"saving figure to {filepath}")
    fig.savefig(filepath)
    plt.close()


def plot_cmp_mode(data: np.ndarray, y_scale=lambda x: np.log10(x)) -> None:
    m, nR, nW, d = data.shape
    assert m == len(sync_modes)
    assert d == 2  # only tracking reads (0) and writes (1)

    def plot_data(y_axis: str, x_axis: str, x_axis_range: np.ndarray, th: int) -> None:
        assert y_axis == "Read" or y_axis == "Write"  # reads or writes (last dim)
        assert x_axis == "Read" or x_axis == "Write"  # reads or writes (last dim)

        fig, ax = plt.subplots(1, 1)
        not_op = "Write" if x_axis == "Read" else "Read"
        ax_plots = []  # for the legends
        last_dim = 0 if (y_axis == "Read") else 1
        for mode in sync_modes:
            if x_axis == "Read":
                cycle_time = data[_sync_modes_idx[mode], x_axis_range, th, last_dim]
            else:
                cycle_time = data[_sync_modes_idx[mode], th, x_axis_range, last_dim]
            assert cycle_time.shape == x_axis_range.shape

            (ax_plot,) = ax.plot(
                x_axis_range[np.isfinite(cycle_time)],
                y_scale(cycle_time[np.isfinite(cycle_time)]),  # ignore plotting None's
                linewidth=3,
                label=f"{mode}",
            )
            ax_plots.append(ax_plot)
        ax.legend(handles=ax_plots)
        ax.set_ylabel("(log10) CPU Cycles (log(ns))")
        ax.set_xlabel(f"Number of {x_axis} threads")
        plt.title(f"(log10) Cycles per {y_axis} with {th} {not_op} threads")
        plt.tight_layout()
        sub_dir: str = "cmp_modes"
        os.makedirs(os.path.join(results, sub_dir), exist_ok=True)
        filepath: str = os.path.join(
            results, sub_dir, f"cmp_{y_axis}_{x_axis}_{th}.jpg"
        )
        print(f"saving figure to {filepath}")
        fig.savefig(filepath)
        plt.close()

    plot_data(
        y_axis="Read",  # plotting read performance
        x_axis="Read",  # all readers
        x_axis_range=np.arange(nR),
        th=2,  # number of not-xaxis threads
    )
    plot_data(
        y_axis="Write",  # plotting write performance
        x_axis="Read",  # all readers
        x_axis_range=np.arange(nR),
        th=2,  # number of not-xaxis threads
    )

    plot_data(
        y_axis="Write",  # plotting write performance
        x_axis="Write",  # all writers
        x_axis_range=np.arange(nW),
        th=2,  # number of not-xaxis threads
    )
    plot_data(
        y_axis="Read",  # plotting read performance
        x_axis="Write",  # all writers
        x_axis_range=np.arange(nW),
        th=2,  # number of not-xaxis threads
    )


def plot_cmp(
    data: np.ndarray,
    num_readers: int,
    num_writers: int,
    modes: list = None,
    y_scale=lambda x: np.log10(x),
) -> None:
    if modes is None:
        modes = list(sync_modes)  # all of them

    cmp_data = np.zeros(shape=(len(modes), 2))
    for m in modes:
        cmp_data[_sync_modes_idx[m], :] = data[
            _sync_modes_idx[m], num_readers, num_writers, :
        ]

    def plot_data(op_type: str) -> None:
        fig, ax = plt.subplots(1, 1)
        idx = 0 if op_type == "Read" else 1
        raw_ideal = cmp_data[_sync_modes_idx[RCU], idx]
        ax.set_ylim(
            None, max([y_scale(cmp_data[_sync_modes_idx[m], idx]) for m in modes]) + 1
        )
        for m in modes:
            raw_ht: float = cmp_data[_sync_modes_idx[m], idx]
            height: float = y_scale(raw_ht)
            ax.bar(
                x=m,
                height=height,
                width=0.4,
                color="r",
            )
            speedup: float = raw_ht / raw_ideal
            ax.text(
                x=m, ha="center", y=height + 0.1, s=f"{speedup:.3f}x", color="black"
            )
        ax.set_ylabel("(log10) CPU Cycles (log(ns))")
        ax.set_xlabel(f"Type of concurrency control/synchronization method")
        plt.title(
            f"{op_type} performance across modes for {num_readers} readers & {num_writers} writers"
        )
        plt.tight_layout()
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


def plot_big_cmp(
    idx=1,  # 0 for read, 1 for write
    readers=(19, 19),
    writers=(1, 29),
    y_scale=lambda x: np.log10(x),
) -> None:
    assert len(readers) == len(writers)

    data_all = {}

    for op in ops:
        np_files = glob.glob(os.path.join("results", op, "*.npy"))
        if len(np_files) != 1:
            raise Exception(
                f'Need exactly one np data (.npy) file for analysis in "{working_dir}"'
            )
        datafile: str = np_files[0]
        data_all[op] = np.load(datafile)

    fig, ax = plt.subplots(1, 1, figsize=(20, 6))
    colors = {
        sync_modes[0]: "r",
        sync_modes[1]: "g",
        sync_modes[2]: "b",
        sync_modes[3]: "y",
        sync_modes[4]: "cyan",
    }
    x = 0.6 * np.arange(len(ops))
    width = 1.0
    order_x = [RCU, RACE, ATOMIC, LOCK, RWLOCK]
    num_in_grp = len(readers) * len(order_x)  # for each bar in the group
    max_y = -np.inf
    patterns = ["", "o"]  # 2nd is textured
    for i, op in enumerate(ops):
        for j, m in enumerate(order_x):
            for k in range(len(readers)):
                raw_data: float = data_all[op][
                    _sync_modes_idx[m], readers[k], writers[k], idx
                ]
                height: float = y_scale(raw_data)
                xpos = (
                    x[i]
                    + (j + k * 1.0 / len(readers) - num_in_grp / 2 + 2.75)
                    * width
                    / num_in_grp
                )
                ax.bar(
                    x=xpos,
                    height=height,
                    width=0.5 * (width / num_in_grp),
                    color=colors[m],
                    label=m if i == 0 and k == 0 else None,  # only first one (for legend)
                    hatch=patterns[k],
                    edgecolor="black",
                )
                ax.text(
                    x=xpos,
                    ha="center",
                    y=height + 0.1,
                    s=f"{height:.2f}",
                    # color=colors[m],
                )
                if height > max_y:
                    max_y = height
    ax.legend()
    ax.set_xticks(x, ops, fontsize=11)
    ax.set_ylabel("log CPU Cycles (log ns)")
    ax.set_ylim(bottom=0, top=max_y + 0.3)
    ax.set_xlabel(f"")
    data_type: str = "Read" if idx == 0 else "Write"
    title: str = f"{data_type} latency (log) across operations and sync modes"
    subtitle: str = f"(Plain) Fixing #readers={readers[0]+1} and #writers={writers[0]+1}\n(Dotted) Fixing #readers={readers[1]+1} and #writers={writers[1]+1}\n"
    plt.title(f"{title}\n{subtitle}", fontsize=11)
    # plt.tight_layout()
    filepath: str = os.path.join(
        results, f"full_cmp_{data_type}_r{readers}_w{writers}.png"
    )
    print(f"saving figure to {filepath}")
    fig.savefig(filepath)
    plt.close()


def data_analysis(working_dir: str):
    np_files = glob.glob(os.path.join(working_dir, "*.npy"))
    if len(np_files) != 1:
        raise Exception(
            f'Need exactly one np data (.npy) file for analysis in "{working_dir}"'
        )
    datafile: str = np_files[0]
    data = np.load(datafile)

    global results
    old_results: str = results
    results = working_dir  # for the next few plots to work here

    # plot individually per mode
    for mode in sync_modes:
        plot_for_mode(mode, data)

    # plot 3d graph
    for mode in sync_modes:
        for ex in ["Read", "Write"]:
            plot_perf_mountain(mode, data, execution=ex)

    plot_cmp_mode(data)

    # plot comparatively across modes
    plot_cmp(data, num_readers=8, num_writers=2)
    plot_cmp(data, num_readers=9, num_writers=1)
    plot_cmp(data, num_readers=8, num_writers=9)

    results = old_results  # back to cwd


if __name__ == "__main__":
    os.makedirs(results, exist_ok=True)

    if not os.path.exists(OUT):
        raise Exception(f'No "{OUT}" directory for binaries! Run make')

    plot_big_cmp(idx=0)
    plot_big_cmp(idx=1)

    for binary in glob.glob(os.path.join(OUT, f"*.{BIN_SUFFIX}")):
        op = os.path.basename(binary).replace(".out", "")
        working_dir: str = os.path.join(results, op)
        os.makedirs(working_dir, exist_ok=True)
        datafile = os.path.join(working_dir, "data.npy")
        # data_collection(datafile, op)
        data_analysis(working_dir)

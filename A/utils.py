import matplotlib.pyplot as plt
from pathlib import Path

MYQ = 'MyQueue'
REM = 'REM'
SLOWLY = 'slowly'
ABRUPT = 'abrupt'


def get_Figure19Path(node):
    return {
        MYQ: Path(f'dctcp_figure_19/MyQueue/UPDATED2-2000-TH40_N{node}_NK1Gbps_LF1Gbps/queue_length.dat'),
        REM: Path(f'dctcp_figure_19/REM/TH40_N{node}_NK1Gbps_LF1Gbps/queue_length.dat')
    }


def get_FairThroughput_Path(fair_tp):
    return {
        MYQ: Path(f'dctcp_late_flow_{fair_tp}/MyQueue/UPDATED2-2000-TH40_N20_NK1Gbps_LF1Gbps/through_puts.dat'),
        REM: Path(f'dctcp_late_flow_{fair_tp}/REM/UPDATED2-2000-TH40_N20_NK1Gbps_LF1Gbps/through_puts.dat')
    }


# Figure19Path = namedtuple('Figure19Path', ['MyQueue', 'REM'])
# figure19Path = Figure19Path(Path('dctcp_figure_19/MyQueue'), Path('dctcp_figure_19/REM'))

def to_MBs(n):
    return round(float(n) * 8 / 0.2 / 1e6, 1)


def get_fair_throughput(queue, fair_tp):
    y = []
    with open(get_FairThroughput_Path(fair_tp)[queue], 'r') as f:
        while not f.readline().strip().startswith("0.2"):
            pass
        for i in range(22):
            line = f.readline().strip().split()
            if len(line) == 0: continue
            y.append([float(i) for i in line[1:-1]])
        return y


def get_figure19(queue, node):
    res = [[], []]
    with open(get_Figure19Path(node)[queue]) as file:
        file.readline()
        for line in file:
            data = line.strip().split()
            if len(data) == 0: continue
            res[1].append(int(data[1]))
            res[0].append(float(data[0][:-1]))
    return res


def draw_figure19(node):
    plt.figure(figsize=(10, 6))
    x, y1 = get_figure19(MYQ, node)
    _, y2 = get_figure19(REM, node)
    # Plot the first line
    plt.plot(x, y1, label='My Queue', color='blue', marker='o')

    # Plot the second line
    plt.plot(x, y2, label='DCTCP Queue', color='red', marker='x')

    # Add a title
    plt.title(f'{node} nodes')

    # Add x and y labels
    plt.xlabel('Time(s)')
    plt.ylabel('Queue Length Packets')

    # Add a legend
    plt.legend()

    # Show the plot
    plt.show()


def jain_fairness(arr, i=20):
    sum, sumSqare = 0, 0
    for n in arr:
        sum += n
        sumSqare += n * n
    return sum*sum / (len(arr) * sumSqare)


def draw_fairness():
    plt.figure(figsize=(10, 6))

    for queue, color in [(MYQ, "red"), (REM, 'green')]:
        for fair_tp, marker in [(SLOWLY, "o"), (ABRUPT, "*")]:
            xi = 0 if fair_tp != ABRUPT else 6
            tps = get_fair_throughput(queue, fair_tp)
            x = [f'{i * 0.2:0.1f}s' for i in range(1, len(tps[0]) + 1)]
            fairs = ([jain_fairness(n) for n in [*zip(*tps[:-1])]])
            plt.plot(x[xi:], fairs[xi:], label=f'{queue} {fair_tp}', color=color, marker=marker)
    plt.title(f'Jain Fairness')
    plt.axvline(x="1.4s", color='black', linestyle='--')

    # Add a note (annotation) for the vertical line
    # plt.text(1, , 'Next 10 flow starts at 1.4s', verticalalignment='center_baseline')

    # Add x and y labels
    plt.xlabel('Time(s)')
    plt.ylabel('Fairness index')

    # Add a legend
    plt.legend()
    plt.show()
def draw_fair_throughput( queue, mode):
    plt.figure(figsize=(10, 6))
    y = get_fair_throughput(queue, mode)

    x = [f'{i*0.2:0.1f}s' for i in range(1, len(y[0])+1)]
    lines = []
    for i, yi in enumerate(y[:-1]):
        line, = plt.plot(x, yi, label=f'Flow {i}')
        lines.append(line)
    line, = plt.plot(x, y[-1], label=f'Flow total', color='red')
    lines.append(line)
    # Add a legend
    first_legend = plt.legend(handles=lines[:-1], ncol=2, loc='upper right', bbox_to_anchor=(1, .95))
    plt.gca().add_artist(first_legend)
    plt.legend(handles=lines[-1:], loc='upper center', bbox_to_anchor=(0.5, .95))
    # Add a title
    plt.title(f'{queue} {mode} flow')
    # plt.xticks(["0.2s", "0.4s"])
    # Add x and y labels
    plt.xlabel('Time(s)')
    plt.ylabel('Throughput MB/s')

    if mode == ABRUPT:
        # Add a vertical dashed line at x = 5
        plt.axvline(x="1.4s", color='black', linestyle='--')

        # Add a note (annotation) for the vertical line
        plt.text(1, 500, 'Next 10 flow starts at 1.4s', verticalalignment='center_baseline')

    # Show the plot
    plt.show()

if __name__ == '__main__':
    print('HI')
    # draw_figure19(2)
    # draw_figure19(40)
    # draw_fair_throughput(MYQ, SLOWLY)
    # draw_fair_throughput(MYQ, ABRUPT)
    # draw_fair_throughput(REM, ABRUPT)
    # draw_fair_throughput(REM, SLOWLY)
    draw_fairness() # dont include total
#     a = [1125096,1459584,1487096,1015048,1420488,1151160,1022288,1510264,1136680,1288720,1155504,1313336,1039664,1117856,987536,1028080,1430624,961472,1294512,1155504,
# ]
#     print(jain_fairness(a))
#     tps = get_fair_throughput(MYQ, SLOWLY)
#     print([*zip(*tps)])

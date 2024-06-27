import matplotlib.pyplot as plt
from pathlib import Path

MYQ = 'MyQueue'
REM = 'REM'


def get_Figure19Path (node):
    return {
    MYQ: Path(f'dctcp_figure_19/MyQueue/UPDATED2-2000-TH40_N{node}_NK1Gbps_LF1Gbps/queue_length.dat'),
    REM: Path(f'dctcp_figure_19/REM/TH40_N{node}_NK1Gbps_LF1Gbps/queue_length.dat')
}


# Figure19Path = namedtuple('Figure19Path', ['MyQueue', 'REM'])
# figure19Path = Figure19Path(Path('dctcp_figure_19/MyQueue'), Path('dctcp_figure_19/REM'))

class Figure19:
    pass


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


if __name__ == '__main__':
    print('HI')
    # draw_figure19(2)
    draw_figure19(40)

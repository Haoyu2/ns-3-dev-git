import matplotlib.pyplot as plt
import numpy as np
from brokenaxes import brokenaxes

# Generate example data
x = np.linspace(0, 10, 100)

# Create the plot with broken y-axis
fig = plt.figure(figsize=(12, 8))
bax = brokenaxes(ylims=((0, 0.4), (0.6, 1.2)), hspace=.05)  # Adjust ylims and hspace as needed

# Plot 20 lines with different colors
lines = []
for i in range(1, 21):
    y = np.sin(x + i * np.pi / 10)
    line, *_ = bax.plot(x, y, label=f'Line {i}')
    lines.append(line)

# Add a title
bax.set_title('Line Chart with 20 Different Lines, Two Legends, and a Vertical Line with Broken Y-Axis')

# Add x and y labels
bax.set_xlabel('X-axis')
bax.set_ylabel('Y-axis')

# Customize x-axis ticks
bax.set_xticks(np.linspace(0, 10, 6))  # Show fewer x-axis labels

# Create the first legend and place it a little lower
first_legend = bax.legend(handles=lines[:10], loc='upper center', bbox_to_anchor=(0.5, 1.05), title='First 10 Lines')

# Add the first legend to the plot
fig.gca().add_artist(first_legend)

# Create the second legend
bax.legend(handles=lines[10:], loc='lower right', title='Next 10 Lines')

# Add a vertical dashed line at x = 5
bax.axvline(x=5, color='black', linestyle='--')

# Add a note (annotation) for the vertical line
bax.text(5, 0.7, 'Vertical Line at x=5', rotation=90, verticalalignment='center', horizontalalignment='right', backgroundcolor='white')

# Show the plot
plt.show()

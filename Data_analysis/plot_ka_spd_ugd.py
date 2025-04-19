import numpy as np
import matplotlib.pyplot as plt

def simulate_positions_three_classes(region_size):
    """
    Simulate random positions for three classes of agents:
      - 'Adult':    e.g. 120 agents
      - 'Senior':    e.g. 50 agents
      - 'Child':     e.g. 30 agents
    The function returns:
      positions (N x 2) array of x,y for all agents
      classes    (N) array with string labels: 'Adult', 'Senior', 'Child'
    """
    np.random.seed(None)  # or a fixed seed if you want repeatable results
    
    # Example distribution of counts: adult=120, seniors=50, children=30
    n_adult  = 62
    n_senior = 31
    n_child  = 17
    total_agents = n_adult + n_senior + n_child
    
    # Generate positions uniformly in [0, region_size]²
    positions = np.random.uniform(0, region_size, size=(total_agents, 2))
    
    # Assign class labels
    classes = np.empty(total_agents, dtype=object)
    classes[:n_adult] = 'Adult Pedestrian'
    classes[n_adult : n_adult + n_senior] = 'Senior Pedestrian'
    classes[n_adult + n_senior : ] = 'Child Pedestrian'
    
    # Shuffle so classes are mixed among the array
    indices = np.arange(total_agents)
    np.random.shuffle(indices)
    positions = positions[indices]
    classes   = classes[indices]
    
    return positions, classes

def compute_grid_metrics(positions, region_size, depth):
    """
    Given positions of agents (regardless of class), compute:
      - average spatial drift
      - average occupant count (k-anonymity)
    for a grid derived from quadtree depth = `depth`.
    A depth of 1 => entire region is one cell,
    A depth of 2 => 2x2 cells, depth=3 => 4x4, etc.
    """
    cells_per_side = 2 ** (depth - 1)
    cell_size = region_size / cells_per_side
    
    # Determine which cell each agent belongs to
    indices = np.floor(positions / cell_size).astype(int)  # shape (N,2)

    # Gather positions in each cell
    cell_dict = {}
    for pos, idx in zip(positions, indices):
        key = (idx[0], idx[1])  # row, col
        if key not in cell_dict:
            cell_dict[key] = []
        cell_dict[key].append(pos)
    
    # For each occupied cell, compute average distance from cell center, occupant count
    drifts = []
    counts = []
    for key, pos_list in cell_dict.items():
        pos_arr = np.array(pos_list)
        center = (np.array(key) + 0.5) * cell_size  # cell center
        dists = np.linalg.norm(pos_arr - center, axis=1)
        drifts.append(np.mean(dists))
        counts.append(len(pos_arr))  # occupant count in that cell
    
    avg_drift = np.mean(drifts) if drifts else 0.0
    avg_k     = np.mean(counts) if counts else 0.0
    return avg_drift, avg_k

# 1) Simulate agent positions for 3 classes
region_size = 100.0
positions, classes = simulate_positions_three_classes(region_size)

# 2) Evaluate multiple depths: 1..5
depths = [1,2,3,4,5]
avg_drifts = []
avg_ks = []

for d in depths:
    drift, k = compute_grid_metrics(positions, region_size, d)
    avg_drifts.append(drift)
    avg_ks.append(k)
    print(f"Depth {d}, cell_size={region_size/(2**(d-1)):.2f}: "
          f"avg_drift={drift:.3f}, avg_k={k:.3f}")

# 3) Plot the "trade-off": x-axis=1/(avg drift), y-axis=avg k
inv_drifts = np.divide(1.0, avg_drifts, out=np.zeros_like(avg_drifts), where=(np.array(avg_drifts)!=0))
drifts = np.divide(avg_drifts, 1.0, out=np.zeros_like(avg_drifts), where=(np.array(avg_drifts)!=0))

plt.figure(figsize=(7,5))
plt.plot(inv_drifts, avg_ks, marker='o', linestyle='-', color='blue')
for i, d in enumerate(depths):
    plt.annotate(f"d={d}", (drifts[i], avg_ks[i]), textcoords="offset points", xytext=(0,10), ha='center')
plt.xlabel('Average Spatial Drift (m)')
plt.ylabel('Average k-Anonymity')
plt.title('Trade-off Between Spatial Accuracy and k-Anonymity')
plt.grid(True)
for i, d in enumerate(depths):
    plt.annotate(f'Depth {d}', (inv_drifts[i], avg_ks[i]), textcoords="offset points", xytext=(0,10), ha='center')
plt.show()

# 4) Pick a depth to visualize, e.g. depth=3
selected_depth = 5
cells_per_side = 2 ** (selected_depth - 1)
cell_size = region_size / cells_per_side

# 5) Plot the agent positions color-coded by class, plus grid lines
plt.figure(figsize=(7,7))

# Color map for classes
color_map = {
    'Adult Pedestrian':  'red',
    'Senior Pedestrian': 'blue',
    'Child Pedestrian':  'green'
}

# Plot each class separately
unique_classes = np.unique(classes)
for cls in unique_classes:
    mask = (classes == cls)
    plt.scatter(positions[mask, 0], positions[mask, 1],
                c=color_map[cls], label=cls, alpha=0.7, s=30)

# Draw the grid lines
for i in range(1, cells_per_side):
    # vertical line
    x = i*cell_size
    plt.axvline(x, color='gray', linestyle='--', linewidth=1)
    # horizontal line
    y = i*cell_size
    plt.axhline(y, color='gray', linestyle='--', linewidth=1)

plt.xlim(0, region_size)
plt.ylim(0, region_size)
plt.xlabel('X (m)')
plt.ylabel('Y (m)')
plt.title(f'Quadtree Depth={selected_depth}: {cells_per_side}x{cells_per_side} Cells')
plt.legend()
plt.show()
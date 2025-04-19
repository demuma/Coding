import numpy as np
import matplotlib.pyplot as plt

# -----------------------------
# 1) Simulate agent positions
# -----------------------------
def simulate_positions_three_classes(region_size):
    """
    Simulate random positions for three classes of agents:
      - 'Adult Pedestrian'    => 62 agents
      - 'Senior Pedestrian'   => 31 agents
      - 'Child Pedestrian'    => 17 agents

    Returns:
      positions (N,2) array of all agent positions
      classes   (N,) array of string labels
    """
    np.random.seed(None)  # or a fixed seed for repeatable results

    n_adult  = 52 * 2
    n_senior = 31 * 2
    n_child  = 17 * 2
    total_agents = n_adult + n_senior + n_child

    # Generate random positions in [0, region_size]^2
    positions = np.random.uniform(0, region_size, size=(total_agents, 2))

    # Assign class labels
    class_labels = np.empty(total_agents, dtype=object)
    class_labels[:n_adult] = 'Adult Pedestrian'
    class_labels[n_adult : n_adult + n_senior] = 'Senior Pedestrian'
    class_labels[n_adult + n_senior : ] = 'Child Pedestrian'

    # Shuffle to mix them
    indices = np.arange(total_agents)
    np.random.shuffle(indices)
    positions    = positions[indices]
    class_labels = class_labels[indices]

    return positions, class_labels

# -----------------------------
# 2) Utility functions
# -----------------------------
def count_per_class(positions, classes, x_min, y_min, x_max, y_max):
    """
    Count how many agents of each class fall within [x_min, x_max) x [y_min, y_max).
    Returns a dict: { 'Adult Pedestrian': N1, 'Senior Pedestrian': N2, 'Child Pedestrian': N3 }
    """
    inside_mask = (
        (positions[:,0] >= x_min) & (positions[:,0] < x_max) &
        (positions[:,1] >= y_min) & (positions[:,1] < y_max)
    )
    subset_classes = classes[inside_mask]
    
    result = {
        'Adult Pedestrian': 0,
        'Senior Pedestrian': 0,
        'Child Pedestrian': 0
    }
    for c in subset_classes:
        result[c] += 1
    return result

def compute_cell_stats(positions, x_min, y_min, x_max, y_max):
    """
    Return (occupant_count, avg_drift) for all agents in [x_min, x_max) x [y_min, y_max).
      occupant_count: total number of agents
      avg_drift: mean distance from cell center
    """
    mask = (
        (positions[:,0] >= x_min) & (positions[:,0] < x_max) &
        (positions[:,1] >= y_min) & (positions[:,1] < y_max)
    )
    pts_in_box = positions[mask]
    occupant_count = len(pts_in_box)
    if occupant_count == 0:
        return 0, 0.0

    center_x = 0.5*(x_min + x_max)
    center_y = 0.5*(y_min + y_max)
    dists = np.linalg.norm(pts_in_box - [center_x, center_y], axis=1)
    avg_drift = np.mean(dists)
    return occupant_count, avg_drift

# -----------------------------
# 3) Quadtree Node
# -----------------------------
class QuadtreeNode:
    """
    Represents a node (which may be subdivided or a leaf) in the dynamic quadtree.
    For each node, we store occupant_count and occupant_drift (the average distance to center).
    """
    def __init__(self, x_min, y_min, x_max, y_max, depth, occupant_count, occupant_drift):
        self.x_min = x_min
        self.y_min = y_min
        self.x_max = x_max
        self.y_max = y_max
        self.depth = depth

        # occupant_count = sum of all agents in this bounding box (like k for that cell)
        # occupant_drift  = average distance from bounding box center (spatial drift)
        self.occupant_count = occupant_count
        self.occupant_drift = occupant_drift

        self.children = []  # up to 4 children if subdivided

    def is_leaf(self):
        return len(self.children) == 0

# -----------------------------
# 4) Build the dynamic quadtree
# -----------------------------
def build_quadtree(positions, classes, x_min, y_min, x_max, y_max,
                   depth=0, max_depth=5, k=5):
    """
    Recursively build a dynamic quadtree that only subdivides if occupant_count >= k for EACH class
    (i.e. each class has at least k) and not above max_depth.

    Also store occupant_count, occupant_drift in each node for final annotation.
    """
    occupant_count_total, occupant_drift = compute_cell_stats(positions, x_min, y_min, x_max, y_max)
    node = QuadtreeNode(x_min, y_min, x_max, y_max, depth,
                        occupant_count_total, occupant_drift)

    # Count occupant per class in this bounding box
    counts_dict = count_per_class(positions, classes, x_min, y_min, x_max, y_max)
    occupant_sum = sum(counts_dict.values())

    # If occupant_sum=0, no agents => remain leaf
    if occupant_sum == 0:
        return node
    
    # Check if each class has >=k
    subdivide_ok = all(v >= k for v in counts_dict.values())
    
    # Subdivide if depth < max_depth AND subdivide_ok
    if depth < max_depth and subdivide_ok:
        x_mid = 0.5*(x_min + x_max)
        y_mid = 0.5*(y_min + y_max)

        c1 = build_quadtree(positions, classes, x_min, y_min, x_mid, y_mid,
                            depth+1, max_depth, k)
        c2 = build_quadtree(positions, classes, x_mid, y_min, x_max, y_mid,
                            depth+1, max_depth, k)
        c3 = build_quadtree(positions, classes, x_min, y_mid, x_mid, y_max,
                            depth+1, max_depth, k)
        c4 = build_quadtree(positions, classes, x_mid, y_mid, x_max, y_max,
                            depth+1, max_depth, k)
        node.children = [c1, c2, c3, c4]
    
    return node

def gather_leaves(node):
    """Return a list of all leaf nodes in the quadtree."""
    if node.is_leaf():
        return [node]
    leaves = []
    for child in node.children:
        leaves.extend(gather_leaves(child))
    return leaves

# -----------------------------
# 5) Putting it all together
# -----------------------------
def main():
    region_size = 50.0
    positions, classes = simulate_positions_three_classes(region_size)

    # Build a dynamic quadtree up to depth=5 ensuring each leaf
    # has occupant_count >= 5 in each class (the user asked for that).
    root = build_quadtree(positions, classes, 
                          x_min=0, y_min=0, x_max=region_size, y_max=region_size,
                          depth=0, max_depth=3, k=2)

    # Plot agent positions color-coded by class
    plt.figure(figsize=(8,8))
    color_map = {
        'Adult Pedestrian':  'red',
        'Senior Pedestrian': 'blue',
        'Child Pedestrian':  'green'
    }
    unique_classes = np.unique(classes)
    for cls in unique_classes:
        mask = (classes == cls)
        plt.scatter(positions[mask, 0], positions[mask, 1],
                    c=color_map[cls], label=cls, alpha=0.7, s=30)

    plt.xlim(0, region_size)
    plt.ylim(0, region_size)

    # Gather the final leaves and draw them
    leaves = gather_leaves(root)
    ax = plt.gca()

    for leaf in leaves:
        # draw a rectangle
        width  = leaf.x_max - leaf.x_min
        height = leaf.y_max - leaf.y_min
        rect = plt.Rectangle((leaf.x_min, leaf.y_min),
                             width, height,
                             fill=False, edgecolor='gray', linestyle='--')
        ax.add_patch(rect)

        # Annotate occupant_count (k) and occupant_drift in the center
        cx = 0.5*(leaf.x_min + leaf.x_max)
        cy = 0.5*(leaf.y_min + leaf.y_max)
        text_label = f"n={leaf.occupant_count}\ndr={leaf.occupant_drift:.1f}"
        ax.text(cx, cy, text_label, color='black',
                ha='center', va='center', fontsize=8)

    plt.xlabel('X (m)')
    plt.ylabel('Y (m)')
    plt.title("Dynamic Quadtree with k>=2 per Class (Leaf Cells Annotated)\n(n=total agents, dr=avg spatial drift)")
    plt.legend()
    plt.show()

if __name__ == "__main__":
    main()
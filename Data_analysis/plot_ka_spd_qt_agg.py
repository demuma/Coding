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
    np.random.seed(34)  # For repeatability
    
    n_adult  = 52 * 2
    n_senior = 31 * 2
    n_child  = 10 * 2
    total_agents = n_adult + n_senior + n_child

    # Generate positions uniformly in [0, region_size]²
    positions = np.random.uniform(0, region_size, size=(total_agents, 2))

    # Create an array for class labels
    class_labels = np.empty(total_agents, dtype=object)
    class_labels[:n_adult] = 'Adult Pedestrian'
    class_labels[n_adult : n_adult + n_senior] = 'Senior Pedestrian'
    class_labels[n_adult + n_senior :] = 'Child Pedestrian'

    # Shuffle to mix the classes
    indices = np.arange(total_agents)
    np.random.shuffle(indices)
    positions = positions[indices]
    class_labels = class_labels[indices]

    return positions, class_labels

# -----------------------------
# 2) Utility Functions
# -----------------------------
def count_per_class(positions, classes, x_min, y_min, x_max, y_max):
    """
    Count how many agents of each class fall within [x_min, x_max) x [y_min, y_max).
    Returns a dictionary with keys for each class.
    """
    inside_mask = ((positions[:,0] >= x_min) & (positions[:,0] < x_max) &
                   (positions[:,1] >= y_min) & (positions[:,1] < y_max))
    subset = classes[inside_mask]
    counts = {
        'Adult Pedestrian': 0,
        'Senior Pedestrian': 0,
        'Child Pedestrian': 0
    }
    for c in subset:
        counts[c] += 1
    return counts

def compute_cell_stats(positions, x_min, y_min, x_max, y_max):
    """
    Return (occupant_count, avg_drift) for agents in the given bounding box.
    """
    mask = ((positions[:,0] >= x_min) & (positions[:,0] < x_max) &
            (positions[:,1] >= y_min) & (positions[:,1] < y_max))
    pts = positions[mask]
    occupant_count = len(pts)
    if occupant_count == 0:
        return 0, 0.0
    center = np.array([0.5*(x_min+x_max), 0.5*(y_min+y_max)])
    dists = np.linalg.norm(pts - center, axis=1)
    return occupant_count, np.mean(dists)

# -----------------------------
# 3) Quadtree Node & Full Subdivision
# -----------------------------
class Node:
    """Represents a quadtree node (bounding box) with occupant counts for each class."""
    def __init__(self, x_min, y_min, x_max, y_max, depth, parent=None):
        self.x_min = x_min
        self.y_min = y_min
        self.x_max = x_max
        self.y_max = y_max
        self.depth = depth
        self.parent = parent
        self.children = []  # Will store up to 4 children if subdivided
        
        # Compute occupant counts and total count in this node:
        self.counts = {'Adult Pedestrian': 0,
                       'Senior Pedestrian': 0,
                       'Child Pedestrian': 0}
        self.total_count = 0

    def is_leaf(self):
        return len(self.children) == 0

def full_subdivide(positions, classes, x_min, y_min, x_max, y_max, depth=0, max_depth=5, parent=None):
    """
    Fully subdivide the region to max_depth (if non-empty), regardless of k thresholds.
    """
    node = Node(x_min, y_min, x_max, y_max, depth, parent)
    # Count agents for each class in this node:
    counts = count_per_class(positions, classes, x_min, y_min, x_max, y_max)
    node.counts = counts
    node.total_count = sum(counts.values())

    # If non-empty and depth is less than max_depth, subdivide:
    if depth < max_depth and node.total_count > 0:
        x_mid = 0.5 * (x_min + x_max)
        y_mid = 0.5 * (y_min + y_max)
        node.children.append(full_subdivide(positions, classes, x_min, y_min, x_mid, y_mid, depth+1, max_depth, node))
        node.children.append(full_subdivide(positions, classes, x_mid, y_min, x_max, y_mid, depth+1, max_depth, node))
        node.children.append(full_subdivide(positions, classes, x_min, y_mid, x_mid, y_max, depth+1, max_depth, node))
        node.children.append(full_subdivide(positions, classes, x_mid, y_mid, x_max, y_max, depth+1, max_depth, node))
    return node

def gather_leaves(node):
    """Return all leaf nodes in the quadtree."""
    if node.is_leaf():
        return [node]
    leaves = []
    for child in node.children:
        leaves.extend(gather_leaves(child))
    return leaves

# -----------------------------
# 4) Bottom-Up Merging to Enforce k≥5 per Class
# -----------------------------
def all_classes_ok(node, k=5):
    """
    Check if this node has at least k agents for each class.
    """
    for cls in node.counts:
        if node.counts[cls] < k:
            return False
    return True

def bottom_up_merge(root, k=5):
    """
    Iteratively merge sibling nodes upward: if any leaf does not meet the k threshold for every class,
    merge that node's siblings into their parent.
    """
    changed = True
    while changed:
        changed = False
        leaves = gather_leaves(root)
        for leaf in leaves:
            if not all_classes_ok(leaf, k):
                if leaf.parent is not None:
                    parent = leaf.parent
                    # Recalculate parent's counts as the sum of all its children's counts.
                    new_counts = {'Adult Pedestrian': 0, 'Senior Pedestrian': 0, 'Child Pedestrian': 0}
                    for sibling in parent.children:
                        for cls, count in sibling.counts.items():
                            new_counts[cls] += count
                    parent.counts = new_counts
                    parent.total_count = sum(new_counts.values())
                    # Remove children => parent becomes a leaf.
                    parent.children = []
                    changed = True
                    break  # Restart merging loop due to change.
        if changed:
            # Continue until no more merges occur.
            continue
    return root

# -----------------------------
# 5) Visualization: Annotate Each Leaf and Mark Cells Below k
# -----------------------------
def visualize_leaves(root, ax, k=5):
    """
    Draw each leaf node as a rectangle.
    - Annotate with counts and (mock) spatial drift (here, we compute drift as the average distance
      from the cell center for agents within that cell).
    - If a leaf does not fulfill the k constraint (≥k for each class), fill the rectangle with
      a semi-transparent red background.
    """
    leaves = gather_leaves(root)
    for leaf in leaves:
        width = leaf.x_max - leaf.x_min
        height = leaf.y_max - leaf.y_min
        # Compute the cell center
        center = np.array([0.5*(leaf.x_min + leaf.x_max), 0.5*(leaf.y_min + leaf.y_max)])
        # For spatial drift, compute distances for agents in this cell.
        # (We approximate by assuming uniform distribution in the box)
        # In a realistic implementation you would compute this from the actual agent positions.
        # Here we compute the maximum possible drift (half the cell diagonal) and use an average factor.
        max_possible_drift = np.sqrt((width/2)**2 + (height/2)**2)
        # We'll assume drift is about 0.3 times the maximum (for demonstration).
        avg_drift = 0.3 * max_possible_drift

        # Check if the cell meets the k threshold per class.
        meets_k = all_classes_ok(leaf, k)
        
        # Draw the rectangle; use a red fill if not meeting threshold, else transparent.
        if not meets_k:
            facecolor = (1, 0, 0, 0.1)  # Red with alpha=0.1
            edgecolor = 'red'
        else:
            facecolor = 'none'
            edgecolor = 'gray'
            
        rect = plt.Rectangle((leaf.x_min, leaf.y_min), width, height,
                             fill=True, facecolor=facecolor,
                             edgecolor=edgecolor, linestyle='--')
        ax.add_patch(rect)
        
        # Annotate with counts and spatial drift.
        text = (f"A:{leaf.counts['Adult Pedestrian']}\n"
                f"S:{leaf.counts['Senior Pedestrian']}\n"
                f"C:{leaf.counts['Child Pedestrian']}\n"
                f"dr={avg_drift:.1f}")
        cx, cy = center
        ax.text(cx, cy, text, ha='center', va='center', fontsize=7, color='black')

# -----------------------------
# 6) Main Routine
# -----------------------------
def main():
    region_size = 50.0
    positions, classes = simulate_positions_three_classes(region_size)

    # Build a full quadtree subdividing completely to max depth=5
    root = full_subdivide(positions, classes, 0, 0, region_size, region_size, depth=0, max_depth=5)

    # Merge nodes bottom-up where possible to force leaves that ideally have k>=5 for each class.
    merged_root = bottom_up_merge(root, k=1)

    # Plot agent positions (color-coded by class)
    plt.figure(figsize=(8,8))
    ax = plt.gca()
    color_map = {
        'Adult Pedestrian': 'red',
        'Senior Pedestrian': 'blue',
        'Child Pedestrian': 'green'
    }
    for cls in np.unique(classes):
        mask = (classes == cls)
        plt.scatter(positions[mask, 0], positions[mask, 1],
                    c=color_map[cls], label=cls, alpha=0.7, s=30)

    plt.xlim(0, region_size)
    plt.ylim(0, region_size)
    plt.xlabel("X (m)")
    plt.ylabel("Y (m)")
    # plt.title("Dynamic Quadtree (After Bottom-Up Merge)\nLeaf Cells Annotated with Counts & Drift\n(Red fill: k constraint not fulfilled)")
    plt.title("Dynamic Quadtree \nLeaf Cells Annotated with Counts & Drift\n(Red fill: k constraint not fulfilled)")
    
    # Visualize leaves, marking ones that don't meet k constraint.
    visualize_leaves(merged_root, ax, k=2)
    plt.legend()
    plt.show()

if __name__ == "__main__":
    main()
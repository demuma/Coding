import numpy as np

# positions = np.array([[72, 35], [13, 12], [72, 35], [100, 100]])
positions = np.array([[395, 55]])
cell_size = 600.0
max_depth = 3  # Changed max_depth to 3
cell_ids = []
seen_cell_ids = set()

def get_quadtree_cell(position, cell_size, max_depth):
    center = np.array([cell_size, cell_size]) / 2.0
    cell_id = 3
    for i in range(max_depth):
        cell_id <<= 2
        if position[1] >= center[1]:
            center[1] += cell_size / (2 ** (i + 2))
            cell_id += 2
        else:
            center[1] -= cell_size / (2 ** (i + 2))
        if position[0] >= center[0]:
            center[0] += cell_size / (2 ** (i + 2))
            cell_id += 1
        else:
            center[0] -= cell_size / (2 ** (i + 2))
    return cell_id

def get_split_sequence(cell_id, max_depth):
    split_sequence = []
    for _ in range(max_depth):  # Corrected: Iterate only max_depth times
        split_sequence.insert(0, cell_id & 3)
        cell_id >>= 2
    return split_sequence

for i in range(len(positions)):
    cell_id = get_quadtree_cell(positions[i], cell_size, max_depth)
    if cell_id not in seen_cell_ids:
        cell_ids.append(cell_id)
        seen_cell_ids.add(cell_id)

split_sequences = []
for cell_id in cell_ids:
    split_sequences.append(get_split_sequence(cell_id, max_depth))

print(f"cell ids: {cell_ids}")
print(f"split sequences: {split_sequences}")
import igraph as ig
import matplotlib.pyplot as plt

g = ig.Graph.Famous('petersen')

# fig, ax=plt.subplots()
# ig.plot(g, target=ax)
# plt.show()

ig.plot(g)
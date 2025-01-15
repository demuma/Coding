#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <functional>
#include <stdexcept>

class Node {
public:
    sf::RectangleShape shape;
    bool isSplit = false;
    Node* parent = nullptr;
    Node* children[4] = {nullptr, nullptr, nullptr, nullptr};
    int id; // Unique integer ID
    int level; // Depth level of the node

    Node(float x, float y, float size, int id, int level, Node* parent = nullptr) : id(id), level(level), parent(parent) {
        shape.setSize({size, size});
        shape.setPosition(x, y);
        shape.setFillColor(sf::Color::Transparent);
        shape.setOutlineThickness(1);
        shape.setOutlineColor(sf::Color::Black); // Black lines for cells
    }

    void split(std::unordered_map<int, Node*>& nodeMap) {
        if (isSplit) return;

        float x = shape.getPosition().x;
        float y = shape.getPosition().y;
        float size = shape.getSize().x / 2;

        // Create and add 4 child nodes with unique IDs
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 2; ++j) {
                int childIndex = (i << 1) | j;
                // Calculate child ID using 2 additional bits
                int childId = (id << 2) | childIndex; 
                Node* child = new Node(x + j * size, y + i * size, size, childId, level + 1, this);
                child->shape.setOutlineColor(sf::Color::Black);
                children[childIndex] = child;
                nodeMap[childId] = child;
            }
        }

        isSplit = true;
    }

    void draw(sf::RenderWindow &window, sf::Font& font) {
        window.draw(shape);

        // Draw the ID in the center of the cell
        sf::Text text;
        text.setFont(font);
        text.setString(std::to_string(id));
        text.setCharacterSize(10);
        text.setFillColor(sf::Color::Black);

        sf::FloatRect textBounds = text.getLocalBounds();
        text.setPosition(
            shape.getPosition().x + shape.getSize().x / 2 - textBounds.width / 2,
            shape.getPosition().y + shape.getSize().y / 2 - textBounds.height / 2
        );
        window.draw(text);

        if (isSplit) {
            for (auto &child : children) {
                if (child) child->draw(window, font);
            }
        }
    }
};

class Quadtree {
private:
    int rows, cols;
    float cellSize;
    std::vector<Node*> baseNodes;
    std::unordered_map<int, Node*> nodeMap;

    // Function to get the depth of a cell based on its ID
    int getDepth(int id) const {
        // Calculate depth based on the number of bits in the ID
        // Base cells start with 4 bits, and each level adds 2 bits
        int tempId = id;
        int depth = 0;
        while (tempId > 0) {
            tempId >>= 2;
            depth++;
        }
        return depth - 1; // Subtract 1 because base cells have depth 1
    }

    // Convert (row,col) to morton code with initial 4 bits
    int mortonEncode(int row, int col) const {
        // Start with 11 in the most significant bits for root cells
        int morton = 0b11;
        morton <<= 2;
        for (int i = 0; i < 16; ++i) {
            morton |= (col & 1) << (2 * i);
            col >>= 1;
            morton |= (row & 1) << (2 * i + 1);
            row >>= 1;
        }
        return morton;
    }

    // Decode Morton code to (row, col)
    std::pair<int,int> mortonDecode(int id) const {
        int row = 0, col = 0;
        
        // The two most significant bits are removed since they are only used
        // for identifying the level
        id >>= 2;
        for (int i = 0; i < 16; i++) {
            col |= ((id >> (2 * i)) & 1) << i;
            row |= ((id >> (2 * i + 1)) & 1) << i;
        }
        return {row, col};
    }

public:
    Quadtree(int cols, int rows, float cellSize)
        : rows(rows), cols(cols), cellSize(cellSize) {
        // Initialize base cells with 4-bit Morton codes
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                // Base IDs are 12, 13, 14, 15 (1100, 1101, 1110, 1111 in binary)
                int baseId = 0b1100 | mortonEncode(i, j);
                Node* n = new Node(j * cellSize, i * cellSize, cellSize, baseId, 1);
                baseNodes.push_back(n);
                nodeMap[baseId] = n;
            }
        }
    }

    ~Quadtree() {
        for (auto &kv : nodeMap) {
            delete kv.second;
        }
    }

    Node* getNodeById(int id) {
        auto it = nodeMap.find(id);
        if (it != nodeMap.end()) {
            return it->second;
        }
        return nullptr;
    }

    void splitCell(int firstId, std::initializer_list<int> path = {}) {
        Node* currentNode = getNodeById(firstId);
        if (!currentNode) {
            std::cerr << "Error: starting cell ID not found.\n";
            return;
        }

        for (int childIndex : path) {
            if (!currentNode->isSplit) {
                currentNode->split(nodeMap);
            }
            if (childIndex < 0 || childIndex > 3) {
                std::cerr << "Error: Invalid child index.\n";
                return;
            }
            currentNode = currentNode->children[childIndex];
            if (!currentNode) {
                std::cerr << "Error: Path leads to null child.\n";
                return;
            }
        }

        if (!currentNode->isSplit) {
            currentNode->split(nodeMap);
        }
    }

    template <typename... Args>
    void splitCell(Args... args) {
        std::vector<int> pathVec = {args...};
        if (pathVec.empty()) {
            std::cerr << "Error: No path specified for splitCell.\n";
            return;
        }
        int firstId = pathVec[0];
        pathVec.erase(pathVec.begin());
        splitCell(firstId, pathVec);
    }

    sf::Vector2f getCellCenter(int id) const {
        Node* currentNode = nullptr;
        auto it = nodeMap.find(id);
        if (it == nodeMap.end()) {
            throw std::runtime_error("Cell not found.");
        }
        currentNode = it->second;

        float x = currentNode->shape.getPosition().x;
        float y = currentNode->shape.getPosition().y;
        float size = currentNode->shape.getSize().x;

        return sf::Vector2f(x + size / 2, y + size / 2);
    }

    std::vector<int> getNeighboringCells(int id) {
        std::vector<int> neighbors;
        Node* targetNode = getNodeById(id);
        if (!targetNode) {
            std::cerr << "Error: Target cell for neighbor search not found.\n";
            return neighbors;
        }

        int targetDepth = getDepth(id);

        // Lambda to recursively find neighboring cells
        std::function<void(Node*, int)> findNeighborsRecursively = 
            [&](Node* node, int depth) {
            if (!node || depth > targetDepth) return;

            // Add this node if it's at the target depth or is a leaf
            if (depth == targetDepth || !node->isSplit) {
                neighbors.push_back(node->id);
                return;
            }

            if (node->isSplit) {
                for (int i = 0; i < 4; ++i) {
                    if (node->children[i]) {
                        findNeighborsRecursively(node->children[i], depth + 1);
                    }
                }
            }
        };

        // Lambda to find neighbors in a specific direction
        std::function<void(Node*, int, int)> findNeighborsInDirection = 
            [&](Node* node, int dx, int dy) {
            if (!node) return;

            while (node->parent) {
                int parentId = node->parent->id;
                int nodeRow, nodeCol;
                std::tie(nodeRow, nodeCol) = mortonDecode(node->id);
                int parentRow, parentCol;
                std::tie(parentRow, parentCol) = mortonDecode(parentId);

                
                int depthDiff = getDepth(node->id) - getDepth(parentId);

                if ((nodeRow + dy * pow(2, depthDiff)) >= parentRow && (nodeRow + dy * pow(2, depthDiff)) < parentRow + pow(2, depthDiff+1) &&
                    (nodeCol + dx * pow(2, depthDiff)) >= parentCol && (nodeCol + dx * pow(2, depthDiff)) < parentCol + pow(2, depthDiff+1)) {
                    
                    Node* neighbor = nullptr;
                    int childIndex;
                    if (dx == 0 && dy == -1) { // North
                        childIndex = (node == node->parent->children[2]) ? 0 : 1;
                    } else if (dx == 1 && dy == -1) { // North-East
                        childIndex = 2;
                    } else if (dx == 1 && dy == 0) { // East
                        childIndex = (node == node->parent->children[0]) ? 1 : 3;
                    } else if (dx == 1 && dy == 1) { // South-East
                        childIndex = 0;
                    } else if (dx == 0 && dy == 1) { // South
                        childIndex = (node == node->parent->children[0]) ? 2 : 3;
                    } else if (dx == -1 && dy == 1) { // South-West
                        childIndex = 1;
                    } else if (dx == -1 && dy == 0) { // West
                        childIndex = (node == node->parent->children[1]) ? 0 : 2;
                    } else if (dx == -1 && dy == -1) { // North-West
                        childIndex = 3;
                    }
                    neighbor = node->parent->children[childIndex];

                    if (neighbor) {
                        findNeighborsRecursively(neighbor, getDepth(neighbor->id));
                    }
                    break;
                }
                node = node->parent;
            }
        };

        int dx[] = {0, 1, 1, 0, -1, -1, -1, 0};
        int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};

        for (int i = 0; i < 8; ++i) {
            findNeighborsInDirection(targetNode, dx[i], dy[i]);
        }

        // Remove duplicates and sort
        std::sort(neighbors.begin(), neighbors.end());
        neighbors.erase(std::unique(neighbors.begin(), neighbors.end()), neighbors.end());
        return neighbors;
    }

    int getNearestCellIndex(sf::Vector2f position) {
        // Find the base cell that contains the position
        int col = static_cast<int>(position.x / cellSize);
        int row = static_cast<int>(position.y / cellSize);

        col = std::max(0, std::min(col, cols - 1));
        row = std::max(0, std::min(row, rows - 1));

        int baseId = mortonEncode(row, col);
        Node* currentNode = getNodeById(baseId);

        if (!currentNode) {
            std::cerr << "Error: Base cell not found during nearest cell search.\n";
            return -1; 
        }

        // Traverse down the tree to find the smallest cell containing the position
        while (currentNode->isSplit) {
            float midX = currentNode->shape.getPosition().x + currentNode->shape.getSize().x / 2;
            float midY = currentNode->shape.getPosition().y + currentNode->shape.getSize().y / 2;

            int childIndex = 0;
            if (position.y >= midY) {
                childIndex |= 2; 
            }
            if (position.x >= midX) {
                childIndex |= 1;
            }

            Node* nextNode = currentNode->children[childIndex];
            if (!nextNode) {
                std::cerr << "Error: Child cell not found during nearest cell search.\n";
                return -1;
            }

            currentNode = nextNode;
        }

        return currentNode->id;
    }

    void draw(sf::RenderWindow &window, sf::Font& font) {
        for (auto &n : baseNodes) {
            n->draw(window, font);
        }

        sf::RectangleShape border;
        border.setSize({cols * cellSize, rows * cellSize});
        border.setPosition(0, 0);
        border.setFillColor(sf::Color::Transparent);
        border.setOutlineThickness(2);
        border.setOutlineColor(sf::Color::Black);
        window.draw(border);
    }

    void printChildren(int id) {
        Node* targetNode = getNodeById(id);
        if (!targetNode) {
            std::cout << "Children of cell " << id << ": Cell not found.\n";
            return;
        }

        if (!targetNode->isSplit) {
            std::cout << "Children of cell " << id << ": not split.\n";
            return;
        }

        std::function<void(Node*, int)> printChildrenRecursive = [&](Node* node, int depth) {
            if (!node) return;
            for (int i = 0; i < depth; ++i) {
                std::cout << "  ";
            }
            std::cout << "- " << node->id << "\n";
            if (node->isSplit) {
                for (auto c : node->children) {
                    printChildrenRecursive(c, depth + 1);
                }
            }
        };

        std::cout << "Children of cell " << id << ":\n";
        for (auto c : targetNode->children) {
            if (c) printChildrenRecursive(c, 1);
        }
    }
};

int main() {
    sf::RenderWindow window(sf::VideoMode(600, 600), "Quadtree (2x2 Base Grid)");
    sf::Font font;
    if (!font.loadFromFile("/Library/Fonts/Arial Unicode.ttf")) {
        std::cerr << "Error: Failed to load font.\n";
        return -1;
    }

    Quadtree quadtree(2, 2, 600);
    quadtree.splitCell(12);
    quadtree.splitCell(13);
    quadtree.splitCell(12, {0});

    // sf::Vector2f cell = quadtree.getCellCenter(15);
    // std::cout << "Center of cell 15: (" << cell.x << ", " << cell.y << ")\n";

    try {
        // // Test cases to find neighbors
        // std::vector<std::pair<int, std::vector<int>>> neighborTestCases = {
        //     {3, {0, 1, 2, 8, 9, 96, 402, 414}}
        // };

        // for (const auto& testCase : neighborTestCases) {
        //     int cellId = testCase.first;
        //     std::vector<int> expectedNeighbors = testCase.second;
        //     std::vector<int> actualNeighbors = quadtree.getNeighboringCells(cellId);

        //     std::cout << "\nVerifying neighbors of cell ID " << cellId << ":\n";
        //     std::cout << "Neighbors of cell " << cellId << ": ";
        //     for (int neighborId : actualNeighbors) {
        //         std::cout << neighborId << " ";
        //     }
        //     std::cout << std::endl;

        //     std::sort(expectedNeighbors.begin(), expectedNeighbors.end());

        //     if (actualNeighbors == expectedNeighbors) {
        //         std::cout << "Neighbor verification for cell " << cellId << ": PASSED\n";
        //     } else {
        //         std::cout << "Neighbor verification for cell " << cellId << ": FAILED\n";
        //     }
        // }
        
        // quadtree.printChildren(48);

        while (window.isOpen()) {
            sf::Event event;
            while (window.pollEvent(event)) {
                if (event.type == sf::Event::Closed)
                    window.close();

                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::Escape) {
                        window.close();
                    }
                }

                if (event.type == sf::Event::MouseButtonPressed) {
                    sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                    int nearestCellId = quadtree.getNearestCellIndex(mousePos);
                    sf::Vector2f cellCenter = quadtree.getCellCenter(nearestCellId);

                    std::cout << "Clicked at (" << mousePos.x << ", " << mousePos.y << ") -> Nearest cell: " << nearestCellId
                              << " with center at (" << cellCenter.x << ", " << cellCenter.y << ")\n";

                    std::vector<int> neighbors = quadtree.getNeighboringCells(nearestCellId);
                    std::cout << "Neighbors of cell " << nearestCellId << ": ";
                    for (int neighborId : neighbors) {
                        std::cout << neighborId << " ";
                    }
                    std::cout << std::endl;
                }
            }

            window.clear(sf::Color::White);
            quadtree.draw(window, font);
            window.display();
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
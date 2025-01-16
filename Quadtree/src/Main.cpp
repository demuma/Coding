#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <random>

class Node {
public:
    sf::RectangleShape shape;
    bool isSplit = false;
    Node* parent = nullptr;
    Node* children[4] = {nullptr, nullptr, nullptr, nullptr};
    int id; // Unique integer ID
    int depth; // Depth level of the node

    Node(float x, float y, float size, int id, int depth, Node* parent = nullptr) : id(id), depth(depth), parent(parent) {
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
                Node* child = new Node(x + j * size, y + i * size, size, childId, depth + 1, this);
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
        text.setCharacterSize(3);
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
    float cellSize;
    int maxDepth;
    std::vector<Node*> baseNodes;
    std::unordered_map<int, Node*> nodeMap;

    // Function to get the depth of a cell based on its ID
    int getDepth(int id) const {
        // Calculate depth based on the number of bits in the ID
        // Base cells start with 4 bits, and each depth adds 2 bits
        int tempId = id;
        int depth = 0;
        while (tempId > 0) {
            tempId >>= 2;
            depth++;
        }
        return depth - 1; // Subtract 1 because base cells have depth 1
    }

    // Encode morton code
    int mortonEncode(int row, int col) const {
        int morton = 0b1100; // Start with 11 shifted left by 2 (12 in decimal)

        // Only interleave 1 bit each from row and col
        morton |= (col & 1) << 0; // Insert col's least significant bit at position 0
        morton |= (row & 1) << 1; // Insert row's least significant bit at position 1

        return morton;
    }

    // Decode morton code morton code
    std::pair<int, int> mortonDecode(int id) const {
        int row = 0, col = 0;

        id >>= 2; // Remove the initial "11"

        // Only extract 1 bit each for row and col
        col |= (id & 1) << 0; // Extract bit at position 0
        row |= ((id >> 1) & 1) << 0; // Extract bit at position 1

        return {row, col};
    }

    // Helper function to recursively split cells based on a path
    void splitRecursive(Node* node, const std::vector<int>& path, int pathIndex, std::unordered_map<int, Node*>& nodeMap) {
        if (!node) return;

        if (pathIndex == path.size()) {
            // If we have reached the end of the path, split the current node
            if (!node->isSplit) {
                node->split(nodeMap);
            }
            return;
        }

        if (!node->isSplit) {
            node->split(nodeMap);
        }

        int childIndex = path[pathIndex];
        if (childIndex < 0 || childIndex > 3) {
            std::cerr << "Error: Invalid child index in path.\n";
            return;
        }
        splitRecursive(node->children[childIndex], path, pathIndex + 1, nodeMap);
    }

public:
    Quadtree(float cellSize, int maxDepth)
        : maxDepth(maxDepth), cellSize(cellSize) {
        // Initialize base cells with 4-bit Morton codes
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 2; ++j) {
                // Base IDs are 12, 13, 14, 15 (1100, 1101, 1110, 1111 in binary)
                int baseId = 0b1100 | mortonEncode(i, j);
                Node* n = new Node(j * cellSize, i * cellSize, cellSize, baseId, 1);
                baseNodes.push_back(n);
                nodeMap[baseId] = n;
            }
        }
    }

   ~Quadtree() {
        // Use a recursive function for safe and complete deletion
        std::function<void(Node*)> deleteNodeRecursive = [&](Node* node) {
            if (node != nullptr) {
                for (int i = 0; i < 4; ++i) {
                    deleteNodeRecursive(node->children[i]);
                }
                delete node;
            }
        };

        // Start the deletion from each base node
        for (Node* baseNode : baseNodes) {
            deleteNodeRecursive(baseNode);
        }

        // Clear the nodeMap and baseNodes vector after deletion
        nodeMap.clear();
        baseNodes.clear();
    }

    void reset() {
        std::function<void(Node*)> resetNodeRecursive = [&](Node* node) {
            if (node != nullptr) {
                for (int i = 0; i < 4; ++i) {
                    if (node->children[i]) {
                        resetNodeRecursive(node->children[i]);
                        node->children[i] = nullptr;
                    }
                }
                node->isSplit = false;
            }
        };

        for (Node* baseNode : baseNodes) {
            resetNodeRecursive(baseNode);
        }

        nodeMap.clear();

        for (Node* baseNode : baseNodes) {
            nodeMap[baseNode->id] = baseNode;
        }
    }

    void clear() {
        std::function<void(Node*)> deleteNodeRecursive = [&](Node* node) {
            if (node != nullptr) {
                for (int i = 0; i < 4; ++i) {
                    deleteNodeRecursive(node->children[i]);
                    node->children[i] = nullptr; // Set to null after deleting
                }
                delete node;
            }
        };

        // Start deletion from each base node, but skip the base nodes themselves
        for (Node* baseNode : baseNodes) {
            for (int i = 0; i < 4; ++i) {
                deleteNodeRecursive(baseNode->children[i]);
                baseNode->children[i] = nullptr; // Set to null after deleting
            }
            baseNode->isSplit = false; // Reset the isSplit flag
        }

        // Remove non-base nodes from nodeMap
        for (auto it = nodeMap.begin(); it != nodeMap.end();) {
            if (std::find(baseNodes.begin(), baseNodes.end(), it->second) == baseNodes.end()) {
                it = nodeMap.erase(it);
            } else {
                ++it;
            }
        }
    }

    Node* getNodeById(int id) {
        auto it = nodeMap.find(id);
        if (it != nodeMap.end()) {
            return it->second;
        }
        return nullptr;
    }

    void splitCell(int firstId, const std::vector<int>& path) {
        Node* currentNode = getNodeById(firstId);
        if (!currentNode) {
            std::cerr << "Error: starting cell ID not found.\n";
            return;
        }

        splitRecursive(currentNode, path, 0, nodeMap);
    }

    // New function to split based on a sequence starting from the root
    void splitCell(std::vector<int>& sequence) {
        if (baseNodes.empty()) {
            std::cerr << "Error: Quadtree not initialized.\n";
            return;
        }

        // Start from the NW base node (or any of the base nodes)
        splitRecursive(baseNodes[0], sequence, 0, nodeMap);
    }

    template <typename... Args>
    void splitCell(Args... args) {
        std::vector<int> pathVec = {args...};
        if (pathVec.empty()) {
            std::cerr << "Error: No path specified for splitCell.\n";
            return;
        }
        // If the first argument is not a valid node ID, 
        // assume it's the start of a root-based sequence
        if (pathVec.size() > 1 && getNodeById(pathVec[0]) != nullptr) {
            int firstId = pathVec[0];
            pathVec.erase(pathVec.begin());
            splitCell(firstId, pathVec);
        } else {
            splitCell(pathVec);
        }
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

    int getNearestCell(sf::Vector2f position) {
        // Find the base cell that contains the position
        int col = static_cast<int>(position.x / cellSize);
        int row = static_cast<int>(position.y / cellSize);

        col = std::max(0, std::min(col, 1));
        row = std::max(0, std::min(row, 1));

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
        border.setSize({2 * cellSize, 2 * cellSize});
        border.setPosition(0, 0);
        border.setFillColor(sf::Color::Transparent);
        border.setOutlineThickness(2);
        border.setOutlineColor(sf::Color::Black);
        window.draw(border);
    }

    void drawPositions(sf::RenderWindow &window, const std::vector<sf::Vector2f>& positions) {
        float circleSize = 2;
        sf::CircleShape circle(circleSize);
        circle.setFillColor(sf::Color::Red);
        int scale = window.getSize().x / cellSize;
        for (auto pos : positions) {
            pos = sf::Vector2f((pos.x * scale), (pos.y * scale));
            pos -= sf::Vector2f(circleSize, circleSize);
            circle.setPosition(pos);
            window.draw(circle);
        }
    }

    int makeCell(sf::Vector2f position) {

        // Set the initial cell size, center and cell ID
        float currentCellSize = cellSize;
        sf::Vector2f currentCenter = {currentCellSize / 2, currentCellSize / 2};
        int cellID = 0b11; // Start with root cell ID 3

        for (int i = 0; i < maxDepth; ++i) {
            cellID <<= 2; // Shift left by 2 bits for the next depth level

            // Halve the cell size for each depth level
            currentCellSize /= 2.0f;

            // Determine the quadrant
            if (position.y >= currentCenter.y) { // Check if south of the center
                cellID += 2;
                currentCenter.y += currentCellSize / 2; // Adjust position relative to the new quadrant
            }
            else {
                currentCenter.y -= currentCellSize / 2; // Adjust position relative to the new quadrant
            }
            if (position.x >= currentCenter.x) { // Check if east of the center
                cellID += 1;
                currentCenter.x += currentCellSize / 2; // Adjust position relative to the new quadrant
            }
            else {
                currentCenter.x -= currentCellSize / 2; // Adjust position relative to the new quadrant
            }
        }
        // Print the cell ID
        // std::cout << "Cell ID: " << cellID << std::endl;

        return cellID;
    }

    std::vector<int> getSplitSequence(int cellID) {
        std::vector<int> splitSequence;
        for (int i = 0; i < maxDepth - 1; ++i) {
            
            splitSequence.push_back((cellID >> (2 * (maxDepth - i - 1))) & 3);
        }

        // Print the split sequence
        // std::cout << "Split sequence for cell " << cellID << ": ";
        // for (int i = 0; i < splitSequence.size(); ++i) {
        //     std::cout << splitSequence[i] << " ";
        // }
        // std::cout << std::endl;

        return splitSequence;
    }

    std::vector<std::vector<int>> getSplitSequences(std::vector<sf::Vector2f> positions) {
        std::vector<std::vector<int>> splitSequences;
        std::unordered_map<sf::Vector2f, int, Vector2fHash> positionToCellID;

        // Calculate and store cell IDs for unique positions
        for (const auto& pos : positions) {
            if (positionToCellID.find(pos) == positionToCellID.end()) {
                int cellID = makeCell(pos);
                positionToCellID[pos] = cellID;
            }
        }

        // Calculate split sequences using stored cell IDs
        for (const auto& pos : positions) {
            int cellID = positionToCellID[pos];
            splitSequences.push_back(getSplitSequence(cellID));
        }

        // Sort the split sequences
        std::sort(splitSequences.begin(), splitSequences.end());

        // Remove duplicates
        splitSequences.erase(std::unique(splitSequences.begin(), splitSequences.end()), splitSequences.end());

        return splitSequences;
    }

    void splitFromPositions(const std::vector<sf::Vector2f>& positions) {

        if(!positions.empty()) {
            
            std::vector<std::vector<int>> splitSequences = getSplitSequences(positions);

            for (const auto& seq : splitSequences) {
                splitCell(seq);
            }
        }
        else {
            std::cerr << "Error: No positions provided for split.\n";
        }
    }

    // Custom hash function for sf::Vector2f
    struct Vector2fHash {
        std::size_t operator()(const sf::Vector2f& v) const {
            std::size_t h1 = std::hash<float>()(v.x);
            std::size_t h2 = std::hash<float>()(v.y);
            return h1 ^ (h2 << 1); // Combine hashes
        }
    };

    std::vector<sf::Vector2f> generatePositions(int number) {
        std::vector<sf::Vector2f> positions;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.0, cellSize);

        for (int i = 0; i < number; ++i) {
            positions.push_back(sf::Vector2f(dis(gen), dis(gen)));
        }

        return positions;
    }

    // Move every position to the right by x pixels
    void movePositionsRight(std::vector<sf::Vector2f>& positions, float x) {
        for (auto& pos : positions) {
            pos.x += x;

            // If position is out of bounds, wrap around
            if (pos.x >= cellSize) {
                pos.x -= cellSize;
            }
        }
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
    sf::RenderWindow window(sf::VideoMode(1200, 1200), "Quadtree (2x2 Base Grid)");
    sf::Font font;
    if (!font.loadFromFile("/Library/Fonts/Arial Unicode.ttf")) {
        std::cerr << "Error: Failed to load font.\n";
        return -1;
    }

    // std::vector<sf::Vector2f> positions = {sf::Vector2f(395, 55), sf::Vector2f(54, 32)};

    Quadtree quadtree(1200, 8);

    sf::Clock clock;
    sf::Time timePerFrame = sf::seconds(1.f / 30.f); // 30 Hz
    sf::Time timeSinceLastUpdate = sf::Time::Zero;

    std::vector<sf::Vector2f> positions = quadtree.generatePositions(1);

    try {
        while (window.isOpen()) {
            timeSinceLastUpdate += clock.restart();

            while (timeSinceLastUpdate > timePerFrame) {
                timeSinceLastUpdate -= timePerFrame;

                sf::Event event;
                while (window.pollEvent(event)) {
                    if (event.type == sf::Event::Closed)
                        window.close();

                    if (event.type == sf::Event::KeyPressed) {
                        if (event.key.code == sf::Keyboard::Escape) {
                            window.close();
                        } 
                        if (event.key.code == sf::Keyboard::R) {
                            quadtree.clear();
                            positions.clear();
                        }
                    }
                    if (event.type == sf::Event::MouseButtonPressed) {
                        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                        int nearestCellId = quadtree.getNearestCell(mousePos);
                        sf::Vector2f cellCenter = quadtree.getCellCenter(nearestCellId);

                        std::cout << "Clicked at (" << mousePos.x << ", " << mousePos.y << ") -> Nearest cell: " << nearestCellId
                                << " with center at (" << cellCenter.x << ", " << cellCenter.y << ")\n";

                        std::cout << std::endl;

                        positions.push_back(mousePos);

                    }
                }
                // Clear the existing quadtree
                // quadtree.clear();
                quadtree.reset();

                // Move positions
                quadtree.movePositionsRight(positions, 1);

                // Split the quadtree based on new positions
                quadtree.splitFromPositions(positions);
            }

            window.clear(sf::Color::White);
            quadtree.draw(window, font);
            quadtree.drawPositions(window, positions); // Assuming you want to draw positions every frame
            window.display();
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
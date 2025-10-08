#pragma once

#include "exception.hpp"
#include <unordered_map>
#include <vector>
#include <memory>
#include <cassert>
#include <cstring>

template<typename T, T NULL_VALUE>
struct Node
{
  T parentId = NULL_VALUE;
  size_t index = 0;
  size_t numDescendents = 0;
};

template<typename T, T NULL_VALUE>
class Graph
{
  public:
    using TNode = Node<T, NULL_VALUE>;

    std::vector<T> dfs;
    std::vector<T> parents;
    std::unordered_map<T, std::unique_ptr<TNode>> nodes;

    Graph(T root)
    {
      dfs.push_back(root);
      parents.push_back(NULL_VALUE);
      nodes.insert({ root, std::make_unique<TNode>() });
    }

    bool hasItem(T itemId) const
    {
      return nodes.contains(itemId);
    }

    void addItem(T itemId, T parent)
    {
      auto& parentNode = *nodes.at(parent);

      incrementDescendentCount(parentNode);
      size_t index = parentNode.index + parentNode.numDescendents;
      size_t numEntities = dfs.size();

      dfs.push_back(0);
      parents.push_back(0);

      if (index < numEntities) {
        size_t remainder = numEntities - index;
        std::memmove(&dfs[index + 1], &dfs[index], remainder * sizeof(T));
        std::memmove(&parents[index + 1], &parents[index], remainder * sizeof(T));
        assert(dfs.size() == parents.size());
      }

      dfs[index] = itemId;
      parents[index] = parent;

      auto node = std::make_unique<TNode>(parent, index, 0);
      nodes.insert({ itemId, std::move(node) });

      for (size_t i = index; i < dfs.size(); ++i) {
        nodes.at(dfs[i])->index = i;
      }
    }

    void removeItem(T itemId)
    {
      assert(dfs.size() > 0);

      if (itemId == dfs[0]) {
        EXCEPTION("Cannot delete root node");
      }

      auto it = nodes.find(itemId);
      if (it == nodes.end()) {
        return;
      }

      auto& node = *it->second;
      size_t index = node.index;

      if (node.numDescendents > 0) {
        EXCEPTION("Cannot remove node with children; delete children first");
      }

      assert(dfs[index] == itemId);

      if (index + 1 < dfs.size()) {
        size_t remainder = dfs.size() - index - 1;
        std::memmove(&dfs[index], &dfs[index + 1], remainder * sizeof(T));
        std::memmove(&parents[index], &parents[index + 1], remainder * sizeof(T));
      }
    
      dfs.pop_back();
      parents.pop_back();

      auto& parentNode = *nodes.at(node.parentId);
      decrementDescendentCount(parentNode);

      for (size_t i = index; i < dfs.size(); ++i) {
        nodes.at(dfs[i])->index = i;
      }

      nodes.erase(it);
    }

  private:
    void incrementDescendentCount(TNode& node)
    {
      ++node.numDescendents;
      if (node.parentId != NULL_VALUE) {
        incrementDescendentCount(*nodes.at(node.parentId));
      }
    }

    void decrementDescendentCount(TNode& node)
    {
      --node.numDescendents;
      if (node.parentId != NULL_VALUE) {
        decrementDescendentCount(*nodes.at(node.parentId));
      }
    }
};

template<typename T, T NULL_VALUE>
using GraphPtr = std::unique_ptr<Graph<T, NULL_VALUE>>;

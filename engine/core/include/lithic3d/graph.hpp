#pragma once

#include "exception.hpp"
#include <unordered_map>
#include <vector>
#include <memory>
#include <cassert>
#include <cstring>

namespace lithic3d
{

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

      assert(dfs.size() == parents.size());
      assert(nodes.size() == dfs.size());
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

      dfs.push_back(NULL_VALUE);
      parents.push_back(NULL_VALUE);

      if (index < numEntities) {
        size_t remainder = numEntities - index;
        std::memmove(&dfs[index + 1], &dfs[index], remainder * sizeof(T));
        std::memmove(&parents[index + 1], &parents[index], remainder * sizeof(T));
      }

      dfs[index] = itemId;
      parents[index] = parent;

      auto node = std::make_unique<TNode>(parent, index, 0);
      nodes.insert({ itemId, std::move(node) });

      for (size_t i = index; i < dfs.size(); ++i) {
        nodes.at(dfs[i])->index = i;
      }

      assert(dfs.size() == parents.size());
      assert(nodes.size() == dfs.size());
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
      auto numDescendents = node.numDescendents;
      auto parentId = node.parentId;
      size_t index = node.index;

      assert(dfs[index] == itemId);

      for (size_t i = 0; i < numDescendents + 1; ++i) {
        nodes.erase(dfs[index + i]);
      }

      if (index + numDescendents < dfs.size()) {
        size_t remainder = dfs.size() - (index + 1 + numDescendents);
        std::memmove(&dfs[index], &dfs[index + 1 + numDescendents], remainder * sizeof(T));
        std::memmove(&parents[index], &parents[index + 1 + numDescendents], remainder * sizeof(T));
      }
    
      dfs.resize(dfs.size() - (numDescendents + 1));
      parents.resize(parents.size() - (numDescendents + 1));

      auto& parentNode = *nodes.at(parentId);
      decrementDescendentCount(parentNode, numDescendents + 1);

      for (size_t i = index; i < dfs.size(); ++i) {
        nodes.at(dfs[i])->index = i;
      }

      assert(dfs.size() == parents.size());
      assert(nodes.size() == dfs.size());
    }

  private:
    void incrementDescendentCount(TNode& node)
    {
      ++node.numDescendents;
      if (node.parentId != NULL_VALUE) {
        incrementDescendentCount(*nodes.at(node.parentId));
      }
    }

    void decrementDescendentCount(TNode& node, size_t n)
    {
      node.numDescendents -= n;
      if (node.parentId != NULL_VALUE) {
        decrementDescendentCount(*nodes.at(node.parentId), n);
      }
    }
};

template<typename T, T NULL_VALUE>
using GraphPtr = std::unique_ptr<Graph<T, NULL_VALUE>>;

} // namespace lithic3d

#pragma once

#include "exception.hpp"
#include <unordered_map>
#include <vector>
#include <memory>
#include <cassert>
#include <cstring>

namespace lithic3d
{

template<typename T_KEY, T_KEY NULL_KEY>
struct Node
{
  T_KEY parentKey = NULL_KEY;
  size_t index = 0;
  size_t numDescendents = 0;
};

template<typename T_KEY, typename T_VAL, T_KEY NULL_KEY>
class Graph
{
  public:
    struct Entry
    {
      T_KEY key;
      T_VAL value;
    };

    using TNode = Node<T_KEY, NULL_KEY>;

    std::vector<Entry> dfs;
    std::vector<T_KEY> parents;
    std::unordered_map<T_KEY, std::unique_ptr<TNode>> nodes;

    Graph(T_KEY rootKey, T_VAL rootVal)
    {
      dfs.push_back({ rootKey, rootVal });
      parents.push_back(NULL_KEY);
      nodes.insert({ rootKey, std::make_unique<TNode>() });

      assert(dfs.size() == parents.size());
      assert(nodes.size() == dfs.size());
    }

    bool hasItem(T_KEY itemKey) const
    {
      return nodes.contains(itemKey);
    }

    void addItem(T_KEY itemKey, T_VAL item, T_KEY parent)
    {
      auto& parentNode = *nodes.at(parent);

      incrementDescendentCount(parentNode);
      size_t index = parentNode.index + parentNode.numDescendents;
      size_t numEntities = dfs.size();

      dfs.push_back(Entry{});
      parents.push_back(NULL_KEY);

      if (index < numEntities) {
        size_t remainder = numEntities - index;
        std::memmove(&dfs[index + 1], &dfs[index], remainder * sizeof(Entry));
        std::memmove(&parents[index + 1], &parents[index], remainder * sizeof(T_KEY));
      }

      dfs[index] = Entry{itemKey, item};
      parents[index] = parent;

      auto node = std::make_unique<TNode>(parent, index, 0);
      nodes.insert({ itemKey, std::move(node) });

      for (size_t i = index; i < dfs.size(); ++i) {
        nodes.at(dfs[i].key)->index = i;
      }

      assert(dfs.size() == parents.size());
      assert(nodes.size() == dfs.size());
    }

    void removeItem(T_KEY itemKey)
    {
      assert(dfs.size() > 0);

      //if (itemKey == dfs[0]) {
      //  EXCEPTION("Cannot delete root node");
      //}

      auto it = nodes.find(itemKey);
      if (it == nodes.end()) {
        return;
      }

      auto& node = *it->second;
      auto numDescendents = node.numDescendents;
      auto parentKey = node.parentKey;
      size_t index = node.index;

      //assert(dfs[index] == itemKey);

      for (size_t i = 0; i < numDescendents + 1; ++i) {
        nodes.erase(dfs[index + i].key);
      }

      if (index + numDescendents + 1 < dfs.size()) {
        size_t remainder = dfs.size() - (index + 1 + numDescendents);
        std::memmove(&dfs[index], &dfs[index + 1 + numDescendents], remainder * sizeof(Entry));
        std::memmove(&parents[index], &parents[index + 1 + numDescendents], remainder * sizeof(T_KEY));
      }
    
      dfs.resize(dfs.size() - (numDescendents + 1));
      parents.resize(parents.size() - (numDescendents + 1));

      auto& parentNode = *nodes.at(parentKey);
      decrementDescendentCount(parentNode, numDescendents + 1);

      for (size_t i = index; i < dfs.size(); ++i) {
        nodes.at(dfs[i].key)->index = i;
      }

      assert(dfs.size() == parents.size());
      assert(nodes.size() == dfs.size());
    }

  private:
    void incrementDescendentCount(TNode& node)
    {
      ++node.numDescendents;
      if (node.parentKey != NULL_KEY) {
        incrementDescendentCount(*nodes.at(node.parentKey));
      }
    }

    void decrementDescendentCount(TNode& node, size_t n)
    {
      node.numDescendents -= n;
      if (node.parentKey != NULL_KEY) {
        decrementDescendentCount(*nodes.at(node.parentKey), n);
      }
    }
};

template<typename T_KEY, typename T_VAL, T_KEY NULL_KEY>
using GraphPtr = std::unique_ptr<Graph<T_KEY, T_VAL, NULL_KEY>>;

} // namespace lithic3d

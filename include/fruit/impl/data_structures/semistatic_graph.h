/*
 * Copyright 2014 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SEMISTATIC_GRAPH_H
#define SEMISTATIC_GRAPH_H

#include "semistatic_map.h"

#ifdef FRUIT_EXTRA_DEBUG
#include <iostream>
#endif

namespace fruit {
namespace impl {

// The alignas ensures that a SemistaticGraphInternalNodeId* always has 0 in the low-order bit.
struct alignas(2) alignas(alignof(std::size_t)) SemistaticGraphInternalNodeId {
  std::size_t id;
  
  bool operator==(const SemistaticGraphInternalNodeId& x) const {
    return id == x.id;
  }
  
  bool operator<(const SemistaticGraphInternalNodeId& x) const {
    return id < x.id;
  }
};

/**
 * A direct graph implementation where most of the graph is fixed at construction time, but a few nodes and edges can be added
 * later.
 * 
 * Also, nodes can either be normal nodes or terminal nodes. Terminal nodes can't have outgoing edges. Note that a node with no
 * outgoing edges may or may not be marked as terminal.
 * 
 * While insertion of elements after construction is supported, inserting or changing the neighbors of more than O(1) nodes
 * after construction will raise the cost of any further operations to more than O(1).
 * 
 * Even though adding nodes/edges after construction is inefficient, it is efficient to turn non-terminal nodes into terminal ones
 * (and therefore removing all the outgoing edges from the node) after construction.
 */
template <typename NodeId, typename Node>
class SemistaticGraph {
private:
  using InternalNodeId = SemistaticGraphInternalNodeId;
  
  // The node data for nodeId is in nodes[nodeIndexMap.at(nodeId)].
  // To avoid hash table lookups, the edges in edgesStorage are stored as indexes of `nodes' instead of as NodeIds.
  // nodeIndexMap contains all known NodeIds, including ones known only due to an outgoing edge ending there from another node.
  SemistaticMap<NodeId, InternalNodeId> nodeIndexMap;
  
  struct NodeData {
#ifdef FRUIT_EXTRA_DEBUG
    NodeId key;
#endif
    
    Node node;
    // If edgesBeginOffset==0, this is a terminal node.
    // If edgesBeginOffset==1, this node doesn't exist, it's just referenced by another node.
    // Otherwise, reinterpret_cast<InternalNodeId*>(edgesBegin) is the beginning of the edges range.
    std::uintptr_t edgesBegin;
  };
  
  std::size_t firstUnusedIndex;
  
  std::vector<NodeData> nodes;
  
  // Stores vectors of dependencies as contiguous chunks of elements.
  // The NormalizedBindingData elements in typeRegistry contain indexes into this vector.
  // The first element is unused.
  std::vector<InternalNodeId> edgesStorage;
  
  InternalNodeId getOrAllocateInternalNodeId(NodeId nodeId);
  
  // TODO: Remove.
  void printEdgesBegin(std::ostream& os, std::uintptr_t edgesBegin);
    
public:
  
  class edge_iterator;
  
  class node_iterator {
  private:
    typename std::vector<NodeData>::iterator itr;
    
    friend class SemistaticGraph<NodeId, Node>;
    
    node_iterator(typename std::vector<NodeData>::iterator itr);
    
  public:
    Node& getNode();
    
    bool isTerminal();
    
    // Turns the node into a terminal node, also removing all the deps.
    void setTerminal();
  
    // Assumes !isTerminal().
    // neighborsEnd() is NOT provided/stored for efficiency, the client code is expected to know the number of neighbors.
    edge_iterator neighborsBegin();
    
    bool operator==(const node_iterator&) const;
  };
  
  class const_node_iterator {
  private:
    typename std::vector<NodeData>::const_iterator itr;
    
    friend class SemistaticGraph<NodeId, Node>;
    
    const_node_iterator(typename std::vector<NodeData>::const_iterator itr);
    
  public:
    const Node& getNode();
    
    bool isTerminal();
    
    // Turns the node into a terminal node, also removing all the deps.
    void setTerminal();
  
    // Assumes !isTerminal().
    // neighborsEnd() is NOT provided/stored for efficiency, the client code is expected to know the number of neighbors.
    edge_iterator neighborsBegin(SemistaticGraph<NodeId, Node>& graph);
    
    bool operator==(const const_node_iterator&) const;
  };
  
  class edge_iterator {
  private:
    // Iterator on edgesStorage.
    InternalNodeId* itr;
    
    friend class SemistaticGraph<NodeId, Node>;
    friend class SemistaticGraph<NodeId, Node>::node_iterator;
    
    edge_iterator(InternalNodeId* itr);

  public:
    node_iterator getNodeIterator(SemistaticGraph<NodeId, Node>& graph);
    
    void operator++();
    
    // Equivalent to i times operator++ followed by getNodeIterator(graph).
    node_iterator getNodeIterator(std::size_t i, SemistaticGraph<NodeId, Node>& graph);
  };
  
  // Constructs an *invalid* graph (as if this graph was just moved from).
  SemistaticGraph() = default;
  
  // A value x obtained dereferencing a NodeIter::value_type must support the following operations:
  // * x.getId(), returning a NodeId
  // * x.getValue(), returning a Node
  // * x.isTerminal(), returning a bool
  // * x.getEdgesBegin() and x.getEdgesEnd(), that if !x.isTerminal() define a range of values of type NodeId (the outgoing edges).
  // 
  // This constructor is *not* defined in semistatic_graph.templates.h, but only in semistatic_graph.cc.
  // All instantiations must have a matching instantiation in semistatic_graph.cc.
  template <typename NodeIter>
  SemistaticGraph(NodeIter first, NodeIter last);
  
  SemistaticGraph(SemistaticGraph&&) = default;
  SemistaticGraph(const SemistaticGraph&) = delete;
  
  // Creates a copy of x with the additional nodes in [first, last). The requirements on NodeIter as the same as for the 2-arg
  // constructor.
  // The nodes in [first, last) must NOT be already in x, but can be neighbors of nodes in x.
  // The new graph will share data with `x', so must be destroyed before `x' is destroyed.
  // Also, after this is called, `x' must not be modified until this object has been destroyed.
  template <typename NodeIter>
  SemistaticGraph(const SemistaticGraph& x, NodeIter first, NodeIter last);
  
  SemistaticGraph& operator=(const SemistaticGraph&) = delete;
  SemistaticGraph& operator=(SemistaticGraph&&) = default;
  
  node_iterator end();
  const_node_iterator end() const;
  
  // Precondition: `nodeId' must exist in the graph.
  // Unlike std::map::at(), this yields undefined behavior if the precondition isn't satisfied (instead of throwing).
  node_iterator at(NodeId nodeId);
  
  // Prefer using at() when possible, this is slightly slower.
  // Returns end() if the node ID was not found.
  node_iterator find(NodeId nodeId);
  const_node_iterator find(NodeId nodeId) const;
    
  // Changes the node with ID nodeId (that must exist) to a terminal node.
  void changeNodeToTerminal(NodeId nodeId);
  
#ifdef FRUIT_EXTRA_DEBUG
  // Emits a runtime error if some node was not created but there is an edge pointing to it.
  void checkFullyConstructed();
#endif
};

} // namespace impl
} // namespace fruit

#include "semistatic_graph.defn.h"

// semistatic_graph.templates.h is not included here to limit the transitive includes. Include it explicitly (in .cpp files).

#endif // SEMISTATIC_GRAPH_H

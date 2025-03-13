#pragma once

#include <unordered_map>
#include <string_view>
#include <memory> // std::unique_ptr
#include <vector>
#include <utility> // std::pair
#include <set>


class SuffixTree {
public:
    // an abstract node class as the base class for LeafNode and InternalNode,
    // each node includes the node and the edge leading to the node,
    // the string label for the edge is represented as 
    // a pair of indices [start, end] of the input text
    // (note that the length of an edge is computed as end-start rather than 
    //  end-start+1 because `end` is the actual end index plus one)
    class Node {
    public:
        uint32_t start;
        virtual uint32_t edge_length() = 0;
        // ("= 0" for pure virtual function) 
        Node(uint32_t i): start(i) {}
    };

    class LeafNode : public Node {
    public:
        // use a pointer for fast leaf end index updates
        // (see `global_end`, a private field in SuffixTree below)
        uint32_t* end_ptr;
        uint32_t edge_length() override; 
        LeafNode(uint32_t i, uint32_t* j): Node(i), end_ptr(j) {}
        virtual ~LeafNode() {};
    };

    class InternalNode : public Node {
    public:
        uint32_t end;
        uint32_t edge_length() override;
    
        // split the child nodes into internal and leaf nodes
        std::unordered_map<char, InternalNode*> internal_children;
        std::unordered_map<char, LeafNode*> leaf_children;
    
        InternalNode* suffix_link;
        // use vector instead of set for faster traversal (at the cost of slower construction)
        std::vector<InternalNode*> weiner_links;

        // net frequency value stored at each internal node
        uint32_t nf;

        InternalNode(uint32_t i, uint32_t j): 
            Node(i), end(j), 
            suffix_link(nullptr), weiner_links({}),
            nf(0) {}
        virtual ~InternalNode();
    };

private:
    // the input text
    std::string_view txt;

public:
    // todo: write an internal node iterator
    // at the moment we are exposing the root node to allow traversing the nodes
    std::unique_ptr<InternalNode> root;

private:
    // ------------------------ the following are used in Ukkonen's algorithm ------------------------

    // in each phase, a pointer to the node that needs a suffix link
    InternalNode* need_link;
    // used to update `end` for the leaf nodes ("once a leaf, always a leaf")
    uint32_t global_end;
    // in each phase, remainder = the number of suffixes that need to be inserted explicitly:
    //      i.e., suffixes remaining from previous phases and txt[k] from the k-th (current) phase,
    //      i.e., suffixes that are not automatically updated by global_end
    uint32_t remainder;
    // active point: specified by a triple (active_node, active_edge, active_length)
    // indicating from where we start inserting a new suffix (the start of next phase/extension)
    // (note that a LeafNode or InternalNode contains the edge leading to the node,
    //  but active_edge is an outgoing edge of active_node)
    InternalNode* active_node;
    uint32_t active_edge; // the corresponding character is txt[active_edge]
    uint32_t active_length;

    void extend(uint32_t k);
    void add_links(InternalNode* node);
    // ------------------------------------------------------------------------------------------------

public:
    // constructor
    SuffixTree(std::string_view _txt);

    std::pair<InternalNode*, uint32_t> find_internal_node(std::string_view s);

    uint32_t single_nf(std::string_view s);

    void all_nf();

};

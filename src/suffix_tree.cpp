#include "./suffix_tree.hpp"

#include <assert.h>
#include <iostream>
#include <functional>
#include <algorithm> // std::find, std::min
#include <unordered_set>
#include <iomanip> 
#include <fstream>



// ==========================================================================================
//                              net frequency related
// ==========================================================================================



// compute the net frequency of a single substring s
uint32_t SuffixTree::single_nf(std::string_view s) {
    auto [S, left_len_S] = find_internal_node(s);
    // s doesn't exist, or is unique, or is non-branching
    if (S == nullptr || left_len_S != 0) return 0;

    // initialise the net frequency to the number of unique right extensions of s
    uint32_t nf = (uint32_t)S->leaf_children.size();
    // no leaf children
    if (nf == 0) return 0;
    // for each repeated left extension xS
    for (const auto& xS : S->weiner_links) {
        for (const auto& [y, _] : xS->leaf_children) {
            // if Sy is a leaf
            if (S->leaf_children.contains(y)){
                nf--;
            }
        }
    }
    return nf;
}


// compute the net frequencies for all the branching substrings
void SuffixTree::all_nf() {

    // a recursive function that processes each internal node
    std::function<void(SuffixTree::InternalNode*)> process;
    process = [&process](SuffixTree::InternalNode* xS) {
        if (!xS->leaf_children.empty()) {
            xS->nf += xS->leaf_children.size();
            auto S = xS->suffix_link;
            for (auto& [y, _] : xS->leaf_children) {
                if (S->leaf_children.contains(y)) {
                    S->nf--;
                }
            }
        }
        for (auto& [_, child] : xS->internal_children) {
            process(child);
        }
    };

    // a recursive function that prints each string of positive NF
    std::function<void(SuffixTree::InternalNode*, uint32_t, uint32_t)> report;
    report = [&report, this](SuffixTree::InternalNode* S, uint32_t start_pos, uint32_t string_depth) {
        if (S->nf) {
            std::cout << txt.substr(start_pos, string_depth)
                      << '\t' << S->nf << std::endl;
        }
        for (auto& [_, child] : S->internal_children) {
            report(child, start_pos, string_depth + child->edge_length());
        }
    };

    for (auto& [_, xS] : root.get()->internal_children) {
        process(xS);
    }

    for (auto& [_, S] : root.get()->internal_children) {
        report(S, S->start, S->edge_length());
    }
}



/*
find the internal node corresponding to substring s, 
also return the number of characters left on the last edge if s is non-branching 
(i.e., the search doesn't end at a node)
(e.g., when searching for "anan" in "banana$", two character are left on the edge;
 on the other hand, the search for "ana" ends at a node so 0 characters are returned)

caveat: when the first return value is nullptr, 
        the second return value can only be 0 or 1, representing the true frequency of s:
b) return {nullptr, 0} if s doesn't exist, 
a) return {nullptr, 1} if s is unique (its corresponding node is a leaf node)
*/
std::pair<SuffixTree::InternalNode*, uint32_t> SuffixTree::find_internal_node(std::string_view s) {
    auto node = root.get(); // start from the root
    uint32_t i = 0; // at each iteration, search for s[i:]
    while (true) {
        // all characters in s have been matched: s exists and its is an internal node
        if (i >= s.size()) return { node, i - s.size() };

        auto internal_pair = node->internal_children.find(s[i]);
        // if the traversal leads to an internal node
        if (internal_pair != node->internal_children.end()) { 
            auto internal_child = internal_pair->second;
            // the number of characters need to be compared for this edge
            auto len = std::min(internal_child->edge_length(), (uint32_t)s.size() - i);
            
            // match: go to this internal node
            if (s.substr(i, len) == txt.substr(internal_child->start, len)) {
                node = internal_child;
                i += node->edge_length();
            }
            else { // mismatch: s doesn't exist
                return {nullptr, 0};
            }
        }
        else { // the traversal leads to a leaf node
            // s corresponds to an leaf node
            if (node->leaf_children.contains(s[i])) {
                return {nullptr, 1};
            }
            else { // s doesn't exist
                return {nullptr, 0};
            }
        }
    }
    assert(false);
}





// ==========================================================================================
//                             Ukkkonen's algorithm related
// ==========================================================================================

/*

high-level idea:
 - the algorithm consists of n phases; 
 - in the i-th phase, build the i-th implicit suffix tree incrementally using the (i-1)-th implicit suffix tree;
 - the i-th phase involves i suffix extensions, one for each of the i suffixes of txt[0...i];
 - in the j-th extension of the i-th phase, first find the end of the path from the root to txt[j..i-1], then add the new character txt[i] to extend the path if needed

suffix extension rules:
    rule 1 - path ends at a leaf
    rule 2 - path doesn't end at a leaf and the next character doesn't exist
        rule 2a - within an edge
        rule 2b - at a node
    rule 3 - path doesn't end at a leaf and the next character exists

trick 1 - skip/count trick
trick 2 - space efficient representation of edge labels
trick 3 - rule 3 is a show stopper
trick 4 - rapid leaf extension (once a leaf, always a leaf)

(resources: https://stackoverflow.com/a/9513423, https://brenden.github.io/ukkonen-animation/)
*/

void SuffixTree::extend(uint32_t k) {
    need_link = nullptr;
    remainder++;

    while (remainder > 0) {
        if (active_length == 0) { // currently right at a node
            active_edge = k;
        }
        // if leaf and internal children were not split,
        // `Node* node = active_node->children.at(txt[active_edge]);`
        // now we need figure out if `node` is a leaf or an internal node
        auto node_leaf_pair = active_node->leaf_children.find(txt[active_edge]);
        auto node_internal_pair = active_node->internal_children.find(txt[active_edge]);
        bool is_leaf = (node_leaf_pair != active_node->leaf_children.end());
        bool is_internal = (node_internal_pair != active_node->internal_children.end());
        assert(!(is_leaf && is_internal));

        // rule 2b
        if (!is_leaf && !is_internal) { // `node` doesn't exist   
            LeafNode* leaf = new LeafNode(k, &global_end);
            active_node->leaf_children[txt[active_edge]] = leaf;
            add_links(active_node);
        }
        else {
            // trick 1
            auto len = is_leaf ? node_leaf_pair->second->edge_length() : node_internal_pair->second->edge_length();

            // keep walking down until len is strictly greater than active_length
            if (active_length >= len) {
                assert(is_internal);
                active_edge += len;
                active_length -= len;
                active_node = node_internal_pair->second;
                // while walking down we might also need to handle the previous situations, so we continue
                continue;
            }
            // rule 3
            auto prev_start = is_leaf ? node_leaf_pair->second->start : node_internal_pair->second->start;
            if (txt[prev_start + active_length] == txt[k]) {
                active_length++;
                add_links(active_node);
                // trick 3
                break;
            }
            /*
                rule 2a
            
                    /                       /
                   @ active_node           @ active_node
                   |                       |
                   /             ==>       @ internal_node
                  /                       / \
                  @ node                 /   @ leaf
                 /                      @ node
                                       /
            */
            // split the edge
            // (could combine the following two cases but excessive use of ternary is needed)
            if (is_leaf) {
                auto node = node_leaf_pair->second;
                node->start += active_length;
                
                InternalNode* internal_node = new InternalNode(prev_start, node->start);
                LeafNode* leaf = new LeafNode(k, &global_end);
                internal_node->leaf_children[txt[k]] = leaf;
                
                active_node->internal_children[txt[active_edge]] = internal_node;
                internal_node->leaf_children[txt[node->start]] = node;
                // `node` becomes a leaf child of 'internal_node',
                // which means it's no longer a leaf child of `active_node`
                // (average case O(1) for `erase`)
                active_node->leaf_children.erase(node_leaf_pair);
                add_links(internal_node);
            }
            else if (is_internal) {
                auto node = node_internal_pair->second;
                node->start += active_length;
                
                InternalNode* internal_node = new InternalNode(prev_start, node->start);
                LeafNode* leaf = new LeafNode(k, &global_end);
                internal_node->leaf_children[txt[k]] = leaf;

                active_node->internal_children[txt[active_edge]] = internal_node;
                internal_node->internal_children[txt[node->start]] = node;
                // `node` becomes an internal child of 'internal_node',
                // which means it's no longer an internal child of `active_node`,
                // but we don't need to do anything because it's replaced by `internal_node` already
                add_links(internal_node);
            }
        }
        remainder--;

        // after an insertion from root 
        if (active_node == root.get() && active_length > 0) {
            active_length--;
            // shift active_edge to the first character of the next suffix we will insert
            active_edge = k - remainder + 1;
        }
        else {
            // follow the suffix link if possible
            if (active_node->suffix_link != nullptr) {
                active_node = active_node->suffix_link;
            }
            else {
                active_node = root.get();
            }
        }
    }

    global_end++;
}

void SuffixTree::add_links(InternalNode* node) {
    // add a suffix link from need_link to node
    // add a weiner link from node to need_link
    if (need_link != nullptr) {
        need_link->suffix_link = node;
        auto wls = node->weiner_links;
        if (std::find(wls.begin(), wls.end(), need_link) == wls.end()) {
            node->weiner_links.push_back(need_link);
        }
    }
    need_link = node;
}




// ==========================================================================================
//                                  other functions
// ==========================================================================================


// internal node destructor
SuffixTree::InternalNode::~InternalNode() {
    for (auto& [_, child] : internal_children) {
        delete child;
    }
    for (auto& [_, child] : leaf_children) {
        delete child;
    }
}

// suffix tree constructor
SuffixTree::SuffixTree(std::string_view _txt) :
    txt(_txt),
    root(std::make_unique<InternalNode>(0, 0)),
    need_link(nullptr),
    global_end(0),
    remainder(0),
    active_node(root.get()),
    active_edge(0),
    active_length(0) {
    for (uint32_t k = 0; k < txt.size(); k++) {
        extend(k);
    }
}

uint32_t SuffixTree::LeafNode::edge_length() {
    return *end_ptr - start;
}

uint32_t SuffixTree::InternalNode::edge_length() {
    return end - start;
}

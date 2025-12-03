// yoga_raii.hpp
// Minimalist RAII wrapper for Yoga — NO exceptions, NO strings, NO debug names.
// All invalid operations are silently ignored (no-op).
// Designed for safe use in immediate-mode UIs (e.g., ImGui), crash-free even on misuse.

#pragma once

#include <memory>
#include <vector>
#include <algorithm>
#include <yoga/Yoga.h>

namespace yoga_raii {

class Node : public std::enable_shared_from_this<Node> {
public:
    using Ptr = std::shared_ptr<Node>;

    // Factory: always create through shared_ptr
    static Ptr create() {
        return std::make_shared<Node>();
    }

    // Constructor: Yoga node creation never throws
    explicit Node() {
        node_ = YGNodeNew();
    }

    // Disable copy and move — ensures a single RAII owner
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;

    // Destructor: safely releases Yoga resources and detaches from parent
    ~Node() {
        // Remove this node from its parent wrapper (if any)
        if (auto p = parent_.lock()) {
            p->removeChildInternal(this);
        }

        // Invalidate children so they do not double-free Yoga nodes later
        for (auto &child : children_) {
            if (child) child->invalidateNode();
        }
        children_.clear();

        // Free the Yoga subtree
        if (node_) {
            YGNodeFreeRecursive(node_);
            node_ = nullptr;
        }
    }

    // ---------------- Tree management ----------------

    // Adds a child node to this node, maintaining proper parent relationships
    void addChild(const Ptr &child) {
        if (!node_ || !child || !child->node_) return;

        // Detach from previous parent, if any
        if (auto oldp = child->parent_.lock()) {
            oldp->removeChild(child);
        }

        // Avoid duplicates
        auto it = std::find_if(children_.begin(), children_.end(),
            [&](const Ptr &p){ return p.get() == child.get(); });
        if (it != children_.end()) return;

        // Insert into Yoga tree and C++ child list
        YGNodeInsertChild(node_, child->node_, static_cast<uint32_t>(children_.size()));
        children_.push_back(child);
        child->parent_ = shared_from_this();
    }

    // Removes a child via shared_ptr
    void removeChild(const Ptr &child) {
        if (!child) return;
        removeChildInternal(child.get());
        child->parent_.reset();
    }

    // Internal version of removeChild, uses raw pointer
    void removeChildInternal(Node *raw) {
        if (!raw || !node_) return;

        auto it = std::find_if(children_.begin(), children_.end(),
            [&](const Ptr &p){ return p.get() == raw; });

        if (it == children_.end()) return;

        // Remove Yoga child pointer if still valid
        if (raw->node_) {
            YGNodeRemoveChild(node_, raw->node_);
        }

        children_.erase(it);
    }

    // Parent accessor (returns nullptr if no parent)
    Ptr parent() const { return parent_.lock(); }

    // Number of children
    size_t childrenCount() const { return children_.size(); }

    // ---------------- Style setters ----------------
    // Every setter checks node_ — safe to call on invalid or destroyed wrappers.

    void setWidth(float v)               { if(node_) YGNodeStyleSetWidth(node_, v); }
    void setWidthPercent(float v)        { if(node_) YGNodeStyleSetWidthPercent(node_, v); }
    void setHeight(float v)              { if(node_) YGNodeStyleSetHeight(node_, v); }
    void setHeightPercent(float v)       { if(node_) YGNodeStyleSetHeightPercent(node_, v); }

    void setFlexGrow(float v)            { if(node_) YGNodeStyleSetFlexGrow(node_, v); }
    void setFlexShrink(float v)          { if(node_) YGNodeStyleSetFlexShrink(node_, v); }
    void setFlexBasis(float v)           { if(node_) YGNodeStyleSetFlexBasis(node_, v); }
    void setFlexBasisPercent(float v)    { if(node_) YGNodeStyleSetFlexBasisPercent(node_, v); }

    void setFlexDirection(YGFlexDirection v)   { if(node_) YGNodeStyleSetFlexDirection(node_, v); }
    void setJustifyContent(YGJustify v)        { if(node_) YGNodeStyleSetJustifyContent(node_, v); }
    void setAlignItems(YGAlign v)              { if(node_) YGNodeStyleSetAlignItems(node_, v); }
    void setAlignSelf(YGAlign v)               { if(node_) YGNodeStyleSetAlignSelf(node_, v); }

    void setMargin(YGEdge e, float v)          { if(node_) YGNodeStyleSetMargin(node_, e, v); }
    void setMarginPercent(YGEdge e, float v)   { if(node_) YGNodeStyleSetMarginPercent(node_, e, v); }
    void setMarginAuto(YGEdge e)               { if(node_) YGNodeStyleSetMarginAuto(node_, e); }

    void setPadding(YGEdge e, float v)         { if(node_) YGNodeStyleSetPadding(node_, e, v); }
    void setPaddingPercent(YGEdge e, float v)  { if(node_) YGNodeStyleSetPaddingPercent(node_, e, v); }

    void setWidthAuto()                        { if(node_) YGNodeStyleSetWidthAuto(node_); }
    void setHeightAuto()                       { if(node_) YGNodeStyleSetHeightAuto(node_); }

    void setMinWidth(float v)                  { if(node_) YGNodeStyleSetMinWidth(node_, v); }
    void setMaxWidth(float v)                  { if(node_) YGNodeStyleSetMaxWidth(node_, v); }
    void setMinHeight(float v)                 { if(node_) YGNodeStyleSetMinHeight(node_, v); }
    void setMaxHeight(float v)                 { if(node_) YGNodeStyleSetMaxHeight(node_, v); }

    // ---------------- Layout ----------------

    // Computes layout for this node and all descendants
    // Width/height may be YGUndefined to allow Yoga to resolve automatically.
    void calculateLayout(float w = YGUndefined, float h = YGUndefined, YGDirection dir = YGDirectionLTR) {
        if (!node_) return;
        YGNodeCalculateLayout(node_, w, h, dir);
    }

    // Layout getters — always safe (return 0 if node_ is invalid)
    float getLayoutLeft() const   { return node_ ? YGNodeLayoutGetLeft(node_) : 0; }
    float getLayoutTop() const    { return node_ ? YGNodeLayoutGetTop(node_) : 0; }
    float getLayoutWidth() const  { return node_ ? YGNodeLayoutGetWidth(node_) : 0; }
    float getLayoutHeight() const { return node_ ? YGNodeLayoutGetHeight(node_) : 0; }
    float getLayoutRight() const   { return getLayoutLeft()+getLayoutWidth(); }
    float getLayoutBottom() const   { return getLayoutTop()+getLayoutHeight(); }

    // compute absolute top-left by summing left/top up the parent chain
    ImVec2 getAbsolutePos() {
        ImVec2 pos = {0.0f, 0.0f};

        auto cur = shared_from_this();
        while (cur) {
            pos.x += cur->getLayoutLeft();
            pos.y += cur->getLayoutTop();
            cur = cur->parent(); // wrapper has parent()
        }
        return pos;
    }

    // compute absolute rect (min and max)
    void getAbsoluteRect(ImVec2 &outMin, ImVec2 &outMax) {
        if (!node_) {
            outMin = outMax = ImVec2{0,0};
            return;
        }
        ImVec2 pos = getAbsolutePos();
        float w = getLayoutWidth();
        float h = getLayoutHeight();
        outMin = pos;
        outMax = ImVec2{ pos.x + w, pos.y + h };
    }

    // Access underlying Yoga node (use with care)
    YGNodeRef raw() const { return node_; }

private:
    friend class std::shared_ptr<Node>;

    // Called when parent is destroyed — prevents double free
    void invalidateNode() { node_ = nullptr; }

    YGNodeRef node_ = nullptr;         // The Yoga node we manage
    std::vector<Ptr> children_;        // Strong references to children
    std::weak_ptr<Node> parent_;       // Weak reference to parent
};

} // namespace yoga_raii

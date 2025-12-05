// yoga_raii.hpp
// Minimalist RAII wrapper for Yoga — NO exceptions.
// Adds optional node names, hierarchical lookup (getById "a/b/c" with leading '/' support),
// root discovery (findRoot) and recursive descendant search (getByIdRecursive).
// All invalid operations are silently ignored (no-op).

#pragma once

#include <memory>
#include <vector>
#include <algorithm>
#include <string>
#include <stack>
#include <unordered_map>
#include <functional>
#include <cmath>
#include <iostream>
#include <yoga/Yoga.h>
#include <glm/glm.hpp>

namespace yoga_raii {


    // ---------------- Debug printing ----------------
    // Helpers: convert Yoga enums to readable strings
    static const char* flexDirectionToString(YGFlexDirection d) {
        switch (d) {
            case YGFlexDirectionColumn: return "column";
            case YGFlexDirectionColumnReverse: return "column-reverse";
            case YGFlexDirectionRow: return "row";
            case YGFlexDirectionRowReverse: return "row-reverse";
            default: return "unknown";
        }
    }
    static const char* justifyToString(YGJustify j) {
        switch (j) {
            case YGJustifyFlexStart: return "flex-start";
            case YGJustifyCenter: return "center";
            case YGJustifyFlexEnd: return "flex-end";
            case YGJustifySpaceBetween: return "space-between";
            case YGJustifySpaceAround: return "space-around";
            case YGJustifySpaceEvenly: return "space-evenly";
            default: return "unknown";
        }
    }
    static const char* alignToString(YGAlign a) {
        switch (a) {
            case YGAlignAuto: return "auto";
            case YGAlignFlexStart: return "flex-start";
            case YGAlignCenter: return "center";
            case YGAlignFlexEnd: return "flex-end";
            case YGAlignStretch: return "stretch";
            case YGAlignBaseline: return "baseline";
            default: return "unknown";
        }
    }
    static const char* unitToString(YGUnit u) {
        switch (u) {
            case YGUnitUndefined: return "undefined";
            case YGUnitPoint:     return "px";
            case YGUnitPercent:   return "%";
            case YGUnitAuto:      return "auto";
            default: return "unknown";
        }
    }

    // Print a YGValue in the form "123px" or "50%"
    static void printYGValue(const YGValue &v) {
        if (v.unit == YGUnitUndefined) {
            std::cout << "undefined";
        } else if (v.unit == YGUnitAuto) {
            std::cout << "auto";
        } else if (v.unit == YGUnitPoint) {
            std::cout << v.value << "px";
        } else if (v.unit == YGUnitPercent) {
            std::cout << v.value << "%";
        } else {
            std::cout << v.value << " (unit=" << v.unit << ")";
        }
    }

    // Safe wrappers to obtain style YGValue and numeric properties.
    static YGValue getStyleValueSafe(YGNodeRef node, YGValue (*getter)(YGNodeRef)) {
        if (!node) return YGValue{0.0f, YGUnitUndefined};
        return getter(node);
    }
    static float getStyleFloatSafe(YGNodeRef node, float (*getter)(YGNodeRef)) {
        if (!node) return 0.0f;
        return getter(node);
    }
    static float getStyleEdgeFloatSafe(YGNodeRef node, float (*getter)(YGNodeRef, YGEdge), YGEdge e) {
        if (!node) return 0.0f;
        return getter(node, e);
    }
    static YGValue getStyleEdgeValueSafe(YGNodeRef node, YGValue (*getter)(YGNodeRef, YGEdge), YGEdge e) {
        if (!node) return YGValue{0.0f, YGUnitUndefined};
        return getter(node, e);
    }

    // ---------------- Trampolines for measure/baseline ----------------
    // We keep maps from YGNodeRef -> std::function so we can store C++ callables
    static std::unordered_map<YGNodeConstRef, std::function<YGSize(YGNodeConstRef, float, YGMeasureMode, float, YGMeasureMode)>> g_measureMap;
    static std::unordered_map<YGNodeConstRef, std::function<float(YGNodeConstRef, float, float)>> g_baselineMap;

    static YGSize measureTrampoline(YGNodeConstRef node, float width, YGMeasureMode widthMode, float height, YGMeasureMode heightMode) {
        auto it = g_measureMap.find(node);
        if (it == g_measureMap.end() || !it->second) return YGSize{0,0};
        return it->second(node, width, widthMode, height, heightMode);
    }

    static float baselineTrampoline(YGNodeConstRef node, float width, float height) {
        auto it = g_baselineMap.find(node);
        if (it == g_baselineMap.end() || !it->second) return 0.0f;
        return it->second(node, width, height);
    }

class Node : public std::enable_shared_from_this<Node> {
public:
    using Ptr = std::shared_ptr<Node>;

    // Factory: create node with optional name
    static Ptr create(const std::string &name = {}) {
        return std::make_shared<Node>(name);
    }

    // Constructor: Yoga node creation never throws
    explicit Node(const std::string &name = {}) : name_(name) {
        node_ = YGNodeNew();
    }

    // Disable copy and move — ensures a single RAII owner
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;

    // Destructor: safely releases Yoga resources and detaches from parent
    ~Node() {
        // remove callbacks entries if present
        if (node_) {
            g_measureMap.erase(node_);
            g_baselineMap.erase(node_);
        }

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

    // clear all children
    void clearChildren() {
        if (!node_) return;
        for (auto &c : children_) {
            if (c && c->node_) YGNodeRemoveChild(node_, c->node_);
            if (c) c->parent_.reset();
        }
        children_.clear();
    }

    // insert child at index
    void insertChildAt(const Ptr &child, uint32_t idx) {
        if (!node_ || !child || !child->node_) return;
        if (idx > children_.size()) idx = static_cast<uint32_t>(children_.size());
        YGNodeInsertChild(node_, child->node_, idx);
        children_.insert(children_.begin() + idx, child);
        child->parent_ = shared_from_this();
    }

    // replace a child
    void replaceChild(const Ptr &oldChild, const Ptr &newChild) {
        if (!node_ || !oldChild || !newChild) return;
        auto it = std::find_if(children_.begin(), children_.end(), [&](const Ptr &p){ return p.get() == oldChild.get(); });
        if (it == children_.end()) return;
        uint32_t idx = static_cast<uint32_t>(std::distance(children_.begin(), it));
        // remove old from yoga
        if (oldChild->node_) YGNodeRemoveChild(node_, oldChild->node_);
        // insert new
        YGNodeInsertChild(node_, newChild->node_, idx);
        *it = newChild;
        newChild->parent_ = shared_from_this();
        oldChild->parent_.reset();
    }

    // Detach this node from its parent
    void detachFromParent() {
        if (!node_) return;
        if (auto p = parent_.lock()) {
            p->removeChildInternal(this);
            parent_.reset();
        }
    }

    // Parent accessor (returns nullptr if no parent)
    Ptr parent() const { return parent_.lock(); }

    // Number of children
    size_t childrenCount() const { return children_.size(); }

    // Print debug information for this node and all descendants.
    // depth: current indentation level (pass 0 to start)
    void debugDump(int depth = 0) const {
        // indentation helper
        auto indent = [&](int d){
            for (int i = 0; i < d; ++i) std::cout << "  ";
        };

        // basic node info
        indent(depth);
        std::cout << "[Node] name=\"" << name_ << "\" this=" << this;
        if (auto p = parent_.lock()) std::cout << " parent=" << p.get();
        else std::cout << " parent=null";
        std::cout << " children=" << children_.size() << "\n";

        // layout (computed) - these are safe getters
        indent(depth);
        std::cout << "  layout: left=" << getLayoutLeft()
                  << " top=" << getLayoutTop()
                  << " w=" << getLayoutWidth()
                  << " h=" << getLayoutHeight() << "\n";

        // style properties: width/height/min/max/flexBasis/flex/flexGrow/shrink etc.
        if (node_) {
            // width/height/min/max
            indent(depth);
            std::cout << "  style:\n";
            indent(depth);
            std::cout << "    width: ";
            printYGValue( getStyleValueSafe(node_, [](YGNodeRef n){ return YGNodeStyleGetWidth(n); }) );
            std::cout << "    height: ";
            printYGValue( getStyleValueSafe(node_, [](YGNodeRef n){ return YGNodeStyleGetHeight(n); }) );
            std::cout << "\n";;

            indent(depth);
            std::cout << "    minWidth: ";
            printYGValue( getStyleValueSafe(node_, [](YGNodeRef n){ return YGNodeStyleGetMinWidth(n); }) );
            std::cout << "    minHeight: ";
            printYGValue( getStyleValueSafe(node_, [](YGNodeRef n){ return YGNodeStyleGetMinHeight(n); }) );
            std::cout << "\n";

            indent(depth);
            std::cout << "    maxWidth: ";
            printYGValue( getStyleValueSafe(node_, [](YGNodeRef n){ return YGNodeStyleGetMaxWidth(n); }) );
            std::cout << "    maxHeight: ";
            printYGValue( getStyleValueSafe(node_, [](YGNodeRef n){ return YGNodeStyleGetMaxHeight(n); }) );
            std::cout << "\n";

            // flexBasis, grow, shrink
            indent(depth);
            std::cout << "    flexBasis: ";
            printYGValue( getStyleValueSafe(node_, [](YGNodeRef n){ return YGNodeStyleGetFlexBasis(n); }) );
            std::cout << "    flexGrow: " << getStyleFloatSafe(node_, [](YGNodeRef n){ return YGNodeStyleGetFlexGrow(n); })
                      << "    flexShrink: " << getStyleFloatSafe(node_, [](YGNodeRef n){ return YGNodeStyleGetFlexShrink(n); }) << "\n";

            // flexDirection, justify, align
            indent(depth);
            std::cout << "    flexDirection: " << flexDirectionToString(YGNodeStyleGetFlexDirection(node_))
                      << "    justifyContent: " << justifyToString(YGNodeStyleGetJustifyContent(node_))
                      << "    alignItems: " << alignToString(YGNodeStyleGetAlignItems(node_))
                      << "    alignSelf: " << alignToString(YGNodeStyleGetAlignSelf(node_))
                      << "\n";

            // margin and padding per edge
            indent(depth);
            std::cout << "    margin (T R B L): ";
            printYGValue( getStyleEdgeValueSafe(node_, [](YGNodeRef n, YGEdge e){ return YGNodeStyleGetMargin(n, e); }, YGEdgeTop) ); std::cout << " ";
            printYGValue( getStyleEdgeValueSafe(node_, [](YGNodeRef n, YGEdge e){ return YGNodeStyleGetMargin(n, e); }, YGEdgeRight) ); std::cout << " ";
            printYGValue( getStyleEdgeValueSafe(node_, [](YGNodeRef n, YGEdge e){ return YGNodeStyleGetMargin(n, e); }, YGEdgeBottom) ); std::cout << " ";
            printYGValue( getStyleEdgeValueSafe(node_, [](YGNodeRef n, YGEdge e){ return YGNodeStyleGetMargin(n, e); }, YGEdgeLeft) ); std::cout << "\n";

            indent(depth);
            std::cout << "    padding (T R B L): ";
            printYGValue( getStyleEdgeValueSafe(node_, [](YGNodeRef n, YGEdge e){ return YGNodeStyleGetPadding(n, e); }, YGEdgeTop) ); std::cout << " ";
            printYGValue( getStyleEdgeValueSafe(node_, [](YGNodeRef n, YGEdge e){ return YGNodeStyleGetPadding(n, e); }, YGEdgeRight) ); std::cout << " ";
            printYGValue( getStyleEdgeValueSafe(node_, [](YGNodeRef n, YGEdge e){ return YGNodeStyleGetPadding(n, e); }, YGEdgeBottom) ); std::cout << " ";
            printYGValue( getStyleEdgeValueSafe(node_, [](YGNodeRef n, YGEdge e){ return YGNodeStyleGetPadding(n, e); }, YGEdgeLeft) ); std::cout << "\n";

            // border (numeric values)
            indent(depth);
            std::cout << "    border (T R B L): "
                      << getStyleEdgeFloatSafe(node_, [](YGNodeRef n, YGEdge e){ return YGNodeStyleGetBorder(n, e); }, YGEdgeTop) << " "
                      << getStyleEdgeFloatSafe(node_, [](YGNodeRef n, YGEdge e){ return YGNodeStyleGetBorder(n, e); }, YGEdgeRight) << " "
                      << getStyleEdgeFloatSafe(node_, [](YGNodeRef n, YGEdge e){ return YGNodeStyleGetBorder(n, e); }, YGEdgeBottom) << " "
                      << getStyleEdgeFloatSafe(node_, [](YGNodeRef n, YGEdge e){ return YGNodeStyleGetBorder(n, e); }, YGEdgeLeft)
                      << "\n";

            // aspect ratio (new)
            float aspect = YGNodeStyleGetAspectRatio(node_);
            indent(depth);
            std::cout << "    aspectRatio: ";
            if (aspect == YGUndefined) std::cout << "undefined";
            else std::cout << aspect;
            std::cout << "\n";

            // layout computed edges/padding/border
            indent(depth);
            std::cout << "    layout margin(T R B L): "
                      << YGNodeLayoutGetMargin(node_, YGEdgeTop) << " "
                      << YGNodeLayoutGetMargin(node_, YGEdgeRight) << " "
                      << YGNodeLayoutGetMargin(node_, YGEdgeBottom) << " "
                      << YGNodeLayoutGetMargin(node_, YGEdgeLeft) << "\n";

            indent(depth);
            std::cout << "    layout padding(T R B L): "
                      << YGNodeLayoutGetPadding(node_, YGEdgeTop) << " "
                      << YGNodeLayoutGetPadding(node_, YGEdgeRight) << " "
                      << YGNodeLayoutGetPadding(node_, YGEdgeBottom) << " "
                      << YGNodeLayoutGetPadding(node_, YGEdgeLeft) << "\n";

            indent(depth);
            std::cout << "    layout border(T R B L): "
                      << YGNodeLayoutGetBorder(node_, YGEdgeTop) << " "
                      << YGNodeLayoutGetBorder(node_, YGEdgeRight) << " "
                      << YGNodeLayoutGetBorder(node_, YGEdgeBottom) << " "
                      << YGNodeLayoutGetBorder(node_, YGEdgeLeft) << "\n";

        } else {
            indent(depth);
            std::cout << "  (no yoga node)\n";
        }

        // recurse children
        for (const auto &c : children_) {
            if (c) c->debugDump(depth + 1);
            else {
                indent(depth + 1);
                std::cout << "[null child]\n";
            }
        }
    }

    // find a direct child by exact name (returns nullptr if not found)
    Ptr getChildByName(std::string_view childName) const {
    if (childName.empty()) return nullptr;

    for (const auto& c : children_) {
        if (c && c->name_ == childName)
            return c;
    }
    return nullptr;
}

    // ---------------- Name utilities ----------------

    void setName(const std::string &n) { name_ = n; }
    const std::string &name() const { return name_; }

    // ---------------- Lookup utilities ----------------

    // Find topmost ancestor (root of this wrapper tree). If this node has no parent, returns itself.
    Ptr findRoot() const {
        // climb parents until null
        Ptr cur = const_cast<Node*>(this)->shared_from_this();
        if (!cur) return nullptr;
        while (true) {
            auto p = cur->parent();
            if (!p) break;
            cur = p;
        }
        return cur;
    }

    // Hierarchical lookup by path "a/b/c".
    // If path starts with '/', search begins from the root (findRoot()).
    // Otherwise search starts from this node.
    // Matching is exact and case-sensitive. Empty segments are ignored.
    Ptr getById(std::string_view path) const {
        if (path.empty()) return nullptr;

        size_t idx = 0;
        Ptr current;

        // Start node: root if path begins with '/', else this
        if (!path.empty() && path.front() == '/') {
            current = findRoot();
            // skip leading slash(es)
            while (idx < path.size() && path[idx] == '/') ++idx;
        } else {
            current = const_cast<Node*>(this)->shared_from_this();
        }

        if (!current) return nullptr;

        // split and descend
        while (idx < path.size() && current) {
            // find next segment
            size_t j = idx;
            while (j < path.size() && path[j] != '/') ++j;

            std::string_view seg = path.substr(idx, j - idx);
            idx = j;

            // skip empty segments
            if (seg.empty()) {
                while (idx < path.size() && path[idx] == '/') ++idx;
                continue;
            }

            // find direct child
            Ptr next = current->getChildByName(seg);  // perfeito: accepts string_view
            if (!next) return nullptr;

            current = next;

            // skip slash if present
            while (idx < path.size() && path[idx] == '/') ++idx;
        }

        return current;
    }

    // Recursive search: find any descendant (depth-first) whose name matches `targetName`.
    // Returns the first match found (pre-order). Returns nullptr if not found.
    Ptr getByIdRecursive(std::string_view targetName) const {
        if (targetName.empty()) return nullptr;
        // iterative DFS using stack to avoid recursion depth issues
        std::stack<Ptr> st;
        // start searching from children (not including `this` node). If you want to include `this`, check name first.
        for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
            if (*it) st.push(*it);
        }

        while (!st.empty()) {
            Ptr n = st.top(); st.pop();
            if (!n) continue;
            if (n->name_ == targetName) return n;
            // push children in reverse so we traverse in natural order
            for (auto it = n->children_.rbegin(); it != n->children_.rend(); ++it) {
                if (*it) st.push(*it);
            }
        }
        return nullptr;
    }

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
    void setFlexBasisAuto()              { if(node_) YGNodeStyleSetFlexBasisAuto(node_); }

    void setFlexDirection(YGFlexDirection v)   { if(node_) YGNodeStyleSetFlexDirection(node_, v); }
    void setJustifyContent(YGJustify v)        { if(node_) YGNodeStyleSetJustifyContent(node_, v); }
    void setAlignItems(YGAlign v)              { if(node_) YGNodeStyleSetAlignItems(node_, v); }
    void setAlignSelf(YGAlign v)               { if(node_) YGNodeStyleSetAlignSelf(node_, v); }

    void setMargin(YGEdge e, float v)          { if(node_) YGNodeStyleSetMargin(node_, e, v); }
    void setMarginPercent(YGEdge e, float v)   { if(node_) YGNodeStyleSetMarginPercent(node_, e, v); }
    void setMarginAuto(YGEdge e)               { if(node_) YGNodeStyleSetMarginAuto(node_, e); }

    void setMargin(const glm::vec4 &v) {
        if (!node_) return;
        YGNodeStyleSetMargin(node_, YGEdgeTop,    v.x);
        YGNodeStyleSetMargin(node_, YGEdgeRight,  v.y);
        YGNodeStyleSetMargin(node_, YGEdgeBottom, v.z);
        YGNodeStyleSetMargin(node_, YGEdgeLeft,   v.w);
    }

    void setPadding(YGEdge e, float v)         { if(node_) YGNodeStyleSetPadding(node_, e, v); }
    void setPaddingPercent(YGEdge e, float v)  { if(node_) YGNodeStyleSetPaddingPercent(node_, e, v); }

    void setPadding(const glm::vec4 &v) {
        if (!node_) return;
        YGNodeStyleSetPadding(node_, YGEdgeTop,    v.x);
        YGNodeStyleSetPadding(node_, YGEdgeRight,  v.y);
        YGNodeStyleSetPadding(node_, YGEdgeBottom, v.z);
        YGNodeStyleSetPadding(node_, YGEdgeLeft,   v.w);
    }

    void setWidthAuto()                        { if(node_) YGNodeStyleSetWidthAuto(node_); }
    void setHeightAuto()                       { if(node_) YGNodeStyleSetHeightAuto(node_); }

    void setMinWidth(float v)                  { if(node_) YGNodeStyleSetMinWidth(node_, v); }
    void setMaxWidth(float v)                  { if(node_) YGNodeStyleSetMaxWidth(node_, v); }
    void setMinHeight(float v)                 { if(node_) YGNodeStyleSetMinHeight(node_, v); }
    void setMaxHeight(float v)                 { if(node_) YGNodeStyleSetMaxHeight(node_, v); }

    // position helpers
    void setPosition(YGEdge e, float v)        { if(node_) YGNodeStyleSetPosition(node_, e, v); }
    void setPositionPercent(YGEdge e, float v) { if(node_) YGNodeStyleSetPositionPercent(node_, e, v); }
    void setPositionType(YGPositionType t)     { if(node_) YGNodeStyleSetPositionType(node_, t); }

    void setBorder(YGEdge e, float v)          { if(node_) YGNodeStyleSetBorder(node_, e, v); }
    void setOverflow(YGOverflow o)             { if(node_) YGNodeStyleSetOverflow(node_, o); }
    void setDisplay(YGDisplay d)               { if(node_) YGNodeStyleSetDisplay(node_, d); }
    void setFlex(float v)                      { if(node_) YGNodeStyleSetFlex(node_, v); }

    void setMinWidthPercent(float v)           { if(node_) YGNodeStyleSetMinWidthPercent(node_, v); }
    void setMaxWidthPercent(float v)           { if(node_) YGNodeStyleSetMaxWidthPercent(node_, v); }
    void setMinHeightPercent(float v)          { if(node_) YGNodeStyleSetMinHeightPercent(node_, v); }
    void setMaxHeightPercent(float v)          { if(node_) YGNodeStyleSetMaxHeightPercent(node_, v); }

    void setDirection(YGDirection d)           { if(node_) YGNodeStyleSetDirection(node_, d); }

    // Sets the aspect ratio. Use 1.0 for square (height = width).
    // If you want to clear aspect ratio, call clearAspectRatio().
    void setAspectRatio(float ratio) { if (node_) YGNodeStyleSetAspectRatio(node_, ratio); }
    // Clears aspect ratio (sets to undefined)
    void clearAspectRatio() { if (node_) YGNodeStyleSetAspectRatio(node_, YGUndefined); }
    // Gets current aspect ratio; returns YGUndefined if undefined
    float getAspectRatio() const { return node_ ? YGNodeStyleGetAspectRatio(node_) : YGUndefined; }

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
    float getLayoutBottom() const  { return getLayoutTop()+getLayoutHeight(); }

    float getLayoutMargin(YGEdge e) const { return node_ ? YGNodeLayoutGetMargin(node_, e) : 0.0f; }
    float getLayoutPadding(YGEdge e) const { return node_ ? YGNodeLayoutGetPadding(node_, e) : 0.0f; }
    float getLayoutBorder(YGEdge e) const { return node_ ? YGNodeLayoutGetBorder(node_, e) : 0.0f; }

    bool isDirty() const { return node_ ? YGNodeIsDirty(node_) : false; }
    void markDirty() { if(node_) YGNodeMarkDirty(node_); }

    // compute absolute top-left by summing left/top up the parent chain
    // Note: ImVec2 type assumed available where you use this wrapper (ImGui)
    glm::vec2 getAbsolutePos() const {
        glm::vec2 pos = {0.0f, 0.0f};

        auto cur = shared_from_this();
        while (cur) {
            pos.x += cur->getLayoutLeft();
            pos.y += cur->getLayoutTop();
            cur = cur->parent(); // wrapper has parent()
        }
        return pos;
    }

    // compute absolute rect (min and max)
    void getAbsoluteRect(glm::vec2 &outMin, glm::vec2 &outMax) const {
        if (!node_) {
            outMin = outMax = glm::vec2{0,0};
            return;
        }
        glm::vec2 pos = getAbsolutePos();
        float w = getLayoutWidth();
        float h = getLayoutHeight();
        outMin = pos;
        outMax = glm::vec2{ pos.x + w, pos.y + h };
    }

    glm::vec2 getAbsoluteCenter() const {
        if (!node_) return glm::vec2{0.0f, 0.0f};

        glm::vec2 pos = const_cast<Node*>(this)->getAbsolutePos();
        float w = getLayoutWidth();
        float h = getLayoutHeight();

        return glm::vec2{
            pos.x + w * 0.5f,
            pos.y + h * 0.5f
        };
    }

    // ---------------- Context / utils ----------------
    void setContext(void* ctx) { if(node_) YGNodeSetContext(node_, ctx); }
    void* getContext() const { return node_ ? YGNodeGetContext(node_) : nullptr; }

    void cloneStyleFrom(const Node& other) { if(node_ && other.node_) YGNodeCopyStyle(node_, other.node_); }

    // measure/baseline setters using std::function trampolines
    void setMeasureFunc(std::function<YGSize(YGNodeConstRef, float, YGMeasureMode, float, YGMeasureMode)> f) {
        if (!node_) return;
        if (f) {
            g_measureMap[node_] = std::move(f);
            YGNodeSetMeasureFunc(node_, measureTrampoline);
        } else {
            YGNodeSetMeasureFunc(node_, nullptr);
            g_measureMap.erase(node_);
        }
    }

    void setBaselineFunc(std::function<float(YGNodeConstRef, float, float)> f) {
        if (!node_) return;
        if (f) {
            g_baselineMap[node_] = std::move(f);
            YGNodeSetBaselineFunc(node_, baselineTrampoline);
        } else {
            YGNodeSetBaselineFunc(node_, nullptr);
            g_baselineMap.erase(node_);
        }
    }

    // ---------------- Convenience style getters ----------------
    YGValue getStyleWidth() const { return node_ ? YGNodeStyleGetWidth(node_) : YGValue{0,YGUnitUndefined}; }
    YGValue getStyleHeight() const { return node_ ? YGNodeStyleGetHeight(node_) : YGValue{0,YGUnitUndefined}; }
    YGValue getStyleMargin(YGEdge e) const { return node_ ? YGNodeStyleGetMargin(node_, e) : YGValue{0,YGUnitUndefined}; }
    YGValue getStylePadding(YGEdge e) const { return node_ ? YGNodeStyleGetPadding(node_, e) : YGValue{0,YGUnitUndefined}; }

    // ---------------- Layout ----------------
    // Computes layout for this node and all descendants
    // Width/height may be YGUndefined to allow Yoga to resolve automatically.
    void calculateLayoutRecursive(float w = YGUndefined, float h = YGUndefined, YGDirection dir = YGDirectionLTR) {
        calculateLayout(w,h,dir);
    }

    // ---------------- Draw / Walk utilities ----------------
    // Apply a visitor to every node in subtree (pre-order)
    void walkTree(const std::function<void(Node&)>& visitor) {
        visitor(*this);
        for (auto &c : children_) if (c) c->walkTree(visitor);
    }

    // ---------------- Style setters (percent variants) ----------------
    void setPositionPercentAll(float v) { if(node_) { for (YGEdge e : EDGES4) YGNodeStyleSetPositionPercent(node_, (YGEdge)e, v); } }

    // ---------------- Implied helpers ----------------
    // Access underlying Yoga node (use with care)
    YGNodeRef raw() const { return node_; }

private:
    friend class std::shared_ptr<Node>;

    YGEdge EDGES4[4] = {
        YGEdgeLeft, YGEdgeTop, YGEdgeRight, YGEdgeBottom
    };

    // Called when parent is destroyed — prevents double free
    void invalidateNode() { 
        // remove callbacks
        if (node_) {
            g_measureMap.erase(node_);
            g_baselineMap.erase(node_);
        }
        node_ = nullptr; 
    }

    YGNodeRef node_ = nullptr;         // The Yoga node we manage
    std::vector<Ptr> children_;        // Strong references to children
    std::weak_ptr<Node> parent_;       // Weak reference to parent
    std::string name_;                 // Optional node name for hierarchical lookup
};

} // namespace yoga_raii

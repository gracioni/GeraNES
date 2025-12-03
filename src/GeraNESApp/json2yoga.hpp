// json_to_yoga.hpp
// Build a Yoga tree from nlohmann::json and map ids to Node::Ptr.
// Defensive: ignores unknown fields, performs safe conversions.

/*

example json:

{
  "id": "root",
  "style": { "width": "800", "height": "600", "flexDirection": "row", "justifyContent": "center", "alignItems": "center" },
  "children": [
    {
      "id": "main",
      "style": { "width": "80", "height": "60", "margin": "8" }
    }
  ]
}

example C++:

#include <iostream>
#include <fstream>
#include "json_to_yoga.hpp"

using json = nlohmann::json;

void example_build_and_layout() {
    // load JSON from file or string
    std::ifstream ifs("layout.json");
    json j; ifs >> j;

    json_yoga::IdMap idMap;
    auto root = json_yoga::buildTree(j, idMap);
    if (!root) {
        std::cout << "failed to build tree\n";
        return;
    }

    // call layout with the viewport size (or YGUndefined to let Yoga resolve)
    root->calculateLayout( (float)800.0f, (float)600.0f );

    // lookup by id
    auto it = idMap.find("main");
    if (it != idMap.end()) {
        auto mainNode = it->second;
        std::cout << "main layout: " << mainNode->getLayoutLeft()
                  << "," << mainNode->getLayoutTop()
                  << " " << mainNode->getLayoutWidth()
                  << "x" << mainNode->getLayoutHeight() << "\n";
    }
}
*/

#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <nlohmann/json.hpp>
#include "yoga_raii.hpp"

using json = nlohmann::json;

namespace json_yoga {

// result map type
using IdMap = std::unordered_map<std::string, yoga_raii::Node::Ptr>;

// helper: parse a CSS-like length value: "50%", "80", 80
// returns pair: (isPercent, value). If not present / invalid -> std::nullopt
static std::optional<std::pair<bool, float>> parseLength(const json &j) {
    if (j.is_null()) return std::nullopt;
    if (j.is_number()) {
        return std::make_pair(false, j.get<float>());
    }
    if (!j.is_string()) return std::nullopt;
    std::string s = j.get<std::string>();
    // trim
    auto trim = [](std::string &t){
        size_t a=0; while(a<t.size() && isspace((unsigned char)t[a])) a++;
        size_t b=t.size(); while(b>a && isspace((unsigned char)t[b-1])) b--;
        t = t.substr(a, b-a);
    };
    trim(s);
    if (s.empty()) return std::nullopt;
    if (s.back() == '%') {
        // percent
        try {
            float v = std::stof(s.substr(0, s.size()-1));
            return std::make_pair(true, v);
        } catch(...) {
            return std::nullopt;
        }
    } else {
        try {
            float v = std::stof(s);
            return std::make_pair(false, v);
        } catch(...) {
            return std::nullopt;
        }
    }
}

// map simple string tokens to Yoga enums (few examples)
static YGFlexDirection parseFlexDirection(const std::string &s) {
    if (s == "row") return YGFlexDirectionRow;
    if (s == "column") return YGFlexDirectionColumn;
    if (s == "row-reverse") return YGFlexDirectionRowReverse;
    if (s == "column-reverse") return YGFlexDirectionColumnReverse;
    return YGFlexDirectionColumn; // default
}

static YGJustify parseJustifyContent(const std::string &s) {
    if (s=="flex-start") return YGJustifyFlexStart;
    if (s=="center") return YGJustifyCenter;
    if (s=="flex-end") return YGJustifyFlexEnd;
    if (s=="space-between") return YGJustifySpaceBetween;
    if (s=="space-around") return YGJustifySpaceAround;
    if (s=="space-evenly") return YGJustifySpaceEvenly;
    return YGJustifyFlexStart;
}

static YGAlign parseAlign(const std::string &s) {
    if (s=="flex-start") return YGAlignFlexStart;
    if (s=="center") return YGAlignCenter;
    if (s=="flex-end") return YGAlignFlexEnd;
    if (s=="stretch") return YGAlignStretch;
    if (s=="baseline") return YGAlignBaseline;
    return YGAlignAuto;
}

// apply a length to node: either percent or px
static void applyLengthToNode(yoga_raii::Node::Ptr node,
                              const std::optional<std::pair<bool,float>> &len,
                              std::function<void(yoga_raii::Node::Ptr,float)> pxSetter,
                              std::function<void(yoga_raii::Node::Ptr,float)> pctSetter) {
    if (!node) return;
    if (!len) return;
    bool isPct = len->first;
    float v = len->second;
    if (isPct) pctSetter(node, v);
    else pxSetter(node, v);
}

// Apply style object to a node (silently ignores unknown props)
static void applyStyle(yoga_raii::Node::Ptr node, const json &style) {
    if (!node || style.is_null() || !style.is_object()) return;

    // size
    auto w = parseLength(style.value("width", json()));
    if (w) {
        applyLengthToNode(node, w,
            [](yoga_raii::Node::Ptr n, float px){ n->setWidth(px); },
            [](yoga_raii::Node::Ptr n, float pct){ n->setWidthPercent(pct); });
    }

    auto h = parseLength(style.value("height", json()));
    if (h) {
        applyLengthToNode(node, h,
            [](yoga_raii::Node::Ptr n, float px){ n->setHeight(px); },
            [](yoga_raii::Node::Ptr n, float pct){ n->setHeightPercent(pct); });
    }

    // min/max
    auto mw = parseLength(style.value("minWidth", json()));
    if (mw) applyLengthToNode(node, mw,
        [](yoga_raii::Node::Ptr n, float px){ n->setMinWidth(px); },
        [](yoga_raii::Node::Ptr n, float pct){ n->setMinWidth(pct); /* percent setter not implemented in wrapper? */ });

    auto Mxw = parseLength(style.value("maxWidth", json()));
    if (Mxw) applyLengthToNode(node, Mxw,
        [](yoga_raii::Node::Ptr n, float px){ n->setMaxWidth(px); },
        [](yoga_raii::Node::Ptr n, float pct){ n->setMaxWidth(pct); /* percent setter not implemented */ });

    auto mh = parseLength(style.value("minHeight", json()));
    if (mh) applyLengthToNode(node, mh,
        [](yoga_raii::Node::Ptr n, float px){ n->setMinHeight(px); },
        [](yoga_raii::Node::Ptr n, float pct){ n->setMinHeight(pct); /* percent setter not implemented */ });

    auto Mxh = parseLength(style.value("maxHeight", json()));
    if (Mxh) applyLengthToNode(node, Mxh,
        [](yoga_raii::Node::Ptr n, float px){ n->setMaxHeight(px); },
        [](yoga_raii::Node::Ptr n, float pct){ n->setMaxHeight(pct); /* percent setter not implemented */ });

    // flex properties
    if (style.contains("flexGrow") && style["flexGrow"].is_number()) {
        node->setFlexGrow(style["flexGrow"].get<float>());
    }
    if (style.contains("flexShrink") && style["flexShrink"].is_number()) {
        node->setFlexShrink(style["flexShrink"].get<float>());
    }
    // flexBasis
    if (style.contains("flexBasis")) {
        if (style["flexBasis"].is_string()) {
            std::string s = style["flexBasis"].get<std::string>();
            if (s == "auto") {
                // nothing to do (wrapper doesn't have setFlexBasisAuto)
            } else {
                auto fb = parseLength(style["flexBasis"]);
                if (fb) applyLengthToNode(node, fb,
                    [](yoga_raii::Node::Ptr n, float px){ n->setFlexBasis(px); },
                    [](yoga_raii::Node::Ptr n, float pct){ n->setFlexBasisPercent(pct); });
            }
        } else {
            auto fb = parseLength(style["flexBasis"]);
            if (fb) applyLengthToNode(node, fb,
                [](yoga_raii::Node::Ptr n, float px){ n->setFlexBasis(px); },
                [](yoga_raii::Node::Ptr n, float pct){ n->setFlexBasisPercent(pct); });
        }
    }

    // direction / justify / align
    if (style.contains("flexDirection") && style["flexDirection"].is_string()) {
        node->setFlexDirection(parseFlexDirection(style["flexDirection"].get<std::string>()));
    }
    if (style.contains("justifyContent") && style["justifyContent"].is_string()) {
        node->setJustifyContent(parseJustifyContent(style["justifyContent"].get<std::string>()));
    }
    if (style.contains("alignItems") && style["alignItems"].is_string()) {
        node->setAlignItems(parseAlign(style["alignItems"].get<std::string>()));
    }
    if (style.contains("alignSelf") && style["alignSelf"].is_string()) {
        node->setAlignSelf(parseAlign(style["alignSelf"].get<std::string>()));
    }

    // margin shorthand and individual edges
    auto applyMarginEdge = [&](const std::string &k, YGEdge edge){
        if (!style.contains(k)) return;
        auto val = parseLength(style[k]);
        if (!val) return;
        if (val->first) node->setMarginPercent(edge, val->second);
        else node->setMargin(edge, val->second);
    };

    if (style.contains("margin")) {
        auto m = parseLength(style["margin"]);
        if (m) {
            if (m->first) {
                node->setMarginPercent(YGEdgeTop, m->second);
                node->setMarginPercent(YGEdgeRight, m->second);
                node->setMarginPercent(YGEdgeBottom, m->second);
                node->setMarginPercent(YGEdgeLeft, m->second);
            } else {
                node->setMargin(YGEdgeTop, m->second);
                node->setMargin(YGEdgeRight, m->second);
                node->setMargin(YGEdgeBottom, m->second);
                node->setMargin(YGEdgeLeft, m->second);
            }
        }
    } else {
        applyMarginEdge("marginTop", YGEdgeTop);
        applyMarginEdge("marginRight", YGEdgeRight);
        applyMarginEdge("marginBottom", YGEdgeBottom);
        applyMarginEdge("marginLeft", YGEdgeLeft);
    }

    // padding shorthand and edges
    auto applyPaddingEdge = [&](const std::string &k, YGEdge edge){
        if (!style.contains(k)) return;
        auto val = parseLength(style[k]);
        if (!val) return;
        if (val->first) node->setPaddingPercent(edge, val->second);
        else node->setPadding(edge, val->second);
    };

    if (style.contains("padding")) {
        auto p = parseLength(style["padding"]);
        if (p) {
            if (p->first) {
                node->setPaddingPercent(YGEdgeTop, p->second);
                node->setPaddingPercent(YGEdgeRight, p->second);
                node->setPaddingPercent(YGEdgeBottom, p->second);
                node->setPaddingPercent(YGEdgeLeft, p->second);
            } else {
                node->setPadding(YGEdgeTop, p->second);
                node->setPadding(YGEdgeRight, p->second);
                node->setPadding(YGEdgeBottom, p->second);
                node->setPadding(YGEdgeLeft, p->second);
            }
        }
    } else {
        applyPaddingEdge("paddingTop", YGEdgeTop);
        applyPaddingEdge("paddingRight", YGEdgeRight);
        applyPaddingEdge("paddingBottom", YGEdgeBottom);
        applyPaddingEdge("paddingLeft", YGEdgeLeft);
    }
}

// Recursively build a node and children, filling idMap with nodes that have "id" in JSON.
// Returns created node (or nullptr if creation failed).
static yoga_raii::Node::Ptr buildNodeFromJson(const json &j, IdMap &idMap) {
    // create node
    auto node = yoga_raii::Node::create();
    if (!node) return nullptr;

    // apply style if present
    if (j.contains("style") && j["style"].is_object()) {
        applyStyle(node, j["style"]);
    }

    // register id if present
    if (j.contains("id") && j["id"].is_string()) {
        idMap[j["id"].get<std::string>()] = node;
    }

    // children
    if (j.contains("children") && j["children"].is_array()) {
        for (const auto &childJ : j["children"]) {
            if (!childJ.is_object()) continue;
            auto childNode = buildNodeFromJson(childJ, idMap);
            if (childNode) {
                node->addChild(childNode);
            }
        }
    }

    return node;
}

// Public entry: builds a tree from rootJson and fills idMap.
// Example: json root should be an object (node). Returns root node pointer (or nullptr).
static yoga_raii::Node::Ptr buildTree(const json &rootJson, IdMap &outIdMap) {
    outIdMap.clear();
    if (!rootJson.is_object()) return nullptr;
    return buildNodeFromJson(rootJson, outIdMap);
}

} // namespace json_yoga

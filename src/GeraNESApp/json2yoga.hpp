// json2yoga.hpp
// Build a Yoga tree from nlohmann::json and interpret length units
// Supports: numbers (px), "123", "50%", "20px", "2in", "5cm", "10mm", "0.1m"
// Requires glm::vec2 for DPI: x = horizontal DPI (px/in), y = vertical DPI (px/in).
// No exceptions thrown; invalid values are ignored (no-op).

#pragma once

#include <string>
#include <optional>
#include <functional>
#include <charconv> // kept for compatibility with other code, but not used for float parsing here
#include <nlohmann/json.hpp>
#include <glm/vec2.hpp>
#include "yoga_raii.hpp"

#include <cerrno>
#include <cstdlib>
#include <cctype>
#include <cmath>

using json = nlohmann::json;

namespace json_yoga {

// --------------------------- UTIL: parse raw length token ---------------------------
// parseLengthRaw: accepts json that can be a number or string.
// returns structure describing either percent or absolute with unit token and numeric value.
// if invalid -> nullopt
//
struct LengthRaw {
    // kind:
    //  - percent: value interpreted as percent (0..100)
    //  - number: unitless number (we'll treat as px)
    //  - unit: has unit string (px, in, cm, mm, m)
    enum Kind { Percent, Number, Unit } kind;
    float value;       // numeric part
    std::string unit;  // only meaningful if kind == Unit
};

// --------------------------- portable float parser (strtof-based) ---------------------------
// parseFloatStrict: parse whole string (allow surrounding whitespace), reject trailing garbage.
// returns true on success and sets out. Does not throw.
static bool parseFloatStrict(const std::string &s_in, float &out) {
    std::string s = s_in;
    // trim leading/trailing whitespace
    size_t a = 0; while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    size_t b = s.size(); while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) --b;
    if (b <= a) return false;
    s = s.substr(a, b - a);

    const char* cstr = s.c_str();
    char* end = nullptr;
    errno = 0;
    float v = std::strtof(cstr, &end);
    if (end == cstr) return false;                  // nothing consumed
    if (errno == ERANGE) return false;              // out of range
    // skip trailing whitespace
    while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (*end != '\0') return false;                 // leftover non-space characters -> invalid
    out = v;
    return true;
}

static bool parseDoubleStrict(const std::string &s_in, double &out) {
    std::string s = s_in;
    size_t a = 0; while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    size_t b = s.size(); while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) --b;
    if (b <= a) return false;
    s = s.substr(a, b - a);

    const char* cstr = s.c_str();
    char* end = nullptr;
    errno = 0;
    double v = std::strtod(cstr, &end);
    if (end == cstr) return false;
    if (errno == ERANGE) return false;
    while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (*end != '\0') return false;
    out = v;
    return true;
}

// parseLengthRaw implementation (uses parseFloatStrict instead of from_chars)
static std::optional<LengthRaw> parseLengthRaw(const json &j) {
    if (j.is_null()) return std::nullopt;
    // numeric -> treat as px (Number)
    if (j.is_number()) {
        return LengthRaw{LengthRaw::Number, j.get<float>(), "px"};
    }
    if (!j.is_string()) return std::nullopt;

    std::string s = j.get<std::string>();
    // trim
    auto trim = [](std::string &t){
        size_t a=0; while(a<t.size() && std::isspace((unsigned char)t[a])) a++;
        size_t b=t.size(); while(b>a && std::isspace((unsigned char)t[b-1])) b--;
        t = t.substr(a, b-a);
    };
    trim(s);
    if (s.empty()) return std::nullopt;

    // check special tokens
    std::string sl = s;
    for (auto &c : sl) c = (char)tolower((unsigned char)c);
    if (sl == "auto") {
        // signal "auto" by returning Number with NaN
        return LengthRaw{LengthRaw::Number, std::nanf(""), "auto"};
    }

    // percentage
    if (s.back() == '%') {
        std::string num = s.substr(0, s.size()-1);
        trim(num);
        float v = 0.0f;
        if (parseFloatStrict(num, v)) {
            return LengthRaw{LengthRaw::Percent, v, "%"};
        }
        return std::nullopt;
    }

    // suffix-aware parsing: check common units (px, in, cm, mm, m)
    // accept both lower and upper case
    auto ends_with = [&](const std::string &str, const char *suffix)->bool{
        size_t ln = strlen(suffix);
        if (str.size() < ln) return false;
        std::string end = str.substr(str.size()-ln);
        // normalize lower
        for (auto &c : end) c = (char)tolower((unsigned char)c);
        std::string suf(suffix);
        for (auto &c : suf) c = (char)tolower((unsigned char)c);
        return end == suf;
    };

    // check "px"
    if (ends_with(s, "px")) {
        std::string num = s.substr(0, s.size()-2);
        trim(num);
        float v = 0.0f;
        if (parseFloatStrict(num, v)) return LengthRaw{LengthRaw::Unit, v, "px"};
        return std::nullopt;
    }
    if (ends_with(s, "in")) {
        std::string num = s.substr(0, s.size()-2);
        trim(num);
        float v = 0.0f;
        if (parseFloatStrict(num, v)) return LengthRaw{LengthRaw::Unit, v, "in"};
        return std::nullopt;
    }
    if (ends_with(s, "cm")) {
        std::string num = s.substr(0, s.size()-2);
        trim(num);
        float v = 0.0f;
        if (parseFloatStrict(num, v)) return LengthRaw{LengthRaw::Unit, v, "cm"};
        return std::nullopt;
    }
    if (ends_with(s, "mm")) {
        std::string num = s.substr(0, s.size()-2);
        trim(num);
        float v = 0.0f;
        if (parseFloatStrict(num, v)) return LengthRaw{LengthRaw::Unit, v, "mm"};
        return std::nullopt;
    }
    if (ends_with(s, "m")) { // meter; careful: "m" single char
        std::string num = s.substr(0, s.size()-1);
        trim(num);
        float v = 0.0f;
        if (parseFloatStrict(num, v)) return LengthRaw{LengthRaw::Unit, v, "m"};
        return std::nullopt;
    }

    // last attempt: plain numeric string -> treat as px (Number)
    {
        float v = 0.0f;
        if (parseFloatStrict(s, v)) return LengthRaw{LengthRaw::Number, v, "px"};
        return std::nullopt;
    }
}

// --------------------------- parseNumber helper ---------------------------
// Accepts json number or numeric string. Returns optional<float>.
// Uses parseFloatStrict to avoid exceptions.
static std::optional<float> parseNumber(const json &j) {
    if (j.is_null()) return std::nullopt;
    if (j.is_number()) return j.get<float>();
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
    float v = 0.0f;
    if (parseFloatStrict(s, v)) return v;
    return std::nullopt;
}

// --------------------------- convert raw length into percent/pixels ---------------------------
// returns: optional pair (isPercent, value)
// - if isPercent==true -> value is percent (0..100) and should be applied via setWidthPercent / setHeightPercent
// - if isPercent==false -> value is pixels (converted using DPI when unit was physical)
//
// axis: 0 = horizontal (use dpi.x), 1 = vertical (use dpi.y)
// dpi: glm::vec2(dpiX, dpiY) in pixels per inch
static std::optional<std::pair<bool,float>> lengthToPixelsForAxis(const json &j, int axis, const glm::vec2 &dpi) {
    auto rawOpt = parseLengthRaw(j);
    if (!rawOpt) return std::nullopt;
    LengthRaw raw = *rawOpt;

    // if parseLengthRaw returned "auto" marker (unit == "auto" and value is NaN), signal no numeric value
    if (raw.unit == "auto") return std::nullopt;

    if (raw.kind == LengthRaw::Percent) {
        // percent -> return percent value (no conversion)
        return std::make_pair(true, raw.value);
    }

    // number or unit -> convert to pixels. axis selects which DPI to use for unit conversions that involve inches.
    float pixels = 0.0f;
    std::string unit = raw.unit;
    // normalize unit to lower
    for (auto &c : unit) c = (char)tolower((unsigned char)c);

    if (raw.kind == LengthRaw::Number || unit == "px" || unit.empty()) {
        pixels = raw.value;
        return std::make_pair(false, pixels);
    }

    // physical units -> convert to inches then multiply by dpi
    float inches = 0.0f;
    if (unit == "in") {
        inches = raw.value;
    } else if (unit == "cm") {
        inches = raw.value / 2.54f;
    } else if (unit == "mm") {
        inches = raw.value / 25.4f;
    } else if (unit == "m") {
        inches = raw.value * 100.0f / 2.54f; // meters -> cm -> inches (1 m = 100 cm)
    } else {
        // unknown unit -> fail silently
        return std::nullopt;
    }

    float useDpi = (axis == 0) ? dpi.x : dpi.y;
    // defensive: if dpi is zero or negative, fallback to 96 (common default)
    if (!(useDpi > 0.0f)) useDpi = 96.0f;

    pixels = inches * useDpi;
    return std::make_pair(false, pixels);
}

// --------------------------- existing helpers (enum parsing etc) ---------------------------
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

// --------------------------- apply style (uses lengthToPixelsForAxis) ---------------------------
static YGDisplay parseDisplay(const std::string &s) {
    if (s == "flex") return YGDisplayFlex;
    return YGDisplayNone;
}
static YGPositionType parsePositionType(const std::string &s) {
    if (s == "absolute") return YGPositionTypeAbsolute;
    return YGPositionTypeRelative;
}
static YGOverflow parseOverflow(const std::string &s) {
    if (s == "hidden") return YGOverflowHidden;
    if (s == "scroll") return YGOverflowScroll;
    return YGOverflowVisible;
}
static YGDirection parseDirection(const std::string &s) {
    if (s == "rtl") return YGDirectionRTL;
    if (s == "ltr") return YGDirectionLTR;
    return YGDirectionInherit;
}

static void applyStyle(yoga_raii::Node::Ptr node, const json &style, const glm::vec2 &dpi) {
    if (!node || style.is_null() || !style.is_object()) return;

    // ---------- width/height (support "auto") ----------
    if (style.contains("width")) {
        const auto &j = style["width"];
        if (j.is_string() && j.get<std::string>() == "auto") node->setWidthAuto();
        else {
            auto w = lengthToPixelsForAxis(j, 0, dpi);
            if (w) {
                if (w->first) node->setWidthPercent(w->second);
                else node->setWidth(w->second);
            }
        }
    }

    if (style.contains("height")) {
        const auto &j = style["height"];
        if (j.is_string() && j.get<std::string>() == "auto") node->setHeightAuto();
        else {
            auto h = lengthToPixelsForAxis(j, 1, dpi);
            if (h) {
                if (h->first) node->setHeightPercent(h->second);
                else node->setHeight(h->second);
            }
        }
    }

    // min/max (width axis) - support percent if wrapper provides percent setters
    if (style.contains("minWidth")) {
        auto mw = lengthToPixelsForAxis(style["minWidth"], 0, dpi);
        if (mw) {
            if (mw->first) node->setMinWidthPercent(mw->second);
            else node->setMinWidth(mw->second);
        }
    }
    if (style.contains("maxWidth")) {
        auto Mxw = lengthToPixelsForAxis(style["maxWidth"], 0, dpi);
        if (Mxw) {
            if (Mxw->first) node->setMaxWidthPercent(Mxw->second);
            else node->setMaxWidth(Mxw->second);
        }
    }

    if (style.contains("minHeight")) {
        auto mh = lengthToPixelsForAxis(style["minHeight"], 1, dpi);
        if (mh) {
            if (mh->first) node->setMinHeightPercent(mh->second);
            else node->setMinHeight(mh->second);
        }
    }
    if (style.contains("maxHeight")) {
        auto Mxh = lengthToPixelsForAxis(style["maxHeight"], 1, dpi);
        if (Mxh) {
            if (Mxh->first) node->setMaxHeightPercent(Mxh->second);
            else node->setMaxHeight(Mxh->second);
        }
    }

    // flex properties: accept number or numeric-string
    if (style.contains("flexGrow")) {
        if (auto v = parseNumber(style["flexGrow"])) node->setFlexGrow(*v);
    }
    if (style.contains("flexShrink")) {
        if (auto v = parseNumber(style["flexShrink"])) node->setFlexShrink(*v);
    }
    // flexBasis: can be percent or length; treat axis depending on main axis later.
    if (style.contains("flexBasis")) {
        const auto &fbj = style["flexBasis"];
        if (fbj.is_string() && fbj.get<std::string>() == "auto") {
            node->setFlexBasisAuto();
        } else {
            auto fb_h = lengthToPixelsForAxis(fbj, 0, dpi);
            if (fb_h) {
                if (fb_h->first) node->setFlexBasisPercent(fb_h->second);
                else node->setFlexBasis(fb_h->second);
            } else {
                if (auto v = parseNumber(fbj)) {
                    node->setFlexBasis(*v);
                }
            }
        }
    }

    // shorthand flex (css-like)
    if (style.contains("flex")) {
        if (auto v = parseNumber(style["flex"])) node->setFlex(*v);
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

    // position type / display / overflow / direction
    if (style.contains("positionType") && style["positionType"].is_string()) {
        node->setPositionType(parsePositionType(style["positionType"].get<std::string>()));
    }
    if (style.contains("display") && style["display"].is_string()) {
        node->setDisplay(parseDisplay(style["display"].get<std::string>()));
    }
    if (style.contains("overflow") && style["overflow"].is_string()) {
        node->setOverflow(parseOverflow(style["overflow"].get<std::string>()));
    }
    if (style.contains("direction") && style["direction"].is_string()) {
        node->setDirection(parseDirection(style["direction"].get<std::string>()));
    }

    // position edges: top/right/bottom/left
    auto applyPositionEdge = [&](const std::string &k, YGEdge edge, int axis){
        if (!style.contains(k)) return;
        const auto &j = style[k];
        if (j.is_string() && j.get<std::string>() == "auto") {
            // Yoga has no "auto" position setter; ignore
            return;
        }
        auto val = lengthToPixelsForAxis(j, axis, dpi);
        if (!val) return;
        if (val->first) node->setPositionPercent(edge, val->second);
        else node->setPosition(edge, val->second);
    };

    applyPositionEdge("top", YGEdgeTop, 1);
    applyPositionEdge("right", YGEdgeRight, 0);
    applyPositionEdge("bottom", YGEdgeBottom, 1);
    applyPositionEdge("left", YGEdgeLeft, 0);

    // border shorthand and individual edges
    if (style.contains("border")) {
        if (auto v = parseNumber(style["border"])) {
            node->setBorder(YGEdgeTop, *v);
            node->setBorder(YGEdgeRight, *v);
            node->setBorder(YGEdgeBottom, *v);
            node->setBorder(YGEdgeLeft, *v);
        }
    }
    if (style.contains("borderTop") && parseNumber(style["borderTop"])) node->setBorder(YGEdgeTop, *parseNumber(style["borderTop"]));
    if (style.contains("borderRight") && parseNumber(style["borderRight"])) node->setBorder(YGEdgeRight, *parseNumber(style["borderRight"]));
    if (style.contains("borderBottom") && parseNumber(style["borderBottom"])) node->setBorder(YGEdgeBottom, *parseNumber(style["borderBottom"]));
    if (style.contains("borderLeft") && parseNumber(style["borderLeft"])) node->setBorder(YGEdgeLeft, *parseNumber(style["borderLeft"]));

    // margin shorthand and individual edges
    auto applyMarginEdge = [&](const std::string &k, YGEdge edge, int axis){
        if (!style.contains(k)) return;
        auto val = lengthToPixelsForAxis(style[k], axis, dpi);
        if (!val) return;
        if (val->first) node->setMarginPercent(edge, val->second);
        else node->setMargin(edge, val->second);
    };

    if (style.contains("margin")) {
        auto m = parseLengthRaw(style["margin"]); // we just detect percent or px/unit
        if (m) {
            if (m->kind == LengthRaw::Percent) {
                for (YGEdge e : {YGEdgeTop, YGEdgeRight, YGEdgeBottom, YGEdgeLeft}) node->setMarginPercent(e, m->value);
            } else {
                // margin numbers in px or units -> convert using horizontal DPI for left/right and vertical for top/bottom
                auto top = lengthToPixelsForAxis(style["margin"], 1, dpi);
                auto horiz = lengthToPixelsForAxis(style["margin"], 0, dpi);
                if (top) node->setMargin(YGEdgeTop, top->second);
                if (horiz) node->setMargin(YGEdgeRight, horiz->second);
                if (top) node->setMargin(YGEdgeBottom, top->second);
                if (horiz) node->setMargin(YGEdgeLeft, horiz->second);
            }
        }
    } else {
        applyMarginEdge("marginTop", YGEdgeTop, 1);
        applyMarginEdge("marginRight", YGEdgeRight, 0);
        applyMarginEdge("marginBottom", YGEdgeBottom, 1);
        applyMarginEdge("marginLeft", YGEdgeLeft, 0);
    }

    // aspectRatio
    if (style.contains("aspectRatio")) {
        auto m = parseLengthRaw(style["aspectRatio"]);
        if (m) {
            if (m->kind == LengthRaw::Number) {
                node->setAspectRatio(m->value);
            }
        }
    }

    // padding shorthand and edges
    auto applyPaddingEdge = [&](const std::string &k, YGEdge edge, int axis){
        if (!style.contains(k)) return;
        auto val = lengthToPixelsForAxis(style[k], axis, dpi);
        if (!val) return;
        if (val->first) node->setPaddingPercent(edge, val->second);
        else node->setPadding(edge, val->second);
    };

    if (style.contains("padding")) {
        auto pRaw = parseLengthRaw(style["padding"]);
        if (pRaw) {
            if (pRaw->kind == LengthRaw::Percent) {
                for (YGEdge e : {YGEdgeTop, YGEdgeRight, YGEdgeBottom, YGEdgeLeft}) node->setPaddingPercent(e, pRaw->value);
            } else {
                // convert top/bottom using vertical DPI, left/right using horizontal DPI
                auto pVert = lengthToPixelsForAxis(style["padding"], 1, dpi);
                auto pH = lengthToPixelsForAxis(style["padding"], 0, dpi);
                if (pVert) node->setPadding(YGEdgeTop, pVert->second);
                if (pH) node->setPadding(YGEdgeRight, pH->second);
                if (pVert) node->setPadding(YGEdgeBottom, pVert->second);
                if (pH) node->setPadding(YGEdgeLeft, pH->second);
            }
        }
    } else {
        applyPaddingEdge("paddingTop", YGEdgeTop, 1);
        applyPaddingEdge("paddingRight", YGEdgeRight, 0);
        applyPaddingEdge("paddingBottom", YGEdgeBottom, 1);
        applyPaddingEdge("paddingLeft", YGEdgeLeft, 0);
    }
}

// --------------------------- Builder (recursive) ---------------------------

static yoga_raii::Node::Ptr buildNodeFromJson(const json &j, const glm::vec2 &dpi) {
    auto node = yoga_raii::Node::create();
    if (!node) return nullptr;

    // set id/name
    if (j.contains("id") && j["id"].is_string()) {
        node->setName(j["id"].get<std::string>());
    }

    // style with DPI
    if (j.contains("style") && j["style"].is_object()) {
        applyStyle(node, j["style"], dpi);
    }

    // children
    if (j.contains("children") && j["children"].is_array()) {
        for (const auto &childJ : j["children"]) {
            if (!childJ.is_object()) continue;
            auto childNode = buildNodeFromJson(childJ, dpi);
            if (childNode) node->addChild(childNode);
        }
    }

    return node;
}

// Entry point: build the tree using the supplied DPI
// dpi.x = horizontal DPI (px/in), dpi.y = vertical DPI (px/in)
static yoga_raii::Node::Ptr buildTree(const json &rootJson, const glm::vec2 &dpi = glm::vec2(96.0f,96.0f)) {
    if (!rootJson.is_object()) return nullptr;
    return buildNodeFromJson(rootJson, dpi);
}

} // namespace json_yoga

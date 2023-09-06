#ifndef SVG_HPP
#define SVG_HPP

#include <iostream>
#include <fstream>
#include <map>
#include <utility>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include "csv.h"
#include "rapidxml-1.13/rapidxml.hpp"
#include "rapidxml-1.13/rapidxml_utils.hpp"
#include "rapidxml-1.13/rapidxml_print.hpp"
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

static inline void append_attributes(rapidxml::xml_document<char> *doc,
        rapidxml::xml_node<char> *node,
        std::vector<std::pair<std::string, std::string>> attrs) {
    if (node) {

        for (auto attr : attrs) {
            node->append_attribute(
                    doc->allocate_attribute(
                            doc->allocate_string(attr.first.c_str()),
                            doc->allocate_string(attr.second.c_str())));
        }
    }
}

static inline rapidxml::xml_node<char>* add_dashed_line(rapidxml::xml_document<char> *doc,
        float x1, float y1, float x2,
        float y2, float font_size) {
    float stroke_width = font_size / 10;
    float line = font_size / 2;
    float space = line / 2;

    auto line_node = doc->allocate_node(rapidxml::node_element, "line");
    append_attributes(doc, line_node, { { "x1", std::to_string(x1) },
            { "y1", std::to_string(y1) }, { "x2", std::to_string(x2) },
            { "y2", std::to_string(y2) }, { "stroke", "gray" },
            { "stroke-width", std::to_string(stroke_width) }, { "stroke-dasharray", std::to_string(line) + ", " + std::to_string(space) }, });

    return line_node;
}

static inline rapidxml::xml_node<char>* add_label(rapidxml::xml_document<char> *doc,
        float x, float y,
        const char *label) {
    auto label_node = doc->allocate_node(rapidxml::node_element, "text",
            doc->allocate_string(label));

    append_attributes(doc, label_node, { { "x", std::to_string(x) },
            { "y", std::to_string(y) }, { "class", "label" }, });

    return label_node;
}

rapidxml::xml_node<char>* add_tube(rapidxml::xml_document<char> *doc,
        TubeEntry tube, std::string id, float radius) {
    auto tube_group_node = doc->allocate_node(rapidxml::node_element, "g");
    append_attributes(doc, tube_group_node,
            { { "id", doc->allocate_string(id.c_str()) },
              { "data-col", tube.x_label }, { "data-row", tube.y_label } });

    auto tube_node = doc->allocate_node(rapidxml::node_element, "circle");
    append_attributes(doc, tube_node, { { "cx", std::to_string(tube.coords.x) },
            { "cy", std::to_string(tube.coords.y) }, { "r", std::to_string(radius) },
            { "class", "tube" }, });

    auto tooltip_node = doc->allocate_node(rapidxml::node_element, "title");
    tooltip_node->value(
            doc->allocate_string(
                    (std::string("Col=") + tube.x_label + " Row=" + tube.y_label).c_str()));
    tube_group_node->append_node(tooltip_node);
    tube_group_node->append_node(tube_node);
    auto number_node = doc->allocate_node(rapidxml::node_element, "text",
            doc->allocate_string(id.substr(3).c_str()));
    //number_node->value();
    append_attributes(doc, number_node, { { "class", "tube_num" },
            { "x", std::to_string(tube.coords.x) }, { "y", std::to_string(tube.coords.y) },
            { "transform-origin", std::to_string(tube.coords.x) + " " + std::to_string(tube.coords.y) },
            { "transform", "scale(1,-1)" }, });

    tube_group_node->append_node(number_node);
    return tube_group_node;

}

#endif 		// SVG_HPP

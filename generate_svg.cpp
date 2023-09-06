#include "generate_svg.hpp"

void append_attributes(rapidxml::xml_document<char> *doc,
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

rapidxml::xml_node<char>* add_dashed_line(rapidxml::xml_document<char> *doc,
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

rapidxml::xml_node<char>* add_label(rapidxml::xml_document<char> *doc,
        float x, float y,
        const char *label) {
    auto label_node = doc->allocate_node(rapidxml::node_element, "text",
            doc->allocate_string(label));

    append_attributes(doc, label_node, { { "x", std::to_string(x) },
            { "y", std::to_string(y) }, { "class", "label" }, });

    return label_node;
}

rapidxml::xml_node<char>* add_tube(rapidxml::xml_document<char> *doc,
        Tube tube, std::string id, float radius) {
    auto tube_group_node = doc->allocate_node(rapidxml::node_element, "g");
    append_attributes(doc, tube_group_node,
            { { "id", doc->allocate_string(id.c_str()) },
              { "data-col", tube.coords.x_label }, { "data-row", tube.coords.y_label } });

    auto tube_node = doc->allocate_node(rapidxml::node_element, "circle");
    append_attributes(doc, tube_node, { { "cx", std::to_string(tube.coords.x) },
            { "cy", std::to_string(tube.coords.y) }, { "r", std::to_string(radius) },
            { "class", "tube" }, });

    auto tooltip_node = doc->allocate_node(rapidxml::node_element, "title");
    tooltip_node->value(
            doc->allocate_string(
                    (std::string("Col=") + tube.coords.x_label + " Row=" + tube.coords.y_label).c_str()));
    tube_group_node->append_node(tooltip_node);
    tube_group_node->append_node(tube_node);
    auto number_node = doc->allocate_node(rapidxml::node_element, "text",
            doc->allocate_string(id.substr(3).c_str()));
    //number_node->value();
    append_attributes(doc, number_node, { { "class", "tube_num" },
            { "x", std::to_string(tube.coords.x) }, { "y", std::to_string(tube.coords.y) },
            { "transform-origin", std::to_string(tube.coors.x) + " " + std::to_string(tube.coords.y) },
            { "transform", "scale(1,-1)" }, });

    tube_group_node->append_node(number_node);
    return tube_group_node;

}

int generate_svg(InspectionSession insp_session) {

    std::cout << "Generating SVG..." << "\n";
    namespace po = boost::program_options;
    float min_x, width;
    float min_y, height;
    std::string font_size;
    std::string x_labels_param;
    std::string y_labels_param;
    std::vector<std::string> config_x_labels_coords;
    std::vector<std::string> config_y_labels_coords;

    try {
        po::options_description settings_desc("HX Settings");
        settings_desc.add_options()("min_x",
                po::value<float>(&min_x)->default_value(0),
                "viewBox X minimum coord");
        settings_desc.add_options()("width",
                po::value<float>(&width)->default_value(0), "viewBoxWidth");
        settings_desc.add_options()("min_y",
                po::value<float>(&min_y)->default_value(0),
                "viewBox Y minimum coord");
        settings_desc.add_options()("height",
                po::value<float>(&height)->default_value(0), "viewBox Height");
        settings_desc.add_options()("font_size",
                po::value<std::string>(&font_size)->default_value("0.25"),
                "Number font size in px");
        settings_desc.add_options()("x_labels",
                po::value<std::string>(&x_labels_param)->default_value("0"),
                "Where to locate x axis labels, can use several coords separated by space");
        settings_desc.add_options()("y_labels",
                po::value<std::string>(&y_labels_param)->default_value("0"),
                "Where to locate x axis labels, can use several coords separated by space");
        settings_desc.add_options()("help,h", "Help screen"); // what an strange syntax...
        // ("config", po::value<std::string>(), "Config file");
        po::variables_map vm;

        //po::store(po::parse_command_line(argc, argv, settings_desc), vm);
        std::filesystem::path config = insp_session.hx_directory / insp_session.hx / "config.ini";
        if (std::filesystem::exists(config)) {
            std::ifstream config_is = std::ifstream(config);
            po::store(po::parse_config_file(config_is, settings_desc, true),
                    vm);
        }
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << settings_desc << '\n';
        }

        boost::split(config_x_labels_coords, x_labels_param, boost::is_any_of(" "));
        boost::split(config_y_labels_coords, y_labels_param, boost::is_any_of(" "));

    } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
    }

    float tube_r = insp_session.tube_od / 2;

    // Create the SVG document
    rapidxml::xml_document<char> document;
    rapidxml::xml_document<char> *doc = &document;
    auto svg_node = doc->allocate_node(rapidxml::node_element, "svg");

    append_attributes(doc, svg_node,
            { { "xmlns", "http://www.w3.org/2000/svg" }, { "version", "1.1" },
                    { "id", "tubesheet_svg" },
                    { "viewBox", std::to_string(min_x)
                    + " " + std::to_string(min_y) + " " + std::to_string(width)
                    + " " + std::to_string(height) }, });

    auto style_node = doc->allocate_node(rapidxml::node_element, "style");
    append_attributes(doc, style_node, { { "type", "text/css" } });

    float stroke_width = stof(font_size) / 10;

    std::string style =
            std::string(".tube {stroke: black; stroke-width: "+ std::to_string(stroke_width) + " ; fill: white;} ")
                    + ".tube_num { text-anchor: middle; alignment-baseline: middle; font-family: sans-serif; font-size: "
                    + font_size
                    + "px; fill: black;}"
                    ".label { text-anchor: middle; alignment-baseline: middle; font-family: sans-serif; font-size: " + font_size + "; fill: red;}";

    style_node->value(style.c_str());

    svg_node->append_node(style_node);
    doc->append_node(svg_node);

    auto cartesian_g_node = doc->allocate_node(rapidxml::node_element, "g");
    append_attributes(doc, cartesian_g_node, { { "id", "cartesian" },
            { "transform", "scale(1,-1)" } }
    );
    svg_node->append_node(cartesian_g_node);

    auto x_axis = add_dashed_line(doc, 0, min_y, 0, min_y + height, stof(font_size));
    auto y_axis = add_dashed_line(doc, min_x, 0, min_x + width, 0, stof(font_size));
    cartesian_g_node->append_node(x_axis);
    cartesian_g_node->append_node(y_axis);

    for (auto config_coord : config_x_labels_coords) {
        for (auto [label, coord] : x_labels) {
            auto label_x = add_label(doc, coord,
                    std::stof(config_coord), label.c_str());
            append_attributes(doc, label_x,
                      { { "transform-origin", std::to_string(coord) + " " + config_coord },
                        { "transform", "scale(1, -1) rotate(270)" },}
            );
            cartesian_g_node->append_node(label_x);
        }
    }

    for (auto config_coord : config_y_labels_coords) {
        for (auto [label, coord] : y_labels) {
            auto label_y = add_label(doc, std::stof(config_coord),
                    coord, label.c_str());
            append_attributes(doc, label_y,
                    { { "transform-origin", config_coord + " " + std::to_string(coord) },
                      { "transform", "scale(1, -1)" }, }
            );
           cartesian_g_node->append_node(label_y);
        }
    }

    // Create an SVG circle element for each tube in the CSV data
    for (const auto &tube_pair : insp_session.tubes) {
        auto tube = tube_pair.second;

        auto tube_node = add_tube(doc, tube, tube_pair.first, tube_r);
        cartesian_g_node->append_node(tube_node);
    }

    // Write the SVG document to a file
    std::filesystem::path svg_path = insp_session.hx_directory / insp_session.hx / "tubesheet.svg";
    std::ofstream file(svg_path);
    file << document;
    file.close();

    return 0;
}

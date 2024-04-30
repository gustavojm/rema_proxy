#include <map>
#include <filesystem>

#include "spdlog/spdlog.h"
#include "HX.hpp"

const std::filesystem::path HX::hxs_path = "./HXs";

void HX::process_csv_from_disk(std::string hx) {
    std::filesystem::path csv_file = hxs_path / hx / "tubesheet.csv";
    SPDLOG_INFO("Reading {}", csv_file.string());

    std::ifstream filestream(csv_file);
    std::istream &inputstream = filestream;
    process_csv(csv_file.replace_extension(), inputstream);
}


void HX::process_csv(std::string hx_name, std::istream &stream) {
    io::CSVReader<7, io::trim_chars<' ', '\t'>, io::no_quote_escape<';'>> in(hx_name, stream);
    in.read_header(io::ignore_extra_column, "x_label", "y_label", "cl_x",
            "cl_y", "hl_x", "hl_y", "tube_id");
    std::string x_label, y_label;
    float cl_x, cl_y, hl_x, hl_y;
    std::string tube_id;
    while (in.read_row(x_label, y_label, cl_x, cl_y, hl_x, hl_y, tube_id)) {
        if (leg == "cold" || leg == "both") {
            tubes.insert( { std::string("CL_") + tube_id.substr(5),
                    { x_label, y_label, {cl_x, cl_y, 0 }} });
            svg.x_labels.insert(std::make_pair(x_label, cl_x));
            svg.y_labels.insert(std::make_pair(y_label, cl_y));
        }

        if (leg == "hot" || leg == "both") {
            tubes.insert( { std::string("HL_") + tube_id.substr(5),
                    { x_label, y_label, {hl_x, hl_y, 0 }} });
            svg.x_labels.insert(std::make_pair(x_label, hl_x));
            svg.y_labels.insert(std::make_pair(y_label, hl_y));
        }
    }
}

void HX::generate_svg(std::string hx) {
    SPDLOG_INFO("Generating SVG...");

    float min_x, width;
    float min_y, height;
    std::string font_size;
    std::string x_labels_param;
    std::string y_labels_param;
    std::vector<std::string> config_x_labels_coords;
    std::vector<std::string> config_y_labels_coords;

    try {
        std::filesystem::path config_file_path = hxs_path / hx / "config.json";
        std::ifstream config_file(config_file_path);

        if (config_file.is_open()) {
            nlohmann::json config;
            config = nlohmann::json::parse(config_file, nullptr, true, true);

            min_x = config.value("min_x", 0.0F);        
            min_y = config.value("min_y", 0.0F);
            width = config.value("width", 0.0F);
            height = config.value("height", 0.0F);
            font_size = config.value("font_size", "0.25");   // Number font size in px  
            x_labels_param = config.value("x_labels", "0");  // Where to locate x axis labels, can use several coords separated by space
            y_labels_param = config.value("y_labels", "0");  // Where to locate y axis labels, can use several coords separated by space
            boost::split(config_x_labels_coords, x_labels_param, boost::is_any_of(" "));
            boost::split(config_y_labels_coords, y_labels_param, boost::is_any_of(" "));
        } else {
            SPDLOG_WARN("{} not found", config_file_path.string());
        }
    } catch (std::exception &e) {
        SPDLOG_WARN(e.what());
    }

    float tube_r = tube_od / 2;

    // Create the SVG document
    rapidxml::xml_document<char> document;
    rapidxml::xml_document<char> *doc = &document;
    auto *svg_node = doc->allocate_node(rapidxml::node_element, "svg");

    append_attributes(doc, svg_node,
            { { "xmlns", "http://www.w3.org/2000/svg" }, { "version", "1.1" },
                    { "id", "tubesheet_svg" },
                    { "viewBox", std::to_string(min_x)
                    + " " + std::to_string(min_y) + " " + std::to_string(width)
                    + " " + std::to_string(height) }, });

    auto *style_node = doc->allocate_node(rapidxml::node_element, "style");
    append_attributes(doc, style_node, { { "type", "text/css" } });

    float stroke_width = stof(font_size) / 10;

    std::string style =
                    ".tube {stroke: black; stroke-width: "+ std::to_string(stroke_width) + " ; fill: white;} "
                    + ".tube_num { text-anchor: middle; alignment-baseline: middle; font-family: sans-serif; font-size: "
                    + font_size
                    + "px; fill: black;}"
                    ".label { text-anchor: middle; alignment-baseline: middle; font-family: sans-serif; font-size: " + font_size + "; fill: red;}";

    style_node->value(style.c_str());

    svg_node->append_node(style_node);
    doc->append_node(svg_node);

    auto *cartesian_g_node = doc->allocate_node(rapidxml::node_element, "g");
    append_attributes(doc, cartesian_g_node, { { "id", "cartesian" },
            { "transform", "scale(1,-1)" } }
    );
    svg_node->append_node(cartesian_g_node);

    auto *x_axis = add_dashed_line(doc, 0, min_y, 0, min_y + height, stof(font_size));
    auto *y_axis = add_dashed_line(doc, min_x, 0, min_x + width, 0, stof(font_size));
    cartesian_g_node->append_node(x_axis);
    cartesian_g_node->append_node(y_axis);

    for (const auto &config_coord : config_x_labels_coords) {
        for (auto [label, coord] : svg.x_labels) {
            auto *label_x = add_label(doc, coord,
                    std::stof(config_coord), label.c_str());
            append_attributes(doc, label_x,
                      { { "transform-origin", std::to_string(coord) + " " + config_coord },
                        { "transform", "scale(1, -1) rotate(270)" },}
            );
            cartesian_g_node->append_node(label_x);
        }
    }

    for (const auto &config_coord : config_y_labels_coords) {
        for (auto [label, coord] : svg.y_labels) {
            auto *label_y = add_label(doc, std::stof(config_coord),
                    coord, label.c_str());
            append_attributes(doc, label_y,
                    { { "transform-origin", config_coord + " " + std::to_string(coord) },
                      { "transform", "scale(1, -1)" }, }
            );
           cartesian_g_node->append_node(label_y);
        }
    }

    // Create an SVG circle element for each tube in the CSV data
    for (const auto &tube_pair : tubes) {
        auto tube = tube_pair.second;

        auto *tube_node = add_tube(doc, tube, tube_pair.first, tube_r);
        cartesian_g_node->append_node(tube_node);
    }

    // // Write the SVG document to a file
    // std::filesystem::path svg_path = hx_directory / hx / "tubesheet.svg";
    // std::ofstream file(svg_path);
    // file << document;
    // file.close();
    std::ostringstream stream;
    stream << document;
    tubesheet_svg = stream.str();
}

void HX::load_config(std::string hx) {
    tubesheet_csv = hxs_path / hx / "tubesheet.csv";
    
    try {
        std::filesystem::path config_file_path = hxs_path / hx / "config.json";
        std::ifstream config_file(config_file_path);
        if (config_file.is_open()){
            nlohmann::json config;
            config_file >> config;

            leg = config.value("leg", "both");
            tube_od = config.value("tube_od", 1.F);
            unit = config.value("unit", "inch");
            scale = (unit == "inch" ? 1 : 25.4);
        } else {
            SPDLOG_WARN("{} not found", config_file_path.string());
        }
    } catch (std::exception &e) {
        SPDLOG_WARN(e.what());
    }
}

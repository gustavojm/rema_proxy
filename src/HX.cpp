#include <filesystem>
#include <map>

#include "HX.hpp"
#include "spdlog/spdlog.h"

const std::filesystem::path HX::hxs_path = "./HXs";

void HX::process_csv_from_disk(std::string hx) {
    load_config_from_disk(hx);
    std::filesystem::path csv_file = hxs_path / hx / "tubesheet.csv";
    SPDLOG_INFO("Reading {}", csv_file.string());

    std::ifstream filestream(csv_file);
    std::istream &inputstream = filestream;
    process_csv(csv_file.replace_extension(), inputstream);
}

void HX::process_csv(std::string hx_name, std::istream &stream) {
    io::CSVReader<7, io::trim_chars<' ', '\t'>, io::no_quote_escape<';'>> in(hx_name, stream);
    in.read_header(io::ignore_extra_column, "x_label", "y_label", "cl_x", "cl_y", "hl_x", "hl_y", "tube_id");
    std::string x_label, y_label;
    float cl_x, cl_y, hl_x, hl_y;
    std::string tube_id;

    std::set<std::pair<std::string, float>> x_labels;       // sets for uniqueness
    std::set<std::pair<std::string, float>> y_labels;   

    while (in.read_row(x_label, y_label, cl_x, cl_y, hl_x, hl_y, tube_id)) {
        if (leg == "cold" || leg == "both") {
            tubes.insert({ std::string("CL_") + tube_id.substr(5), { x_label, y_label, { cl_x, cl_y, 0 } } });
            x_labels.insert(std::make_pair(x_label, cl_x));
            y_labels.insert(std::make_pair(y_label, cl_y));
        }

        if (leg == "hot" || leg == "both") {
            tubes.insert({ std::string("HL_") + tube_id.substr(5), { x_label, y_label, { hl_x, hl_y, 0 } } });
            x_labels.insert(std::make_pair(x_label, hl_x));
            y_labels.insert(std::make_pair(y_label, hl_y));
        }
    }

    create_label_coords(x_labels, y_labels);
}

void HX::create_label_coords(std::set<std::pair<std::string, float>> x_labels, std::set<std::pair<std::string, float>> y_labels) {
    for (const auto &config_coord : svg.config_x_labels_coords) {
        for (auto [label, coord] : x_labels) {
            Point3D label_coord = Point3D(coord, std::stof(config_coord), 0);
            svg.x_labels.push_back({label, label_coord});
        }
    }

    for (const auto &config_coord : svg.config_y_labels_coords) {
        for (auto [label, coord] : y_labels) {
            Point3D label_coord = Point3D(std::stof(config_coord), coord, 0);
            svg.y_labels.push_back({label, label_coord});
        }
    }
}

void HX::generate_svg() {
    SPDLOG_INFO("Generating SVG...");

    float tube_r = tube_od / 2;

    // Create the SVG document
    rapidxml::xml_document<char> document;
    rapidxml::xml_document<char>* doc = &document;
    auto* svg_node = doc->allocate_node(rapidxml::node_type::node_element, "svg");

    append_attributes(
        doc,
        svg_node,
        {
            { "xmlns", "http://www.w3.org/2000/svg" },
            { "version", "1.1" },
            { "id", "tubesheet_svg" },
            { "viewBox",
              std::to_string(svg.min_x) + " " + std::to_string(svg.min_y) + " " + std::to_string(svg.width) + " " +
                  std::to_string(svg.height) },
        });

    auto* style_node = doc->allocate_node(rapidxml::node_type::node_element, "style");
    append_attributes(doc, style_node, { { "type", "text/css" } });

    float stroke_width = stof(svg.font_size) / 10;

    std::string style =
        ".tube {stroke: black; stroke-width: " + std::to_string(stroke_width) + " ; fill: white;} " +
        ".tube_num { text-anchor: middle; alignment-baseline: middle; font-family: sans-serif; font-size: " + svg.font_size +
        "px; fill: black;}"
        ".label { text-anchor: middle; alignment-baseline: middle; font-family: sans-serif; font-size: " +
        svg.font_size + "; fill: red;}";

    style_node->value(style.c_str());

    svg_node->append_node(style_node);
    doc->append_node(svg_node);

    auto* cartesian_g_node = doc->allocate_node(rapidxml::node_type::node_element, "g");
    append_attributes(doc, cartesian_g_node, { { "id", "cartesian" }, { "transform", "scale(1,-1)" } });
    svg_node->append_node(cartesian_g_node);

    auto* x_axis = add_dashed_line(doc, 0, svg.min_y, 0, svg.min_y + svg.height, stof(svg.font_size));
    auto* y_axis = add_dashed_line(doc, svg.min_x, 0, svg.min_x + svg.width, 0, stof(svg.font_size));
    cartesian_g_node->append_node(x_axis);
    cartesian_g_node->append_node(y_axis);

    for (auto& [label, coord] : svg.x_labels) {
        auto* label_x = add_label(doc, coord.x, coord.y, label.c_str());
        append_attributes(
            doc,
            label_x,
            {
                { "transform-origin", std::to_string(coord.x) + " " + std::to_string(coord.y) },
                { "transform", "scale(1, -1) rotate(270)" },
            });
        cartesian_g_node->append_node(label_x);
    }

    for (auto& [label, coord] : svg.y_labels) {
        auto* label_y = add_label(doc, coord.x, coord.y, label.c_str());
        append_attributes(
            doc,
            label_y,
            {
                { "transform-origin", std::to_string(coord.x) + " " + std::to_string(coord.y) },
                { "transform", "scale(1, -1)" },
            });
        cartesian_g_node->append_node(label_y);
    }

    // Create an SVG circle element for each tube in the CSV data
    for (const auto &tube_pair : tubes) {
        auto tube = tube_pair.second;

        auto* tube_node = add_tube(doc, tube, tube_pair.first, tube_r);
        cartesian_g_node->append_node(tube_node);
    }

    // Write the SVG document to a file
    // std::filesystem::path svg_path = hxs_path / hx / "tubesheet.svg";
    // std::ofstream file(svg_path);
    // file << document;

    std::ostringstream stream;
    stream << document;
    tubesheet_svg = stream.str();
}

void HX::load_config_from_disk(std::string hx) {
    std::filesystem::path tubesheet_csv = hxs_path / hx / "tubesheet.csv";

    try {
        std::filesystem::path config_file_path = hxs_path / hx / "config.json";
        std::ifstream config_file(config_file_path);
        if (config_file.is_open()) {
            nlohmann::json config = nlohmann::json::parse(config_file, nullptr, true, true);
            load_config(config);
        } else {
            SPDLOG_WARN("{} not found", config_file_path.string());
        }
    } catch (std::exception &e) {
        SPDLOG_WARN(e.what());
    }
}

void HX::load_config(nlohmann::json config) {
    leg = config.value("leg", "both");
    tube_od = config.value("tube_od", 1.F);
    unit = config.value("unit", "inch");
    scale = (unit == "inch" ? 1 : 25.4);

    svg.min_x = config.value("min_x", 0.0F);
    svg.min_y = config.value("min_y", 0.0F);
    svg.width = config.value("width", 0.0F);
    svg.height = config.value("height", 0.0F);
    svg.font_size = config.value("font_size", "0.25"); // Number font size in px
    svg.x_labels_param =
        config.value("x_labels", "0"); // Where to locate x axis labels, can use several coords separated by space
    svg.y_labels_param =
        config.value("y_labels", "0"); // Where to locate y axis labels, can use several coords separated by space
    boost::split(svg.config_x_labels_coords, svg.x_labels_param, boost::is_any_of(" "));
    boost::split(svg.config_y_labels_coords, svg.y_labels_param, boost::is_any_of(" "));
}

bool HX::create(std::string hx_name, std::string tubesheet_csv, std::string config_json) {
    try {
        if (std::filesystem::is_directory(hxs_path / hx_name)) {
            throw std::invalid_argument("HX with the same name already exists");
        }
        std::filesystem::create_directory(hxs_path / hx_name);
        std::filesystem::path tubesheet_csv_path(hxs_path / hx_name / "tubesheet.csv");
        // Open a file for writing
        std::ofstream tubesheet_csv_file(tubesheet_csv_path);
        tubesheet_csv_file << tubesheet_csv;

        std::filesystem::path config_json_path(hxs_path / hx_name / "config.json");
        // Open a file for writing
        std::ofstream config_json_file(config_json_path);
        config_json_file << config_json;
        return true;
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Error creating HX: " << e.what() << std::endl;
        return false;
    }
}

bool HX::erase(std::string hx_name) {
    try {
        std::filesystem::path normalized_path = std::filesystem::canonical(hxs_path / hx_name);
        std::filesystem::remove_all(normalized_path);
        return true;
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Error deleting directory: " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::string> HX::list() {
    std::vector<std::string> res;
    for (const auto &entry : std::filesystem::directory_iterator(hxs_path)) {
        if (entry.is_directory()) {
            res.push_back(entry.path().filename());
        }
    }
    return res;
}

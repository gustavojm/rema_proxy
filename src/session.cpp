#include <open3d/Open3D.h>
#include <Eigen/Eigen>
#include <spdlog/spdlog.h>

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <csv.h>

#include "session.hpp"
#include "svg.hpp"

Session::Session() noexcept = default;

Session::Session(const std::filesystem::path &session_file) {
    load(session_file);
}

void Session::copy_tubes_to_aligned_tubes() {
    for (auto [id, tube] : tubes) {
        aligned_tubes[id] = tube.coords;
    }
}

void Session::process_csv() {
    std::filesystem::path csv_file = hx_directory / hx / "tubesheet.csv";
    SPDLOG_INFO("Reading {}", csv_file.string());

    io::CSVReader<7, io::trim_chars<' ', '\t'>, io::no_quote_escape<';'>> in(csv_file);
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
    generate_svg();

    copy_tubes_to_aligned_tubes();

    calculate_aligned_tubes();
}

Session::Session(const std::string &session_name,
        const std::filesystem::path &hx_path) : name(session_name),
        hx(hx_path) {

    hx_directory = std::filesystem::path("HXs");

    tubesheet_csv = hx_directory / hx / "tubesheet.csv";
    
    try {
        std::filesystem::path config_file_path = hx_directory / hx / "config.json";
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

    process_csv();
}

bool Session::load(std::string session_name) {
    std::filesystem::path session_path = sessions_dir / (session_name + std::string(".json"));
    std::ifstream i_file_stream(session_path);

    nlohmann::json json;
    i_file_stream >> json;
    *this = json;
    loaded = true;    
    name = session_name;

    return true;
}

void Session::load_plan(std::string plan_name, std::istream &stream) {
    // Parse the CSV file to extract the data for the plan
    io::CSVReader<4, io::trim_chars<' ', '\t'>, io::no_quote_escape<';'>> ip(
            plan_name, stream);
    ip.read_header(io::ignore_extra_column, "SEQ", "ROW", "COL", "TUBE");
    int seq;
    std::string row, col;
    std::string tube_num;
    while (ip.read_row(seq, row, col, tube_num)) {
        std::string tube_num_stripped = tube_num.substr(5);
        plans[plan_name][tube_num_stripped] = PlanEntry {
                seq, row, col, false };
    }
}

void Session::load_plan_from_disk(std::filesystem::path plan_file) {
    std::ifstream filestream(plan_file);
    std::istream &inputstream = filestream;
    load_plan(plan_file.filename().replace_extension(), inputstream);
}

std::string Session::load_plans() {
    std::stringstream out;
    std::filesystem::path plans_path = hx_directory / hx / "plans";
    if (std::filesystem::is_directory(plans_path)) {
        for (const auto &entry : std::filesystem::directory_iterator(
                plans_path)) {
            if (!(entry.path().filename().extension() == ".csv")) {
                continue;
            }
            out << "Procesando plan: " << entry.path().filename() << "\n";
            load_plan_from_disk(entry);
        }
    }
    loaded = true;
    return out.str();
}

std::map<std::string, PlanEntry> Session::plan_get(const std::string &plan) {
    last_selected_plan = plan;

    auto it = plans.find(plan);
    if (it != plans.end()) {
        return it->second;
    } else  {
        return {};
    }
}

void Session::plan_remove(const std::string &plan) {
    last_selected_plan = "";

    auto it = plans.find(plan);
    if (it != plans.end()) {
        plans.erase(it);
    }
}

void Session::save_to_disk() const {
    std::filesystem::path session_file = sessions_dir / (name + std::string(".json"));
    std::ofstream file(session_file);
    nlohmann::json json(*this);
    json.erase("name");
    json.erase("last_write_time");    
    file << json;
}

void Session::set_selected_plan(std::string &plan) {
    last_selected_plan = plan;
    changed = true;
}

std::string Session::get_selected_plan() const {
    return last_selected_plan;
}

void Session::set_tube_executed(std::string &plan,
        std::string &tube_id, bool state) {
    plans[plan][tube_id].executed = state;
    changed = true;
}

int Session::total_tubes_in_plans() {
    int total = 0;
    for (auto plan: plans) {
        total += plan.second.size();
    }
    return total;
}

int Session::total_tubes_executed() {
    int total = 0;
    for (auto &[key, value]: plans) {
        total += std::count_if(value.begin(), value.end(), [](auto &entry) {
            return entry.second.executed;
        });
    }
    return total;
}

void Session::cal_points_add_update(const std::string &tube_id, const std::string &col, const std::string &row, Point3D &ideal_coords, Point3D &determined_coords)  {
    CalPointEntry cpe = {
            col,
            row,
            ideal_coords,
            determined_coords,
            true,
    };
    cal_points[tube_id] = cpe;
    changed = true;
}

void Session::cal_points_delete(const std::string &tube_id)  {
    cal_points.erase(tube_id);
    changed = true;
}

Point3D Session::get_tube_coordinates(const std::string &tube_id, bool ideal = true) {
    if (ideal) {
        if (auto iter = tubes.find(tube_id); iter != tubes.end()) {
            return iter->second.coords;
        }
    } else {
        if (auto iter = aligned_tubes.find(tube_id); iter != aligned_tubes.end()) {
            return iter->second;
        }
    }
    return {};
};

void Session::generate_svg() {
    SPDLOG_INFO("Generating SVG...");

    float min_x, width;
    float min_y, height;
    std::string font_size;
    std::string x_labels_param;
    std::string y_labels_param;
    std::vector<std::string> config_x_labels_coords;
    std::vector<std::string> config_y_labels_coords;

    try {
        std::filesystem::path config_file_path = hx_directory / hx / "config.json";
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


std::map<std::string, Point3D>& Session::calculate_aligned_tubes() {    
    SPDLOG_INFO("Aligning Tubes...");
    //std::vector<Point3D> src_points = { { 1.625, 0.704, 0 },
    //        {16.656, 2.815, 0},
    //        {71.125, 3.518, 0},
    //        };

    //std::vector<Point3D> dst_points = { { 11.625, 10.704, 1 },
    //        {26.656, 12.815, 2},
    //        {81.125, 13.518, 3},
    //        };

    // Convert vector of Point3D to Open3D point clouds
    open3d::geometry::PointCloud source_cloud;
    open3d::geometry::PointCloud target_cloud;

    int used_points = 0;
    for (const auto &cal_point: cal_points) {
        if (cal_point.second.determined) {
            source_cloud.points_.emplace_back(
                    Eigen::Vector3d(cal_point.second.ideal_coords.x, cal_point.second.ideal_coords.y, cal_point.second.ideal_coords.z));

            target_cloud.points_.emplace_back(
                    Eigen::Vector3d(cal_point.second.determined_coords.x, cal_point.second.determined_coords.y, cal_point.second.determined_coords.z));
            used_points++;
        }
    }

    if (used_points < 2) {
        SPDLOG_ERROR("At least two alignment points are required");
        copy_tubes_to_aligned_tubes();
        return aligned_tubes;
    }

    // Set ICP parameters and perform ICP
    open3d::pipelines::registration::ICPConvergenceCriteria icp_criteria;
    icp_criteria.max_iteration_ = 50;
    icp_criteria.relative_fitness_ = 1e-6;
    icp_criteria.relative_rmse_ = 1e-6;

    bool with_scaling = true;
    auto result = open3d::pipelines::registration::RegistrationICP(source_cloud,
            target_cloud, 1000, Eigen::Matrix4d::Identity(),
            open3d::pipelines::registration::TransformationEstimationPointToPoint(with_scaling),
            icp_criteria);

    Eigen::Matrix4d transformation_matrix = result.transformation_;
    std::cout << transformation_matrix << std::endl;

    // Transform the source point cloud
    for (const auto& [id, tube] : tubes) {

        // Code from Open3D Geometry3D.cpp TransformPoints() method
        Eigen::Vector4d point(tube.coords.x, tube.coords.y, tube.coords.z, 1.0);
        Eigen::Vector4d new_point = transformation_matrix * point;
        Eigen::Vector3d transformed_point = new_point.head<3>() / new_point(3);

        aligned_tubes[id] = {transformed_point.x(), transformed_point.y(), transformed_point.z()};
    }

    return aligned_tubes;
}




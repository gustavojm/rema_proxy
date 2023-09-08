#include <Open3D/Geometry/PointCloud.h>
#include <Open3D/Registration/ColoredICP.h>
#include <Open3D/IO/ClassIO/ImageIO.h>
#include <Eigen/Eigen>

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <csv.h>
#include "inspection-session.hpp"
#include "boost/program_options.hpp"
#include "svg.hpp"

InspectionSession::InspectionSession() :
        loaded(false) {
}

InspectionSession::InspectionSession(
        std::filesystem::path inspection_session_file) {

    std::ifstream i(inspection_session_file);
    nlohmann::json j;
    i >> j;
    *this = j;
    this->loaded = true;
    this->name = inspection_session_file.filename().replace_extension();
}

bool InspectionSession::load(std::string session_name) {

    std::filesystem::path session_path = insp_sessions_dir / (session_name + std::string(".json"));

    std::ifstream i(session_path);

    nlohmann::json j;
    i >> j;
    *this = j;
    this->loaded = true;
    this->name = session_name;

    std::filesystem::path csv_file = hx_directory / hx / "tubesheet.csv";
    std::cout << "Reading " << csv_file << "\n";

    io::CSVReader<7, io::trim_chars<' ', '\t'>, io::no_quote_escape<';'>> in(csv_file);
    in.read_header(io::ignore_extra_column, "x_label", "y_label", "cl_x",
            "cl_y", "hl_x", "hl_y", "tube_id");
    std::string x_label, y_label;
    float cl_x, cl_y, hl_x, hl_y;
    std::string tube_id;
    while (in.read_row(x_label, y_label, cl_x, cl_y, hl_x, hl_y, tube_id)) {
        if (leg == "cold" || leg == "both") {
            tubes.insert( { std::string(std::string("CL_") + tube_id.substr(5)),
                    { x_label, y_label, {cl_x, cl_y, 0 }} });
            svg.x_labels.insert(std::make_pair(x_label, cl_x));
            svg.y_labels.insert(std::make_pair(y_label, cl_y));
        }

        if (leg == "hot" || leg == "both") {
            tubes.insert( { std::string(std::string("HL_") + tube_id.substr(5)),
                    { x_label, y_label, {hl_x, hl_y, 0 }} });
            svg.x_labels.insert(std::make_pair(x_label, hl_x));
            svg.y_labels.insert(std::make_pair(y_label, hl_y));
        }
    }

    generate_svg();
    calculate_aligned_tubes();

    return true;
}

InspectionSession::InspectionSession(std::string session_name,
        std::filesystem::path hx) : name(session_name),
        hx(hx) {

    hx_directory = std::filesystem::path("HXs");

    tubesheet_csv = hx_directory / hx / "tubesheet.csv";
    tubesheet_svg = hx_directory / hx / "tubesheet.svg";

    try {
        namespace po = boost::program_options;
        po::options_description settings_desc("HX Settings");
        settings_desc.add_options()("leg",
                po::value<std::string>(&leg)->default_value("both"),
                "Leg (hot, cold, both)");
        settings_desc.add_options()("tube_od",
                po::value<float>(&tube_od)->default_value(1.f),
                "Tube Outside Diameter");
        settings_desc.add_options()("unit",
                po::value<std::string>(&unit)->default_value("inch"),
                "inch/mm");

        po::variables_map vm;

        std::filesystem::path config = hx_directory / hx / "config.ini";
        if (std::filesystem::exists(config)) {
            std::ifstream config_is = std::ifstream(config);
            po::store(po::parse_config_file(config_is, settings_desc, true),
                    vm);
        }
        po::notify(vm);
    } catch (std::exception &e) {
        std::cout << e.what() << "\n";
    }
}

std::string InspectionSession::load_plans() {
    std::stringstream out;
    std::filesystem::path insp_plans_path = hx_directory / hx / "insp_plans";
    for (const auto &entry : std::filesystem::directory_iterator(
            insp_plans_path)) {
        if (!(entry.path().filename().extension() == ".csv")) {
            continue;
        }
        out << "Procesando plan: " << entry.path().filename() << "\n";

        // Parse the CSV file to extract the data for the plan
        io::CSVReader<4, io::trim_chars<' ', '\t'>, io::no_quote_escape<';'> > ip(
                entry.path());
        ip.read_header(io::ignore_extra_column, "SEQ", "ROW", "COL", "TUBE");
        int seq;
        std::string row, col;
        std::string tube_num;
        while (ip.read_row(seq, row, col, tube_num)) {
            std::string tube_num_stripped = tube_num.substr(5);
            insp_plans[entry.path().filename().replace_extension()][tube_num_stripped] = InspectionPlanEntry {
                    seq, row, col, false };
        }
    }
    loaded = true;
    return out.str();
}

std::map<std::string, struct InspectionPlanEntry> InspectionSession::inspection_plan_get(std::string insp_plan) {
    last_selected_plan = insp_plan;

    auto it = insp_plans.find(insp_plan);
    if (it != insp_plans.end()) {
        return it->second;
    } else  {
        return std::map<std::string, struct InspectionPlanEntry>();
    }
}

void InspectionSession::save_to_disk() const {
    std::filesystem::path session_file = insp_sessions_dir / (name + std::string(".json"));
    std::ofstream file(session_file);
    nlohmann::json j(*this);
    j.erase("name");
    j.erase("last_write_time");
    file << j;
}

void InspectionSession::set_selected_plan(std::string plan) {
    last_selected_plan = plan;
    changed = true;
}

std::string InspectionSession::get_selected_plan() const {
    return last_selected_plan;
}

void InspectionSession::set_tube_inspected(std::string tube_id, bool state) {
    insp_plans[last_selected_plan][tube_id].inspected = state;
    changed = true;
}

void InspectionSession::set_tube_inspected(std::string insp_plan,
        std::string tube_id, bool state) {
    insp_plans[insp_plan][tube_id].inspected = state;
    changed = true;
}

void InspectionSession::cal_points_add_update(std::string tube_id, std::string col, std::string row, Point3D ideal_coords, Point3D determined_coords)  {
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

void InspectionSession::cal_points_delete(std::string tube_id)  {
    cal_points.erase(tube_id);
    changed = true;
}

Point3D InspectionSession::get_tube_coordinates(std::string tube_id, bool ideal) {
    return Point3D();
};

void InspectionSession::generate_svg() {
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
        std::filesystem::path config = hx_directory / hx / "config.ini";
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

    float tube_r = tube_od / 2;

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
        for (auto [label, coord] : svg.x_labels) {
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
        for (auto [label, coord] : svg.y_labels) {
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
    for (const auto &tube_pair : tubes) {
        auto tube = tube_pair.second;

        auto tube_node = add_tube(doc, tube, tube_pair.first, tube_r);
        cartesian_g_node->append_node(tube_node);
    }

    // Write the SVG document to a file
    std::filesystem::path svg_path = hx_directory / hx / "tubesheet.svg";
    std::ofstream file(svg_path);
    file << document;
    file.close();
}


std::map<std::string, Point3D>& InspectionSession::calculate_aligned_tubes() {
    std::cout << "Aligning Tubes... \n";
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

    for (auto cal_point: cal_points) {
        if (cal_point.second.determined) {
            source_cloud.points_.push_back(
                    Eigen::Vector3d(cal_point.second.ideal_coords.x, cal_point.second.ideal_coords.y, cal_point.second.ideal_coords.z));

            target_cloud.points_.push_back(
                    Eigen::Vector3d(cal_point.second.determined_coords.x, cal_point.second.determined_coords.y, cal_point.second.determined_coords.z));

        }
    }

    // Set ICP parameters and perform ICP
    open3d::registration::ICPConvergenceCriteria icp_criteria;
    icp_criteria.max_iteration_ = 50;
    icp_criteria.relative_fitness_ = 1e-6;
    icp_criteria.relative_rmse_ = 1e-6;
    auto result = open3d::registration::RegistrationICP(source_cloud,
            target_cloud, 1000, Eigen::Matrix4d::Identity(),
            open3d::registration::TransformationEstimationPointToPoint(true),
            icp_criteria);

    // Get the transformation matrix
    Eigen::Matrix4d transformation_matrix = result.transformation_;
    std::cout << transformation_matrix << std::endl;

    // Transform the source point cloud
    for (const auto &tube : tubes) {
        open3d::geometry::PointCloud one_tube_cloud;
        one_tube_cloud.points_.push_back(
                Eigen::Vector3d(tube.second.coords.x, tube.second.coords.y, tube.second.coords.z));

        one_tube_cloud.Transform(transformation_matrix);
        aligned_tubes[tube.first] = {(*one_tube_cloud.points_.begin()).x(), (*one_tube_cloud.points_.begin()).y(), (*one_tube_cloud.points_.begin()).z()};
    }

    return aligned_tubes;
}




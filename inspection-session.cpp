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

    // Parse the CSV file to extract the data for each tube
    io::CSVReader<5, io::trim_chars<' ', '\t'>, io::no_quote_escape<';'>> in(
            hx_directory / hx / "tubesheet.csv");
    in.read_header(io::ignore_extra_column, "cl_x", "cl_y", "hl_x", "hl_y",
            "tube_id");
    double cl_x, cl_y, hl_x, hl_y;
    std::string tube_id;

    while (in.read_row(cl_x, cl_y, hl_x, hl_y, tube_id)) {
        if (leg == "cold" || leg == "both") {
            tubes[std::string("CL_") + tube_id.substr(5)] = { cl_x, cl_y, 0 };
        }

        if (leg == "hot" || leg == "both") {
            tubes[std::string("HL_") + tube_id.substr(5)] = { hl_x, hl_y, 0 };
        }
    }

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

void InspectionSession::cal_points_add(std::string tube_id, std::string col, std::string row, Point3D ideal_coords, Point3D determined_coords)  {
    CalPointEntry cpe = {
            col,
            row,
            ideal_coords,
            determined_coords,
            false,
    };
    cal_points[tube_id] = cpe;
    changed = true;
}

void InspectionSession::cal_points_delete(std::string tube_id)  {
    cal_points.erase(tube_id);
    changed = true;
}

void InspectionSession::cal_points_set_determined_coords(std::string tube_id, Point3D determined_coords)  {
    cal_points[tube_id].determined_coords = determined_coords;
    cal_points[tube_id].determined = true;
    changed = true;
}

Point3D InspectionSession::get_tube_coordinates(std::string tube_id, bool ideal) {
    return Point3D();
};

std::map<std::string, Point3D> InspectionSession::calculate_aligned_tubes() {
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
    std::map<std::string, Point3D> aligned_tubes;
    for (const auto &tube : tubes) {
        open3d::geometry::PointCloud one_tube_cloud;
        one_tube_cloud.points_.push_back(
                Eigen::Vector3d(tube.second.x, tube.second.y, tube.second.z));

        one_tube_cloud.Transform(transformation_matrix);
        aligned_tubes[tube.first] = {(*one_tube_cloud.points_.begin()).x(), (*one_tube_cloud.points_.begin()).y(), (*one_tube_cloud.points_.begin()).z()};
    }

    return aligned_tubes;
}




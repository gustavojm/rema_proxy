#include <open3d/Open3D.h>
#include <Eigen/Eigen>
#include <spdlog/spdlog.h>

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <csv.h>

#include "session.hpp"

Session::Session() noexcept = default;

Session::Session(const std::filesystem::path &session_file) {
    load(session_file);
}

void Session::copy_tubes_to_aligned_tubes() {
    for (auto [id, tube] : hx.tubes) {
        aligned_tubes[id] = tube.coords;
    }
}

Session::Session(const std::string &session_name,
        const std::filesystem::path &hx_dir_) : name(session_name),
        hx_dir(hx_dir_) {

    hx.process_csv_from_disk(hx_dir);
    hx.generate_svg();
    copy_tubes_to_aligned_tubes();
    calculate_aligned_tubes();
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
    
    std::filesystem::path plans_path = HX::hxs_path / hx_dir / "plans";
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
        if (auto iter = hx.tubes.find(tube_id); iter != hx.tubes.end()) {
            return iter->second.coords;
        }
    } else {
        if (auto iter = aligned_tubes.find(tube_id); iter != aligned_tubes.end()) {
            return iter->second;
        }
    }
    return {};
};


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
    for (const auto& [id, tube] : hx.tubes) {

        // Code from Open3D Geometry3D.cpp TransformPoints() method
        Eigen::Vector4d point(tube.coords.x, tube.coords.y, tube.coords.z, 1.0);
        Eigen::Vector4d new_point = transformation_matrix * point;
        Eigen::Vector3d transformed_point = new_point.head<3>() / new_point(3);

        aligned_tubes[id] = {transformed_point.x(), transformed_point.y(), transformed_point.z()};
    }

    return aligned_tubes;
}




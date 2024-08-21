#include <Eigen/Eigen>
#include <open3d/Open3D.h>
#include <spdlog/spdlog.h>

#include <csv.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include "session.hpp"

Session::Session() : transformation_matrix(Eigen::Matrix4d::Identity()) {};

Session::Session(const std::filesystem::path& session_file) {
    Session();
    load(session_file);
}

Session::Session(const std::string& session_name, const std::filesystem::path& hx_dir_)
    : name(session_name), hx_dir(hx_dir_) {
    Session();
    hx.process_csv_from_disk(hx_dir);
    hx.generate_svg();
}

void Session::delete_session(std::string session_name) {
    std::filesystem::remove(sessions_dir / (session_name + std::string(".json")));
}

bool Session::load(const std::string& session_name) {
    std::filesystem::path session_path = sessions_dir / (session_name + std::string(".json"));
    std::ifstream i_file_stream(session_path);

    // nlohmann::json json;
    // i_file_stream >> json;
    from_json_from_disk(nlohmann::json::parse(i_file_stream));

    is_loaded = true;
    name = session_name;
    return true;
}

void Session::load_plan(const std::string& plan_name, std::istream& stream) {
    // Parse the CSV file to extract the data for the plan
    io::CSVReader<4, io::trim_chars<' ', '\t'>, io::no_quote_escape<';'>> ip(plan_name, stream);
    ip.read_header(io::ignore_extra_column, "SEQ", "ROW", "COL", "TUBE");
    int seq;
    std::string row, col;
    std::string tube_num;
    while (ip.read_row(seq, row, col, tube_num)) {
        std::string tube_num_stripped = tube_num.substr(5);
        plans[plan_name][tube_num_stripped] = PlanEntry{ seq, row, col, false };
    }
}

void Session::load_plan_from_disk(const std::filesystem::path& plan_file) {
    std::ifstream filestream(plan_file);
    std::istream& inputstream = filestream;
    load_plan(plan_file.filename().replace_extension(), inputstream);
}

std::string Session::load_plans() {
    std::stringstream out;

    std::filesystem::path plans_path = HX::hxs_path / hx_dir / "plans";
    if (std::filesystem::is_directory(plans_path)) {
        for (const auto& entry : std::filesystem::directory_iterator(plans_path)) {
            if (!(entry.path().filename().extension() == ".csv")) {
                continue;
            }
            out << "Procesando plan: " << entry.path().filename() << "\n";
            load_plan_from_disk(entry);
        }
    }
    is_loaded = true;
    return out.str();
}

std::map<std::string, PlanEntry> Session::plan_get(const std::string& plan) {
    last_selected_plan = plan;

    auto it = plans.find(plan);
    if (it != plans.end()) {
        return it->second;
    } else {
        return {};
    }
}

void Session::plan_remove(const std::string& plan) {
    last_selected_plan = "";

    auto it = plans.find(plan);
    if (it != plans.end()) {
        plans.erase(it);
    }
}

Point3D Session::from_rema_to_ui(Point3D coords, Tool* tool) {
    if (tool) {
        return ((coords - tool->offset) * hx.scale);
    } else {
        return (coords * hx.scale);
    }
}

double Session::from_rema_to_ui(double meassure) {
    return (meassure * hx.scale);
}

Point3D Session::from_ui_to_rema(Point3D coords, Tool* tool) {
    if (tool) {
        return ((coords / hx.scale) + tool->offset);
    } else {
        return (coords / hx.scale);
    }
}

double Session::from_ui_to_rema(double meassure) {
    return (meassure / hx.scale);
}

std::vector<Session> Session::sessions_list() {
    std::vector<Session> res;

    for (const auto& entry : std::filesystem::directory_iterator(sessions_dir)) {
        if (entry.is_regular_file()) {
            std::time_t tt = to_time_t(entry.last_write_time());
            std::tm* gmt = std::gmtime(&tt);
            std::stringstream buffer;
            buffer << std::put_time(gmt, "%A, %d %B %Y %H:%M");
            std::string formattedFileTime = buffer.str();

            Session session(entry.path().filename().replace_extension());
            session.last_write_time = buffer.str();

            res.push_back(session);
        }
    }
    return res;
}

void Session::save_to_disk() const {
    std::filesystem::path session_file = sessions_dir / (name + std::string(".json"));
    std::ofstream file(session_file);
    nlohmann::json json = to_json_to_disk();
    file << json;
}

void Session::set_selected_plan(std::string& plan) {
    last_selected_plan = plan;
    is_changed = true;
}

std::string Session::get_selected_plan() const {
    return last_selected_plan;
}

void Session::set_tube_executed(std::string& plan, std::string& tube_id, bool state) {
    plans[plan][tube_id].executed = state;
    is_changed = true;
}

int Session::total_tubes_in_plans() {
    int total = 0;
    for (auto plan : plans) {
        total += plan.second.size();
    }
    return total;
}

int Session::total_tubes_executed() {
    int total = 0;
    for (auto& [key, value] : plans) {
        total += std::count_if(value.begin(), value.end(), [](auto& entry) { return entry.second.executed; });
    }
    return total;
}

void Session::cal_points_add_update(
    const std::string& tube_id,
    const std::string& col,
    const std::string& row,
    Point3D& ideal_coords,
    Point3D& determined_coords) {
    CalPointEntry cpe = {
        col, row, ideal_coords, determined_coords, true,
    };
    cal_points[tube_id] = cpe;
    is_changed = true;
}

void Session::cal_points_delete(const std::string& tube_id) {
    cal_points.erase(tube_id);
    is_changed = true;
}

Point3D Session::get_tube_coordinates(const std::string& tube_id, bool ideal = true) {
    if (auto iter = hx.tubes.find(tube_id); iter != hx.tubes.end()) {
        return (ideal ? iter->second.coords : transform_point_if_aligned(iter->second.coords));
    }
    return {};
};

Point3D Session::transform_point_if_aligned(Point3D point, bool inverse)  {
    // Code from Open3D Geometry3D.cpp TransformPoints() method
    if (is_aligned) {
        Eigen::Vector4d point4d(point.x, point.y, point.z, 1.0);
        Eigen::Vector4d new_point;
        new_point = (inverse ? inverse_transformation_matrix : transformation_matrix) * point4d;
        Eigen::Vector3d transformed_point = new_point.head<3>() / new_point(3);
        return {transformed_point.x(), transformed_point.y(), transformed_point.z()};
    } else {
        return point;
    }
}

std::map<std::string, TubeEntry> Session::calculate_aligned_tubes() {
    std::map<std::string, TubeEntry> aligned_tubes = hx.tubes;
    is_aligned = false;
    SPDLOG_INFO("Aligning Tubes...");
    // std::vector<Point3D> src_points = { { 1.625, 0.704, 0 },
    //         {16.656, 2.815, 0},
    //         {71.125, 3.518, 0},
    //         };

    // std::vector<Point3D> dst_points = { { 11.625, 10.704, 1 },
    //         {26.656, 12.815, 2},
    //         {81.125, 13.518, 3},
    //         };

    // Convert vector of Point3D to Open3D point clouds
    open3d::geometry::PointCloud source_cloud;
    open3d::geometry::PointCloud target_cloud;

    int used_points = 0;
    for (const auto& cal_point : cal_points) {
        if (cal_point.second.determined) {
            source_cloud.points_.emplace_back(Eigen::Vector3d(
                cal_point.second.ideal_coords.x, cal_point.second.ideal_coords.y, cal_point.second.ideal_coords.z));

            target_cloud.points_.emplace_back(Eigen::Vector3d(
                cal_point.second.determined_coords.x,
                cal_point.second.determined_coords.y,
                cal_point.second.determined_coords.z));
            used_points++;
        }
    }

    if (used_points < 3) {
        SPDLOG_ERROR("At least 3 alignment points are required");
        return aligned_tubes;
    }

    // Set ICP parameters and perform ICP
    open3d::pipelines::registration::ICPConvergenceCriteria icp_criteria;
    icp_criteria.max_iteration_ = 50;
    icp_criteria.relative_fitness_ = 1e-6;
    icp_criteria.relative_rmse_ = 1e-6;

    bool with_scaling = false;  // always false, to make homogeneous transforms
    transformation_matrix = open3d::pipelines::registration::RegistrationICP(
        source_cloud,
        target_cloud,
        1000,
        Eigen::Matrix4d::Identity(),
        open3d::pipelines::registration::TransformationEstimationPointToPoint(with_scaling),
        icp_criteria).transformation_;

        // Extract the rotation matrix (3x3) and translation vector (3x1)
        Eigen::Matrix3d rotation_matrix = transformation_matrix.block<3,3>(0,0);
        Eigen::Vector3d translation_vector = transformation_matrix.block<3,1>(0,3);
        double scale_factor = transformation_matrix(3,3);        

        // Calculate the inverse of the transformation matrix
        Eigen::Matrix3d transpose_rotation_matrix = rotation_matrix.transpose();            // Transpose of the rotation matrix
        Eigen::Vector3d inv_translation_vector = -transpose_rotation_matrix * translation_vector;            // Inverse translation vector

        // Construct the inverse transformation matrix
        inverse_transformation_matrix.block<3,3>(0,0) = transpose_rotation_matrix;
        inverse_transformation_matrix.block<3,1>(0,3) = inv_translation_vector;
        inverse_transformation_matrix.block<1,3>(3,0) = Eigen::RowVector3d::Zero();         // First 3 elements of last row must be 0 for homogeneous transform
        inverse_transformation_matrix(3,3) = scale_factor;

    is_aligned = true;

    // Transform the source point cloud
    for (const auto& [id, tube] : hx.tubes) {
        TubeEntry aligned_tube = tube;
        aligned_tube.coords = transform_point_if_aligned(tube.coords);
        aligned_tubes[id] = aligned_tube;
    }
    return aligned_tubes;
}

nlohmann::json Session::to_json_to_disk() const {
    nlohmann::json json;
    json["hx_dir"] = hx_dir;
    json["hx"] = hx;
    json["last_selected_plan"] = last_selected_plan;
    json["plans"] = plans;
    json["cal_points"] = cal_points;
    return json;
}

void Session::from_json_from_disk(const nlohmann::json& json) {
    const Session nlohmann_json_default_obj{};
    hx_dir = json.value("hx_dir", nlohmann_json_default_obj.hx_dir);
    hx = json.value("hx", nlohmann_json_default_obj.hx);
    last_selected_plan = json.value("last_selected_plan", nlohmann_json_default_obj.last_selected_plan);
    plans = json.value("plans", nlohmann_json_default_obj.plans);
    cal_points = json.value("cal_points", nlohmann_json_default_obj.cal_points);
}

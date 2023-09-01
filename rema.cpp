#include <Open3D/Geometry/PointCloud.h>
#include <Open3D/Registration/ColoredICP.h>
#include <Open3D/IO/ClassIO/ImageIO.h>
#include <Eigen/Eigen>
#include <iostream>
#include <vector>
#include <algorithm>
#include <csv.h>

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <csv.h>
#include <json.hpp>

#include "rema.hpp"
#include "inspection-session.hpp"
#include "tool.hpp"

void REMA::save_to_disk() const {
    std::ofstream file(rema_file);
    nlohmann::json j;

    // Default JSON deserialization not possible because REMA is not default constructible (to enforce singleton pattern)
    j["last_selected_tool"] = last_selected_tool;
    file << j;
}

void REMA::set_selected_tool(std::string tool) {
    last_selected_tool = tool;
    save_to_disk();
}

std::filesystem::path REMA::get_selected_tool() const {
    return last_selected_tool;
}

void REMA::update_telemetry(boost::asio::streambuf &rx_buffer) {
    nlohmann::json json;
    try {
        std::lock_guard<std::mutex> lock(mtx);
        std::string stream(
                boost::asio::buffer_cast<const char*>((rx_buffer).data()));
        if (!stream.empty()) {
            //cout << stream << endl;
            json = nlohmann::json::parse(stream);

            if (json.contains("telemetry")) {
                telemetry = json["telemetry"];
            }

            if (json.contains("temps")) {
                temps = json["temps"];
            }
        }
    } catch (std::exception &e) {
        std::string message = std::string(e.what());
        std::cerr << "TELEMETRY COMMUNICATIONS ERROR " << message << "\n";
    }
    return;
}

std::vector<PointWithID> REMA::get_aligned_tubes(InspectionSession insp_sess, std::vector<Point3D> src_points, std::vector<Point3D> dst_points) {

    // Parse the CSV file to extract the data for each tube
    io::CSVReader<5, io::trim_chars<' ', '\t'>, io::no_quote_escape<';'>> in(
            insp_sess.hx_directory / insp_sess.hx / "tubesheet.csv");
    in.read_header(io::ignore_extra_column, "cl_x", "cl_y", "hl_x", "hl_y",
            "tube_id");
    double cl_x, cl_y, hl_x, hl_y;
    std::string tube_id;

    std::vector<PointWithID> tubes;

    while (in.read_row(cl_x, cl_y, hl_x, hl_y, tube_id)) {
        if (insp_sess.leg == "cold" || insp_sess.leg == "both") {
            tubes.push_back( { tube_id.substr(5), { cl_x, cl_y, 0 } });
        }

        if (insp_sess.leg == "hot" || insp_sess.leg == "both") {
            tubes.push_back( { tube_id.substr(5), { hl_x, hl_y, 0 } });
        }
    }

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

    for (const Point3D &point : src_points) {
        source_cloud.points_.push_back(
                Eigen::Vector3d(point.x, point.y, point.z));
    }

    for (const Point3D &point : dst_points) {
        target_cloud.points_.push_back(
                Eigen::Vector3d(point.x, point.y, point.z));
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
    std::vector<PointWithID> aligned_points;
    for (const PointWithID &point : tubes) {
        open3d::geometry::PointCloud one_tube_cloud;
        one_tube_cloud.points_.push_back(
                Eigen::Vector3d(point.coords.x, point.coords.y, point.coords.z));

        one_tube_cloud.Transform(transformation_matrix);
        aligned_points.push_back( {point.tube_id, {(*one_tube_cloud.points_.begin()).x(), (*one_tube_cloud.points_.begin()).y(), (*one_tube_cloud.points_.begin()).z()} });
    }


    //tubes_cloud.Transform(transformation_matrix);

    return aligned_points;
}

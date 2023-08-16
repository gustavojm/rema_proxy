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
        std::string stream(
                boost::asio::buffer_cast<const char*>((rx_buffer).data()));
        if (!stream.empty()) {
            //cout << stream << endl;
            json = nlohmann::json::parse(stream);

            telemetry.x_y_on_condition = json["X_Y_ON_COND"];
            telemetry.z_on_condition = json["Z_ON_COND"];
            telemetry.x = json["POS X"];
            telemetry.y = json["POS Y"];
            telemetry.z = json["POS Z"];

            if (json.contains("TEMP_INFO")) {
                temps.x = json["TEMP_INFO"].at("TEMP X");
                temps.y = json["TEMP_INFO"].at("TEMP Y");
                temps.z = json["TEMP_INFO"].at("TEMP Z");
            }

            //rema_instance.probe_touching = json["PROBE_TOUCHING"];
        }
    } catch (std::exception &e) {
        std::string message = std::string(e.what());
        std::cerr << "COMMUNICATIONS ERROR " << message << "\n";
    }
    return;
}

std::vector<Eigen::Matrix<double, 3, 1>> REMA::get_aligned_tubes(std::vector<Point3D> src_points,
        std::vector<Point3D> dst_points) {
// Parse the CSV file to extract the data for each tube
    std::string leg = "hot";
    io::CSVReader<5, io::trim_chars<' ', '\t'>, io::no_quote_escape<';'>> in(
            "tubesheet.csv");
    in.read_header(io::ignore_extra_column, "cl_x", "cl_y", "hl_x", "hl_y",
            "tube_id");
    double cl_x, cl_y, hl_x, hl_y;
    std::string tube_id;

    std::vector<Point3D> tubes;

    while (in.read_row(cl_x, cl_y, hl_x, hl_y, tube_id)) {
        if (leg == "cold" || leg == "both") {
            tubes.push_back( { cl_x, cl_y, 0 });
        }

        if (leg == "hot" || leg == "both") {
            tubes.push_back( { hl_x, hl_y, 0 });
        }
    }

    //1.625; 0.704;TUBE.2
    //16.656;2.815;TUBE.280
    //71.125;3.518;TUBE.431

    std::vector<Point3D> source_points = { { 1.625, 0.704, 0 },
    //        {16.656, 2.815, 0},
    //        {71.125, 3.518, 0},
            };

    std::vector<Point3D> target_points = { { 11.625, 10.704, 1 },
    //        {26.656, 12.815, 2},
    //        {81.125, 13.518, 3},
            };

// Convert vector of Point3D to Open3D point clouds
    open3d::geometry::PointCloud source_cloud;
    open3d::geometry::PointCloud target_cloud;
    open3d::geometry::PointCloud tubes_cloud;

    for (const Point3D &point : source_points) {
        source_cloud.points_.push_back(
                Eigen::Vector3d(point.x, point.y, point.z));
    }

    for (const Point3D &point : target_points) {
        target_cloud.points_.push_back(
                Eigen::Vector3d(point.x, point.y, point.z));
    }

    for (const Point3D &point : tubes) {
        tubes_cloud.points_.push_back(
                Eigen::Vector3d(point.x, point.y, point.z));
    }

// Set ICP parameters and perform ICP
    open3d::registration::ICPConvergenceCriteria icp_criteria;
    icp_criteria.max_iteration_ = 50;
    icp_criteria.relative_fitness_ = 1e-6;
    icp_criteria.relative_rmse_ = 1e-6;
    auto result = open3d::registration::RegistrationICP(source_cloud,
            target_cloud, 1000, Eigen::Matrix4d::Identity(),
            open3d::registration::TransformationEstimationPointToPoint(false),
            icp_criteria);

// Get the transformation matrix
    Eigen::Matrix4d transformation_matrix = result.transformation_;
    std::cout << transformation_matrix << std::endl;

    std::cout << " ----------- TRANSFORMED CLOUD ------------- " << std::endl;

// Transform the source point cloud
    tubes_cloud.Transform(transformation_matrix);
    for (auto p : tubes_cloud.points_)
        std::cout << "X:" << p.x() << " Y:" << p.y() << " Z:" << p.z()
                << std::endl;

    return tubes_cloud.points_;
}

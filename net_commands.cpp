#include <functional>
#include <ctime>
#include <chrono>
#include <fstream>
#include <iostream>
#include <exception>
#include <vector>

#include "csv.h"
#include "json.hpp"
#include "insp_session.hpp"

using namespace std::chrono_literals;
extern inspection_session current_session;

struct tube {
	std::string x_label;
	std::string y_label;
	float cl_x;
	float cl_y;
	float hl_x;
	float hl_y;
	int tube_id;
	std::string color = "white";
	std::string insp_plan = "";
};

void to_json(nlohmann::json &j, const tube &t) {
	j = nlohmann::json { { "x_label", t.x_label }, { "y_label", t.y_label }, {
			"cl_x", t.cl_x }, { "cl_y", t.cl_y }, { "hl_x", t.hl_x }, { "hl_y",
			t.hl_y }, { "tube_id", t.tube_id }, };
}

void from_json(const nlohmann::json &j, tube &t) {
	j.at("x_label").get_to(t.x_label);
	j.at("y_label").get_to(t.y_label);
	j.at("cl_x").get_to(t.cl_x);
	j.at("cl_y").get_to(t.cl_y);
	j.at("hl_x").get_to(t.hl_x);
	j.at("hl_y").get_to(t.hl_y);
	j.at("tube_id").get_to(t.tube_id);
}

nlohmann::json hx_tubesheet_load_cmd(nlohmann::json pars) {
	// Parse the CSV file to extract the data for each tube
	std::vector<tube> tubes;
	io::CSVReader<7, io::trim_chars<' ', '\t'>, io::no_quote_escape<';'>> in(current_session.tubesheet_csv);
	in.read_header(io::ignore_extra_column, "x_label", "y_label", "cl_x",
			"cl_y", "hl_x", "hl_y", "tube_id");
	std::string x_label, y_label;
	float cl_x, cl_y, hl_x, hl_y;
	std::string tube_id;
	while (in.read_row(x_label, y_label, cl_x, cl_y, hl_x, hl_y, tube_id)) {
		tubes.push_back(
				{ x_label, y_label, cl_x, cl_y, hl_x, hl_y, std::stoi(
						tube_id.substr(5)) });
	}

	nlohmann::json res(tubes); //requires to_json and from_json to be defined to be able to serialize the custom object "tube"
	return res;
}

nlohmann::json hx_list_cmd(nlohmann::json pars) {
	nlohmann::json res;

	std::string path = "./HXs";
	for (const auto &entry : std::filesystem::directory_iterator(path)) {
		if (entry.is_directory()) {
			res.push_back( { { "value", entry.path().filename() }, { "text",
					entry.path().filename() } });
		}
	}
	std::sort(res.begin(), res.end());

	return res;
}

nlohmann::json insp_plan_load_cmd(nlohmann::json pars) {
	nlohmann::json res; //requires to_json and from_json to be defined to be able to serialize the custom object "tube"

	try {
		std::string insp_plan_path = pars["insp_plan_path"];
		current_session.last_selected_plan = std::filesystem::path(
				insp_plan_path);

		for (auto [key, value] : current_session.insp_plans.at(insp_plan_path)) {
			res.push_back( { { "tube_id", key }, { "col", value.col }, { "row",
					value.row }, { "inspected", value.inspected } });
		}

	} catch (nlohmann::json::exception &e) {
//		for (const auto& [path, insp_plan] : current_session.insp_plans) {
//			for (const auto& [key, value] : insp_plan) {
//				res.push_back({ { "tube_id", key }, { "col", value.col }, { "row", value.row }, { "inspected", value.inspected } });
//			}
//		}
//		return res;
	}
	return res;
}


nlohmann::json tube_set_status_cmd(nlohmann::json pars) {
	nlohmann::json res; //requires to_json and from_json to be defined to be able to serialize the custom object "tube"

	try {
		std::string insp_plan_path = pars["insp_plan_path"];
		std::string tube_id = pars["tube_id"];
		bool checked = pars["checked"];

		current_session.insp_plans.at(insp_plan_path).at(tube_id).inspected = checked;
		res[tube_id] = checked;

	} catch (nlohmann::json::exception &e) {
	}
	return res;
}


namespace fs = std::filesystem;

template<typename TP>
std::time_t to_time_t(TP tp) {
	using namespace std::chrono;
	auto sctp = time_point_cast<system_clock::duration>(
			tp - TP::clock::now() + system_clock::now());
	return system_clock::to_time_t(sctp);
}

nlohmann::json insp_sessions_list_cmd(nlohmann::json pars) {
	nlohmann::json res;

	std::string path = "./insp_sessions/";
	for (const auto &entry : std::filesystem::directory_iterator(path)) {
		if (entry.is_regular_file()) {

			std::time_t tt = to_time_t(entry.last_write_time());
			std::tm *gmt = std::gmtime(&tt);
			std::stringstream buffer;
			buffer << std::put_time(gmt, "%A, %d %B %Y %H:%M");
			std::string formattedFileTime = buffer.str();

			//res.push_back( std::make_pair(std::make_pair( "file_name", entry.path().filename()), std::make_pair(
			//		"file_date", formattedFileTime)));
			std::string filename = entry.path().filename();
			std::ifstream session_file {entry.path()};
			nlohmann::json j;
			session_file >> j;

			res.push_back(nlohmann::json { { "file_name", filename }, {"hx", j["hx"] }, {"file_date", formattedFileTime }, });
		}
	}
	std::sort(res.begin(), res.end());

	return res;
}

nlohmann::json session_create_cmd(nlohmann::json pars) {
	std::string session_name = pars["session_name"];

	nlohmann::json res;

	std::filesystem::path inspection_session_file = session_name;
	if (inspection_session_file.empty()
			|| !inspection_session_file.has_filename()) {
		res["success"] = false;
		res["logs"] = "no filename specified";
		return res;
	}

	if (pars["hx"].empty()) {
		res["success"] = false;
		res["logs"] = "no HX selected";
		return res;
	}

	try {
		std::filesystem::path hx_directory = std::filesystem::path("HXs");
		std::filesystem::path hx = std::filesystem::path(pars["hx"]);

		std::filesystem::path tubesheet_csv = hx_directory / hx / "tubesheet.csv";
		std::filesystem::path tubesheet_svg = hx_directory / hx / "tubesheet.svg";

		inspection_session new_session(inspection_session_file, hx_directory, hx,
				tubesheet_csv, tubesheet_svg);
		res["logs"] = new_session.load_plans();

		std::filesystem::path complete_inspection_session_file_path =
				std::filesystem::path("insp_sessions").append(
						session_name + ".json");
		std::ofstream session(complete_inspection_session_file_path);
		session << nlohmann::json(new_session);
		current_session = new_session;
		res["success"] = true;
		return res;

	} catch (const std::exception &e) {
		res["success"] = false;
		res["logs"] = e.what();
	}
	return res;
}

nlohmann::json session_load_cmd(nlohmann::json pars) {
	std::string session_name = pars["session_name"];
	nlohmann::json res;

	if (session_name.empty()) {
		res["success"] = false;
		res["logs"] = "no session selected";
		return res;
	}

	current_session.load(
			std::filesystem::path("insp_sessions").append(session_name));
	res["success"] = true;
	return res;
}

nlohmann::json session_info_cmd(nlohmann::json pars) {
	nlohmann::json res = nlohmann::json(nlohmann::json::value_t::object);

	if (current_session.is_loaded()) {
		res["tubesheet_svg_path"] = current_session.tubesheet_svg;
		res["last_selected_plan"] = current_session.get_selected_plan();

		nlohmann::json insp_plans_json;
		for (auto &i_p : current_session.insp_plans) {
			insp_plans_json.push_back(
					{ { "value", i_p.first }, { "text", i_p.first.filename() } });
		}

		res["inspection_plans"] = insp_plans_json;
	}

	return res;
}

nlohmann::json session_delete_cmd(nlohmann::json pars) {
	std::string session_name = pars["session_name"];
	nlohmann::json res = nlohmann::json(nlohmann::json::value_t::object);

	try {
		std::filesystem::remove("./insp_sessions/" + session_name);
		res["success"] = true;
	} catch (const std::filesystem::filesystem_error &e) {
		res["logs"] = std::string("filesystem error: ") + e.what();
	}

	return res;
}

// @formatter:off
std::map<std::string, std::function<nlohmann::json(nlohmann::json)>> commands =
		{ { "hx_list", &hx_list_cmd },
		  { "hx_tubesheet_load", &hx_tubesheet_load_cmd },
		  { "insp_sessions_list", &insp_sessions_list_cmd },
		  { "session_create", &session_create_cmd },
		  { "session_load", &session_load_cmd },
		  { "session_info", &session_info_cmd },
		  { "session_delete", &session_delete_cmd },
		  { "insp_plan_load", &insp_plan_load_cmd },
		  { "tube_set_status", &tube_set_status_cmd },
		};
// @formatter:on

nlohmann::json cmd_execute(std::string command, nlohmann::json par) {
	nlohmann::json res;
	if (auto cmd_entry = commands.find(command); cmd_entry != commands.end()) {
		res = (*cmd_entry).second(par);
	}
	return res;
}

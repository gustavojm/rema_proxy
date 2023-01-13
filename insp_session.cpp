#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <csv.h>
#include "insp_session.hpp"

extern inspection_session current_session;

inspection_session::inspection_session() :loaded(false) {

}

inspection_session::inspection_session(std::filesystem::path inspection_session_file,
		std::filesystem::path hx_directory,
		std::filesystem::path tubesheet_csv,
		std::filesystem::path tubesheet_svg) :
		inspection_session_file(inspection_session_file), hx_directory(hx_directory), tubesheet_csv(
				tubesheet_csv), tubesheet_svg(tubesheet_svg) {

}

std::string inspection_session::load_plans() {
	std::stringstream out;
	for (const auto &entry : std::filesystem::directory_iterator(hx_directory.append("insp_plans"))) {
		if (!(entry.path().filename().extension() == ".csv")) {
			continue;
		}
		out << "Procesando plan: " << entry.path().filename() << "\n";

		// Parse the CSV file to extract the data for the plan
		io::CSVReader<3, io::trim_chars<' ', '\t'>, io::no_quote_escape<';'> >
		ip(	entry.path());
		ip.read_header(io::ignore_extra_column, "ROW", "COL", "TUBE");
		std::string row, col;
		std::string tube_num;
		while (ip.read_row(row, col, tube_num)) {
			std::string tube_num_stripped =tube_num.substr(5);
			insp_plans[entry.path()][tube_num_stripped] = insp_plan_entry{row, col, false};
		}
	}
	loaded = true;
	return out.str();
}

inspection_session inspection_session::load(std::filesystem::path inspection_session_file) {
	this->inspection_session_file = inspection_session_file;
	std::ifstream i(inspection_session_file);

	nlohmann::json j;
	i >> j;

	return j;
}

void inspection_session::set_selected_plan(std::filesystem::path plan) {
	last_selected_plan = plan;
}

std::filesystem::path inspection_session::get_selected_plan() {
	 return last_selected_plan;
}

void inspection_session::set_tube_inspected(std::string tube_id, bool state) {
	insp_plans[last_selected_plan][tube_id].inspected = state;
}

void to_json(nlohmann::json &j, const insp_plan_entry &ipe) {
	j = nlohmann::json { { "row", ipe.row }, { "col", ipe.col }, { "inspected",
			ipe.inspected } };
}

void from_json(const nlohmann::json &j, insp_plan_entry &ipe) {
	j.at("row").get_to(ipe.row);
	j.at("col").get_to(ipe.col);
	j.at("inspected").get_to(ipe.inspected);
}

void to_json(nlohmann::json &j, const inspection_session &is) {
	j = nlohmann::json {
			{ "inspection_session_file", is.inspection_session_file },
			{ "hx_directory", is.hx_directory },
			{ "tubesheet_csv", is.tubesheet_csv },
			{ "tubesheet_svg", is.tubesheet_svg },
			{ "last_selected_plan", is.last_selected_plan },
			{ "insp_plans", nlohmann::json(is.insp_plans) },
	};
}

void from_json(const nlohmann::json &j, inspection_session &is) {
	j.at( "inspection_session_file").get_to(is.inspection_session_file);
	j.at("hx_directory").get_to(is.hx_directory);
	j.at("tubesheet_csv").get_to(is.tubesheet_csv);
	j.at("tubesheet_svg").get_to(is.tubesheet_svg);
	j.at("last_selected_plan").get_to(is.last_selected_plan);
	j.at("insp_plans").get_to(is.insp_plans);

}

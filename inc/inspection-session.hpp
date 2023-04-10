#ifndef INSP_SESSION_HPP
#define INSP_SESSION_HPP

#include <string>
#include <filesystem>
#include <json.hpp>

	struct InspectionPlanEntry {
		std::string row, col;
		bool inspected;
	};

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(InspectionPlanEntry, row, col, inspected);

class InspectionSession {
public:
	InspectionSession();
	InspectionSession(std::filesystem::path inspection_session_file,
			std::filesystem::path hx_directory,
			std::filesystem::path hx,
			std::filesystem::path tubesheet_csv,
			std::filesystem::path tubesheet_svg);

	std::string load_plans();

	InspectionSession load(std::filesystem::path inspection_session_file);

	void save_to_disk() const;

	inline bool is_loaded() {return loaded;};

	inline bool is_changed() const {return changed;};

	inline void set_changed(bool changed) {this->changed = changed;};

	void set_selected_plan(std::filesystem::path plan);

	std::filesystem::path get_selected_plan() const;

	void set_tube_inspected(std::string tube_id, bool state);

	void set_tube_inspected(std::filesystem::path insp_plan_path, std::string tube_id, bool state);

	//<inspection plan path,          <tube_id,   , tube_entry>>
	std::map<std::filesystem::path,
			std::map<std::string, struct InspectionPlanEntry::InspectionPlanEntry>> insp_plans;

	std::filesystem::path inspection_session_file;
	std::filesystem::path hx_directory;
	std::filesystem::path hx;
	std::filesystem::path tubesheet_csv;
	std::filesystem::path tubesheet_svg;
	std::filesystem::path last_selected_plan;
	float tube_od;
	std::string leg = "both";
	bool loaded = false;
	bool changed = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(InspectionSession, inspection_session_file,
        hx_directory, hx, tubesheet_csv, tubesheet_svg, last_selected_plan,
        insp_plans, leg, tube_od);

#endif 		// INSP_SESSION_HPP

#ifndef INSP_SESSION_HPP
#define INSP_SESSION_HPP

#include <string>
#include <filesystem>
#include <json.hpp>

	struct insp_plan_entry {
		std::string row, col;
		bool inspected;
	};

	void to_json(nlohmann::json &j, const insp_plan_entry &ipe);

	void from_json(const nlohmann::json &j, insp_plan_entry &ipe);

class inspection_session {
public:
	inspection_session();
	inspection_session(std::filesystem::path inspection_session_file,
			std::filesystem::path hx_directory,
			std::filesystem::path hx,
			std::filesystem::path tubesheet_csv,
			std::filesystem::path tubesheet_svg);

	std::string load_plans();

	inspection_session load(std::filesystem::path inspection_session_file);

	void save_to_disk();

	inline bool is_loaded() {return loaded;};

	inline bool is_changed() const {return changed;};

	inline void set_changed(bool changed) {this->changed = changed;};

	void set_selected_plan(std::filesystem::path plan);

	std::filesystem::path get_selected_plan() const;

	void set_tube_inspected(std::string tube_id, bool state);

	void set_tube_inspected(std::filesystem::path insp_plan_path, std::string tube_id, bool state);

	//<inspection plan path,          <tube_id,   , tube_entry>>
	std::map<std::filesystem::path,
			std::map<std::string, struct insp_plan_entry::insp_plan_entry>> insp_plans;

//public:
	std::filesystem::path inspection_session_file;
	std::filesystem::path hx_directory;
	std::filesystem::path hx;
	std::filesystem::path tubesheet_csv;
	std::filesystem::path tubesheet_svg;
	std::filesystem::path last_selected_plan;
	bool loaded = false;
	bool changed = false;
};

void to_json(nlohmann::json &j, const inspection_session &is);

void from_json(const nlohmann::json &j, inspection_session &is);

#endif 		// INSP_SESSION_HPP

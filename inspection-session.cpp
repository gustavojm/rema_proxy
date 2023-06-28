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
        io::CSVReader<3, io::trim_chars<' ', '\t'>, io::no_quote_escape<';'> > ip(
                entry.path());
        ip.read_header(io::ignore_extra_column, "ROW", "COL", "TUBE");
        std::string row, col;
        std::string tube_num;
        while (ip.read_row(row, col, tube_num)) {
            std::string tube_num_stripped = tube_num.substr(5);
            insp_plans[entry.path().filename().replace_extension()][tube_num_stripped] = InspectionPlanEntry {
                    row, col, false };
        }
    }
    loaded = true;
    return out.str();
}

std::vector<InspectionPlanEntryWithTubeID> InspectionSession::inspection_plan_get(std::string insp_plan) {
    std::vector<InspectionPlanEntryWithTubeID> res;
    last_selected_plan = insp_plan;

    auto it = insp_plans.find(insp_plan);
    if (it != insp_plans.end()) {
        for (auto [key, value] : it->second ) {
            InspectionPlanEntryWithTubeID entry;
            entry.col = value.col;
            entry.row = value.row;
            entry.inspected = value.inspected;
            entry.tube_id = key;

            res.push_back(entry);
        }
    }
    return res;
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

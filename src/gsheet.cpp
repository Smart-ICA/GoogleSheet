#include <pugg/Kernel.h>
#include <nlohmann/json.hpp>
#include "source.hpp"
#include "common.hpp"

#include <cstdlib>
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <string>
#include <array>
#include <cstring>
#include <queue>
#include <atomic>
#include <fstream>


// Classe dérivée du template Source<std::string>

class GSheetSource : public Source<nlohmann::json> {
public:
    GSheetSource() = default;
    virtual ~GSheetSource() = default;

    static std::string server_name() {
        return "SourceServer";
    }
    static std::string kind_static() { return "gsheet"; }
    
    
    static const int version = PLUGIN_PROTOCOL_VERSION;

    std::string kind() override { return kind_static(); }
    
    void set_params(void const *params) override {
        try {
            auto json_params = static_cast<const nlohmann::json *>(params);
            if (json_params) {
                _params = *json_params;
                _agent_id = _params.value("agent_id", "");
                script_path_ = _params.value("script_path", "fetch_gsheet.py");
                _date_format = _params.value("timestamp_format", "%d/%m/%Y %H:%M:%S");
                std::cout << "[DEBUG] Format d'horodatage utilisé : " << _date_format << std::endl;
                _timestamp_field = _params.value("timestamp_field", "Horodateur");
                std::cout << "[DEBUG] Champ horodateur utilisé : " << _timestamp_field << std::endl;
                if (_params.contains("poll_interval_minutes")) {
                    int interval = _params.value("poll_interval_minutes", 5);
                    if (interval > 0) {
                        _poll_interval = std::chrono::minutes(interval);
                        std::cout << "[DEBUG] Récupération des données toutes les " << interval << " minutes" << std::endl;
                    }
                    std::cout << "[DEBUG] Emplacement du fichier python : " << script_path_ << std::endl;
                    load_last_timestamp();
                }
            } else {
                std::cerr << "[ERROR] set_params: cast vers json* invalide" << std::endl;
            }  
        } catch (const std::exception &e) {
            std::cerr << "[ERROR] Exception dans set_params: " << e.what() << std::endl;
        }
    }

    return_type get_output(nlohmann::json &out, std::vector<unsigned char> *blob = nullptr) override {
        

        if (_data_queue.empty()) {
            std::cout << " " << std::endl;
            std::string raw = execScript();

            try {
                auto parsed = nlohmann::json::parse(raw);
                for (const auto& entry : parsed) {
                    std::string current_ts = entry.value(_timestamp_field, "");
                    if (entry.contains(_timestamp_field) && is_newer(entry[_timestamp_field], _last_timestamp_str)) {
                        _data_queue.push(entry);
                    }
                }
                std::cout << "[DEBUG] Données chargées : " << _data_queue.size() << " éléments" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] Parsing JSON échoué : " << e.what() << std::endl;
                return return_type::error;
            }
  
            if (_data_queue.empty()) {
                std::cout << "[DEBUG] Aucune donnée à envoyer" << std::endl;
                std::this_thread::sleep_for(_poll_interval);
                return return_type::success;
            }
        }

        out = _data_queue.front();
        _data_queue.pop();

        if (!_agent_id.empty()) {
            out["agent_id"] = _agent_id;
        }

        _last_timestamp_str = out[_timestamp_field];
        save_last_timestamp();
        
        std::cout << "[DEBUG] Envoi de : " << out.dump() << std::endl;

        if (_data_queue.empty()) {
            std::cout << "[DEBUG] Dernière ligne envoyée" << std::endl;
            std::this_thread::sleep_for(_poll_interval);
        }
        
        std::cout << "[DEBUG] Message publié sur le topic : gsheet" << std::endl;
        return return_type::success;
    }


    std::string blob_format() {
        return "";
    }

    std::map<std::string, std::string> info() override {
        return {
            {"name", "Google Sheets Source"},
            {"description", "Reads data from a Google Sheet via Python script"}
        };
   
    }

private:
    std::string script_path_;
    std::chrono::minutes _poll_interval = std::chrono::minutes(5);
    std::queue<nlohmann::json> _data_queue;
    std::string _last_timestamp_file = "last_timestamp.txt";
    std::string _last_timestamp_str = "";
    std::string _date_format = "%d%m%Y %H:%M:%S";
    std::string _timestamp_field = "Horodateur";
    
    std::string execScript() {
        std::array<char, 256> buffer;
        std::string result;
        std::string command = "python3 " + script_path_;
        std::cout << "[DEBUG] Commande exécutée : " << command << std::endl;

        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
           std::cerr << "[ERROR] popen a échoué" << std::endl;
           return "{}";
        }

        while (fgets(buffer.data(), buffer.size(), pipe)) {
            result.append(buffer.data());
        }

        int rc = pclose(pipe);
        
        if (result.empty()) return "{}";
        
        if (result.size() > 10 * 1024 * 1024) {
            std::cerr << "[WARN] Output trop volumineux, tronqué" << std::endl;
            result.resize(10 * 1024 * 1024);
        }
  
        std::cout << "[DEBUG] Taille du résultat : " << result.size() << std::endl;
        return result;
    }
    
    std::tm parse_date(const std::string& date_str) {
        std::tm tm = {};
        std::istringstream ss(date_str);
        ss >> std::get_time(&tm, _date_format.c_str());
        if (ss.fail()) {
            throw std::runtime_error("Format date invalide : " + date_str + " avec le format " + _date_format);
        }
        return tm;
    }

    bool is_newer(const std::string& current, const std::string& last) {
        if (last.empty()) return true;
        auto tm_current = parse_date(current);
        auto tm_last = parse_date(last);
        return std::mktime(&tm_current) > std::mktime(&tm_last);
    }
    
    void load_last_timestamp() {
        std::ifstream infile(_last_timestamp_file);
        if (infile.is_open()) {
            std::getline(infile, _last_timestamp_str);
            infile.close();
            std::cout << "[DEBUG] Dernier horodatage chargé : " << _last_timestamp_str << std::endl;
        } else {
            std::cout << "[DEBUG] Aucun fichier de timestamp trouvé (premier lancement ?)" << std::endl;
        }
    }
    
    void save_last_timestamp() {
        std::ofstream outfile(_last_timestamp_file);
        if (outfile.is_open()) {
            outfile << _last_timestamp_str;
            outfile.close();
            std::cout << "[DEBUG] Horodatage sauvegardé : " << _last_timestamp_str << std::endl;
        } else {
            std::cerr << "[ERROR] Impossible d'écrire dans le fichier de timestamp" << std::endl;
        }
    }
};

#define PLUGIN_NAME "gsheet"
INSTALL_SOURCE_DRIVER(GSheetSource, nlohmann::json)

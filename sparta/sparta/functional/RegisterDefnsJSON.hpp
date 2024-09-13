#pragma once

#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

namespace sparta
{

struct RegisterDefn
{
    struct FieldDefn
    {
        std::string name;
        std::string desc;
        RegisterBase::size_type low_bit;
        RegisterBase::size_type high_bit;
        bool readonly;
    };

    RegisterBase::ident_type id;
    std::string name;
    RegisterBase::group_num_type group_num;
    std::string group;
    RegisterBase::group_idx_type group_idx;
    std::string desc;
    RegisterBase::size_type bytes;
    std::vector<FieldDefn> fields;
    std::vector<RegisterBase::bank_idx_type> bank_membership;
    std::vector<std::string> aliases;
    RegisterBase::ident_type subset_of = RegisterBase::INVALID_ID;
    RegisterBase::size_type subset_offset = RegisterBase::INVALID_ID;
    uint64_t initial_value = 0;
    RegisterBase::Definition::HintsT hints = 0;
    RegisterBase::Definition::RegDomainT regdomain = 0;
};

class RegisterDefnsFromJSON
{
public:
    RegisterDefnsFromJSON(const std::vector<std::string>& register_defns_json_filenames)
    {
        for (const auto& filename : register_defns_json_filenames) {
            parse(filename);
        }

        // Add a definition that indicates the end of the array
        register_defns_.push_back(RegisterBase::DEFINITION_END);
    }

    RegisterDefnsFromJSON(const std::string& register_defns_json_filename)
    {
        parse(register_defns_json_filename);

        // Add a definition that indicates the end of the array
        register_defns_.push_back(RegisterBase::DEFINITION_END);
    }

    RegisterBase::Definition* getAllDefns() {
        return register_defns_.data();
    }

private:
    void parse(const std::string& register_defns_json_filename)
    {
        // Read the file into a string
        std::ifstream ifs(register_defns_json_filename);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string json_str = ss.str();

        // Parse the JSON string
        rapidjson::Document document;
        document.Parse(json_str.c_str());

        for (auto& item : document.GetArray()) {
            if (item.HasMember("enabled") && !item["enabled"].GetBool()) {
                continue;
            }

            const RegisterBase::ident_type id = item["num"].GetInt();

            cached_strings_.emplace_back(item["name"].GetString());
            const char *name = cached_strings_.back().raw();

            const RegisterBase::group_num_type group_num = item["group_num"].GetInt();
            auto iter = group_idx_map_.find(group_num);
            if (iter == group_idx_map_.end()) {
                group_idx_map_[group_num] = 0;
            }

            RegisterBase::group_idx_type group_idx = group_idx_map_[group_num]++;
            cached_strings_.emplace_back(item["group_name"].GetString());
            const char* group = cached_strings_.back().raw();

            if (std::string(group).empty()) {
                group_idx = RegisterBase::GROUP_IDX_NONE;
            }

            cached_strings_.emplace_back(item["desc"].GetString());
            const char* desc = cached_strings_.back().raw();

            const RegisterBase::size_type bytes = item["size"].GetInt();

            std::vector<RegisterBase::Field::Definition> field_defns;
            if (item.HasMember("fields")) {
                for (auto it = item["fields"].MemberBegin(); it != item["fields"].MemberEnd(); ++it) {
                    const char* field_name = it->name.GetString();
                    const rapidjson::Value& field_info = it->value;
                    cached_field_defns_.emplace_back(field_name, field_info);
                    field_defns.push_back(cached_field_defns_.back().getDefn());
                }
            }

            static const std::vector<RegisterBase::bank_idx_type> bank_membership;
            
            std::vector<std::string> alias_strings;
            for (auto& alias : item["aliases"].GetArray()) {
                alias_strings.push_back(alias.GetString());
            }
            cached_aliases_.emplace_back(alias_strings);
            const char** aliases = cached_aliases_.back().raw();

            constexpr RegisterBase::ident_type subset_of = RegisterBase::INVALID_ID;
            constexpr RegisterBase::size_type subset_offset = 0;

            const unsigned char *initial_value = nullptr;
            if (item.HasMember("initial_value")) {
                cached_initial_values_.emplace_back(item["initial_value"].GetString());
                initial_value = cached_initial_values_.back().raw();
            }

            constexpr RegisterBase::Definition::HintsT hints = 0;
            constexpr RegisterBase::Definition::RegDomainT regdomain = 0;

            RegisterBase::Definition defn = {
                id,
                name,
                group_num,
                group,
                group_idx,
                desc,
                bytes,
                field_defns,
                bank_membership,
                aliases,
                subset_of,
                subset_offset,
                initial_value,
                hints,
                regdomain
            };

            register_defns_.push_back(defn);
        }
    }

    // Converts a string to a const char* pointer
    class StringRef
    {
    public:
        StringRef(const std::string& str) : storage_(str) {}
        const char* raw() const { return storage_.c_str(); }
    private:
        std::string storage_;
    };

    // Converts a vector of strings to an array of const char* pointers
    class AliasRef
    {
    public:
        AliasRef(const std::vector<std::string>& aliases)
            : storage_(aliases)
        {
            for (const auto& str : storage_) {
                pointers_.push_back(str.c_str());
            }
        }

        const char** raw() {
            return pointers_.data();
        }

    private:
        std::vector<std::string> storage_;
        std::vector<const char*> pointers_;
    };

    // Converts any hex ("0xdeafbeef") to a const unsigned char* pointer
    class InitialValueRef
    {
    public:
        InitialValueRef(const std::string& hex_str)
        {
            // Remove the "0x" prefix if present
            std::string hex = hex_str;
            if (hex.substr(0, 2) == "0x") {
                hex = hex.substr(2);
            }

            // Ensure the hex string has an even length
            sparta_assert(hex.length() % 2 == 0, "Hex string must have an even length");

            // Create a vector to hold the bytes
            hex_bytes_.resize(hex.length() / 2);

            // Convert hex string to bytes
            for (size_t i = 0; i < hex.length(); i += 2) {
                const auto byte_string = hex.substr(i, 2);
                std::istringstream iss(byte_string);
                int byte;
                iss >> std::hex >> byte;
                hex_bytes_[i / 2] = static_cast<char>(byte);
            }
        }

        const unsigned char* raw() const {
            return hex_bytes_.data();
        }

    private:
        std::vector<unsigned char> hex_bytes_;
    };

    // Converts a rapidjson::Value that represents a field to a Field::Definition
    class FieldDefnConverter
    {
    public:
        FieldDefnConverter(const std::string& field_name, const rapidjson::Value& field_info)
            : field_name_(field_name)
            , desc_(field_info["desc"].GetString())
            , field_defn_(field_name_.c_str(),
                          desc_.c_str(),
                          field_info["low_bit"].GetInt(),
                          field_info["high_bit"].GetInt(),
                          field_info["readonly"].GetBool())
        {
        }

        const RegisterBase::Field::Definition& getDefn() const
        {
            return field_defn_;
        }

    private:
        std::string field_name_;
        std::string desc_;
        RegisterBase::Field::Definition field_defn_;
    };

    std::deque<StringRef> cached_strings_;
    std::deque<AliasRef> cached_aliases_;
    std::deque<InitialValueRef> cached_initial_values_;
    std::deque<FieldDefnConverter> cached_field_defns_;
    std::vector<RegisterBase::Definition> register_defns_;

    // TODO: Find the official way to handle group_idx. For now we will just use
    // a map of auto-incrementing group_idx values for each group_num
    std::map<RegisterBase::group_num_type, RegisterBase::group_idx_type> group_idx_map_;
};

} // namespace sparta

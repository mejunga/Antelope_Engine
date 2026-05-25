#ifdef ANTELOPE_EDITOR_MODE
#include <Engine/Scripting/HeaderTool.hpp>
#include <Engine/Debug/Log.hpp>

#include <fstream>
#include <sstream>


namespace Antelope
{
    std::string HeaderTool::Trim(const std::string& str)
    {
        size_t start { str.find_first_not_of(" \t\r\n") };
        if (start == std::string::npos) { return {}; }
        size_t end { str.find_last_not_of(" \t\r\n") };
        return str.substr(start, end - start + 1);
    }

    int32_t HeaderTool::ResolveFieldType(const std::string& typeStr)
    {
        if (typeStr == "float") { return 0; }
        if (typeStr == "int" || typeStr == "int32_t") { return 1; }
        if (typeStr == "bool") { return 2; }
        if (typeStr == "glm::vec3") { return 3; }
        if (typeStr == "std::string") { return 4; }
        return 5;
    }

    void HeaderTool::ScanHeader(
        const std::filesystem::path& headerPath,
        const std::filesystem::path& scriptsDir,
        std::vector<ParsedComponent>& outComponents,
        std::vector<ParsedScript>& outScripts,
        std::vector<ParsedSystem>& outSystems)
    {
        std::ifstream file(headerPath);
        if (!file.is_open()) { return; }

        std::string line;
        bool expectComponent { false };
        bool expectField { false };
        bool expectScript { false };
        bool expectSystem { false };
        bool inComponent { false };
        int braceDepth { 0 };
        ParsedComponent current;

        while (std::getline(file, line))
        {
            std::string trimmed { Trim(line) };
            if (trimmed.empty()) { continue; }

            if (trimmed.find("ANTELOPE_COMPONENT()") != std::string::npos)
            {
                expectComponent = true;
                continue;
            }

            if (expectComponent)
            {
                size_t nameStart { std::string::npos };
                size_t pos { trimmed.find("struct ") };
                if (pos != std::string::npos) { nameStart = pos + 7; }
                else
                {
                    pos = trimmed.find("class ");
                    if (pos != std::string::npos) { nameStart = pos + 6; }
                }

                if (nameStart != std::string::npos)
                {
                    size_t nameEnd { trimmed.find_first_of(" \t{:", nameStart) };
                    std::string name { nameEnd == std::string::npos
                        ? trimmed.substr(nameStart)
                        : trimmed.substr(nameStart, nameEnd - nameStart) };

                    if (!name.empty())
                    {
                        current = {};
                        current.Name = name;
                        std::string rel { std::filesystem::relative(headerPath, scriptsDir).string() };
                        std::replace(rel.begin(), rel.end(), '\\', '/');
                        current.SourceFile = rel;
                        inComponent = true;
                        expectComponent = false;
                        braceDepth = 0;
                        for (char c : line) { if (c == '{') { braceDepth++; } }
                    }
                }
                continue;
            }

            if (inComponent)
            {
                for (char c : line)
                {
                    if (c == '{') { braceDepth++; }
                    else if (c == '}') { braceDepth--; }
                }

                if (braceDepth <= 0)
                {
                    outComponents.push_back(current);
                    inComponent = false;
                    braceDepth = 0;
                    expectField = false;
                    continue;
                }

                if (trimmed.find("ANTELOPE_FIELD()") != std::string::npos)
                {
                    expectField = true;
                    continue;
                }

                if (expectField)
                {
                    std::istringstream iss(trimmed);
                    std::string typeStr, nameStr;
                    iss >> typeStr >> nameStr;

                    if (!nameStr.empty())
                    {
                        size_t cut { nameStr.find_first_of("{;=") };
                        if (cut != std::string::npos) { nameStr = nameStr.substr(0, cut); }
                    }

                    if (!typeStr.empty() && !nameStr.empty())
                    {
                        ParsedField field;
                        field.TypeStr = typeStr;
                        field.Name = nameStr;
                        field.Type = ResolveFieldType(typeStr);
                        current.Fields.push_back(field);
                    }

                    expectField = false;
                }

                continue;
            }

            if (trimmed.find("ANTELOPE_SCRIPT()") != std::string::npos)
            {
                expectScript = true;
                continue;
            }

            if (trimmed.find("ANTELOPE_SYSTEM()") != std::string::npos)
            {
                expectSystem = true;
                continue;
            }

            if (trimmed.rfind("class ", 0) == 0)
            {
                size_t nameEnd { trimmed.find_first_of(" \t{:", 6) };
                std::string className { nameEnd == std::string::npos
                    ? trimmed.substr(6)
                    : trimmed.substr(6, nameEnd - 6) };

                if (!className.empty())
                {
                    std::string rel { std::filesystem::relative(headerPath, scriptsDir).string() };
                    std::replace(rel.begin(), rel.end(), '\\', '/');

                    if (expectScript) { outScripts.push_back({ className, rel }); }
                    if (expectSystem) { outSystems.push_back({ className, rel }); }
                }

                expectScript = false;
                expectSystem = false;
            }
        }
    }

    static std::string SerializeField(const ParsedField& field, const std::string& compName)
    {
        std::ostringstream ss;
        if (field.Type == 3)
        {
            ss << "        *out << YAML::Key << \"" << field.Name << "\" << YAML::Value"
               << " << YAML::Flow << YAML::BeginSeq"
               << " << c." << field.Name << ".x << c." << field.Name << ".y << c." << field.Name << ".z"
               << " << YAML::EndSeq;\n";
        }
        else
        {
            ss << "        *out << YAML::Key << \"" << field.Name << "\" << YAML::Value << c." << field.Name << ";\n";
        }
        return ss.str();
    }

    static std::string DeserializeField(const ParsedField& field)
    {
        std::ostringstream ss;
        ss << "        if ((*node)[\"" << field.Name << "\"])\n        {\n";

        if (field.Type == 0) { ss << "            c." << field.Name << " = (*node)[\"" << field.Name << "\"].as<float>();\n"; }
        else if (field.Type == 1) { ss << "            c." << field.Name << " = (*node)[\"" << field.Name << "\"].as<int>();\n"; }
        else if (field.Type == 2) { ss << "            c." << field.Name << " = (*node)[\"" << field.Name << "\"].as<bool>();\n"; }
        else if (field.Type == 3)
        {
            ss << "            auto n = (*node)[\"" << field.Name << "\"];\n";
            ss << "            c." << field.Name << " = { n[0].as<float>(), n[1].as<float>(), n[2].as<float>() };\n";
        }
        else if (field.Type == 4) { ss << "            c." << field.Name << " = (*node)[\"" << field.Name << "\"].as<std::string>();\n"; }

        ss << "        }\n";
        return ss.str();
    }

    void HeaderTool::GenerateFile(
        const std::vector<ParsedComponent>& components,
        const std::vector<ParsedScript>& scripts,
        const std::vector<ParsedSystem>& systems,
        const std::string& outputDir)
    {
        std::filesystem::create_directories(outputDir);
        std::ofstream out(std::filesystem::path(outputDir) / "Scripts.generated.cpp");

        out << "// AUTO-GENERATED by Antelope HeaderTool. DO NOT EDIT.\n\n";
        out << "#include <Engine/Scripting/ScriptDLLInterface.hpp>\n";
        out << "#include <Engine/Scripting/ScriptMacros.hpp>\n";
        out << "#include <yaml-cpp/yaml.h>\n\n";

        for (const auto& comp : components) { out << "#include \"" << comp.SourceFile << "\"\n"; }
        for (const auto& s : scripts) { out << "#include \"" << s.SourceFile << "\"\n"; }
        for (const auto& s : systems) { out << "#include \"" << s.SourceFile << "\"\n"; }
        out << "\n";

        for (const auto& comp : components)
        {
            out << "static Antelope::ScriptFieldDescriptorC s_" << comp.Name << "_Fields[] =\n{\n";
            for (const auto& field : comp.Fields)
            {
                out << "    { \"" << field.Name << "\", " << field.Type
                    << ", offsetof(" << comp.Name << ", " << field.Name << ") },\n";
            }
            out << "};\n\n";
        }

        if (components.empty())
        {
            out << "static Antelope::ComponentDescriptorC* s_Components { nullptr };\n\n";
        }
        else
        {
            out << "static Antelope::ComponentDescriptorC s_Components[] =\n{\n";
            for (const auto& comp : components)
            {
                out << "    {\n";
                out << "        \"" << comp.Name << "\",\n";
                out << "        s_" << comp.Name << "_Fields,\n";
                out << "        " << comp.Fields.size() << ",\n";
                out << "        [](entt::registry* r, entt::entity e) { r->emplace_or_replace<" << comp.Name << ">(e); },\n";
                out << "        [](entt::registry* r, entt::entity e) { r->remove<" << comp.Name << ">(e); },\n";
                out << "        [](entt::registry* r, entt::entity e) -> bool { return r->all_of<" << comp.Name << ">(e); },\n";
                out << "        [](entt::registry* r, entt::entity e) -> void* { return &r->get<" << comp.Name << ">(e); },\n";
                out << "        [](YAML::Emitter* out, entt::registry* r, entt::entity e)\n        {\n";
                out << "            auto& c { r->get<" << comp.Name << ">(e) };\n";
                out << "            *out << YAML::Key << \"" << comp.Name << "\" << YAML::Value << YAML::BeginMap;\n";
                for (const auto& field : comp.Fields) { out << SerializeField(field, comp.Name); }
                out << "            *out << YAML::EndMap;\n        },\n";
                out << "        [](const YAML::Node* node, entt::registry* r, entt::entity e)\n        {\n";
                out << "            auto& c { r->emplace_or_replace<" << comp.Name << ">(e) };\n";
                for (const auto& field : comp.Fields) { out << DeserializeField(field); }
                out << "        }\n    },\n";
            }
            out << "};\n\n";
        }

        out << "static const char* s_ScriptNames[] = { ";
        for (const auto& s : scripts) { out << "\"" << s.Name << "\", "; }
        out << "nullptr };\n\n";

        out << "static const char* s_SystemNames[] = { ";
        for (const auto& s : systems) { out << "\"" << s.Name << "\", "; }
        out << "nullptr };\n\n";

        out << "extern \"C\"\n{\n";
        out << "    ANTELOPE_SCRIPT_EXPORT Antelope::ComponentDescriptorC* GetComponentDescriptors(uint32_t* outCount)\n";
        out << "    { *outCount = " << components.size() << "; return s_Components; }\n\n";
        out << "    ANTELOPE_SCRIPT_EXPORT const char** GetRegisteredScripts(uint32_t* outCount)\n";
        out << "    { *outCount = " << scripts.size() << "; return s_ScriptNames; }\n\n";
        out << "    ANTELOPE_SCRIPT_EXPORT const char** GetRegisteredSystems(uint32_t* outCount)\n";
        out << "    { *outCount = " << systems.size() << "; return s_SystemNames; }\n\n";

        for (const auto& s : scripts)
        {
            out << "    ANTELOPE_SCRIPT_EXPORT Antelope::Script* CreateScript_" << s.Name
                << "() { return new " << s.Name << "(); }\n";
            out << "    ANTELOPE_SCRIPT_EXPORT void DestroyScript_" << s.Name
                << "(Antelope::Script* s) { delete static_cast<" << s.Name << "*>(s); }\n\n";
        }
        for (const auto& s : systems)
        {
            out << "    ANTELOPE_SCRIPT_EXPORT Antelope::GameSystem* CreateSystem_" << s.Name
                << "() { return new " << s.Name << "(); }\n";
            out << "    ANTELOPE_SCRIPT_EXPORT void DestroySystem_" << s.Name
                << "(Antelope::GameSystem* s) { delete static_cast<" << s.Name << "*>(s); }\n\n";
        }
        out << "}\n";

        AE_ENGINE_INFO("HeaderTool: Generated Scripts.generated.cpp ({0} components, {1} scripts, {2} systems).",
            components.size(), scripts.size(), systems.size());
    }

    void HeaderTool::ScanAndGenerate(const std::string& scriptsDir, const std::string& outputDir)
    {
        std::vector<ParsedComponent> components;
        std::vector<ParsedScript> scripts;
        std::vector<ParsedSystem> systems;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(scriptsDir))
        {
            if (entry.path().extension() == ".hpp")
            {
                ScanHeader(entry.path(), scriptsDir, components, scripts, systems);
            }
        }

        if (components.empty() && scripts.empty() && systems.empty())
        {
            AE_ENGINE_TRACE("HeaderTool: Nothing to generate in '{0}'.", scriptsDir);
            return;
        }

        GenerateFile(components, scripts, systems, outputDir);
    }
}
#endif
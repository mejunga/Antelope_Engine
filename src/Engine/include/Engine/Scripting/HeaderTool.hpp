#pragma once

#ifdef ANTELOPE_EDITOR_MODE
#include <string>
#include <vector>
#include <filesystem>


namespace Antelope
{
    struct ParsedField
    {
        std::string Name;
        std::string TypeStr;
        int32_t Type { 5 };
    };

    struct ParsedComponent
    {
        std::string Name;
        std::string SourceFile;
        std::vector<ParsedField> Fields;
    };

    struct ParsedScript
    {
        std::string Name;
        std::string SourceFile;
    };

    struct ParsedSystem
    {
        std::string Name;
        std::string SourceFile;
    };

    class HeaderTool
    {
        public:
            static void ScanAndGenerate(const std::string& scriptsDir, const std::string& outputDir);

        private:
            static void ScanHeader(
                const std::filesystem::path& headerPath,
                const std::filesystem::path& scriptsDir,
                std::vector<ParsedComponent>& outComponents,
                std::vector<ParsedScript>& outScripts,
                std::vector<ParsedSystem>& outSystems);

            static void GenerateFile(
                const std::vector<ParsedComponent>& components,
                const std::vector<ParsedScript>& scripts,
                const std::vector<ParsedSystem>& systems,
                const std::string& outputDir);

            static int32_t ResolveFieldType(const std::string& typeStr);
            static std::string Trim(const std::string& str);
    };
}
#endif
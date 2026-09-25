#include "jsoncpp_ex/jsoncpp_ex.h"

#include <memory>
#include <sstream>
#include <utility>

namespace ytpp::json_ex {

std::string Serialize(_In_ const Json::Value& value) {
    return Serialize(value, false);
}

std::string Serialize(_In_ const Json::Value& value, _In_ bool formatted) {
    Json::StreamWriterBuilder builder;
    builder["emitUTF8"] = true;
    if (!formatted) {
        builder["indentation"] = "";
        builder["commentStyle"] = "None";
    }
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    std::ostringstream stream;
    writer->write(value, &stream);
    return stream.str();
}

std::string Serialize(_In_ const Json::Value& value, _In_ std::string commentStyle, _In_ std::string indentation,
                      _In_ bool enableYamlCompatibility, _In_ bool dropNullPlaceholders, _In_ bool useSpecialFloats,
                      _In_ int precision, _In_ std::string precisionType, _In_ bool emitUtf8) {
    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = std::move(commentStyle);
    builder["indentation"] = std::move(indentation);
    builder["enableYAMLCompatibility"] = enableYamlCompatibility;
    builder["dropNullPlaceholders"] = dropNullPlaceholders;
    builder["useSpecialFloats"] = useSpecialFloats;
    builder["precision"] = precision;
    builder["precisionType"] = std::move(precisionType);
    builder["emitUTF8"] = emitUtf8;
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    std::ostringstream stream;
    writer->write(value, &stream);
    return stream.str();
}

bool Parse(_In_ const std::string& jsonText, _Out_ Json::Value& value) {
    Json::Reader reader;
    return reader.parse(jsonText, value);
}

bool Parse(_In_ const std::string& jsonText, _Out_ Json::Value& value, _In_ bool collectComments) {
    Json::Reader reader;
    return reader.parse(jsonText, value, collectComments);
}

bool Parse(_In_ const std::string& jsonText, _Out_ Json::Value& value, _Out_opt_ Json::String& error,
           _In_ bool collectComments, _In_ bool allowComments, _In_ bool allowTrailingCommas, _In_ bool strictRoot,
           _In_ bool allowDroppedNullPlaceholders, _In_ bool allowNumericKeys, _In_ bool allowSingleQuotes,
           _In_ int stackLimit, _In_ bool failIfExtra, _In_ bool rejectDuplicateKeys, _In_ bool allowSpecialFloats,
           _In_ bool skipBom) {
    Json::CharReaderBuilder builder;
    builder["collectComments"] = collectComments;
    builder["allowComments"] = allowComments;
    builder["allowTrailingCommas"] = allowTrailingCommas;
    builder["strictRoot"] = strictRoot;
    builder["allowDroppedNullPlaceholders"] = allowDroppedNullPlaceholders;
    builder["allowNumericKeys"] = allowNumericKeys;
    builder["allowSingleQuotes"] = allowSingleQuotes;
    builder["stackLimit"] = stackLimit;
    builder["failIfExtra"] = failIfExtra;
    builder["rejectDupKeys"] = rejectDuplicateKeys;
    builder["allowSpecialFloats"] = allowSpecialFloats;
    builder["skipBOM"] = skipBom;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    return reader->parse(jsonText.data(), jsonText.data() + jsonText.size(), &value, &error);
}

} // namespace ytpp::json_ex

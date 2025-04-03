#include "jsoncpp_ex/jsoncpp_ex.h"
//#include "jsoncpp_ex_self.h"


namespace ytpp {
	namespace json
	{
		std::string json_toString(_In_ const Json::Value & value)
		{
			Json::StreamWriterBuilder swb;
			swb["emitUTF8"] = true;
			unique_ptr<Json::StreamWriter> writer(swb.newStreamWriter());
			stringstream ss;
			writer->write(value, &ss);
			return ss.str();
		}

		std::string json_toString(
			_In_ const Json::Value & value,
			_In_ bool format)
		{
			Json::StreamWriterBuilder swb;
			swb["emitUTF8"] = true;
			if (!format) {
				swb["indentation"] = "";
				swb["commentStyle"] = "None";
			}
			unique_ptr<Json::StreamWriter> writer(swb.newStreamWriter());
			stringstream ss;
			writer->write(value, &ss);
			return ss.str();
		}

		std::string json_toStringEx(
			_In_ const Json::Value & value,
			_In_ bool commentStyle /*= "All"*/,
			_In_ string indentation /*= " "*/,
			_In_ bool enbleYAMLCompatibility /*= false*/,
			_In_ bool dropNullPlaceholders /*= false*/,
			_In_ bool useSpecialFloats /*= false*/,
			_In_ int precision /*= 5*/,
			_In_ string precisionType /*= "significant"*/,
			_In_ bool emitUTF8 /*= false*/)
		{
			Json::StreamWriterBuilder swb;
			swb["commentStyle"] = commentStyle;
			swb["indentation"] = indentation;
			swb["enableYAMLCompatibility"] = enbleYAMLCompatibility;
			swb["dropNullPlaceholders"] = dropNullPlaceholders;
			swb["useSpecialFloats"] = useSpecialFloats;
			swb["precision"] = precision;
			swb["precisionType"] = precisionType;
			swb["emitUTF8"] = emitUTF8;
			unique_ptr<Json::StreamWriter> writer(swb.newStreamWriter());
			stringstream ss;
			writer->write(value, &ss);
			return ss.str();
		}

		bool json_fromString(
			_In_ const string & jsonStr,
			_Out_ Json::Value & value)
		{
			Json::Reader reader;
			return reader.parse(jsonStr, value);
		}

		bool json_fromString(
			_In_ const string & jsonStr,
			_Out_ Json::Value & value,
			_In_ bool collectComments)
		{
			Json::Reader reader;
			return reader.parse(jsonStr, value, collectComments);
		}

		bool json_fromStringEx(
			_In_ const string & jsonStr,
			_Out_ Json::Value & value,
			_Out_ Json::String & error,
			_In_ bool collectComments /*= true*/,
			_In_ bool allowComments /*= true*/,
			_In_ bool allowTrailingCommas /*= true*/,
			_In_ bool strictRoot /*= false*/,
			_In_ bool allowDroppedNullPlaceholders /*= true*/,
			_In_ bool allowNumericKeys /*= true*/,
			_In_ bool allowSingleQuotes /*= true*/,
			_In_ int stackLimit /*= 1024*/,
			_In_ bool failIfExtra /*= false*/,
			_In_ bool rejectDupKeys /*= false*/,
			_In_ bool allowSpecialFloats /*= true*/,
			_In_ bool skipBom /*= true */)
		{
			Json::CharReaderBuilder crb;
			crb["collectComments"] = collectComments;
			crb["allowComments"] = allowComments;
			crb["allowTrailingCommas"] = allowTrailingCommas;
			crb["strictRoot"] = strictRoot;
			crb["allowDroppedNullPlaceholders"] = allowDroppedNullPlaceholders;
			crb["allowNumericKeys"] = allowNumericKeys;
			crb["allowSingleQuotes"] = allowSingleQuotes;
			crb["stackLimit"] = stackLimit;
			crb["failIfExtra"] = failIfExtra;
			crb["rejectDupKeys"] = rejectDupKeys;
			crb["allowSpecialFloats"] = allowSpecialFloats;
			crb["skipBOM"] = skipBom;
			unique_ptr<Json::CharReader> reader(crb.newCharReader());
			return reader->parse(jsonStr.c_str(), jsonStr.c_str() + jsonStr.size(), &value, &error);
		}

	}
}
#include "jsoncpp_ex.h"
//#include "jsoncpp_ex_self.h"


namespace ytpp {
	std::string json_toString( _In_ const Json::Value& value )
	{
		Json::StreamWriterBuilder swb;
		swb["emitUTF8"] = true;
		unique_ptr<Json::StreamWriter> writer( swb.newStreamWriter() );
		stringstream ss; 
		writer->write( value, &ss );
		return ss.str();
	}

	std::string json_toString(
		_In_ const Json::Value& value,
		_In_ bool format )
	{
		Json::StreamWriterBuilder swb;
		swb["emitUTF8"] = true;
		if ( !format ) {
			swb["indentation"] = "";
			swb["commentStyle"] = "None";
		}
		unique_ptr<Json::StreamWriter> writer( swb.newStreamWriter() );
		stringstream ss;
		writer->write( value, &ss );
		return ss.str();
	}

	std::string json_toStringEx( 
		_In_ const Json::Value& value, 
		_In_ bool commentStyle /*= "All"*/, 
		_In_ string indentation /*= " "*/, 
		_In_ bool enbleYAMLCompatibility /*= false*/,
		_In_ bool dropNullPlaceholders /*= false*/,
		_In_ bool useSpecialFloats /*= false*/, 
		_In_ int precision /*= 5*/, 
		_In_ string precisionType /*= "significant"*/, 
		_In_ bool emitUTF8 /*= false*/ )
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
		unique_ptr<Json::StreamWriter> writer( swb.newStreamWriter() );
		stringstream ss;
		writer->write( value, &ss );
		return ss.str();
	}

}
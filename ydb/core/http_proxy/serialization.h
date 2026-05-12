#pragma once

#include "http_req.h"
#include "json_proto_conversion.h"

#include <ydb/library/http_proxy/error/error.h>

#include <contrib/libs/protobuf/src/google/protobuf/message.h>

#include <library/cpp/mime/types/mime.h>
//#include <nlohmann/json.hpp>

namespace NKikimr::NHttpProxy {

template<typename TValue>
void Prepare(TValue& value)
    requires std::is_base_of_v<NProtoBuf::Message, TValue> {
    Y_UNUSED(value);
}

template<typename TValue>
void DeserializeJson(TStringBuf serializedValue, TValue& value)
    requires std::is_base_of_v<NProtoBuf::Message, TValue> {

    auto fromJson = nlohmann::json::parse(serializedValue, nullptr, false);
    if (fromJson.is_discarded()) {
        throw NKikimr::NSQS::TSQSException(NKikimr::NSQS::NErrors::MALFORMED_QUERY_STRING) <<
            "Can not parse request body from JSON";
    } else {
        Prepare(value);
        NlohmannJsonToProto(fromJson, &value);
    }
}

template<typename TValue>
void DeserializeCbor(TStringBuf serializedValue, TValue& value)
    requires std::is_base_of_v<NProtoBuf::Message, TValue> {

    auto fromCbor = nlohmann::json::from_cbor(
        serializedValue.begin(),
        serializedValue.end(),
        true, // strict mode
        false, // allow exceptions
        nlohmann::json::cbor_tag_handler_t::ignore
    );
    if (fromCbor.is_discarded()) {
        throw NKikimr::NSQS::TSQSException(NKikimr::NSQS::NErrors::MALFORMED_QUERY_STRING) <<
            "Can not parse request body from CBOR";
    } else {
        Prepare(value);
        NlohmannJsonToProto(fromCbor, &value);
    }
}

template<typename TValue>
void DeserializeXml(NHttp::THttpIncomingRequestPtr& request, TValue& value)
    requires std::is_base_of_v<NProtoBuf::Message, TValue>;

template<typename TValue>
void Deserialize(THttpRequestContext& context, TValue& value)
    requires std::is_base_of_v<NProtoBuf::Message, TValue> {

    TStringBuf serializedValue = context.Request->Body;
    if (serializedValue.empty()) {
        throw NKikimr::NSQS::TSQSException(NKikimr::NSQS::NErrors::MALFORMED_QUERY_STRING) <<
            "Empty body";
    }

    switch (context.ContentType) {
    case MIME_JSON:
        DeserializeJson(context.Request->Body, value);
        break;
    case MIME_CBOR:
        DeserializeCbor(context.Request->Body, value);
        break;
    case MIME_XML:
        DeserializeXml(context.Request, value);
        break;
    default:
        throw NKikimr::NSQS::TSQSException(NKikimr::NSQS::NErrors::MALFORMED_QUERY_STRING) <<
            "Unknown ContentType";
    }
}


} // namespace NKikimr::NHttpProxy

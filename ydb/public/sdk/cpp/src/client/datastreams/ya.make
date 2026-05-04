LIBRARY()

SRCS(
    datastreams.cpp
)

PEERDIR(
    contrib/restricted/nlohmann_json
    ydb/public/sdk/cpp/src/library/grpc/client
    library/cpp/string_utils/url
    ydb/public/api/grpc/draft
    ydb/public/sdk/cpp/src/library/operation_id
    ydb/public/sdk/cpp/src/client/impl/internal/make_request
    ydb/public/sdk/cpp/src/client/driver
    ydb/core/protos
    ydb/library/services
    ydb/library/actors/protos
)

END()

#pragma once

#include <ydb/core/fq/libs/events/event_subspace.h>
#include <ydb/core/fq/libs/row_dispatcher/events/topic_session_stats.h>
#include <ydb/core/fq/libs/row_dispatcher/protos/events.pb.h>

#include <ydb/library/actors/core/actorid.h>
#include <ydb/library/actors/core/event_local.h>
#include <ydb/library/purecalc/compilation/compile_service.h>
#include <ydb/library/yql/providers/pq/proto/dq_io.pb.h>

#include <yql/essentials/public/issue/yql_issue.h>

#include <util/generic/set.h>
#include <util/generic/map.h>

namespace NFq {

NActors::TActorId RowDispatcherServiceActorId();

struct TEvRowDispatcher {
    // Event ids.
    enum EEv : ui32 {
        EvCoordinatorChanged = YqEventSubspaceBegin(TYqEventSubspace::RowDispatcher),
        EvStartSession,
        EvStartSessionAck,
        EvNewDataArrived,
        EvGetNextBatch,
        EvMessageBatch,
        EvStatistics,
        EvStopSession,
        EvSessionError,
        EvCoordinatorChangesSubscribe,
        EvCoordinatorRequest,
        EvCoordinatorResult,
        EvSessionStatistic,
        EvHeartbeat,
        EvNoSession,
        EvGetInternalStateRequest,
        EvGetInternalStateResponse,
        EvPurecalcCompileRequest,
        EvPurecalcCompileResponse,
        EvPurecalcCompileAbort,
        EvEnd,
    };

    struct TEvCoordinatorChanged : NActors::TEventLocal<TEvCoordinatorChanged, EEv::EvCoordinatorChanged> {
        TEvCoordinatorChanged(NActors::TActorId coordinatorActorId, ui64 generation)
            : CoordinatorActorId(coordinatorActorId)
            , Generation(generation) {
        }
        NActors::TActorId CoordinatorActorId;
        ui64 Generation = 0;
    };

    struct TEvCoordinatorChangesSubscribe : public NActors::TEventLocal<TEvCoordinatorChangesSubscribe, EEv::EvCoordinatorChangesSubscribe> {};

    struct TEvCoordinatorRequest : public NActors::TEventPB<TEvCoordinatorRequest,
        NFq::NRowDispatcherProto::TEvGetAddressRequest, EEv::EvCoordinatorRequest> {
        TEvCoordinatorRequest() = default;
        TEvCoordinatorRequest(
            const NYql::NPq::NProto::TDqPqTopicSource& sourceParams, 
            const std::vector<ui64>& partitionIds) {
            *Record.MutableSource() = sourceParams;
            for (const auto& id : partitionIds) {
                Record.AddPartitionIds(id);
            }
        }
    };

    struct TEvCoordinatorResult : public NActors::TEventPB<TEvCoordinatorResult,
        NFq::NRowDispatcherProto::TEvGetAddressResponse, EEv::EvCoordinatorResult> {
        TEvCoordinatorResult() = default;
    };

// Session events (with seqNo checks)

    struct TEvStartSession : public NActors::TEventPB<TEvStartSession,
        NFq::NRowDispatcherProto::TEvStartSession, EEv::EvStartSession> {
            
        TEvStartSession() = default;
        TEvStartSession(
            const NYql::NPq::NProto::TDqPqTopicSource& sourceParams,
            const std::set<ui32>& partitionIds,
            const TString token,
            const std::map<ui32, ui64>& readOffsets,
            ui64 startingMessageTimestampMs,
            const TString& queryId) {
            *Record.MutableSource() = sourceParams;
            for (auto partitionId : partitionIds) {
                Record.AddPartitionIds(partitionId);
            }
            Record.SetToken(token);
            for (const auto& [partitionId, offset] : readOffsets) {
                auto* partitionOffset = Record.AddOffsets();
                partitionOffset->SetPartitionId(partitionId);
                partitionOffset->SetOffset(offset);
            }
            Record.SetStartingMessageTimestampMs(startingMessageTimestampMs);
            Record.SetQueryId(queryId);
        }
    };

    struct TEvStartSessionAck : public NActors::TEventPB<TEvStartSessionAck,
        NFq::NRowDispatcherProto::TEvStartSessionAck, EEv::EvStartSessionAck> {
        TEvStartSessionAck() = default;
        explicit TEvStartSessionAck(
            const NFq::NRowDispatcherProto::TEvStartSession& consumer) {
            *Record.MutableConsumer() = consumer;
        }
    };

    struct TEvNewDataArrived : public NActors::TEventPB<TEvNewDataArrived,
        NFq::NRowDispatcherProto::TEvNewDataArrived, EEv::EvNewDataArrived> {
        TEvNewDataArrived() = default;
        NActors::TActorId ReadActorId;
    };

    struct TEvGetNextBatch : public NActors::TEventPB<TEvGetNextBatch,
        NFq::NRowDispatcherProto::TEvGetNextBatch, EEv::EvGetNextBatch> {
        TEvGetNextBatch() = default;
    };

    struct TEvStopSession : public NActors::TEventPB<TEvStopSession,
        NFq::NRowDispatcherProto::TEvStopSession, EEv::EvStopSession> {
        TEvStopSession() = default;
    };

    struct TEvMessageBatch : public NActors::TEventPB<TEvMessageBatch,
        NFq::NRowDispatcherProto::TEvMessageBatch, EEv::EvMessageBatch> {
        TEvMessageBatch() = default;
        NActors::TActorId ReadActorId;
    };

    struct TEvStatistics : public NActors::TEventPB<TEvStatistics,
        NFq::NRowDispatcherProto::TEvStatistics, EEv::EvStatistics> {
        TEvStatistics() = default;
    };

    struct TEvSessionError : public NActors::TEventPB<TEvSessionError,
        NFq::NRowDispatcherProto::TEvSessionError, EEv::EvSessionError> {
        TEvSessionError() = default;
        NActors::TActorId ReadActorId;
    };

    struct TEvSessionStatistic : public NActors::TEventLocal<TEvSessionStatistic, EEv::EvSessionStatistic> {
        TEvSessionStatistic(const TTopicSessionStatistic& stat)
        : Stat(stat) {}
        TTopicSessionStatistic Stat;
    };

// Network events (without seqNo checks)

    struct TEvHeartbeat : public NActors::TEventPB<TEvHeartbeat, NFq::NRowDispatcherProto::TEvHeartbeat, EEv::EvHeartbeat> {
        TEvHeartbeat() = default;
    };

    struct TEvNoSession : public NActors::TEventPB<TEvNoSession, NFq::NRowDispatcherProto::TEvNoSession, EEv::EvNoSession> {
        TEvNoSession() = default;
    };

    struct TEvGetInternalStateRequest : public NActors::TEventPB<TEvGetInternalStateRequest,
        NFq::NRowDispatcherProto::TEvGetInternalStateRequest, EEv::EvGetInternalStateRequest> {
        TEvGetInternalStateRequest() = default;
    };

    struct TEvGetInternalStateResponse : public NActors::TEventPB<TEvGetInternalStateResponse,
        NFq::NRowDispatcherProto::TEvGetInternalStateResponse, EEv::EvGetInternalStateResponse> {
        TEvGetInternalStateResponse() = default;
    };
};

} // namespace NFq

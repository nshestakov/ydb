#pragma once

#include <ydb/core/base/statestorage.h>

#include <util/generic/hash_set.h>
#include <util/generic/map.h>
#include <util/generic/string.h>

namespace NKikimr::NReplication {

inline TActorId MakeReplicationServiceId(ui32 nodeId);

namespace NService {

using TBoardInfoEntries = TMap<TActorId, TEvStateStorage::TBoardInfoEntry>;

inline TString MakeDiscoveryPath(const TString& tenant);

ui32 PickMasterNode(const TBoardInfoEntries& infoEntries);

std::optional<TActorId> ResolveMasterReplicationService(const TBoardInfoEntries& infoEntries);

struct TEvMasterElectionPrivate {
    enum EEv {
        EvStateUpdated = EventSpaceBegin(TEvents::ES_PRIVATE),
    };

    struct TEvStateUpdated: public TEventLocal<TEvStateUpdated, EvStateUpdated> {
        bool IsMaster = false;
        ui32 MasterNodeId = 0;
        THashSet<ui32> Peers;
    };
};

IActor* CreateReplicationMasterElection(const TActorId& parent, const TString& tenant);

} // NService

} // NKikimr::NReplication

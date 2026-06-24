#include "logging.h"
#include "master_election.h"
#include "service.h"

#include <ydb/core/base/appdata.h>
#include <ydb/core/base/domain.h>
#include <ydb/core/base/path.h>
#include <ydb/core/base/statestorage.h>
#include <ydb/library/actors/core/actor_bootstrapped.h>
#include <ydb/library/actors/core/hfunc.h>
#include <ydb/library/actors/core/interconnect.h>

namespace NKikimr::NReplication::NService {

ui32 PickMasterNode(const TBoardInfoEntries& infoEntries) {
    ui32 result = 0;
    for (const auto& [actor, entry] : infoEntries) {
        if (entry.Dropped) {
            continue;
        }

        const ui32 nodeId = actor.NodeId();
        if (result == 0 || nodeId < result) {
            result = nodeId;
        }
    }

    return result;
}

std::optional<TActorId> ResolveMasterReplicationService(const TBoardInfoEntries& infoEntries) {
    const auto masterNode = PickMasterNode(infoEntries);
    if (masterNode == 0) {
        return std::nullopt;
    }

    return MakeReplicationServiceId(masterNode);
}

class TReplicationMasterElection: public TActorBootstrapped<TReplicationMasterElection> {
    TStringBuf GetLogPrefix() const {
        if (!LogPrefix) {
            LogPrefix = TStringBuilder()
                << "[MasterElection]"
                << SelfId() << " ";
        }

        return LogPrefix.GetRef();
    }

    void SubscribeToBoard() {
        if (BoardSubscriber) {
            Send(BoardSubscriber, new TEvents::TEvPoison());
            BoardSubscriber = {};
        }

        BoardSubscriber = Register(CreateBoardLookupActor(
            BoardPath, SelfId(), EBoardLookupMode::Subscription));
    }

    void PublishToBoard() {
        if (BoardPublisher) {
            Send(BoardPublisher, new TEvents::TEvPoison());
            BoardPublisher = {};
        }

        BoardPublisher = Register(CreateBoardPublishActor(BoardPath, TString(), SelfId(), 0, true));
    }

    THashSet<ui32> CollectPeers() const {
        THashSet<ui32> peers;
        for (const auto& [actor, entry] : BoardInfo) {
            if (!entry.Dropped) {
                peers.insert(actor.NodeId());
            }
        }

        return peers;
    }

    void UpdateMaster() {
        const ui32 masterNodeId = PickMasterNode(BoardInfo);
        const bool isMaster = masterNodeId == SelfId().NodeId();
        const auto peers = CollectPeers();

        if (isMaster == IsMaster && masterNodeId == MasterNodeId && peers == Peers) {
            return;
        }

        IsMaster = isMaster;
        MasterNodeId = masterNodeId;
        Peers = std::move(peers);

        LOG_I("Master state updated"
            << ": isMaster# " << IsMaster
            << ", masterNode# " << MasterNodeId
            << ", peers# " << Peers.size());

        auto ev = MakeHolder<TEvMasterElectionPrivate::TEvStateUpdated>();
        ev->IsMaster = IsMaster;
        ev->MasterNodeId = MasterNodeId;
        ev->Peers = Peers;
        Send(Parent, ev.Release());
    }

    void ApplyBoardEntries(const TBoardInfoEntries& entries) {
        for (const auto& [actor, entry] : entries) {
            if (entry.Dropped) {
                BoardInfo.erase(actor);
            } else {
                BoardInfo.insert_or_assign(actor, entry);
            }
        }

        UpdateMaster();
    }

    void Handle(TEvStateStorage::TEvBoardInfo::TPtr& ev) {
        if (ev->Get()->Status != TEvStateStorage::TEvBoardInfo::EStatus::Ok) {
            SubscribeToBoard();
            return;
        }

        BoardInfo = ev->Get()->InfoEntries;
        UpdateMaster();
    }

    void Handle(TEvStateStorage::TEvBoardInfoUpdate::TPtr& ev) {
        if (ev->Get()->Status != TEvStateStorage::TEvBoardInfo::EStatus::Ok) {
            SubscribeToBoard();
            return;
        }

        ApplyBoardEntries(ev->Get()->Updates);
    }

    void Handle(TEvInterconnect::TEvNodeDisconnected::TPtr& ev) {
        const ui32 nodeId = ev->Get()->NodeId;
        bool changed = false;

        for (auto it = BoardInfo.begin(); it != BoardInfo.end(); ) {
            if (it->first.NodeId() == nodeId) {
                it = BoardInfo.erase(it);
                changed = true;
            } else {
                ++it;
            }
        }

        if (changed) {
            LOG_I("Node disconnected"
                << ": node# " << nodeId);
            UpdateMaster();
        }
    }

    void PassAway() override {
        if (BoardPublisher) {
            Send(BoardPublisher, new TEvents::TEvPoison());
            BoardPublisher = {};
        }

        if (BoardSubscriber) {
            Send(BoardSubscriber, new TEvents::TEvPoison());
            BoardSubscriber = {};
        }

        TActorBootstrapped<TReplicationMasterElection>::PassAway();
    }

public:
    TReplicationMasterElection(const TActorId& parent, const TString& tenant)
        : Parent(parent)
        , Tenant(tenant)
        , BoardPath(MakeDiscoveryPath(tenant))
    {
    }

    void Bootstrap() {
        auto* domainInfo = AppData()->DomainsInfo->GetDomainByName(ExtractDomain(Tenant));
        if (!domainInfo) {
            return PassAway();
        }

        Become(&TThis::StateWork);
        PublishToBoard();
        SubscribeToBoard();
    }

    STATEFN(StateWork) {
        switch (ev->GetTypeRewrite()) {
            hFunc(TEvStateStorage::TEvBoardInfo, Handle);
            hFunc(TEvStateStorage::TEvBoardInfoUpdate, Handle);
            hFunc(TEvInterconnect::TEvNodeDisconnected, Handle);
            sFunc(TEvents::TEvPoison, PassAway);
        }
    }

private:
    const TActorId Parent;
    const TString Tenant;
    const TString BoardPath;

    mutable TMaybe<TString> LogPrefix;
    TActorId BoardPublisher;
    TActorId BoardSubscriber;
    TBoardInfoEntries BoardInfo;

    bool IsMaster = false;
    ui32 MasterNodeId = 0;
    THashSet<ui32> Peers;
};

IActor* CreateReplicationMasterElection(const TActorId& parent, const TString& tenant) {
    return new TReplicationMasterElection(parent, tenant);
}

} // NKikimr::NReplication::NService

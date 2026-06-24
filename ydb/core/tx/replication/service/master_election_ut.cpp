#include <ydb/core/tx/replication/service/master_election.h>
#include <ydb/core/tx/replication/service/service.h>

#include <ydb/core/testlib/basics/appdata.h>
#include <ydb/core/testlib/basics/helpers.h>
#include <ydb/core/base/domain.h>
#include <ydb/core/base/statestorage.h>

#include <ydb/library/actors/core/interconnect.h>
#include <ydb/library/actors/interconnect/interconnect_impl.h>
#include <library/cpp/testing/unittest/registar.h>

namespace NKikimr::NReplication::NService {

namespace {

using TStateUpdate = TEvMasterElectionPrivate::TEvStateUpdated;

TBoardInfoEntries MakeEntries(std::initializer_list<ui32> nodeIds, ui32 droppedNodeId = 0) {
    TBoardInfoEntries entries;
    for (ui32 nodeId : nodeIds) {
        entries[MakeReplicationServiceId(nodeId)] = {
            TString(),
            droppedNodeId == nodeId,
        };
    }
    return entries;
}

class TMasterElectionTestBase {
public:
    static constexpr TStringBuf Tenant = "/Root";

    void SetUpRuntime(ui32 nodeCount = 3) {
        Runtime = MakeHolder<TTestBasicRuntime>(nodeCount);
        SetupCustomStateStorage(*Runtime, 3, 3, 1);

        TAppPrepare app;
        app.AddDomain(TDomainsInfo::TDomain::ConstructEmptyDomain("Root").Release());
        app.AddHive(0);
        Runtime->Initialize(app.Unwrap());
    }

    void TearDownRuntime() {
        Runtime.Reset();
    }

    struct TInstance {
        TActorId Edge;
        TActorId Election;
        ui32 NodeIndex = 0;
    };

    TInstance StartMasterElection(ui32 nodeIndex) {
        const TActorId edge = Runtime->AllocateEdgeActor(nodeIndex);
        const TActorId election = Runtime->Register(
            CreateReplicationMasterElection(edge, TString{Tenant}),
            nodeIndex
        );
        Runtime->EnableScheduleForActor(election);
        return {edge, election, nodeIndex};
    }

    TStateUpdate::TPtr WaitForPeers(const TActorId& edge, size_t peersCount) {
        for (size_t attempt = 0; attempt < 100; ++attempt) {
            auto update = Runtime->GrabEdgeEvent<TStateUpdate>(edge);
            if (update && update->Get()->Peers.size() == peersCount) {
                return update;
            }
        }
        return nullptr;
    }

    ui32 GetNodeId(ui32 nodeIndex) const {
        return Runtime->GetNodeId(nodeIndex);
    }

    ui32 GetNodeCount() const {
        return Runtime->GetNodeCount();
    }

    ui32 MinNodeId() const {
        ui32 result = Max<ui32>();
        for (ui32 nodeIndex = 0; nodeIndex < Runtime->GetNodeCount(); ++nodeIndex) {
            result = Min(result, Runtime->GetNodeId(nodeIndex));
        }
        return result;
    }

    void Disconnect(ui32 nodeIndexFrom, ui32 nodeIndexTo) {
        const TActorId proxy = Runtime->GetInterconnectProxy(nodeIndexFrom, nodeIndexTo);
        Runtime->Send(new IEventHandle(proxy, TActorId(), new TEvInterconnect::TEvDisconnect()), nodeIndexFrom, true);

        TDispatchOptions options;
        options.FinalEvents.emplace_back(TEvInterconnect::EvNodeDisconnected);
        Runtime->DispatchEvents(options);
    }

private:
    THolder<TTestBasicRuntime> Runtime;
};

} // namespace

Y_UNIT_TEST_SUITE(MasterElection) {

Y_UNIT_TEST(PickMasterNodeSelectsMinNodeId) {
    const auto entries = MakeEntries({3, 1, 2});
    UNIT_ASSERT_VALUES_EQUAL(PickMasterNode(entries), 1u);
}

Y_UNIT_TEST(PickMasterNodeSkipsDroppedEntries) {
    const auto entries = MakeEntries({1, 2, 3}, 1);
    UNIT_ASSERT_VALUES_EQUAL(PickMasterNode(entries), 2u);
}

Y_UNIT_TEST(PickMasterNodeReturnsZeroForEmptyBoard) {
    UNIT_ASSERT_VALUES_EQUAL(PickMasterNode({}), 0u);
}

Y_UNIT_TEST(ResolveMasterReplicationService) {
    const auto entries = MakeEntries({3, 1, 2});
    const auto master = ResolveMasterReplicationService(entries);
    UNIT_ASSERT(master);
    UNIT_ASSERT_VALUES_EQUAL(master->NodeId(), 1u);
    UNIT_ASSERT_VALUES_EQUAL(*master, MakeReplicationServiceId(1));
}

Y_UNIT_TEST(ResolveMasterReplicationServiceReturnsEmptyForEmptyBoard) {
    UNIT_ASSERT(!ResolveMasterReplicationService({}));
}

Y_UNIT_TEST(MasterElectionSelectsMinNode) {
    TMasterElectionTestBase fixture;
    fixture.SetUpRuntime(3);

    const auto node0 = fixture.StartMasterElection(0);
    const auto node1 = fixture.StartMasterElection(1);
    const auto node2 = fixture.StartMasterElection(2);

    const auto state0 = fixture.WaitForPeers(node0.Edge, 3);
    const auto state1 = fixture.WaitForPeers(node1.Edge, 3);
    const auto state2 = fixture.WaitForPeers(node2.Edge, 3);

    UNIT_ASSERT(state0);
    UNIT_ASSERT(state1);
    UNIT_ASSERT(state2);

    const ui32 masterNodeId = fixture.MinNodeId();
    UNIT_ASSERT_VALUES_EQUAL(state0->Get()->MasterNodeId, masterNodeId);
    UNIT_ASSERT_VALUES_EQUAL(state1->Get()->MasterNodeId, masterNodeId);
    UNIT_ASSERT_VALUES_EQUAL(state2->Get()->MasterNodeId, masterNodeId);

    UNIT_ASSERT_VALUES_EQUAL(state0->Get()->IsMaster, fixture.GetNodeId(0) == masterNodeId);
    UNIT_ASSERT_VALUES_EQUAL(state1->Get()->IsMaster, fixture.GetNodeId(1) == masterNodeId);
    UNIT_ASSERT_VALUES_EQUAL(state2->Get()->IsMaster, fixture.GetNodeId(2) == masterNodeId);

    for (const auto* state : {state0->Get(), state1->Get(), state2->Get()}) {
        UNIT_ASSERT_VALUES_EQUAL(state->Peers.size(), 3u);
        UNIT_ASSERT(state->Peers.contains(fixture.GetNodeId(0)));
        UNIT_ASSERT(state->Peers.contains(fixture.GetNodeId(1)));
        UNIT_ASSERT(state->Peers.contains(fixture.GetNodeId(2)));
    }

    const auto* masterState = state0->Get()->IsMaster ? state0->Get()
        : state1->Get()->IsMaster ? state1->Get()
        : state2->Get();
    UNIT_ASSERT(masterState);
    UNIT_ASSERT(masterState->IsMaster);
    UNIT_ASSERT_VALUES_EQUAL(masterState->Peers.size(), 3u);

    fixture.TearDownRuntime();
}

Y_UNIT_TEST(MasterElectionFailoverOnNodeDisconnect) {
    TMasterElectionTestBase fixture;
    fixture.SetUpRuntime(3);

    const auto node0 = fixture.StartMasterElection(0);
    const auto node1 = fixture.StartMasterElection(1);
    const auto node2 = fixture.StartMasterElection(2);

    UNIT_ASSERT(fixture.WaitForPeers(node0.Edge, 3));
    UNIT_ASSERT(fixture.WaitForPeers(node1.Edge, 3));
    UNIT_ASSERT(fixture.WaitForPeers(node2.Edge, 3));

    const ui32 masterNodeId = fixture.MinNodeId();
    ui32 masterNodeIndex = 0;
    for (ui32 nodeIndex = 0; nodeIndex < fixture.GetNodeCount(); ++nodeIndex) {
        if (fixture.GetNodeId(nodeIndex) == masterNodeId) {
            masterNodeIndex = nodeIndex;
            break;
        }
    }

    for (ui32 nodeIndex = 0; nodeIndex < fixture.GetNodeCount(); ++nodeIndex) {
        if (nodeIndex != masterNodeIndex) {
            fixture.Disconnect(nodeIndex, masterNodeIndex);
        }
    }

    const auto* survivor0 = masterNodeIndex == 0 ? &node1 : &node0;
    const auto* survivor1 = masterNodeIndex == 1 ? &node2 : (masterNodeIndex == 0 ? &node2 : &node1);

    const auto state0 = fixture.WaitForPeers(survivor0->Edge, 2);
    const auto state1 = fixture.WaitForPeers(survivor1->Edge, 2);

    UNIT_ASSERT(state0);
    UNIT_ASSERT(state1);

    ui32 expectedMasterNodeId = Max<ui32>();
    for (ui32 nodeIndex = 0; nodeIndex < fixture.GetNodeCount(); ++nodeIndex) {
        if (nodeIndex == masterNodeIndex) {
            continue;
        }
        expectedMasterNodeId = Min(expectedMasterNodeId, fixture.GetNodeId(nodeIndex));
    }

    UNIT_ASSERT_VALUES_EQUAL(state0->Get()->MasterNodeId, expectedMasterNodeId);
    UNIT_ASSERT_VALUES_EQUAL(state1->Get()->MasterNodeId, expectedMasterNodeId);
    UNIT_ASSERT_VALUES_EQUAL(state0->Get()->IsMaster, fixture.GetNodeId(survivor0->NodeIndex) == expectedMasterNodeId);
    UNIT_ASSERT_VALUES_EQUAL(state1->Get()->IsMaster, fixture.GetNodeId(survivor1->NodeIndex) == expectedMasterNodeId);
    UNIT_ASSERT(!state0->Get()->Peers.contains(masterNodeId));
    UNIT_ASSERT(!state1->Get()->Peers.contains(masterNodeId));

    fixture.TearDownRuntime();
}

} // Y_UNIT_TEST_SUITE(MasterElection)

} // NKikimr::NReplication::NService

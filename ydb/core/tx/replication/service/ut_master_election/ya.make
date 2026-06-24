UNITTEST_FOR(ydb/core/tx/replication/service)

FORK_SUBTESTS()

SIZE(SMALL)

PEERDIR(
    ydb/core/base
    ydb/core/testlib/basics
    ydb/core/testlib/basics/default
    ydb/core/tx/replication/service
    ydb/library/actors/core
    ydb/library/actors/interconnect
    library/cpp/testing/unittest
    yql/essentials/minikql/comp_nodes/llvm16
)

SRCS(
    master_election_ut.cpp
)

YQL_LAST_ABI_VERSION()

END()

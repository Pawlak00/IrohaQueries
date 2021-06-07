/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "torii/impl/command_service_transport_grpc.hpp"

#include <memory>

#include <gtest/gtest.h>
#include <libfuzzer/libfuzzer_macro.h>
#include "backend/protobuf/proto_transport_factory.hpp"
#include "backend/protobuf/proto_tx_status_factory.hpp"
#include "backend/protobuf/transaction.hpp"
#include "interfaces/iroha_internal/transaction_batch_factory_impl.hpp"
#include "interfaces/iroha_internal/transaction_batch_parser_impl.hpp"
#include "logger/dummy_logger.hpp"
#include "module/irohad/ametsuchi/ametsuchi_mocks.hpp"
#include "module/irohad/ametsuchi/mock_tx_presence_cache.hpp"
#include "module/irohad/common/validators_config.hpp"
#include "module/irohad/multi_sig_transactions/mst_mocks.hpp"
#include "module/irohad/network/network_mocks.hpp"
#include "synchronizer/synchronizer_common.hpp"
#include "torii/impl/command_service_impl.hpp"
#include "torii/impl/status_bus_impl.hpp"
#include "torii/processor/transaction_processor_impl.hpp"
#include "transaction.pb.h"
#include "validators/default_validator.hpp"
#include "validators/protobuf/proto_transaction_validator.hpp"

using testing::Return;

struct CommandFixture {
  std::shared_ptr<iroha::torii::CommandService> service_;
  std::shared_ptr<iroha::torii::CommandServiceTransportGrpc> service_transport_;
  std::shared_ptr<iroha::torii::TransactionProcessorImpl> tx_processor_;
  std::shared_ptr<iroha::network::MockPeerCommunicationService> pcs_;
  std::shared_ptr<iroha::MockMstProcessor> mst_processor_;
  std::vector<iroha::torii::CommandServiceTransportGrpc::ConsensusGateEvent>
      consensus_gate_objects_{2};
  std::shared_ptr<iroha::torii::CommandServiceImpl::CacheType> cache_;
  std::shared_ptr<iroha::ametsuchi::MockTxPresenceCache> tx_presence_cache_;

  rxcpp::subjects::subject<iroha::network::OrderingEvent> prop_notifier_;
  rxcpp::subjects::subject<iroha::simulator::VerifiedProposalCreatorEvent>
      vprop_notifier_;
  rxcpp::subjects::subject<iroha::synchronizer::SynchronizationEvent>
      sync_event_notifier_;
  rxcpp::subjects::subject<iroha::DataType> mst_notifier_;
  rxcpp::subjects::subject<std::shared_ptr<iroha::MstState>>
      mst_state_notifier_;
  rxcpp::subjects::subject<iroha::consensus::GateObject> consensus_notifier_;
  rxcpp::subjects::subject<
      std::shared_ptr<const shared_model::interface::Block>>
      commit_notifier_;

  CommandFixture() {
    pcs_ = std::make_shared<iroha::network::MockPeerCommunicationService>();
    EXPECT_CALL(*pcs_, onProposal())
        .WillRepeatedly(Return(prop_notifier_.get_observable()));
    EXPECT_CALL(*pcs_, onSynchronization())
        .WillRepeatedly(Return(sync_event_notifier_.get_observable()));
    EXPECT_CALL(*pcs_, onVerifiedProposal())
        .WillRepeatedly(Return(vprop_notifier_.get_observable()));

    mst_processor_ =
        std::make_shared<iroha::MockMstProcessor>(logger::getDummyLoggerPtr());
    EXPECT_CALL(*mst_processor_, onStateUpdateImpl())
        .WillRepeatedly(Return(mst_state_notifier_.get_observable()));
    EXPECT_CALL(*mst_processor_, onPreparedBatchesImpl())
        .WillRepeatedly(Return(mst_notifier_.get_observable()));
    EXPECT_CALL(*mst_processor_, onExpiredBatchesImpl())
        .WillRepeatedly(Return(mst_notifier_.get_observable()));

    cache_ = std::make_shared<iroha::torii::CommandServiceImpl::CacheType>();

    auto status_bus = std::make_shared<iroha::torii::StatusBusImpl>();
    auto status_factory =
        std::make_shared<shared_model::proto::ProtoTxStatusFactory>();
    tx_processor_ = std::make_shared<iroha::torii::TransactionProcessorImpl>(
        pcs_,
        mst_processor_,
        status_bus,
        status_factory,
        commit_notifier_.get_observable(),
        logger::getDummyLoggerPtr());
    auto storage = std::make_shared<iroha::ametsuchi::MockStorage>();
    service_ = std::make_shared<iroha::torii::CommandServiceImpl>(
        tx_processor_,
        storage,
        status_bus,
        status_factory,
        cache_,
        tx_presence_cache_,
        logger::getDummyLoggerPtr());

    std::unique_ptr<shared_model::validation::AbstractValidator<
        shared_model::interface::Transaction>>
        transaction_validator =
            std::make_unique<shared_model::validation::
                                 DefaultOptionalSignedTransactionValidator>(
                iroha::test::kTestsValidatorsConfig);
    std::unique_ptr<shared_model::validation::AbstractValidator<
        iroha::protocol::Transaction>>
        proto_transaction_validator = std::make_unique<
            shared_model::validation::ProtoTransactionValidator>();
    std::shared_ptr<shared_model::interface::AbstractTransportFactory<
        shared_model::interface::Transaction,
        iroha::protocol::Transaction>>
        transaction_factory =
            std::make_shared<shared_model::proto::ProtoTransportFactory<
                shared_model::interface::Transaction,
                shared_model::proto::Transaction>>(
                std::move(transaction_validator),
                std::move(proto_transaction_validator));
    std::shared_ptr<shared_model::interface::TransactionBatchParser>
        batch_parser = std::make_shared<
            shared_model::interface::TransactionBatchParserImpl>();
    std::shared_ptr<shared_model::validation::AbstractValidator<
        shared_model::interface::TransactionBatch>>
        batch_validator =
            std::make_shared<shared_model::validation::DefaultBatchValidator>(
                iroha::test::kTestsValidatorsConfig);
    std::shared_ptr<shared_model::interface::TransactionBatchFactory>
        transaction_batch_factory = std::make_shared<
            shared_model::interface::TransactionBatchFactoryImpl>(
            batch_validator);

    service_transport_ =
        std::make_shared<iroha::torii::CommandServiceTransportGrpc>(
            service_,
            status_bus,
            status_factory,
            transaction_factory,
            batch_parser,
            transaction_batch_factory,
            rxcpp::observable<>::iterate(consensus_gate_objects_),
            2,
            logger::getDummyLoggerPtr());
  }
};

DEFINE_BINARY_PROTO_FUZZER(const iroha::protocol::Transaction &tx) {
  static CommandFixture handler;
  handler.service_transport_->Torii(nullptr, &tx, nullptr);
}

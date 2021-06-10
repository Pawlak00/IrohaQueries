/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "backend/protobuf/queries/proto_tx_pagination_meta.hpp"

#include <optional>
#include "cryptography/hash.hpp"

namespace types = shared_model::interface::types;

using namespace shared_model::proto;

TxPaginationMeta::TxPaginationMeta(iroha::protocol::TxPaginationMeta &meta)
    : meta_{meta}, ordering_(meta.ordering()) {
  /// default values
  ordering_.append(interface::Ordering::Field::kPosition,
                   interface::Ordering::Direction::kAscending);
}

types::TransactionsNumberType TxPaginationMeta::pageSize() const {
  return meta_.page_size();
}

std::optional<types::HashType> TxPaginationMeta::firstTxHash() const {
  if (meta_.opt_first_tx_hash_case()
      == iroha::protocol::TxPaginationMeta::OptFirstTxHashCase::
             OPT_FIRST_TX_HASH_NOT_SET) {
    return std::nullopt;
  }
  return types::HashType::fromHexString(meta_.first_tx_hash());
}
//////////////// added by me
std::optional<std::string> TxPaginationMeta::firstTxTime() const {
  // if (meta_.opt_first_tx_time()
  //     == iroha::protocol::TxPaginationMeta::OptFirstTxHashCase::
  //            OPT_FIRST_TX_HASH_NOT_SET) {
  //   return std::nullopt;
  // }
  //find function to convert to string
  return meta_.first_tx_time();
}
std::optional<std::string> TxPaginationMeta::lastTxTime() const {
  //verification should be added
  // if (meta_.opt_first_tx_time()
  //     == iroha::protocol::TxPaginationMeta::OptFirstTxHashCase::
  //            OPT_FIRST_TX_HASH_NOT_SET) {
  //   return std::nullopt;
  // }
  //that is a bad solution, converting function for this Type should be written
  return meta_.last_tx_time();
}
////
shared_model::interface::Ordering const &TxPaginationMeta::ordering() const {
  return ordering_;
}
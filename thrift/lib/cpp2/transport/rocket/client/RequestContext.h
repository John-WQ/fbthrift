/*
 * Copyright 2018-present Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <utility>

#include <boost/intrusive/unordered_set.hpp>

#include <folly/IntrusiveList.h>
#include <folly/Likely.h>
#include <folly/Portability.h>
#include <folly/fibers/Baton.h>

#include <thrift/lib/cpp2/transport/rocket/Types.h>
#include <thrift/lib/cpp2/transport/rocket/framing/FrameType.h>
#include <thrift/lib/cpp2/transport/rocket/framing/Frames.h>
#include <thrift/lib/cpp2/transport/rocket/framing/Serializer.h>
#include <thrift/lib/thrift/gen-cpp2/RpcMetadata_types.h>

namespace folly {
namespace io {
class IOBuf;
} // namespace io
} // namespace folly

namespace apache {
namespace thrift {
namespace rocket {

class RocketClientWriteCallback;

class RequestContextQueue;

class RequestContext {
 public:
  enum class State : uint8_t {
    WRITE_NOT_SCHEDULED,
    WRITE_SCHEDULED,
    WRITE_SENDING, /* AsyncSocket::writev() called, but not yet confirmed as
                      written to the underlying socket */
    WRITE_SENT, /* Write to socket completed (possibly with error) */
    RESPONSE_RECEIVED,
    REQUEST_ABORTED, /* Request timed out, errored out, or was aborted */
  };

  template <class Frame>
  RequestContext(
      Frame&& frame,
      RequestContextQueue& queue,
      SetupFrame* setupFrame = nullptr,
      RocketClientWriteCallback* writeCallback = nullptr)
      : queue_(queue),
        streamId_(frame.streamId()),
        frameType_(Frame::frameType()),
        writeCallback_(writeCallback) {
    serialize(std::forward<Frame>(frame), setupFrame);
  }

  RequestContext(const RequestContext&) = delete;
  RequestContext(RequestContext&&) = delete;
  RequestContext& operator=(const RequestContext&) = delete;
  RequestContext& operator=(RequestContext&&) = delete;

  // For REQUEST_RESPONSE contexts, where an immediate matching response is
  // expected
  FOLLY_NODISCARD folly::Try<Payload> waitForResponse(
      std::chrono::milliseconds timeout);

  // For request types for which an immediate matching response is not
  // necessarily expected, e.g., REQUEST_FNF and REQUEST_STREAM
  FOLLY_NODISCARD folly::Try<void> waitForWriteToComplete();

  void scheduleTimeoutForResponse() {
    DCHECK(isRequestResponse());
    // In some edge cases, response may arrive before write to socket finishes.
    if (state_ != State::RESPONSE_RECEIVED) {
      awaitResponseTimeoutHandler_.scheduleTimeout(awaitResponseTimeout_);
    }
  }

  std::unique_ptr<folly::IOBuf> serializedChain() {
    DCHECK(serializedFrame_);
    return std::move(serializedFrame_);
  }

  State state() const {
    return state_;
  }

  StreamId streamId() const {
    return streamId_;
  }

  bool isRequestResponse() const {
    return frameType_ == FrameType::REQUEST_RESPONSE;
  }

  void onPayloadFrame(PayloadFrame&& payloadFrame);
  void onErrorFrame(ErrorFrame&& errorFrame);

  void onWriteSuccess() noexcept;

 private:
  RequestContextQueue& queue_;
  folly::SafeIntrusiveListHook queueHook_;
  std::unique_ptr<folly::IOBuf> serializedFrame_;
  const StreamId streamId_;
  const FrameType frameType_;
  State state_{State::WRITE_NOT_SCHEDULED};

  boost::intrusive::unordered_set_member_hook<> setHook_;
  folly::fibers::Baton baton_;
  std::chrono::milliseconds awaitResponseTimeout_{1000};
  folly::fibers::Baton::TimeoutHandler awaitResponseTimeoutHandler_;
  folly::Try<Payload> responsePayload_;
  RocketClientWriteCallback* const writeCallback_;

  template <class Frame>
  void serialize(Frame&& frame, SetupFrame* setupFrame) {
    Serializer writer;
    if (setupFrame != nullptr) {
      std::move(*setupFrame).serialize(writer);
    }
    std::forward<Frame>(frame).serialize(writer);

    DCHECK(!serializedFrame_);
    serializedFrame_ = std::move(writer).move();
  }

  struct Equal {
    bool operator()(const RequestContext& ctxa, const RequestContext& ctxb)
        const noexcept {
      return ctxa.streamId_ == ctxb.streamId_;
    }
  };

  struct Hash {
    size_t operator()(const RequestContext& ctx) const noexcept {
      return std::hash<StreamId::underlying_type>()(
          static_cast<uint32_t>(ctx.streamId_));
    }
  };

 public:
  using Queue =
      folly::CountedIntrusiveList<RequestContext, &RequestContext::queueHook_>;

  using UnorderedSet = boost::intrusive::unordered_set<
      RequestContext,
      boost::intrusive::member_hook<
          RequestContext,
          decltype(setHook_),
          &RequestContext::setHook_>,
      boost::intrusive::equal<Equal>,
      boost::intrusive::hash<Hash>>;

 private:
  friend class RequestContextQueue;
};

} // namespace rocket
} // namespace thrift
} // namespace apache

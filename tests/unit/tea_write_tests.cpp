// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/runtime/types.h"

#include "helpers.h"
#include "native/tea.h"
#include "support/native_equivalence.h"
#include "utils/cursor.h"
#include "utils/log.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {

struct LogCapture {
  std::vector<std::string> messages;

  LogCapture() {
    pistoris::log_fn = [](ArxLogLevel level, const char* message, void* userdata) {
      if (level == ARX_LOG_INFO && message) static_cast<LogCapture*>(userdata)->messages.emplace_back(message);
    };
    pistoris::log_ud = this;
  }

  ~LogCapture() {
    pistoris::log_fn = nullptr;
    pistoris::log_ud = nullptr;
  }
};

}  // namespace

static pistoris::tea::Data parse(const std::vector<uint8_t>& buf) {
  pistoris::ReadCursor rc(buf.data(), buf.size());
  pistoris::tea::Data d;
  REQUIRE(pistoris::loadTea(&d, rc) == ARX_OK);
  return d;
}

static std::vector<uint8_t> save(const pistoris::tea::Data& d) {
  pistoris::WriteCursor wc;
  REQUIRE(pistoris::saveTea(&d, wc) == ARX_OK);
  return wc.take();
}

TEST_SUITE("tea") {
  TEST_CASE("Tea validation requires bounded native strings") {
    pistoris::tea::Data data = parse(makeKeyframeTea());
    std::fill(std::begin(data.name), std::end(data.name), 'a');
    CHECK(pistoris::validateTea(&data) == ARX_TEA_BAD_NAME);

    data = parse(makeKeyframeTea());
    data.keyframes.front().sample.emplace();
    std::fill(std::begin(data.keyframes.front().sample->name), std::end(data.keyframes.front().sample->name), 'a');
    CHECK(pistoris::validateTea(&data) == ARX_TEA_BAD_SAMPLE_PATH);
  }

  TEST_CASE("TeaValidationRequiresFiniteTransforms") {
    pistoris::tea::Data data = parse(makeKeyframeTea());
    data.keyframes.front().translate->x = std::numeric_limits<float>::infinity();
    CHECK(pistoris::validateTea(&data) == ARX_TEA_BAD_ROOT_TRANSFORM);

    data = parse(makeKeyframeTea());
    data.keyframes.front().groups.front().quat.w = std::numeric_limits<float>::quiet_NaN();
    CHECK(pistoris::validateTea(&data) == ARX_TEA_BAD_GROUP_TRANSFORM);
  }

  TEST_CASE("TeaWriteExactMinimal") {
    auto fixture = makeMinimalTea();
    setNumKeyframes(fixture, 1);
    appendKeyframe2014(fixture);
    auto bytes = save(parse(fixture));
    REQUIRE(bytes.size() == fixture.size());
    CHECK(std::memcmp(bytes.data(), fixture.data(), fixture.size()) == 0);
  }

  TEST_CASE("TeaWriteExactKeyframe") {
    auto fixture = makeKeyframeTea();
    auto bytes = save(parse(fixture));
    REQUIRE(bytes.size() == fixture.size());
    CHECK(std::memcmp(bytes.data(), fixture.data(), fixture.size()) == 0);
  }

  TEST_CASE("TeaWriteRoundtrip") {
    auto d1 = parse(makeKeyframeTea());
    auto d2 = parse(save(d1));
    test_support::checkEquivalent(d1, d2);
  }

  TEST_CASE("Tea logs keyframes, active groups, and timeline duration") {
    pistoris::tea::Data data;
    data.num_frames = 27;
    data.num_groups = 2;
    data.keyframes.resize(2);
    data.keyframes[0].num_frame = 0;
    data.keyframes[1].num_frame = 27;
    for (pistoris::tea::Keyframe& keyframe : data.keyframes) keyframe.groups.resize(2);
    data.keyframes[1].groups[1].translate.x = 1.0f;

    LogCapture logs;
    const std::vector<uint8_t> bytes = save(data);
    parse(bytes);

    REQUIRE(logs.messages.size() == 2);
    CHECK(logs.messages[0] == "TEA saving: 2 keyframes, 2 groups (1 active), timeline 27 frames at 24 fps (1.125 s)");
    CHECK(logs.messages[1] == "TEA loaded: 2 keyframes, 2 groups (1 active), timeline 27 frames at 24 fps (1.125 s)");
  }

  TEST_CASE("TeaWriteSampleRoundtrip") {
    auto fixture = makeMinimalTea();
    setNumKeyframes(fixture, 1);

    // 32-byte v2014 keyframe header
    std::size_t kf_off = fixture.size();
    fixture.insert(fixture.end(), kTeaKf2014Size, 0);
    int32_t flag = pistoris::kTeaFlagFrameNone;  // 0 is not a valid flag value
    std::memcpy(fixture.data() + kf_off + kTeaKfFlagFrameOff, &flag, 4);

    int32_t num_sample = 0;  // not -1: sample follows
    auto* nsp = reinterpret_cast<const uint8_t*>(&num_sample);
    fixture.insert(fixture.end(), nsp, nsp + 4);

    char sample_name[256] = {};
    std::strcpy(sample_name, "footstep.wav");
    fixture.insert(fixture.end(),
                   reinterpret_cast<const uint8_t*>(sample_name),
                   reinterpret_cast<const uint8_t*>(sample_name) + 256);

    fixture.insert(fixture.end(), 4, 0);  // sample_size = 0, audio always dropped on read
    fixture.insert(fixture.end(), 4, 0);  // num_sfx = 0

    auto d1 = parse(fixture);
    REQUIRE(d1.keyframes.size() == 1);
    REQUIRE(d1.keyframes[0].sample.has_value());
    CHECK(std::string(d1.keyframes[0].sample->name) == "footstep");

    auto bytes = save(d1);
    auto d2 = parse(bytes);
    test_support::checkEquivalent(d1, d2);
  }

  // v2015 -> v2014 on save: output drops info_frame[256] per keyframe
  TEST_CASE("TeaWriteV2015DowngradesTo2014") {
    auto buf = makeMinimalTea(pistoris::kTeaVersionAlt);
    int32_t nf = 24, ng = 0, nkf = 1;
    std::memcpy(buf.data() + kTeaNumFramesOff, &nf, 4);
    std::memcpy(buf.data() + kTeaNumGroupsOff, &ng, 4);
    std::memcpy(buf.data() + kTeaNumKfOff, &nkf, 4);

    // v2015 keyframe header (288 bytes); key_move at offset 272
    constexpr std::size_t kV2015KeyMoveOff = kTeaKfKeyMoveOff + 256;
    std::size_t kf = buf.size();
    buf.insert(buf.end(), kTeaKf2015Size, 0);
    int32_t flag = pistoris::kTeaFlagFrameStep, one = 1;
    std::memcpy(buf.data() + kf + kTeaKfFlagFrameOff, &flag, 4);
    std::memcpy(buf.data() + kf + kV2015KeyMoveOff, &one, 4);
    float tr[3] = {1.f, 2.f, 3.f};
    buf.insert(buf.end(), reinterpret_cast<uint8_t*>(tr), reinterpret_cast<uint8_t*>(tr) + 12);
    int32_t no_sample = -1;
    auto* p = reinterpret_cast<const uint8_t*>(&no_sample);
    buf.insert(buf.end(), p, p + 4);
    buf.insert(buf.end(), 4, 0);  // num_sfx

    auto d1 = parse(buf);
    auto bytes = save(d1);

    CHECK(bytes.size() == buf.size() - 256);  // 1 keyframe loses info_frame[256]

    auto d2 = parse(bytes);
    test_support::checkEquivalent(d1, d2);
  }

  // 3-keyframe v2015 input mixing key_move=0/1: each loses info_frame[256] on downgrade
  TEST_CASE("TeaWriteMultiKeyframeV2015Downgrade") {
    auto buf = makeMinimalTea(pistoris::kTeaVersionAlt);

    int32_t nf = 24, ng = 0, nkf = 3;
    std::memcpy(buf.data() + kTeaNumFramesOff, &nf, 4);
    std::memcpy(buf.data() + kTeaNumGroupsOff, &ng, 4);
    std::memcpy(buf.data() + kTeaNumKfOff, &nkf, 4);

    constexpr std::size_t kV2015KeyMoveOff = kTeaKfKeyMoveOff + 256;

    auto append_v2015_kf = [&](int32_t num_frame, int32_t flag, int32_t key_move, float tx, float ty, float tz) {
      std::size_t kf = buf.size();
      buf.insert(buf.end(), kTeaKf2015Size, 0);
      std::memcpy(buf.data() + kf + 0, &num_frame, 4);
      std::memcpy(buf.data() + kf + kTeaKfFlagFrameOff, &flag, 4);
      std::memcpy(buf.data() + kf + kV2015KeyMoveOff, &key_move, 4);
      if (key_move != 0) {
        float tr[3] = {tx, ty, tz};
        buf.insert(buf.end(), reinterpret_cast<uint8_t*>(tr), reinterpret_cast<uint8_t*>(tr) + 12);
      }
      int32_t no_sample = -1;
      auto* p = reinterpret_cast<const uint8_t*>(&no_sample);
      buf.insert(buf.end(), p, p + 4);
      buf.insert(buf.end(), 4, 0);  // num_sfx
    };

    // num_frame must be strictly increasing across keyframes
    append_v2015_kf(0, pistoris::kTeaFlagFrameStep, 1, 1.f, 2.f, 3.f);
    append_v2015_kf(8, pistoris::kTeaFlagFrameNone, 0, 0.f, 0.f, 0.f);
    append_v2015_kf(16, pistoris::kTeaFlagFrameNone, 1, 4.f, 5.f, 6.f);

    auto d1 = parse(buf);
    auto bytes = save(d1);

    CHECK(bytes.size() == buf.size() - 3 * 256);

    auto d2 = parse(bytes);
    test_support::checkEquivalent(d1, d2);
  }

}  // TEST_SUITE("tea")

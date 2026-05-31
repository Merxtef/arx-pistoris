// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/pistoris_types.h"
#include "arx_pistoris/tea_data.hpp"

#include "arx/tea.h"
#include "helpers.h"
#include "utils/cursor.h"

#include <cstdint>
#include <cstring>
#include <vector>

static ArxReturnCode load(const std::vector<uint8_t>& buf, pistoris::tea::Data& d) {
  pistoris::ReadCursor c(buf.data(), buf.size());
  return pistoris::loadTea(&d, c);
}

TEST_SUITE("tea") {
  // --- Bad identifier / version ---

  TEST_CASE("TeaTruncatedIdentity") {
    std::vector<uint8_t> buf(10, 0);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_UNEXPECTED_EOF);
  }

  TEST_CASE("TeaBadIdentifier") {
    auto buf = makeMinimalTea();
    buf[0]   = 'X';
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_INVALID_IDENTIFIER);
  }

  // buffer ends exactly after 20-byte identity; version read fails
  TEST_CASE("TeaTruncatedAfterIdentity") {
    std::vector<uint8_t> buf(20, 0);
    std::memcpy(buf.data(), pistoris::kTeaMagic, sizeof(pistoris::kTeaMagic));
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_UNEXPECTED_EOF);
  }

  TEST_CASE("TeaBadVersion") {
    auto buf     = makeMinimalTea();
    uint32_t bad = pistoris::kTeaVersion - 1;
    std::memcpy(buf.data() + kTeaVersionOff, &bad, 4);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_TEA_BAD_VERSION);
  }

  // --- Truncated header ---

  // valid identity + version, then cut off before anim_name + num_frames
  TEST_CASE("TeaTruncatedBeforeNumFrames") {
    std::vector<uint8_t> buf(24, 0);
    std::memcpy(buf.data(), pistoris::kTeaMagic, sizeof(pistoris::kTeaMagic));
    std::memcpy(buf.data() + kTeaVersionOff, &pistoris::kTeaVersion, 4);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_UNEXPECTED_EOF);
  }

  // --- Bad header field values ---

  TEST_CASE("TeaBadFramesN") {
    auto buf    = makeMinimalTea();
    int32_t bad = -1;
    std::memcpy(buf.data() + kTeaNumFramesOff, &bad, 4);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_TEA_BAD_FRAMES_N);
  }

  TEST_CASE("TeaBadGroupsNNegative") {
    auto buf    = makeMinimalTea();
    int32_t bad = -1;
    std::memcpy(buf.data() + kTeaNumGroupsOff, &bad, 4);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_TEA_BAD_GROUPS_N);
  }

  TEST_CASE("TeaBadGroupsNTooLarge") {
    auto buf    = makeMinimalTea();
    int32_t bad = static_cast<int32_t>(pistoris::kTeaMaxGroups + 1);
    std::memcpy(buf.data() + kTeaNumGroupsOff, &bad, 4);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_TEA_BAD_GROUPS_N);
  }

  TEST_CASE("TeaBadKeyframesNNegative") {
    auto buf    = makeMinimalTea();
    int32_t bad = -1;
    std::memcpy(buf.data() + kTeaNumKfOff, &bad, 4);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_TEA_BAD_KEYFRAMES_N);
  }

  // --- Truncated keyframe body (v2014) ---

  // num_key_frames=1 but no keyframe bytes follow
  TEST_CASE("TeaTruncatedKeyframeHeader") {
    auto buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_UNEXPECTED_EOF);
  }

  // key_move=1 but no translate bytes follow
  TEST_CASE("TeaTruncatedInTranslate") {
    auto buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    std::size_t base = buf.size();
    buf.insert(buf.end(), kTeaKf2014Size, 0);
    int32_t one = 1;
    std::memcpy(buf.data() + base + kTeaKfKeyMoveOff, &one, 4);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_UNEXPECTED_EOF);
  }

  // key_orient=1 but no THEO_ANGLE or ArxQuat bytes follow
  TEST_CASE("TeaTruncatedInQuat") {
    auto buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    std::size_t base = buf.size();
    buf.insert(buf.end(), kTeaKf2014Size, 0);
    int32_t one = 1;
    std::memcpy(buf.data() + base + kTeaKfKeyOrientOff, &one, 4);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_UNEXPECTED_EOF);
  }

  // key_morph=1 but no 16-byte morph block follows
  TEST_CASE("TeaTruncatedInMorph") {
    auto buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    std::size_t base = buf.size();
    buf.insert(buf.end(), kTeaKf2014Size, 0);
    int32_t one = 1;
    std::memcpy(buf.data() + base + kTeaKfKeyMorphOff, &one, 4);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_UNEXPECTED_EOF);
  }

  // num_groups=1 but keyframe header has no group data
  TEST_CASE("TeaTruncatedInGroups") {
    auto buf    = makeMinimalTea();
    int32_t one = 1;
    std::memcpy(buf.data() + kTeaNumGroupsOff, &one, 4);
    setNumKeyframes(buf, 1);
    buf.insert(buf.end(), kTeaKf2014Size, 0);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_UNEXPECTED_EOF);
  }

  // full keyframe header + 0 groups, then truncated before num_sample
  TEST_CASE("TeaTruncatedInNumSample") {
    auto buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    buf.insert(buf.end(), kTeaKf2014Size, 0);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_UNEXPECTED_EOF);
  }

  // num_sample != -1 but no sample name bytes follow
  TEST_CASE("TeaTruncatedInSampleName") {
    auto buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    buf.insert(buf.end(), kTeaKf2014Size, 0);
    int32_t has_sample = 0;
    auto* p            = reinterpret_cast<const uint8_t*>(&has_sample);
    buf.insert(buf.end(), p, p + 4);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_UNEXPECTED_EOF);
  }

  // sample present but sample_size = -1
  TEST_CASE("TeaBadSampleSize") {
    auto buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    buf.insert(buf.end(), kTeaKf2014Size, 0);
    int32_t has_sample = 0;
    auto* p            = reinterpret_cast<const uint8_t*>(&has_sample);
    buf.insert(buf.end(), p, p + 4);
    buf.insert(buf.end(), 256, 'A');  // sample name
    int32_t bad_size = -1;
    auto* q          = reinterpret_cast<const uint8_t*>(&bad_size);
    buf.insert(buf.end(), q, q + 4);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_TEA_BAD_SAMPLE_SIZE);
  }

  // sample_size=10 but only 5 audio bytes follow
  TEST_CASE("TeaTruncatedInAudioBytes") {
    auto buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    buf.insert(buf.end(), kTeaKf2014Size, 0);
    int32_t has_sample = 0;
    auto* p            = reinterpret_cast<const uint8_t*>(&has_sample);
    buf.insert(buf.end(), p, p + 4);
    buf.insert(buf.end(), 256, 0);  // sample name
    int32_t audio_size = 10;
    auto* q            = reinterpret_cast<const uint8_t*>(&audio_size);
    buf.insert(buf.end(), q, q + 4);
    buf.insert(buf.end(), 5, 0);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_UNEXPECTED_EOF);
  }

  // num_sample=-1 but no num_sfx follows
  TEST_CASE("TeaTruncatedInNumSfx") {
    auto buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    buf.insert(buf.end(), kTeaKf2014Size, 0);
    int32_t no_sample = -1;
    auto* p           = reinterpret_cast<const uint8_t*>(&no_sample);
    buf.insert(buf.end(), p, p + 4);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_UNEXPECTED_EOF);
  }

  // --- Truncated v2015 keyframe ---

  // v2015: only num_frame+flag_frame present, info_frame[256] skip runs past EOF
  TEST_CASE("TeaTruncatedInInfoFrame") {
    auto buf = makeMinimalTea(pistoris::kTeaVersionAlt);
    setNumKeyframes(buf, 1);
    buf.insert(buf.end(), 8, 0);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_UNEXPECTED_EOF);
  }

  // --- ValidateTea ---

  TEST_CASE("TeaBadFlagFrame") {
    auto buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    appendKeyframe2014(buf);
    int32_t bad = 42;
    std::memcpy(buf.data() + kTeaHeaderSize + kTeaKfFlagFrameOff, &bad, 4);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_TEA_BAD_FLAG_FRAME);
  }

  // 0 keyframes has no animation body
  TEST_CASE("TeaEmptyKeyframesRejected") {
    auto buf = makeMinimalTea();
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_TEA_BAD_KEYFRAMES_N);
  }

  TEST_CASE("TeaMinimalValid") {
    auto buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    appendKeyframe2014(buf);
    pistoris::tea::Data d;
    REQUIRE(load(buf, d) == ARX_OK);
    CHECK(d.keyframes.size() == 1);
    CHECK(d.num_groups == 0);
    CHECK(d.num_frames == 0);
  }

  TEST_CASE("TeaOneKeyframeV2014") {
    auto buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    appendKeyframe2014(buf);
    pistoris::tea::Data d;
    REQUIRE(load(buf, d) == ARX_OK);
    CHECK(d.keyframes.size() == 1);
    CHECK(!d.keyframes[0].translate);
    CHECK(!d.keyframes[0].quat);
    CHECK(!d.keyframes[0].sample);
  }

  // v2015 keyframe: num_frame(4)+flag_frame(4)+info_frame[256]+remaining 6 fields(24)
  TEST_CASE("TeaOneKeyframeV2015") {
    auto buf = makeMinimalTea(pistoris::kTeaVersionAlt);
    setNumKeyframes(buf, 1);
    buf.insert(buf.end(), kTeaKf2015Size, 0);
    int32_t flag = pistoris::kTeaFlagFrameNone;
    std::memcpy(buf.data() + kTeaHeaderSize + kTeaKfFlagFrameOff, &flag, 4);
    int32_t no_sample = -1;
    auto* p           = reinterpret_cast<const uint8_t*>(&no_sample);
    buf.insert(buf.end(), p, p + 4);
    buf.insert(buf.end(), 4, 0);  // num_sfx
    pistoris::tea::Data d;
    REQUIRE(load(buf, d) == ARX_OK);
    CHECK(d.keyframes.size() == 1);
  }

  TEST_CASE("TeaKeyframeWithTranslate") {
    auto buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    appendKeyframe2014(buf, 0, pistoris::kTeaFlagFrameNone, 1);  // key_move=1
    std::size_t tr_off = kTeaHeaderSize + kTeaKf2014Size;
    float xyz[3]       = {1.f, 2.f, 3.f};
    std::memcpy(buf.data() + tr_off, xyz, 12);
    pistoris::tea::Data d;
    REQUIRE(load(buf, d) == ARX_OK);
    REQUIRE(d.keyframes[0].translate.has_value());
    const auto& tr = *d.keyframes[0].translate;
    CHECK(tr.x == doctest::Approx(1.f));
    CHECK(tr.y == doctest::Approx(2.f));
    CHECK(tr.z == doctest::Approx(3.f));
  }

  TEST_CASE("TeaKeyframeWithQuat") {
    auto buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    appendKeyframe2014(buf, 0, pistoris::kTeaFlagFrameNone, 0, 1);  // key_orient=1
    std::size_t quat_off = kTeaHeaderSize + kTeaKf2014Size + 8;     // +8 for THEO_ANGLE
    float wxyz[4]        = {1.f, 0.f, 0.f, 0.f};
    std::memcpy(buf.data() + quat_off, wxyz, 16);
    pistoris::tea::Data d;
    REQUIRE(load(buf, d) == ARX_OK);
    REQUIRE(d.keyframes[0].quat.has_value());
    CHECK(d.keyframes[0].quat->w == doctest::Approx(1.f));
  }

  TEST_CASE("TeaKeyframeWithSample") {
    auto buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    buf.insert(buf.end(), kTeaKf2014Size, 0);
    int32_t flag = pistoris::kTeaFlagFrameNone;
    std::memcpy(buf.data() + kTeaHeaderSize + kTeaKfFlagFrameOff, &flag, 4);
    int32_t has_sample = 0;
    auto* p            = reinterpret_cast<const uint8_t*>(&has_sample);
    buf.insert(buf.end(), p, p + 4);
    buf.insert(buf.end(), 256, 0);  // sample name
    const char* name = "test.wav";
    std::memcpy(buf.data() + buf.size() - 256, name, std::strlen(name));
    int32_t audio_size = 4;
    auto* q            = reinterpret_cast<const uint8_t*>(&audio_size);
    buf.insert(buf.end(), q, q + 4);
    buf.insert(buf.end(), 4, 0);  // 4 audio bytes
    buf.insert(buf.end(), 4, 0);  // num_sfx
    pistoris::tea::Data d;
    REQUIRE(load(buf, d) == ARX_OK);
    REQUIRE(d.keyframes[0].sample.has_value());
    CHECK(std::strcmp(d.keyframes[0].sample->name, "test.wav") == 0);
  }

  // key_morph=1: 16-byte block must be skipped without error
  TEST_CASE("TeaKeyframeWithMorph") {
    auto buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    appendKeyframe2014(buf, 0, pistoris::kTeaFlagFrameNone, 0, 0, 1);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_OK);
  }

  // flag_frame=9 (footstep) is valid
  TEST_CASE("TeaFlagFrameStep") {
    auto buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    appendKeyframe2014(buf, 0, pistoris::kTeaFlagFrameStep);
    pistoris::tea::Data d;
    REQUIRE(load(buf, d) == ARX_OK);
    CHECK(d.keyframes[0].flag_frame == pistoris::kTeaFlagFrameStep);
  }

  // 1 keyframe, 2 groups
  TEST_CASE("TeaGroupsParsed") {
    auto buf    = makeMinimalTea();
    int32_t two = 2;
    std::memcpy(buf.data() + kTeaNumGroupsOff, &two, 4);
    setNumKeyframes(buf, 1);
    appendKeyframe2014(buf, 2);
    pistoris::tea::Data d;
    REQUIRE(load(buf, d) == ARX_OK);
    CHECK(d.keyframes[0].groups.size() == 2);
  }

  TEST_CASE("TeaBadKeyframesNTooLarge") {
    auto buf    = makeMinimalTea();
    int32_t bad = static_cast<int32_t>(pistoris::kTeaMaxKeyframes + 1);
    std::memcpy(buf.data() + kTeaNumKfOff, &bad, 4);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_TEA_BAD_KEYFRAMES_N);
  }

  // num_frames is stored verbatim; verify it survives load
  TEST_CASE("TeaNumFramesNonZero") {
    auto buf    = makeMinimalTea();
    int32_t val = 240;
    std::memcpy(buf.data() + kTeaNumFramesOff, &val, 4);
    setNumKeyframes(buf, 1);
    appendKeyframe2014(buf);
    pistoris::tea::Data d;
    REQUIRE(load(buf, d) == ARX_OK);
    CHECK(d.num_frames == 240);
  }

  // key_move=1, key_orient=1, key_morph=1 all in one keyframe
  TEST_CASE("TeaKeyframeAllOptionals") {
    auto buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    appendKeyframe2014(buf, 0, pistoris::kTeaFlagFrameNone, 1, 1, 1);
    pistoris::tea::Data d;
    REQUIRE(load(buf, d) == ARX_OK);
    CHECK(d.keyframes[0].translate.has_value());
    CHECK(d.keyframes[0].quat.has_value());
  }

  // group quat/translate/zoom bytes land in the right fields
  TEST_CASE("TeaGroupFieldValues") {
    auto buf    = makeMinimalTea();
    int32_t one = 1;
    std::memcpy(buf.data() + kTeaNumGroupsOff, &one, 4);
    setNumKeyframes(buf, 1);
    appendKeyframe2014(buf, 1);

    // group layout: key_group(4) + angle(8) + quat(16) + translate(12) + zoom(12)
    std::size_t gr = kTeaHeaderSize + kTeaKf2014Size;
    float gq[4]    = {0.f, 0.707f, 0.f, 0.707f};
    float gt[3]    = {10.f, 20.f, 30.f};
    float gz[3]    = {0.f, 0.f, 3.f};
    std::memcpy(buf.data() + gr + 12, gq, 16);
    std::memcpy(buf.data() + gr + 28, gt, 12);
    std::memcpy(buf.data() + gr + 40, gz, 12);

    pistoris::tea::Data d;
    REQUIRE(load(buf, d) == ARX_OK);
    REQUIRE(d.keyframes[0].groups.size() == 1);
    CHECK(d.keyframes[0].groups[0].quat.x == doctest::Approx(0.707f));
    CHECK(d.keyframes[0].groups[0].translate.y == doctest::Approx(20.f));
    CHECK(d.keyframes[0].groups[0].zoom.z == doctest::Approx(3.f));
  }

  // proves cursor alignment: info_frame[256] skip lands at the right offset for v2015
  TEST_CASE("TeaV2015FieldsAfterInfoFrameSkip") {
    auto buf = makeMinimalTea(pistoris::kTeaVersionAlt);
    setNumKeyframes(buf, 1);
    int32_t nf = 10;
    std::memcpy(buf.data() + kTeaNumFramesOff, &nf, 4);

    constexpr std::size_t kV2015KeyMoveOff = kTeaKfKeyMoveOff + 256;  // 272
    std::size_t kf                         = buf.size();
    buf.insert(buf.end(), kTeaKf2015Size, 0);
    int32_t num_frame = 7, flag = pistoris::kTeaFlagFrameStep, one = 1;
    std::memcpy(buf.data() + kf + 0, &num_frame, 4);
    std::memcpy(buf.data() + kf + kTeaKfFlagFrameOff, &flag, 4);
    std::memcpy(buf.data() + kf + kV2015KeyMoveOff, &one, 4);

    float tr[3] = {5.f, 6.f, 7.f};
    buf.insert(buf.end(), reinterpret_cast<uint8_t*>(tr), reinterpret_cast<uint8_t*>(tr) + 12);

    int32_t no_sample = -1;
    auto* p           = reinterpret_cast<const uint8_t*>(&no_sample);
    buf.insert(buf.end(), p, p + 4);
    buf.insert(buf.end(), 4, 0);

    pistoris::tea::Data d;
    REQUIRE(load(buf, d) == ARX_OK);
    CHECK(d.keyframes[0].num_frame == 7);
    CHECK(d.keyframes[0].flag_frame == pistoris::kTeaFlagFrameStep);
    REQUIRE(d.keyframes[0].translate.has_value());
    const auto& trv = *d.keyframes[0].translate;
    CHECK(trv.x == doctest::Approx(5.f));
    CHECK(trv.y == doctest::Approx(6.f));
    CHECK(trv.z == doctest::Approx(7.f));
  }

  // two keyframes both at num_frame=5 -> second is not strictly after first
  TEST_CASE("TeaNonMonotonicFrames") {
    auto buf    = makeMinimalTea();
    int32_t two = 2;
    std::memcpy(buf.data() + kTeaNumKfOff, &two, 4);
    appendKeyframe2014(buf, 0, pistoris::kTeaFlagFrameNone, 0, 0, 0, 5);
    appendKeyframe2014(buf, 0, pistoris::kTeaFlagFrameNone, 0, 0, 0, 5);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_TEA_NON_MONOTONIC_FRAMES);
  }

  TEST_CASE("TeaDecreasingFrames") {
    auto buf    = makeMinimalTea();
    int32_t two = 2;
    std::memcpy(buf.data() + kTeaNumKfOff, &two, 4);
    appendKeyframe2014(buf, 0, pistoris::kTeaFlagFrameNone, 0, 0, 0, 10);
    appendKeyframe2014(buf, 0, pistoris::kTeaFlagFrameNone, 0, 0, 0, 5);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_TEA_NON_MONOTONIC_FRAMES);
  }

  // strictly increasing 0 -> 5 -> 10, all within num_frames=10
  TEST_CASE("TeaMonotonicFrames") {
    auto buf      = makeMinimalTea();
    int32_t three = 3, nf = 10;
    std::memcpy(buf.data() + kTeaNumFramesOff, &nf, 4);
    std::memcpy(buf.data() + kTeaNumKfOff, &three, 4);
    appendKeyframe2014(buf, 0, pistoris::kTeaFlagFrameNone, 0, 0, 0, 0);
    appendKeyframe2014(buf, 0, pistoris::kTeaFlagFrameNone, 0, 0, 0, 5);
    appendKeyframe2014(buf, 0, pistoris::kTeaFlagFrameNone, 0, 0, 0, 10);
    pistoris::tea::Data d;
    REQUIRE(load(buf, d) == ARX_OK);
    CHECK(d.keyframes.size() == 3);
  }

  // last keyframe num_frame (20) exceeds num_frames (10) -> rejected
  TEST_CASE("TeaLastKeyframePastNumFrames") {
    auto buf    = makeMinimalTea();
    int32_t one = 1, nf = 10, kf_frame = 20;
    std::memcpy(buf.data() + kTeaNumFramesOff, &nf, 4);
    std::memcpy(buf.data() + kTeaNumKfOff, &one, 4);
    appendKeyframe2014(buf, 0, pistoris::kTeaFlagFrameNone, 0, 0, 0, kf_frame);
    pistoris::tea::Data d;
    CHECK(load(buf, d) == ARX_TEA_BAD_FRAMES_N);
  }

  // sample name fills all 256 bytes without null; must be clamped on load
  TEST_CASE("TeaSampleNameNoNullTerm") {
    auto buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    buf.insert(buf.end(), kTeaKf2014Size, 0);
    int32_t flag = pistoris::kTeaFlagFrameNone;
    std::memcpy(buf.data() + kTeaHeaderSize + kTeaKfFlagFrameOff, &flag, 4);
    int32_t has_sample = 0;
    auto* p            = reinterpret_cast<const uint8_t*>(&has_sample);
    buf.insert(buf.end(), p, p + 4);
    buf.insert(buf.end(), 256, 'A');  // sample name, no null
    buf.insert(buf.end(), 4, 0);      // sample_size=0
    buf.insert(buf.end(), 4, 0);      // num_sfx
    pistoris::tea::Data d;
    REQUIRE(load(buf, d) == ARX_OK);
    REQUIRE(d.keyframes[0].sample.has_value());
    CHECK(d.keyframes[0].sample->name[255] == '\0');
  }

}  // TEST_SUITE("tea")

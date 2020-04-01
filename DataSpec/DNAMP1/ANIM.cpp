#include "ANIM.hpp"
#include "zeus/CVector3f.hpp"
#include "hecl/Blender/Connection.hpp"

namespace DataSpec::DNAMP1 {

using ANIMOutStream = hecl::blender::ANIMOutStream;

void ANIM::IANIM::sendANIMToBlender(hecl::blender::PyOutStream& os, const DNAANIM::RigInverter<CINF>& rig) const {
  os.format(fmt(
      "act.hecl_fps = round({})\n"
      "act.hecl_looping = {}\n"),
      (1.0f / mainInterval), looping ? "True" : "False");

  auto kit = chanKeys.begin();

  std::vector<zeus::CQuaternion> fixedRotKeys;
  std::vector<zeus::CVector3f> fixedTransKeys;

  for (const std::pair<atUint32, bool>& bone : bones) {
    const std::string* bName = rig.getCINF().getBoneNameFromId(bone.first);
    if (!bName) {
      ++kit;
      if (bone.second)
        ++kit;
      continue;
    }

    os.format(fmt("bone_string = '{}'\n"), *bName);
    os << "action_group = act.groups.new(bone_string)\n"
          "\n"
          "rotCurves = []\n"
          "rotCurves.append(act.fcurves.new('pose.bones[\"'+bone_string+'\"].rotation_quaternion', index=0, "
          "action_group=bone_string))\n"
          "rotCurves.append(act.fcurves.new('pose.bones[\"'+bone_string+'\"].rotation_quaternion', index=1, "
          "action_group=bone_string))\n"
          "rotCurves.append(act.fcurves.new('pose.bones[\"'+bone_string+'\"].rotation_quaternion', index=2, "
          "action_group=bone_string))\n"
          "rotCurves.append(act.fcurves.new('pose.bones[\"'+bone_string+'\"].rotation_quaternion', index=3, "
          "action_group=bone_string))\n"
          "\n";

    if (bone.second)
      os << "transCurves = []\n"
            "transCurves.append(act.fcurves.new('pose.bones[\"'+bone_string+'\"].location', index=0, "
            "action_group=bone_string))\n"
            "transCurves.append(act.fcurves.new('pose.bones[\"'+bone_string+'\"].location', index=1, "
            "action_group=bone_string))\n"
            "transCurves.append(act.fcurves.new('pose.bones[\"'+bone_string+'\"].location', index=2, "
            "action_group=bone_string))\n"
            "\n";

    ANIMOutStream ao = os.beginANIMCurve();

    {
      const std::vector<DNAANIM::Value>& rotKeys = *kit++;
      fixedRotKeys.clear();
      fixedRotKeys.resize(rotKeys.size());

      for (int c = 0; c < 4; ++c) {
        size_t idx = 0;
        for (const DNAANIM::Value& val : rotKeys)
          fixedRotKeys[idx++][c] = val.simd[c];
      }

      for (zeus::CQuaternion& rot : fixedRotKeys)
        rot = rig.invertRotation(bone.first, rot);

      for (int c = 0; c < 4; ++c) {
        auto frameit = frames.begin();
        ao.changeCurve(ANIMOutStream::CurveType::Rotate, c, rotKeys.size());
        for (const zeus::CQuaternion& val : fixedRotKeys)
          ao.write(*frameit++, val[c]);
      }
    }

    if (bone.second) {
      const std::vector<DNAANIM::Value>& transKeys = *kit++;
      fixedTransKeys.clear();
      fixedTransKeys.resize(transKeys.size());

      for (int c = 0; c < 3; ++c) {
        size_t idx = 0;
        for (const DNAANIM::Value& val : transKeys)
          fixedTransKeys[idx++][c] = val.simd[c];
      }

      for (zeus::CVector3f& t : fixedTransKeys)
        t = rig.invertPosition(bone.first, t, true);

      for (int c = 0; c < 3; ++c) {
        auto frameit = frames.begin();
        ao.changeCurve(ANIMOutStream::CurveType::Translate, c, fixedTransKeys.size());
        for (const zeus::CVector3f& val : fixedTransKeys)
          ao.write(*frameit++, val[c]);
      }
    }
  }
}

UniqueID32 ANIM::GetEVNTId(athena::io::IStreamReader& reader) {
  atUint32 version = reader.readUint32Big();
  switch (version) {
  case 0: {
    ANIM0 anim0;
    anim0.read(reader);
    return anim0.evnt;
  }
  case 2:
  case 3:
    reader.seek(4);
    return reader.readUint32Big();
  default:
    Log.report(logvisor::Error, fmt("unrecognized ANIM version"));
    break;
  }
  return {};
}

template <>
void ANIM::Enumerate<BigDNA::Read>(typename Read::StreamT& reader) {
  atUint32 version = reader.readUint32Big();
  switch (version) {
  case 0:
    m_anim = std::make_unique<ANIM0>();
    m_anim->read(reader);
    break;
  case 2:
    m_anim = std::make_unique<ANIM2>(false);
    m_anim->read(reader);
    break;
  case 3:
    m_anim = std::make_unique<ANIM2>(true);
    m_anim->read(reader);
    break;
  default:
    Log.report(logvisor::Error, fmt("unrecognized ANIM version"));
    break;
  }
}

template <>
void ANIM::Enumerate<BigDNA::Write>(typename Write::StreamT& writer) {
  writer.writeUint32Big(m_anim->m_version);
  m_anim->write(writer);
}

template <>
void ANIM::Enumerate<BigDNA::BinarySize>(typename BinarySize::StreamT& s) {
  s += 4;
  m_anim->binarySize(s);
}

std::string_view ANIM::ANIM0::DNAType() { return "ANIM0"sv; }

template <>
void ANIM::ANIM0::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader) {
  Header head;
  head.read(reader);
  mainInterval = head.interval;

  frames.clear();
  frames.reserve(head.keyCount);
  for (size_t k = 0; k < head.keyCount; ++k)
    frames.push_back(k);

  std::map<atUint8, atUint32> boneMap;
  for (size_t b = 0; b < head.boneSlotCount; ++b) {
    atUint8 idx = reader.readUByte();
    if (idx == 0xff)
      continue;
    boneMap[idx] = b;
  }

  atUint32 boneCount = reader.readUint32Big();
  bones.clear();
  bones.reserve(boneCount);
  channels.clear();
  for (size_t b = 0; b < boneCount; ++b) {
    bones.emplace_back(boneMap[b], false);
    atUint8 idx = reader.readUByte();
    channels.emplace_back();
    DNAANIM::Channel& chan = channels.back();
    chan.type = DNAANIM::Channel::Type::Rotation;
    if (idx != 0xff) {
      bones.back().second = true;
      channels.emplace_back();
      DNAANIM::Channel& chan = channels.back();
      chan.type = DNAANIM::Channel::Type::Translation;
    }
  }

  reader.readUint32Big();
  chanKeys.clear();
  chanKeys.reserve(channels.size());
  for (const std::pair<atUint32, bool>& bone : bones) {
    chanKeys.emplace_back();
    std::vector<DNAANIM::Value>& keys = chanKeys.back();
    for (size_t k = 0; k < head.keyCount; ++k)
      keys.emplace_back(reader.readVec4fBig());

    if (bone.second)
      chanKeys.emplace_back();
  }

  reader.readUint32Big();
  auto kit = chanKeys.begin();
  for (const std::pair<atUint32, bool>& bone : bones) {
    ++kit;
    if (bone.second) {
      std::vector<DNAANIM::Value>& keys = *kit++;
      for (size_t k = 0; k < head.keyCount; ++k)
        keys.emplace_back(reader.readVec3fBig());
    }
  }

  evnt.read(reader);
}

template <>
void ANIM::ANIM0::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer) {
  Header head;
  head.unk0 = 0;
  head.unk1 = 0;
  head.unk2 = 0;
  head.keyCount = frames.size();
  head.duration = head.keyCount * mainInterval;
  head.interval = mainInterval;

  atUint32 maxId = 0;
  for (const std::pair<atUint32, bool>& bone : bones)
    maxId = std::max(maxId, bone.first);
  head.boneSlotCount = maxId + 1;
  head.write(writer);

  for (size_t s = 0; s < head.boneSlotCount; ++s) {
    size_t boneIdx = 0;
    bool found = false;
    for (const std::pair<atUint32, bool>& bone : bones) {
      if (s == bone.first) {
        writer.writeUByte(boneIdx);
        found = true;
        break;
      }
      ++boneIdx;
    }
    if (!found)
      writer.writeUByte(0xff);
  }

  writer.writeUint32Big(bones.size());
  size_t boneIdx = 0;
  for (const std::pair<atUint32, bool>& bone : bones) {
    if (bone.second)
      writer.writeUByte(boneIdx);
    else
      writer.writeUByte(0xff);
    ++boneIdx;
  }

  writer.writeUint32Big(bones.size() * head.keyCount);
  auto cit = chanKeys.begin();
  atUint32 transKeyCount = 0;
  for (const std::pair<atUint32, bool>& bone : bones) {
    const std::vector<DNAANIM::Value>& keys = *cit++;
    auto kit = keys.begin();
    for (size_t k = 0; k < head.keyCount; ++k)
      writer.writeVec4fBig(atVec4f{(*kit++).simd});
    if (bone.second) {
      transKeyCount += head.keyCount;
      ++cit;
    }
  }

  writer.writeUint32Big(transKeyCount);
  cit = chanKeys.begin();
  for (const std::pair<atUint32, bool>& bone : bones) {
    ++cit;
    if (bone.second) {
      const std::vector<DNAANIM::Value>& keys = *cit++;
      auto kit = keys.begin();
      for (size_t k = 0; k < head.keyCount; ++k)
        writer.writeVec3fBig(atVec3f{(*kit++).simd});
    }
  }

  evnt.write(writer);
}

template <>
void ANIM::ANIM0::Enumerate<BigDNA::BinarySize>(size_t& __isz) {
  Header head;

  atUint32 maxId = 0;
  for (const std::pair<atUint32, bool>& bone : bones)
    maxId = std::max(maxId, bone.first);

  head.binarySize(__isz);
  __isz += maxId + 1;
  __isz += bones.size() + 4;

  __isz += 8;
  for (const std::pair<atUint32, bool>& bone : bones) {
    __isz += head.keyCount * 16;
    if (bone.second)
      __isz += head.keyCount * 12;
  }

  __isz += 4;
}

std::string_view ANIM::ANIM2::DNAType() { return "ANIM2"sv; }

template <>
void ANIM::ANIM2::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader) {
  Header head;
  head.read(reader);
  evnt = head.evnt;
  mainInterval = head.interval;
  looping = bool(head.looping);

  WordBitmap keyBmp;
  keyBmp.read(reader, head.keyBitmapBitCount);
  frames.clear();
  atUint32 frameAccum = 0;
  for (bool bit : keyBmp) {
    if (bit)
      frames.push_back(frameAccum);
    ++frameAccum;
  }
  reader.seek(8);

  bones.clear();
  bones.reserve(head.boneChannelCount);
  channels.clear();
  channels.reserve(head.boneChannelCount);
  atUint32 keyframeCount = 0;

  if (m_version == 3) {
    for (size_t b = 0; b < head.boneChannelCount; ++b) {
      ChannelDescPC desc;
      desc.read(reader);
      bones.emplace_back(desc.id, desc.keyCount2 != 0);

      if (desc.keyCount1) {
        channels.emplace_back();
        DNAANIM::Channel& chan = channels.back();
        chan.type = DNAANIM::Channel::Type::Rotation;
        chan.id = desc.id;
        chan.i[0] = atInt32(desc.QinitRX) >> 8;
        chan.q[0] = desc.QinitRX & 0xff;
        chan.i[1] = atInt32(desc.QinitRY) >> 8;
        chan.q[1] = desc.QinitRY & 0xff;
        chan.i[2] = atInt32(desc.QinitRZ) >> 8;
        chan.q[2] = desc.QinitRZ & 0xff;
      }
      keyframeCount = std::max(keyframeCount, desc.keyCount1);

      if (desc.keyCount2) {
        channels.emplace_back();
        DNAANIM::Channel& chan = channels.back();
        chan.type = DNAANIM::Channel::Type::Translation;
        chan.id = desc.id;
        chan.i[0] = atInt32(desc.QinitTX) >> 8;
        chan.q[0] = desc.QinitTX & 0xff;
        chan.i[1] = atInt32(desc.QinitTY) >> 8;
        chan.q[1] = desc.QinitTY & 0xff;
        chan.i[2] = atInt32(desc.QinitTZ) >> 8;
        chan.q[2] = desc.QinitTZ & 0xff;
      }
    }
  } else {
    for (size_t b = 0; b < head.boneChannelCount; ++b) {
      ChannelDesc desc;
      desc.read(reader);
      bones.emplace_back(desc.id, desc.keyCount2 != 0);

      if (desc.keyCount1) {
        channels.emplace_back();
        DNAANIM::Channel& chan = channels.back();
        chan.type = DNAANIM::Channel::Type::Rotation;
        chan.id = desc.id;
        chan.i[0] = desc.initRX;
        chan.q[0] = desc.qRX;
        chan.i[1] = desc.initRY;
        chan.q[1] = desc.qRY;
        chan.i[2] = desc.initRZ;
        chan.q[2] = desc.qRZ;
      }
      keyframeCount = std::max(keyframeCount, atUint32(desc.keyCount1));

      if (desc.keyCount2) {
        channels.emplace_back();
        DNAANIM::Channel& chan = channels.back();
        chan.type = DNAANIM::Channel::Type::Translation;
        chan.id = desc.id;
        chan.i[0] = desc.initTX;
        chan.q[0] = desc.qTX;
        chan.i[1] = desc.initTY;
        chan.q[1] = desc.qTY;
        chan.i[2] = desc.initTZ;
        chan.q[2] = desc.qTZ;
      }
    }
  }

  size_t bsSize = DNAANIM::ComputeBitstreamSize(keyframeCount, channels);
  std::unique_ptr<atUint8[]> bsData = reader.readUBytes(bsSize);
  DNAANIM::BitstreamReader bsReader;
  chanKeys = bsReader.read(bsData.get(), keyframeCount, channels, head.rotDiv, head.translationMult, 0.f);
}

template <>
void ANIM::ANIM2::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer) {
  Header head;
  head.evnt = evnt;
  head.unk0 = 1;
  head.interval = mainInterval;
  head.rootBoneId = 3;
  head.looping = looping;
  head.unk3 = 1;

  WordBitmap keyBmp;
  size_t frameCount = 0;
  for (atUint32 frame : frames) {
    if (!keyBmp.getBit(frame)) {
      keyBmp.setBit(frame);
      frameCount += 1;
    }
  }
  head.keyBitmapBitCount = keyBmp.getBitCount();
  head.duration = frames.back() * mainInterval;
  head.boneChannelCount = bones.size();

  size_t keyframeCount = frameCount - 1;
  std::vector<DNAANIM::Channel> qChannels = channels;
  DNAANIM::BitstreamWriter bsWriter;
  size_t bsSize;
  float scaleMult;
  std::unique_ptr<atUint8[]> bsData =
      bsWriter.write(chanKeys, keyframeCount, qChannels, m_version == 3 ? 0x7fffff : 0x7fff, head.rotDiv,
                     head.translationMult, scaleMult, bsSize);

  /* Tally up buffer size */
  size_t scratchSize = 0;
  head.binarySize(scratchSize);
  keyBmp.binarySize(scratchSize);
  scratchSize += bsSize;
  if (m_version == 3) {
    for (const std::pair<atUint32, bool>& bone : bones) {
      ChannelDescPC desc;
      desc.keyCount1 = keyframeCount;
      if (bone.second)
        desc.keyCount2 = keyframeCount;
      desc.binarySize(scratchSize);
    }
  } else {
    for (const std::pair<atUint32, bool>& bone : bones) {
      ChannelDesc desc;
      desc.keyCount1 = keyframeCount;
      if (bone.second)
        desc.keyCount2 = keyframeCount;
      desc.binarySize(scratchSize);
    }
  }
  head.scratchSize = scratchSize;

  head.write(writer);
  keyBmp.write(writer);
  writer.writeUint32Big(head.boneChannelCount);
  writer.writeUint32Big(head.boneChannelCount);
  auto cit = qChannels.begin();

  if (m_version == 3) {
    for (const std::pair<atUint32, bool>& bone : bones) {
      ChannelDescPC desc;
      desc.id = bone.first;
      DNAANIM::Channel& chan = *cit++;
      desc.keyCount1 = keyframeCount;
      desc.QinitRX = (chan.i[0] << 8) | chan.q[0];
      desc.QinitRY = (chan.i[1] << 8) | chan.q[1];
      desc.QinitRZ = (chan.i[2] << 8) | chan.q[2];
      if (bone.second) {
        DNAANIM::Channel& chan = *cit++;
        desc.keyCount2 = keyframeCount;
        desc.QinitTX = (chan.i[0] << 8) | chan.q[0];
        desc.QinitTY = (chan.i[1] << 8) | chan.q[1];
        desc.QinitTZ = (chan.i[2] << 8) | chan.q[2];
      }
      desc.write(writer);
    }
  } else {
    for (const std::pair<atUint32, bool>& bone : bones) {
      ChannelDesc desc;
      desc.id = bone.first;
      DNAANIM::Channel& chan = *cit++;
      desc.keyCount1 = keyframeCount;
      desc.initRX = chan.i[0];
      desc.qRX = chan.q[0];
      desc.initRY = chan.i[1];
      desc.qRY = chan.q[1];
      desc.initRZ = chan.i[2];
      desc.qRZ = chan.q[2];
      if (bone.second) {
        DNAANIM::Channel& chan = *cit++;
        desc.keyCount2 = keyframeCount;
        desc.initTX = chan.i[0];
        desc.qTX = chan.q[0];
        desc.initTY = chan.i[1];
        desc.qTY = chan.q[1];
        desc.initTZ = chan.i[2];
        desc.qTZ = chan.q[2];
      }
      desc.write(writer);
    }
  }

  writer.writeUBytes(bsData.get(), bsSize);
}

template <>
void ANIM::ANIM2::Enumerate<BigDNA::BinarySize>(size_t& __isz) {
  Header head;

  WordBitmap keyBmp;
  for (atUint32 frame : frames)
    keyBmp.setBit(frame);

  head.binarySize(__isz);
  keyBmp.binarySize(__isz);
  __isz += 8;
  if (m_version == 3) {
    for (const std::pair<atUint32, bool>& bone : bones) {
      __isz += 24;
      if (bone.second)
        __isz += 12;
    }
  } else {
    for (const std::pair<atUint32, bool>& bone : bones) {
      __isz += 17;
      if (bone.second)
        __isz += 9;
    }
  }

  __isz += DNAANIM::ComputeBitstreamSize(frames.size(), channels);
}

ANIM::ANIM(const BlenderAction& act, const std::unordered_map<std::string, atInt32>& idMap,
           const DNAANIM::RigInverter<CINF>& rig, bool pc) {
  m_anim = std::make_unique<ANIM2>(pc);
  IANIM& newAnim = *m_anim;
  newAnim.looping = act.looping;

  newAnim.bones.reserve(act.channels.size());
  size_t extChanCount = 0;
  std::unordered_set<atInt32> addedBones;
  addedBones.reserve(act.channels.size());
  for (const BlenderAction::Channel& chan : act.channels) {
    auto search = idMap.find(chan.boneName);
    if (search == idMap.cend()) {
      Log.report(logvisor::Warning, fmt("unable to find id for bone '{}'"), chan.boneName);
      continue;
    }
    if (addedBones.find(search->second) != addedBones.cend())
      continue;
    addedBones.insert(search->second);

    extChanCount += std::max(zeus::PopCount(chan.attrMask), 2);
    newAnim.bones.emplace_back(search->second, (chan.attrMask & 0x2) != 0);
  }

  newAnim.frames.reserve(act.frames.size());
  for (int32_t frame : act.frames)
    newAnim.frames.push_back(frame);

  newAnim.channels.reserve(extChanCount);
  newAnim.chanKeys.reserve(extChanCount);

  for (const BlenderAction::Channel& chan : act.channels) {
    auto search = idMap.find(chan.boneName);
    if (search == idMap.cend())
      continue;

    newAnim.channels.emplace_back();
    DNAANIM::Channel& newChan = newAnim.channels.back();
    newChan.type = DNAANIM::Channel::Type::Rotation;
    newChan.id = search->second;

    newAnim.chanKeys.emplace_back();
    std::vector<DNAANIM::Value>& rotVals = newAnim.chanKeys.back();
    rotVals.reserve(chan.keys.size());
    float sign = 0.f;
    for (const BlenderAction::Channel::Key& key : chan.keys) {
      zeus::CQuaternion q(key.rotation.val);
      q = rig.restoreRotation(newChan.id, q);
      if (sign == 0.f)
        sign = q.w() < 0.f ? -1.f : 1.f;
      q *= sign;
      q.normalize();
      rotVals.emplace_back(q.mSimd);
    }

    if (chan.attrMask & 0x2) {
      newAnim.channels.emplace_back();
      DNAANIM::Channel& newChan = newAnim.channels.back();
      newChan.type = DNAANIM::Channel::Type::Translation;
      newChan.id = search->second;

      newAnim.chanKeys.emplace_back();
      std::vector<DNAANIM::Value>& transVals = newAnim.chanKeys.back();
      transVals.reserve(chan.keys.size());
      for (const BlenderAction::Channel::Key& key : chan.keys) {
        zeus::CVector3f pos(key.position.val);
        pos = rig.restorePosition(newChan.id, pos, true);
        transVals.emplace_back(pos.mSimd);
      }
    }
  }

  /* Retro's original data uses microsecond precision */
  newAnim.mainInterval = std::trunc(act.interval * 1000000.0) / 1000000.0;
}

} // namespace DataSpec::DNAMP1

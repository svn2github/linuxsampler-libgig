/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2017 Christian Schoenebeck                              *
 *                      <cuse@users.sourceforge.net>                       *
 *                                                                         *
 *   This library is part of libgig.                                       *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this library; if not, write to the Free Software           *
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston,                 *
 *   MA  02111-1307  USA                                                   *
 ***************************************************************************/

#include "Serialization.h"

#include <iostream>
#include <assert.h>
#include <string.h> // for memcpy()
#include <stdlib.h> // for atof()

#include "helper.h"

namespace Serialization {

    // *************** DataType ***************
    // *

    static UID _createNullUID() {
        return (UID) { NULL, 0 };
    }

    const UID NO_UID = _createNullUID();

    bool UID::isValid() const {
        return id != NULL && id != (void*)-1 && size;
    }

    // *************** DataType ***************
    // *

    DataType::DataType() {
        m_size = 0;
        m_isPointer = false;
    }

    DataType::DataType(bool isPointer, int size, String baseType, String customType) {
        m_size = size;
        m_isPointer = isPointer;
        m_baseTypeName = baseType;
        m_customTypeName = customType;
    }

    bool DataType::isValid() const {
        return m_size;
    }

    bool DataType::isPointer() const {
        return m_isPointer;
    }

    bool DataType::isClass() const {
        return m_baseTypeName == "class";
    }

    bool DataType::isPrimitive() const {
        return !isClass();
    }

    bool DataType::isInteger() const {
        return m_baseTypeName.substr(0, 3) == "int" ||
               m_baseTypeName.substr(0, 4) == "uint";
    }

    bool DataType::isReal() const {
        return m_baseTypeName.substr(0, 4) == "real";
    }

    bool DataType::isBool() const {
        return m_baseTypeName == "bool";
    }

    bool DataType::isEnum() const {
        return m_baseTypeName == "enum";
    }

    bool DataType::isSigned() const {
        return m_baseTypeName.substr(0, 3) == "int" ||
               isReal();
    }

    bool DataType::operator==(const DataType& other) const {
        return m_baseTypeName   == other.m_baseTypeName &&
               m_customTypeName == other.m_customTypeName &&
               m_size           == other.m_size &&
               m_isPointer      == other.m_isPointer;
    }

    bool DataType::operator!=(const DataType& other) const {
        return !operator==(other);
    }

    bool DataType::operator<(const DataType& other) const {
        return m_baseTypeName  < other.m_baseTypeName ||
              (m_baseTypeName == other.m_baseTypeName &&
               m_customTypeName  < other.m_customTypeName ||
              (m_customTypeName == other.m_customTypeName &&
               m_size  < other.m_size ||
              (m_size == other.m_size &&
               m_isPointer < other.m_isPointer)));
    }

    bool DataType::operator>(const DataType& other) const {
        return !(operator==(other) || operator<(other));
    }

    String DataType::asLongDescr() const {
        //TODO: Demangling of C++ raw type names
        String s = m_baseTypeName;
        if (!m_customTypeName.empty())
            s += " " + m_customTypeName;
        if (isPointer())
            s += " pointer";
        return s;
    }

    // *************** Member ***************
    // *

    Member::Member() {
        m_uid = NO_UID;
        m_offset = 0;
    }

    Member::Member(String name, UID uid, size_t offset, DataType type) {
        m_name = name;
        m_uid  = uid;
        m_offset = offset;
        m_type = type;
    }

    bool Member::isValid() const {
        return m_uid && !m_name.empty() && m_type;
    }

    bool Member::operator==(const Member& other) const {
        return m_uid    == other.m_uid &&
               m_offset == other.m_offset &&
               m_name   == other.m_name &&
               m_type   == other.m_type;
    }

    bool Member::operator!=(const Member& other) const {
        return !operator==(other);
    }

    bool Member::operator<(const Member& other) const {
        return m_uid  < other.m_uid ||
              (m_uid == other.m_uid &&
               m_offset  < other.m_offset ||
              (m_offset == other.m_offset &&
               m_name  < other.m_name ||
              (m_name == other.m_name &&
               m_type < other.m_type)));
    }

    bool Member::operator>(const Member& other) const {
        return !(operator==(other) || operator<(other));
    }

    // *************** Object ***************
    // *

    Object::Object() {
        m_version = 0;
        m_minVersion = 0;
    }

    Object::Object(UIDChain uidChain, DataType type) {
        m_type = type;
        m_uid  = uidChain;
        m_version = 0;
        m_minVersion = 0;
        m_data.resize(type.size());
    }

    bool Object::isValid() const {
        return m_type && !m_uid.empty();
    }

    bool Object::operator==(const Object& other) const {
        // ignoring all other member variables here
        // (since UID stands for "unique" ;-) )
        return m_uid  == other.m_uid &&
               m_type == other.m_type;
    }

    bool Object::operator!=(const Object& other) const {
        return !operator==(other);
    }

    bool Object::operator<(const Object& other) const {
        // ignoring all other member variables here
        // (since UID stands for "unique" ;-) )
        return m_uid  < other.m_uid ||
              (m_uid == other.m_uid &&
               m_type < other.m_type);
    }

    bool Object::operator>(const Object& other) const {
        return !(operator==(other) || operator<(other));
    }

    bool Object::isVersionCompatibleTo(const Object& other) const {
        if (this->version() == other.version())
            return true;
        if (this->version() > other.version())
            return this->minVersion() <= other.version();
        else
            return other.minVersion() <= this->version();
    }

    Member Object::memberNamed(String name) const {
        for (int i = 0; i < m_members.size(); ++i)
            if (m_members[i].name() == name)
                return m_members[i];
        return Member();
    }

    void Object::remove(const Member& member) {
        for (int i = 0; i < m_members.size(); ++i) {
            if (m_members[i] == member) {
                m_members.erase(m_members.begin() + i);
                return;
            }
        }
    }

    std::vector<Member> Object::membersOfType(const DataType& type) const {
        std::vector<Member> v;
        for (int i = 0; i < m_members.size(); ++i) {
            const Member& member = m_members[i];
            if (member.type() == type)
                v.push_back(member);
        }
        return v;
    }

    int Object::sequenceIndexOf(const Member& member) const {
        for (int i = 0; i < m_members.size(); ++i)
            if (m_members[i] == member)
                return i;
        return -1;
    }

    // *************** Archive ***************
    // *

    Archive::Archive() {
        m_operation = OPERATION_NONE;
        m_root = NO_UID;
    }

    Archive::Archive(const RawData& data) {
        m_operation = OPERATION_NONE;
        m_root = NO_UID;
        decode(m_rawData);
    }

    Archive::Archive(const uint8_t* data, size_t size) {
        m_operation = OPERATION_NONE;
        m_root = NO_UID;
        decode(data, size);
    }

    Archive::~Archive() {
    }

    Object& Archive::rootObject() {
        return m_allObjects[m_root];
    }

    static String _encodeBlob(String data) {
        return ToString(data.length()) + ":" + data;
    }

    static String _encode(const UID& uid) {
        String s;
        s += _encodeBlob(ToString(size_t(uid.id)));
        s += _encodeBlob(ToString(size_t(uid.size)));
        return _encodeBlob(s);
    }

    static String _encode(const DataType& type) {
        String s;
        s += _encodeBlob(type.baseTypeName());
        s += _encodeBlob(type.customTypeName());
        s += _encodeBlob(ToString(type.size()));
        s += _encodeBlob(ToString(type.isPointer()));
        return _encodeBlob(s);
    }

    static String _encode(const UIDChain& chain) {
        String s;
        for (int i = 0; i < chain.size(); ++i)
            s += _encode(chain[i]);
        return _encodeBlob(s);
    }

    static String _encode(const Member& member) {
        String s;
        s += _encode(member.uid());
        s += _encodeBlob(ToString(member.offset()));
        s += _encodeBlob(member.name());
        s += _encode(member.type());
        return _encodeBlob(s);
    }

    static String _encode(const std::vector<Member>& members) {
        String s;
        for (int i = 0; i < members.size(); ++i)
            s += _encode(members[i]);
        return _encodeBlob(s);
    }

    static String _encodePrimitiveValue(const Object& obj) {
        String s;
        const DataType& type = obj.type();
        const ID& id = obj.uid().id;
        if (type.isPrimitive() && !type.isPointer()) {
            if (type.isInteger() || type.isEnum()) {
                if (type.isSigned()) {
                    if (type.size() == 1)
                        s = ToString((int16_t)*(int8_t*)id); // int16_t: prevent ToString() to render an ASCII character
                    else if (type.size() == 2)
                        s = ToString(*(int16_t*)id);
                    else if (type.size() == 4)
                        s = ToString(*(int32_t*)id);
                    else if (type.size() == 8)
                        s = ToString(*(int64_t*)id);
                    else
                        assert(false /* unknown signed int type size */);
                } else {
                    if (type.size() == 1)
                        s = ToString((uint16_t)*(uint8_t*)id); // uint16_t: prevent ToString() to render an ASCII character
                    else if (type.size() == 2)
                        s = ToString(*(uint16_t*)id);
                    else if (type.size() == 4)
                        s = ToString(*(uint32_t*)id);
                    else if (type.size() == 8)
                        s = ToString(*(uint64_t*)id);
                    else
                        assert(false /* unknown unsigned int type size */);
                }
            } else if (type.isReal()) {
                if (type.size() == sizeof(float))
                    s = ToString(*(float*)id);
                else if (type.size() == sizeof(double))
                    s = ToString(*(double*)id);
                else
                    assert(false /* unknown floating point type */);
            } else if (type.isBool()) {
                s = ToString(*(bool*)id);
            } else {
                assert(false /* unknown primitive type */);
            }

        }
        return _encodeBlob(s);
    }

    static String _encode(const Object& obj) {
        String s;
        s += _encode(obj.type());
        s += _encodeBlob(ToString(obj.version()));
        s += _encodeBlob(ToString(obj.minVersion()));
        s += _encode(obj.uidChain());
        s += _encode(obj.members());
        s += _encodePrimitiveValue(obj);
        return _encodeBlob(s);
    }

    String _encode(const Archive::ObjectPool& objects) {
        String s;
        for (Archive::ObjectPool::const_iterator itObject = objects.begin();
             itObject != objects.end(); ++itObject)
        {
            const Object& obj = itObject->second;
            s += _encode(obj);
        }
        return _encodeBlob(s);
    }

    #define MAGIC_START "Srx1v"
    #define ENCODING_FORMAT_MINOR_VERSION 0

    String Archive::_encodeRootBlob() {
        String s;
        s += _encodeBlob(ToString(ENCODING_FORMAT_MINOR_VERSION));
        s += _encode(m_root);
        s += _encode(m_allObjects);
        return _encodeBlob(s);
    }

    void Archive::encode() {
        m_rawData.clear();
        String s = MAGIC_START;
        s += _encodeRootBlob();
        m_rawData.resize(s.length() + 1);
        memcpy(&m_rawData[0], &s[0], s.length() + 1);
    }

    struct _Blob {
        const char* p;
        const char* end;
    };

    static _Blob _decodeBlob(const char* p, const char* end, bool bThrow = true) {
        if (!bThrow && p >= end)
            return (_Blob) { p, end };
        size_t sz = 0;
        for (; true; ++p) {
            if (p >= end)
                throw Exception("Decode Error: Missing blob");
            const char& c = *p;
            if (c == ':') break;
            if (c < '0' || c > '9')
                throw Exception("Decode Error: Missing blob size");
            sz *= 10;
            sz += size_t(c - '0');
        }
        ++p;
        if (p + sz > end)
            throw Exception("Decode Error: Premature end of blob");
        return (_Blob) { p, p + sz };
    }

    template<typename T_int>
    static T_int _popIntBlob(const char*& p, const char* end) {
        _Blob blob = _decodeBlob(p, end);
        p   = blob.p;
        end = blob.end;

        T_int sign = 1;
        T_int i = 0;
        if (p >= end)
            throw Exception("Decode Error: premature end of int blob");
        if (*p == '-') {
            sign = -1;
            ++p;
        }
        for (; p < end; ++p) {
            const char& c = *p;
            if (c < '0' || c > '9')
                throw Exception("Decode Error: Invalid int blob format");
            i *= 10;
            i += size_t(c - '0');
        }
        return i * sign;
    }

    template<typename T_int>
    static void _popIntBlob(const char*& p, const char* end, RawData& rawData) {
        const T_int i = _popIntBlob<T_int>(p, end);
        *(T_int*)&rawData[0] = i;
    }

    template<typename T_real>
    static T_real _popRealBlob(const char*& p, const char* end) {
        _Blob blob = _decodeBlob(p, end);
        p   = blob.p;
        end = blob.end;

        if (p >= end || (end - p) < 1)
            throw Exception("Decode Error: premature end of real blob");

        String s(p, size_t(end - p));

        T_real r;
        if (sizeof(T_real) <= sizeof(double))
            r = atof(s.c_str());
        else
            assert(false /* unknown real type */);

        p += s.length();

        return r;
    }

    template<typename T_real>
    static void _popRealBlob(const char*& p, const char* end, RawData& rawData) {
        const T_real r = _popRealBlob<T_real>(p, end);
        *(T_real*)&rawData[0] = r;
    }

    static String _popStringBlob(const char*& p, const char* end) {
        _Blob blob = _decodeBlob(p, end);
        p   = blob.p;
        end = blob.end;
        if (end - p < 0)
            throw Exception("Decode Error: missing String blob");
        String s;
        const size_t sz = end - p;
        s.resize(sz);
        memcpy(&s[0], p, sz);
        p += sz;
        return s;
    }

    DataType _popDataTypeBlob(const char*& p, const char* end) {
        _Blob blob = _decodeBlob(p, end);
        p   = blob.p;
        end = blob.end;

        DataType type;
        type.m_baseTypeName   = _popStringBlob(p, end);
        type.m_customTypeName = _popStringBlob(p, end);
        type.m_size           = _popIntBlob<int>(p, end);
        type.m_isPointer      = _popIntBlob<bool>(p, end);
        return type;
    }

    static UID _popUIDBlob(const char*& p, const char* end) {
        _Blob blob = _decodeBlob(p, end);
        p   = blob.p;
        end = blob.end;

        if (p >= end)
            throw Exception("Decode Error: premature end of UID blob");

        const ID id = (ID) _popIntBlob<size_t>(p, end);
        const size_t size = _popIntBlob<size_t>(p, end);

        return (UID) { id, size };
    }

    static UIDChain _popUIDChainBlob(const char*& p, const char* end) {
        _Blob blob = _decodeBlob(p, end);
        p   = blob.p;
        end = blob.end;

        UIDChain chain;
        while (p < end) {
            const UID uid = _popUIDBlob(p, end);
            chain.push_back(uid);
        }
        assert(!chain.empty());
        return chain;
    }

    Member _popMemberBlob(const char*& p, const char* end) {
        _Blob blob = _decodeBlob(p, end, false);
        p   = blob.p;
        end = blob.end;

        Member m;
        if (p >= end) return m;

        m.m_uid    = _popUIDBlob(p, end);
        m.m_offset = _popIntBlob<size_t>(p, end);
        m.m_name   = _popStringBlob(p, end);
        m.m_type   = _popDataTypeBlob(p, end);
        assert(m.type());
        assert(!m.name().empty());
        assert(m.uid() != NULL);
        return m;
    }

    static std::vector<Member> _popMembersBlob(const char*& p, const char* end) {
        _Blob blob = _decodeBlob(p, end, false);
        p   = blob.p;
        end = blob.end;

        std::vector<Member> members;
        while (p < end) {
            const Member member = _popMemberBlob(p, end);
            if (member)
                members.push_back(member);
            else
                break;
        }
        return members;
    }

    void _popPrimitiveValue(const char*& p, const char* end, Object& obj) {
        const DataType& type = obj.type();
        if (type.isPrimitive() && !type.isPointer()) {
            obj.m_data.resize(type.size());
            if (type.isInteger() || type.isEnum()) {
                if (type.isSigned()) {
                    if (type.size() == 1)
                        _popIntBlob<int8_t>(p, end, obj.m_data);
                    else if (type.size() == 2)
                        _popIntBlob<int16_t>(p, end, obj.m_data);
                    else if (type.size() == 4)
                        _popIntBlob<int32_t>(p, end, obj.m_data);
                    else if (type.size() == 8)
                        _popIntBlob<int64_t>(p, end, obj.m_data);
                    else
                        assert(false /* unknown signed int type size */);
                } else {
                    if (type.size() == 1)
                        _popIntBlob<uint8_t>(p, end, obj.m_data);
                    else if (type.size() == 2)
                        _popIntBlob<uint16_t>(p, end, obj.m_data);
                    else if (type.size() == 4)
                        _popIntBlob<uint32_t>(p, end, obj.m_data);
                    else if (type.size() == 8)
                        _popIntBlob<uint64_t>(p, end, obj.m_data);
                    else
                        assert(false /* unknown unsigned int type size */);
                }
            } else if (type.isReal()) {
                if (type.size() == sizeof(float))
                    _popRealBlob<float>(p, end, obj.m_data);
                else if (type.size() == sizeof(double))
                    _popRealBlob<double>(p, end, obj.m_data);
                else
                    assert(false /* unknown floating point type */);
            } else if (type.isBool()) {
                _popIntBlob<uint8_t>(p, end, obj.m_data);
            } else {
                assert(false /* unknown primitive type */);
            }

        } else {
            // don't whine if the empty blob was not added on encoder side
            _Blob blob = _decodeBlob(p, end, false);
            p   = blob.p;
            end = blob.end;
        }
    }

    Object _popObjectBlob(const char*& p, const char* end) {
        _Blob blob = _decodeBlob(p, end, false);
        p   = blob.p;
        end = blob.end;

        Object obj;
        if (p >= end) return obj;

        obj.m_type       = _popDataTypeBlob(p, end);
        obj.m_version    = _popIntBlob<Version>(p, end);
        obj.m_minVersion = _popIntBlob<Version>(p, end);
        obj.m_uid        = _popUIDChainBlob(p, end);
        obj.m_members    = _popMembersBlob(p, end);
        _popPrimitiveValue(p, end, obj);
        assert(obj.type());
        return obj;
    }

    void Archive::_popObjectsBlob(const char*& p, const char* end) {
        _Blob blob = _decodeBlob(p, end, false);
        p   = blob.p;
        end = blob.end;

        if (p >= end)
            throw Exception("Decode Error: Premature end of objects blob");

        while (true) {
            const Object obj = _popObjectBlob(p, end);
            if (!obj) break;
            m_allObjects[obj.uid()] = obj;
        }
    }

    void Archive::_popRootBlob(const char*& p, const char* end) {
        _Blob blob = _decodeBlob(p, end, false);
        p   = blob.p;
        end = blob.end;

        if (p >= end)
            throw Exception("Decode Error: Premature end of root blob");

        // just in case this encoding format will be extended in future
        // (currently not used)
        const int formatMinorVersion = _popIntBlob<int>(p, end);

        m_root = _popUIDBlob(p, end);
        if (!m_root)
            throw Exception("Decode Error: No root object");

        _popObjectsBlob(p, end);
        if (!m_allObjects[m_root])
            throw Exception("Decode Error: Missing declared root object");
    }

    void Archive::decode(const RawData& data) {
        m_rawData = data;
        m_allObjects.clear();
        const char* p   = (const char*) &data[0];
        const char* end = p + data.size();
        if (memcmp(p, MAGIC_START, std::min(strlen(MAGIC_START), data.size())))
            throw Exception("Decode Error: Magic start missing!");
        p += strlen(MAGIC_START);
        _popRootBlob(p, end);
    }

    void Archive::decode(const uint8_t* data, size_t size) {
        RawData rawData;
        rawData.resize(size);
        memcpy(&rawData[0], data, size);
        decode(rawData);
    }

    String Archive::rawDataFormat() const {
        return MAGIC_START;
    }

    void Archive::clear() {
        m_allObjects.clear();
        m_operation = OPERATION_NONE;
        m_root = NO_UID;
        m_rawData.clear();
    }

    void Archive::remove(const Object& obj) {
        if (!obj.uid()) return;
        m_allObjects.erase(obj.uid());
    }

    Object& Archive::objectByUID(const UID& uid) {
        return m_allObjects[uid];
    }

    // *************** Archive::Syncer ***************
    // *

    Archive::Syncer::Syncer(Archive& dst, Archive& src)
       : m_dst(dst), m_src(src)
    {
        const Object srcRootObj = src.rootObject();
        const Object dstRootObj = dst.rootObject();
        if (!srcRootObj)
            throw Exception("No source root object!");
        if (!dstRootObj)
            throw Exception("Expected destination root object not found!");
        syncObject(dstRootObj, srcRootObj);
    }

    void Archive::Syncer::syncPrimitive(const Object& dstObj, const Object& srcObj) {
        assert(srcObj.rawData().size() == dstObj.type().size());
        void* pDst = (void*)dstObj.uid().id;
        memcpy(pDst, &srcObj.rawData()[0], dstObj.type().size());
    }

    void Archive::Syncer::syncPointer(const Object& dstObj, const Object& srcObj) {
        assert(dstObj.type().isPointer());
        assert(dstObj.type() == srcObj.type());
        const Object& pointedDstObject = m_dst.m_allObjects[dstObj.uid(1)];
        const Object& pointedSrcObject = m_src.m_allObjects[srcObj.uid(1)];
        syncObject(pointedDstObject, pointedSrcObject);
    }

    void Archive::Syncer::syncObject(const Object& dstObj, const Object& srcObj) {
        if (!dstObj || !srcObj) return; // end of recursion
        if (!dstObj.isVersionCompatibleTo(srcObj))
            throw Exception("Version incompatible (destination version " +
                            ToString(dstObj.version()) + " [min. version " +
                            ToString(dstObj.minVersion()) + "], source version " +
                            ToString(srcObj.version()) + " [min. version " +
                            ToString(srcObj.minVersion()) + "])");
        if (dstObj.type() != srcObj.type())
            throw Exception("Incompatible data structure type (destination type " +
                            dstObj.type().asLongDescr() + " vs. source type " +
                            srcObj.type().asLongDescr() + ")");

        // prevent syncing this object again, and thus also prevent endless
        // loop on data structures with cyclic relations
        m_dst.m_allObjects.erase(dstObj.uid());

        if (dstObj.type().isPrimitive() && !dstObj.type().isPointer()) {
            syncPrimitive(dstObj, srcObj);
            return; // end of recursion
        }

        if (dstObj.type().isPointer()) {
            syncPointer(dstObj, srcObj);
            return;
        }

        assert(dstObj.type().isClass());
        for (int iMember = 0; iMember < srcObj.members().size(); ++iMember) {
            const Member& srcMember = srcObj.members()[iMember];
            Member dstMember = dstMemberMatching(dstObj, srcObj, srcMember);
            if (!dstMember)
                throw Exception("Expected member missing in destination object");
            syncMember(dstMember, srcMember);
        }
    }

    Member Archive::Syncer::dstMemberMatching(const Object& dstObj, const Object& srcObj, const Member& srcMember) {
        Member dstMember = dstObj.memberNamed(srcMember.name());
        if (dstMember)
            return (dstMember.type() == srcMember.type()) ? dstMember : Member();
        std::vector<Member> members = dstObj.membersOfType(srcMember.type());
        if (members.size() <= 0)
            return Member();
        if (members.size() == 1)
            return members[0];
        for (int i = 0; i < members.size(); ++i)
            if (members[i].offset() == srcMember.offset())
                return members[i];
        const int srcSeqNr = srcObj.sequenceIndexOf(srcMember);
        assert(srcSeqNr >= 0); // should never happen, otherwise there is a bug
        for (int i = 0; i < members.size(); ++i) {
            const int dstSeqNr = dstObj.sequenceIndexOf(members[i]);
            if (dstSeqNr == srcSeqNr)
                return members[i];
        }
        return Member(); // give up!
    }

    void Archive::Syncer::syncMember(const Member& dstMember, const Member& srcMember) {
        assert(dstMember && srcMember);
        assert(dstMember.type() == srcMember.type());
        const Object dstObj = m_dst.m_allObjects[dstMember.uid()];
        const Object srcObj = m_src.m_allObjects[srcMember.uid()];
        syncObject(dstObj, srcObj);
    }

    // *************** Exception ***************
    // *

    void Exception::PrintMessage() {
        std::cout << "Serialization::Exception: " << Message << std::endl;
    }

} // namespace Serialization

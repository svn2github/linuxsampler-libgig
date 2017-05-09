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

#ifndef LIBGIG_SERIALIZATION_H
#define LIBGIG_SERIALIZATION_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <typeinfo>
#include <string>
#include <vector>
#include <map>
#include <time.h>
#if __cplusplus < 201103L
# include <tr1/type_traits>
# define LIBGIG_IS_CLASS(type) std::tr1::__is_union_or_class<type>::value //NOTE: without compiler support we cannot distinguish union from class
#else
# define LIBGIG_IS_CLASS(type) __is_class(type)
#endif

/** @brief Serialization / deserialization framework.
 *
 * See class Archive as starting point for how to implement serialization and
 * deserialization with your application.
 *
 * The classes in this namespace allow to serialize and deserialize native
 * C++ objects in a portable, easy and flexible way. Serialization is a
 * technique that allows to transform the current state and data of native
 * (in this case C++) objects into a data stream (including all other objects
 * the "serialized" objects relate to); the data stream may then be sent over
 * "wire" (for example via network connection to another computer, which might
 * also have a different OS, CPU architecture, native memory word size and
 * endian type); and finally the data stream would be "deserialized" on that
 * receiver side, that is transformed again to modify all objects and data
 * structures on receiver side to resemble the objects' state and data as it
 * was originally on sender side.
 *
 * In contrast to many other already existing serialization frameworks, this
 * implementation has a strong focus on robustness regarding long-term changes
 * to the serialized C++ classes of the serialized objects. So even if sender
 * and receiver are using different versions of their serialized/deserialized
 * C++ classes, structures and data types (thus having different data structure
 * layout to a certain extent), this framework aims trying to automatically
 * adapt its serialization and deserialization process in that case so that
 * the deserialized objects on receiver side would still reflect the overall
 * expected states and overall data as intended by the sender. For being able to
 * do so, this framework stores all kind of additional information about each
 * serialized object and each data structure member (for example name of each
 * data structure member, but also the offset of each member within its
 * containing data structure, precise data types, and more).
 *
 * Like most other serialization frameworks, this frameworks does not require a
 * tree-structured layout of the serialized data structures. So it automatically
 * handles also cyclic dependencies between serialized data structures
 * correctly, without i.e. causing endless recursion or redundancy.
 *
 * Additionally this framework also allows partial deserialization. Which means
 * the receiver side may for example decide that it wants to restrict
 * deserialization so that it would only modify certain objects or certain
 * members by the deserialization process, leaving all other ones untouched.
 * So this partial deserialization technique for example allows to implement
 * flexible preset features for applications in a powerful and easy way.
 */
namespace Serialization {

    // just symbol prototyping
    class DataType;
    class Object;
    class Member;
    class Archive;
    class ObjectPool;
    class Exception;

    typedef std::string String;

    typedef std::vector<uint8_t> RawData;

    typedef void* ID;

    typedef uint32_t Version;

    enum operation_t {
        OPERATION_NONE,
        OPERATION_SERIALIZE,
        OPERATION_DESERIALIZE
    };

    enum time_base_t {
        LOCAL_TIME,
        UTC_TIME
    };

    template<typename T>
    bool IsEnum(const T& data) {
        #if __cplusplus < 201103L
        return std::tr1::is_enum<T>::value;
        #else
        return __is_enum(T);
        #endif
    }

    template<typename T>
    bool IsUnion(const T& data) {
        #if __cplusplus < 201103L
        return false; // without compiler support we cannot distinguish union from class
        #else
        return __is_union(T);
        #endif
    }

    template<typename T>
    bool IsClass(const T& data) {
        #if __cplusplus < 201103L
        return std::tr1::__is_union_or_class<T>::value; // without compiler support we cannot distinguish union from class
        #else
        return __is_class(T);
        #endif
    }

    /*template<typename T>
    bool IsTrivial(T data) {
        return __is_trivial(T);
    }*/

    /*template<typename T>
    bool IsPOD(T data) {
        return __is_pod(T);
    }*/

    /** @brief Unique identifier for one specific C++ object, member or fundamental variable.
     *
     * Reflects a unique identifier for one specific serialized C++ class
     * instance, struct instance, member, primitive pointer, or fundamental
     * variables.
     */
    class UID {
    public:
        ID id;
        size_t size;

        bool isValid() const;
        operator bool() const { return isValid(); }
        //bool operator()() const { return isValid(); }
        bool operator==(const UID& other) const { return id == other.id && size == other.size; }
        bool operator!=(const UID& other) const { return id != other.id || size != other.size; }
        bool operator<(const UID& other) const { return id < other.id || (id == other.id && size < other.size); }
        bool operator>(const UID& other) const { return id > other.id || (id == other.id && size > other.size); }

        template<typename T>
        static UID from(const T& obj) {
            return Resolver<T>::resolve(obj);
        }

    protected:
        // UID resolver for non-pointer types
        template<typename T>
        struct Resolver {
            static UID resolve(const T& obj) {
                const UID uid = { (ID) &obj, sizeof(obj) };
                return uid;
            }
        };

        // UID resolver for pointer types (of 1st degree)
        template<typename T>
        struct Resolver<T*> {
            static UID resolve(const T* const & obj) {
                const UID uid = { (ID) obj, sizeof(*obj) };
                return uid;
            }
        };
    };

    /**
     * Reflects an invalid UID and behaves similar to NULL as invalid value for
     * pointer types.
     */
    extern const UID NO_UID;

    typedef std::vector<UID> UIDChain;

    // prototyping of private internal friend functions
    static String _encodePrimitiveValue(const Object& obj);
    static DataType _popDataTypeBlob(const char*& p, const char* end);
    static Member _popMemberBlob(const char*& p, const char* end);
    static Object _popObjectBlob(const char*& p, const char* end);
    static void _popPrimitiveValue(const char*& p, const char* end, Object& obj);
    static String _primitiveObjectValueToString(const Object& obj);

    /** @brief Abstract reflection of a native C++ data type.
     *
     * Provides detailed information about a C++ data type, whether it is a
     * fundamental C/C++ data type (like int, float, char, etc.) or custom
     * defined data type like a C++ class, struct, enum, as well as other
     * features of the data type like its native memory size and more.
     */
    class DataType {
    public:
        DataType();
        size_t size() const { return m_size; }
        bool isValid() const;
        bool isPointer() const;
        bool isClass() const;
        bool isPrimitive() const;
        bool isInteger() const;
        bool isReal() const;
        bool isBool() const;
        bool isEnum() const;
        bool isSigned() const;
        operator bool() const { return isValid(); }
        //bool operator()() const { return isValid(); }
        bool operator==(const DataType& other) const;
        bool operator!=(const DataType& other) const;
        bool operator<(const DataType& other) const;
        bool operator>(const DataType& other) const;
        String asLongDescr() const;
        String baseTypeName() const { return m_baseTypeName; }
        String customTypeName() const { return m_customTypeName; }

        template<typename T>
        static DataType dataTypeOf(const T& data) {
            return Resolver<T>::resolve(data);
        }

    protected:
        DataType(bool isPointer, int size, String baseType, String customType = "");

        template<typename T, bool T_isPointer>
        struct ResolverBase {
            static DataType resolve(const T& data) {
                const std::type_info& type = typeid(data);
                const int sz = sizeof(data);

                // for primitive types we are using our own type names instead of
                // using std:::type_info::name(), because the precise output of the
                // latter may vary between compilers
                if (type == typeid(int8_t))   return DataType(T_isPointer, sz, "int8");
                if (type == typeid(uint8_t))  return DataType(T_isPointer, sz, "uint8");
                if (type == typeid(int16_t))  return DataType(T_isPointer, sz, "int16");
                if (type == typeid(uint16_t)) return DataType(T_isPointer, sz, "uint16");
                if (type == typeid(int32_t))  return DataType(T_isPointer, sz, "int32");
                if (type == typeid(uint32_t)) return DataType(T_isPointer, sz, "uint32");
                if (type == typeid(int64_t))  return DataType(T_isPointer, sz, "int64");
                if (type == typeid(uint64_t)) return DataType(T_isPointer, sz, "uint64");
                if (type == typeid(bool))     return DataType(T_isPointer, sz, "bool");
                if (type == typeid(float))    return DataType(T_isPointer, sz, "real32");
                if (type == typeid(double))   return DataType(T_isPointer, sz, "real64");

                if (IsEnum(data)) return DataType(T_isPointer, sz, "enum", rawCppTypeNameOf(data));
                if (IsUnion(data)) return DataType(T_isPointer, sz, "union", rawCppTypeNameOf(data));
                if (IsClass(data)) return DataType(T_isPointer, sz, "class", rawCppTypeNameOf(data));

                return DataType();
            }
        };

        // DataType resolver for non-pointer types
        template<typename T>
        struct Resolver : ResolverBase<T,false> {
            static DataType resolve(const T& data) {
                return ResolverBase<T,false>::resolve(data);
            }
        };

        // DataType resolver for pointer types (of 1st degree)
        template<typename T>
        struct Resolver<T*> : ResolverBase<T,true> {
            static DataType resolve(const T*& data) {
                return ResolverBase<T,true>::resolve(*data);
            }
        };

        template<typename T>
        static String rawCppTypeNameOf(const T& data) {
            #if defined _MSC_VER // Microsoft compiler ...
            # warning type_info::raw_name() demangling has not been tested yet with Microsoft compiler! Feedback appreciated!
            String name = typeid(data).raw_name(); //NOTE: I haven't checked yet what MSC actually outputs here exactly
            #else // i.e. especially GCC and clang ...
            String name = typeid(data).name();
            #endif
            //while (!name.empty() && name[0] >= 0 && name[0] <= 9)
            //    name = name.substr(1);
            return name;
        }

    private:
        String m_baseTypeName;
        String m_customTypeName;
        int m_size;
        bool m_isPointer;

        friend DataType _popDataTypeBlob(const char*& p, const char* end);
        friend class Archive;
    };

    /** @brief Abstract reflection of a native C++ class/struct's member variable.
     *
     * Provides detailed information about a specific C++ member variable of
     * serialized C++ object, like its C++ data type, offset of this member
     * within its containing data structure/class, its C++ member variable name
     * and more.
     */
    class Member {
    public:
        Member();
        UID uid() const { return m_uid; }
        String name() const { return m_name; }
        size_t offset() const { return m_offset; }
        const DataType& type() const { return m_type; }
        bool isValid() const;
        operator bool() const { return isValid(); }
        //bool operator()() const { return isValid(); }
        bool operator==(const Member& other) const;
        bool operator!=(const Member& other) const;
        bool operator<(const Member& other) const;
        bool operator>(const Member& other) const;

    protected:
        Member(String name, UID uid, size_t offset, DataType type);
        friend class Archive;

    private:
        UID m_uid;
        size_t m_offset;
        String m_name;
        DataType m_type;

        friend Member _popMemberBlob(const char*& p, const char* end);
    };

    /** @brief Abstract reflection of a native C++ class/struct instance.
     *
     * Provides detailed information about a specific serialized C++ object,
     * like its C++ member variables, its C++ class/struct name, its native
     * memory size and more.
     */
    class Object {
    public:
        Object();
        Object(UIDChain uidChain, DataType type);

        UID uid(int index = 0) const {
            return (index < m_uid.size()) ? m_uid[index] : NO_UID;
        }

        const UIDChain& uidChain() const { return m_uid; }
        const DataType& type() const { return m_type; }
        const RawData& rawData() const { return m_data; }

        Version version() const { return m_version; }

        void setVersion(Version v) {
            m_version = v;
        }

        Version minVersion() const { return m_minVersion; }

        void setMinVersion(Version v) {
            m_minVersion = v;
        }

        bool isVersionCompatibleTo(const Object& other) const;

        std::vector<Member>& members() { return m_members; }
        const std::vector<Member>& members() const { return m_members; }
        Member memberNamed(String name) const;
        Member memberByUID(const UID& uid) const;
        std::vector<Member> membersOfType(const DataType& type) const;
        int sequenceIndexOf(const Member& member) const;
        bool isValid() const;
        operator bool() const { return isValid(); }
        //bool operator()() const { return isValid(); }
        bool operator==(const Object& other) const;
        bool operator!=(const Object& other) const;
        bool operator<(const Object& other) const;
        bool operator>(const Object& other) const;

    protected:
        void remove(const Member& member);

    private:
        DataType m_type;
        UIDChain m_uid;
        Version m_version;
        Version m_minVersion;
        RawData m_data;
        std::vector<Member> m_members;

        friend String _encodePrimitiveValue(const Object& obj);
        friend Object _popObjectBlob(const char*& p, const char* end);
        friend void _popPrimitiveValue(const char*& p, const char* end, Object& obj);
        friend String _primitiveObjectValueToString(const Object& obj);
        friend class Archive;
    };

    /** @brief Destination container for serialization, and source container for deserialization.
     *
     * This is the main class for implementing serialization and deserialization
     * with your C++ application. This framework does not require a a tree
     * structured layout of your C++ objects being serialized/deserialized, it
     * uses a concept of a "root" object though. So to start serialization
     * construct an empty Archive object and then instruct it to serialize your
     * C++ objects by pointing it to your "root" object:
     * @code
     * Archive a;
     * a.serialize(&myRootObject);
     * @endcode
     * Or if you prefer the look of operator based code:
     * @code
     * Archive a;
     * a << myRootObject;
     * @endcode
     * The Archive object will then serialize all members of the passed C++
     * object, and will recursively serialize all other C++ objects which it
     * contains or points to. So the root object is the starting point for the
     * overall serialization. After the serialize() method returned, you can
     * then access the serialized data stream by calling rawData() and send that
     * data stream over "wire", or store it on disk or whatever you may intend
     * to do with it.
     *
     * Then on receiver side likewise, you create a new Archive object, pass the
     * received data stream i.e. via constructor to the Archive object and call
     * deserialize() by pointing it to the root object on receiver side:
     * @code
     * Archive a(rawDataStream);
     * a.deserialize(&myRootObject);
     * @endcode
     * Or with operator instead:
     * @code
     * Archive a(rawDataStream);
     * a >> myRootObject;
     * @endcode
     * Now this framework automatically handles serialization and
     * deserialization of fundamental data types automatically for you (like
     * i.e. char, int, long int, float, double, etc.). However for your own
     * custom C++ classes and structs you must implement one method which
     * defines which members of your class should actually be serialized and
     * deserialized. That method to be added must have the following signature:
     * @code
     * void serialize(Serialization::Archive* archive);
     * @endcode
     * So let's say you have the following simple data structures:
     * @code
     * struct Foo {
     *     int a;
     *     bool b;
     *     double c;
     * };
     *
     * struct Bar {
     *     char  one;
     *     float two;
     *     Foo   foo1;
     *     Foo*  pFoo2;
     *     Foo*  pFoo3DontTouchMe; // shall not be serialized/deserialized
     * };
     * @endcode
     * So in order to be able to serialize and deserialize objects of those two
     * structures you would first add the mentioned method to each struct
     * definition (i.e. in your header file):
     * @code
     * struct Foo {
     *     int a;
     *     bool b;
     *     double c;
     *
     *     void serialize(Serialization::Archive* archive);
     * };
     *
     * struct Bar {
     *     char  one;
     *     float two;
     *     Foo   foo1;
     *     Foo*  pFoo2;
     *     Foo*  pFoo3DontTouchMe; // shall not be serialized/deserialized
     *
     *     void serialize(Serialization::Archive* archive);
     * };
     * @endcode
     * And then you would implement those two new methods like this (i.e. in
     * your .cpp file):
     * @code
     * #define SRLZ(member) \
     *   archive->serializeMember(*this, member, #member);
     *
     * void Foo::serialize(Serialization::Archive* archive) {
     *     SRLZ(a);
     *     SRLZ(b);
     *     SRLZ(c);
     * }
     *
     * void Bar::serialize(Serialization::Archive* archive) {
     *     SRLZ(one);
     *     SRLZ(two);
     *     SRLZ(foo1);
     *     SRLZ(pFoo2);
     *     // leaving out pFoo3DontTouchMe here
     * }
     * @endcode
     * Now when you serialize such a Bar object, this framework will also
     * automatically serialize the respective Foo object(s) accordingly, also
     * for the pFoo2 pointer for instance (as long as it is not a NULL pointer
     * that is).
     *
     * Note that there is only one method that you need to implement. So the
     * respective serialize() method implementation of your classes/structs are
     * both called for serialization, as well as for deserialization!
     */
    class Archive {
    public:
        Archive();
        Archive(const RawData& data);
        Archive(const uint8_t* data, size_t size);
        virtual ~Archive();

        template<typename T>
        void serialize(const T* obj) {
            m_operation = OPERATION_SERIALIZE;
            m_allObjects.clear();
            m_rawData.clear();
            m_root = UID::from(obj);
            const_cast<T*>(obj)->serialize(this);
            encode();
            m_operation = OPERATION_NONE;
        }

        template<typename T>
        void deserialize(T* obj) {
            Archive a;
            m_operation = OPERATION_DESERIALIZE;
            obj->serialize(&a);
            a.m_root = UID::from(obj);
            Syncer s(a, *this);
            m_operation = OPERATION_NONE;
        }

        template<typename T>
        void operator<<(const T& obj) {
            serialize(&obj);
        }

        template<typename T>
        void operator>>(T& obj) {
            deserialize(&obj);
        }

        const RawData& rawData();
        virtual String rawDataFormat() const;

        template<typename T_classType, typename T_memberType>
        void serializeMember(const T_classType& nativeObject, const T_memberType& nativeMember, const char* memberName) {
            const size_t offset =
            ((const uint8_t*)(const void*)&nativeMember) -
            ((const uint8_t*)(const void*)&nativeObject);
            const UIDChain uids = UIDChainResolver<T_memberType>(nativeMember);
            const DataType type = DataType::dataTypeOf(nativeMember);
            const Member member(memberName, uids[0], offset, type);
            const UID parentUID = UID::from(nativeObject);
            Object& parent = m_allObjects[parentUID];
            if (!parent) {
                const UIDChain uids = UIDChainResolver<T_classType>(nativeObject);
                const DataType type = DataType::dataTypeOf(nativeObject);
                parent = Object(uids, type);
            }
            parent.members().push_back(member);
            const Object obj(uids, type);
            const bool bExistsAlready = m_allObjects.count(uids[0]);
            const bool isValidObject = obj;
            const bool bExistingObjectIsInvalid = !m_allObjects[uids[0]];
            if (!bExistsAlready || (bExistingObjectIsInvalid && isValidObject)) {
                m_allObjects[uids[0]] = obj;
                // recurse serialization for all members of this member
                // (only for struct/class types, noop for primitive types)
                SerializationRecursion<T_memberType>::serializeObject(this, nativeMember);
            }
        }

        virtual void decode(const RawData& data);
        virtual void decode(const uint8_t* data, size_t size);
        void clear();
        bool isModified() const;
        void removeMember(Object& parent, const Member& member);
        void remove(const Object& obj);
        Object& rootObject();
        Object& objectByUID(const UID& uid);
        void setAutoValue(Object& object, String value);
        void setIntValue(Object& object, int64_t value);
        void setRealValue(Object& object, double value);
        void setBoolValue(Object& object, bool value);
        void setEnumValue(Object& object, uint64_t value);
        String valueAsString(const Object& object);
        String name() const;
        void setName(String name);
        String comment() const;
        void setComment(String comment);
        time_t timeStampCreated() const;
        time_t timeStampModified() const;
        tm dateTimeCreated(time_base_t base = LOCAL_TIME) const;
        tm dateTimeModified(time_base_t base = LOCAL_TIME) const;

    protected:
        // UID resolver for non-pointer types
        template<typename T>
        class UIDChainResolver {
        public:
            UIDChainResolver(const T& data) {
                m_uid.push_back(UID::from(data));
            }

            operator UIDChain() const { return m_uid; }
            UIDChain operator()() const { return m_uid; }
        private:
            UIDChain m_uid;
        };

        // UID resolver for pointer types (of 1st degree)
        template<typename T>
        class UIDChainResolver<T*> {
        public:
            UIDChainResolver(const T*& data) {
                const UID uids[2] = {
                    { &data, sizeof(data) },
                    { data, sizeof(*data) }
                };
                m_uid.push_back(uids[0]);
                m_uid.push_back(uids[1]);
            }

            operator UIDChain() const { return m_uid; }
            UIDChain operator()() const { return m_uid; }
        private:
            UIDChain m_uid;
        };

        // SerializationRecursion for non-pointer class/struct types.
        template<typename T, bool T_isRecursive>
        struct SerializationRecursionImpl {
            static void serializeObject(Archive* archive, const T& obj) {
                const_cast<T&>(obj).serialize(archive);
            }
        };

        // SerializationRecursion for pointers (of 1st degree) to class/structs.
        template<typename T, bool T_isRecursive>
        struct SerializationRecursionImpl<T*,T_isRecursive> {
            static void serializeObject(Archive* archive, const T*& obj) {
                if (!obj) return;
                const_cast<T*&>(obj)->serialize(archive);
            }
        };

        // NOOP SerializationRecursion for primitive types.
        template<typename T>
        struct SerializationRecursionImpl<T,false> {
            static void serializeObject(Archive* archive, const T& obj) {}
        };

        // NOOP SerializationRecursion for pointers (of 1st degree) to primitive types.
        template<typename T>
        struct SerializationRecursionImpl<T*,false> {
            static void serializeObject(Archive* archive, const T*& obj) {}
        };

        // Automatically handles recursion for class/struct types, while ignoring all primitive types.
        template<typename T>
        struct SerializationRecursion : SerializationRecursionImpl<T, LIBGIG_IS_CLASS(T)> {
        };

        class ObjectPool : public std::map<UID,Object> {
        public:
            // prevent passing obvious invalid UID values from creating a new pair entry
            Object& operator[](const UID& k) {
                static Object invalid;
                if (!k.isValid()) {
                    invalid = Object();
                    return invalid;
                }
                return std::map<UID,Object>::operator[](k);
            }
        };

        friend String _encode(const ObjectPool& objects);

    private:
        String _encodeRootBlob();
        void _popRootBlob(const char*& p, const char* end);
        void _popObjectsBlob(const char*& p, const char* end);

    protected:
        class Syncer {
        public:
            Syncer(Archive& dst, Archive& src);
        protected:
            void syncObject(const Object& dst, const Object& src);
            void syncPrimitive(const Object& dst, const Object& src);
            void syncPointer(const Object& dst, const Object& src);
            void syncMember(const Member& dstMember, const Member& srcMember);
            static Member dstMemberMatching(const Object& dstObj, const Object& srcObj, const Member& srcMember);
        private:
            Archive& m_dst;
            Archive& m_src;
        };

        virtual void encode();

        ObjectPool m_allObjects;
        operation_t m_operation;
        UID m_root;
        RawData m_rawData;
        bool m_isModified;
        String m_name;
        String m_comment;
        time_t m_timeCreated;
        time_t m_timeModified;
    };

    /**
     * Will be thrown whenever an error occurs during an serialization or
     * deserialization process.
     */
    class Exception {
        public:
            String Message;

            Exception(String Message) { Exception::Message = Message; }
            void PrintMessage();
            virtual ~Exception() {}
    };

} // namespace Serialization

#endif // LIBGIG_SERIALIZATION_H
